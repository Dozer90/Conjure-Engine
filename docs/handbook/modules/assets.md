# Assets module

🛠 **Engineering handbook** · 📘 API: [Assets](../../api/assets/index.md), [Referencing and loading](../../api/assets/referencing-and-loading.md), [Database, importers and hot reload](../../api/assets/database-and-import.md), [Cooking, caches and packs](../../api/assets/cooking-and-packs.md)

`conjure::assets` (target `conjure_assets`, wave 2) is the asset pipeline: stable uuid identity through
`.meta` sidecars, importers that cook sources into hash-validated blobs, an asynchronous loader with
main-thread finalisation, the non-owning `asset_ref<T>` and owning `asset_handle`, explicit unloading, hot
reload with dependency propagation and a deterministic export pack. It depends on `conjure::core` only and
knows no concrete asset type beyond `binary_asset`; renderer, audio, physics, scene, scripting and plugins
register theirs into it. This chapter is for people changing the module: layout, algorithms, threading, the
extension points and the tests. The binding design is [`05_assets.md`](../../design/05_assets.md); the
finer behavioural notes are in `engine/src/assets/README.md`.

## Contents

- [1. Shape of the module](#1-shape-of-the-module)
- [2. Identity and the slot table](#2-identity-and-the-slot-table)
- [3. Database: scan, hashing and import](#3-database-scan-hashing-and-import)
- [4. Cache layout and validation](#4-cache-layout-and-validation)
- [5. The load pipeline](#5-the-load-pipeline)
- [6. Publish, retire and collect](#6-publish-retire-and-collect)
- [7. Threading](#7-threading)
- [8. Hot reload and the watcher job](#8-hot-reload-and-the-watcher-job)
- [9. Cooker and pack determinism](#9-cooker-and-pack-determinism)
- [10. Extension points](#10-extension-points)
- [11. Failure handling](#11-failure-handling)
- [12. Tests](#12-tests)
- [13. Open items](#13-open-items)
- [Related](#related)

## 1. Shape of the module

`engine/src/assets/CMakeLists.txt` declares the module with `conjure_add_module(assets ... PUBLIC_DEPS
conjure::core)`; every public header also compiles alone through `conjure_header_check(conjure_assets)`.
Public headers live under `engine/include/conjure/assets/`, one `.cpp` per header under `engine/src/assets/`.

| Header | Main types | Area |
|---|---|---|
| `assets_error.h` | `asset_error_code`, `make_asset_error`, the `"assets"` category | shared |
| `asset_types.h` | `asset_id`, `asset_type_id`, `make_asset_type_id` (constexpr FNV-1a, same constants as `core::hash`), `asset_state`, `asset_load_priority`, `asset_type_flags`, `asset_type_traits`, `CONJURE_DECLARE_ASSET_TYPE`, constants | shared |
| `asset_payload.h`, `asset_slot.h`, `asset_ref.h` | `asset_payload`, `asset_slot`, `asset_ref<T>`, `asset_handle`, the `ASSET_REF` reflect override | runtime |
| `asset_loader.h`, `asset_type_registry.h` | `asset_loader`, `typed_asset_loader`, `asset_type_desc`, `asset_type_registry`, `asset_type_registry_hooks` | runtime |
| `asset_source.h`, `asset_manager.h` | the four sources, `asset_manager`, batches, events, stats | runtime |
| `asset_importer.h`, `asset_meta.h`, `asset_database.h`, `asset_cache.h`, `cooked_blob.h` | importers and `import_context`, the sidecar, the database, the blob cache, the blob format | editor and development |
| `file_watcher.h`, `hot_reload.h`, `asset_cooker.h`, `asset_pack.h` | `polling_file_watcher`, `hot_reload_controller`, `asset_cooker`, `pack_writer` / `pack_reader` | editor and export |
| `asset_extension_desc.h`, `binary_asset.h` | plugin records (R37), the built-in type | integration |

Private: `asset_slot_table.{h,cpp}` (slots plus `slot_data`), `completion_queue.h`, `load_pipeline.{h,cpp}`
(`load_cpu_stage`, shared by jobs and `load_blocking`), `database_snapshot.{h,cpp}` (`db.bin`),
`binary_io.h` (little-endian field I/O for every on-disk format), `fnv1a.h`, `path_filters.h`
(`is_temporary_path`). Everything is compiled in every configuration (D16); the runtime simply never
constructs the database, watcher, cooker or importers.

## 2. Identity and the slot table

### 2.1 Data layout

| Structure | Where | Contents |
|---|---|---|
| `asset_slot` (public, 1 per uuid) | `core::stable_vector` in `detail::asset_slot_table` | atomics `object`, `version`, `state`, `strongRefs`, `type`; plain `id`, `index` |
| `detail::slot_data` (main thread only) | a parallel `stable_vector`, indexed by `slot.index` | `payload`, `cpuOutput`, `dependencyIds`, `dependencyHandles`, `pendingDependencyHandles`, `lastError`, `job`, `priority`, `sequence`, `payloadBytes`, `generation`, flags (`queued`, `jobOutstanding`, `awaitingFinalise`, `pinned`, `isReload`, `dependencyFailureWarned`, `notFoundLogged`) |
| `asset_ref<T>` | by value in components | `asset_id` first (16 bytes), `const asset_slot *` second; `static_assert`ed to 24 bytes and trivially copyable |
| `asset_handle` | by value in systems | `asset_slot *`; copy and move adjust `strongRefs` with atomics |

Slots are created on first `bind` / `load` (never for the null uuid), live in `stable_vector`s and are never
freed before the manager is destroyed (D3), so refs and handles need no generation counter and `get()` is one
acquire load of `slot.object`. The field order of `asset_ref` is a contract (R25): `core::reflect` serialises
an `ASSET_REF` field as its leading uuid and zeroes the rest on read, and the ECS serialiser records the same
offsets for the scene bind pass (`asset_manager::bind_slot`).

### 2.2 Type identity

`make_asset_type_id(name)` is a `constexpr` 64-bit FNV-1a implemented in the module so blob headers and pack
tables never depend on another module's hashing shape; `asset_type_traits<T>` (from `CONJURE_DECLARE_ASSET_TYPE`)
exposes `NAME` and `id()`, and the `declared_asset_type` concept gates every typed entry point. The registry
keeps each `asset_type_desc` in its own heap block (`vector<unique_ptr>` plus a `vector<const desc *>` view and
a `flat_hash_map` index), so a `find()` pointer survives the removal of any other type, which is why `types()`
returns `span<const asset_type_desc *const>` rather than the span of values the design sketched.

## 3. Database: scan, hashing and import

### 3.1 Scan

`asset_database::scan()` (main thread) loads `db.bin` if valid, then walks each mount in sorted path order.
For every regular file that is not a `.meta` and not an atomic-write temporary (`detail::is_temporary_path`:
`*.tmp` from the cache and pack, `*.tmp-<uuid>` from `core::vfs::write_file_atomic`) it reads or creates the
sidecar (new uuid, importer chosen by extension, `default_settings()`; `NO_IMPORTER` still gets a uuid). A
second meta declaring a uuid already owned becomes `DUPLICATE_UUID` and a corrupt meta `META_INVALID`; both
records carry a **transient** uuid that is never persisted, so they are browsable and loads of them fail with
their status. A uuid edited in an existing meta is refused (ERROR, in-memory uuid kept). Removed sources
become `MISSING_SOURCE`; the record stays.

### 3.2 Hashing and the stale rule

`(size, mtime)` equal to the snapshot skips hashing, except for sources with side files, which are always
re-hashed because a side file's change is invisible in the owner's stat. Hashing runs through
`jobs->parallel_for` when a job system was given, inline otherwise: `sourceHash` = `hash_bytes` over the
source bytes followed by every side file sorted by path; `settingsHash` = `canonical_settings_hash`
(sorted keys, minified). A primary is **stale** when its blob header is missing or unreadable or any of
`sourceHash`, `settingsHash`, `importerVersion`, `typeVersion`, `type` or `formatVersion` differs from the
expectation (`asset_cache::is_valid`). The scan then deletes leftover `*.tmp` blobs and saves the snapshot.

### 3.3 Import

`import(ids, jobs)` maps sub-asset ids to their primaries, skips records that cannot import (`MISSING_SOURCE`,
`META_INVALID`, `DUPLICATE_UUID`, `NO_IMPORTER`) and runs one `source_job` per source through
`jobs.parallel_for`, bracketed by `importer_registry::begin_imports()` / `end_imports()`. Each job builds an
`import_context` (settings from the meta, a `sub_asset_table` seeded with the meta's entries, a resolver into
the database), calls `importer.import()`, builds one blob per `imported_asset` (header from the record,
`make_cooked_blob`), and writes it through `asset_cache::write`. The main thread then merges: new sub-asset
uuids into the meta (rewritten sorted; append-only, so a dropped sub-asset keeps its uuid for when it
returns), record types and display names, side-file owners, `payloadHash` (the report's `changed` list is
every asset whose hash differs or is new), then rebuilds the indexes and graphs, writes `import_log.txt` and
saves the snapshot. A failing source is recorded `IMPORT_FAILED` with its previous blob kept; the importer
named in the meta is authoritative, so a hand-edited importer id is honoured (unknown name: `NO_IMPORTER`).

### 3.4 Graphs and snapshot

Dependencies are stored per entry (sorted, unique) and compiled into forward and reverse CSR arrays
(`firstEdge` / `edgeCount` per entry into `mForwardEdges`, likewise for `mReverseEdges`) after every import.
Cycle detection is Tarjan's strongly connected components in the **iterative** form (explicit stack), so depth
is not bounded by the C stack; every member of a non-trivial component is flagged `CYCLIC_DEPENDENCY`.
`dependency_closure` is a DFS over the forward graph returning uuid order; `collect_dependents` a BFS over the
reverse graph returning roots first, depth ascending, deduplicated. The snapshot (`detail::database_snapshot`:
mount names, records, dependency lists, side files, little-endian with a versioned header) is purely a cache:
missing, corrupt, another version or different mounts all log INFO and fall back to a full scan.

## 4. Cache layout and validation

```
<project>/.conjure/cache/
  db.bin                               database snapshot (rebuildable)
  import_log.txt                       last import errors and warnings
  <platform>/blobs/<xx>/<uuid>.cab     one cooked blob per asset, xx = first two hex chars of the uuid
```

A `.cab` is the 80-byte `cooked_blob_header` (magic `CJAB`, written field by field through `binary_io.h`),
`dependencyCount` uuids, then the payload at `align_up(80 + 16 * n, 16)`. `parse_cooked_blob` validates magic,
format version, every declared range against the buffer **in subtraction form** (so a corrupt `payloadSize`
or `dependencyCount` cannot wrap the sum past the buffer and reach a 64 GiB reserve or a wild read; the
regression tests pin this) and the payload hash. `asset_cache::read` additionally rejects a header whose `id`
is not the uuid of the path. Every write is `<path>.tmp` + rename; `remove_temporaries()` clears what a crash
left behind and `scan()` calls it.

## 5. The load pipeline

### 5.1 States

`UNLOADED -> QUEUED -> LOADING -> FINALISING -> LOADED | FAILED`; `LOADED -> RELOADING -> LOADED` (or back to
`LOADED` with the old payload on failure). `LOADED` and `FAILED` are terminal (`is_terminal`); `RELOADING` is
not, which gives hot reload its dependencies-first order for free.

### 5.2 `update()`

1. **Dispatch**: while `jobsInFlight < maxJobsInFlight`, pop the highest-priority queue (three FIFO queues,
   `slot_data::sequence` breaks ties; `load()` on a queued slot with a higher priority moves it), set
   `LOADING`, bump `slot_data::generation`, schedule a job. The job takes the source lock (shared) for the
   `asset_source::read`, then runs `detail::load_cpu_stage` (type check against the slot's type when it is
   not `INVALID`, `find_loader`, `load_cpu`) and pushes a `load_completion {slotIndex, generation, header,
   dependencies, output}` to the `completion_queue`.
2. **Drain completions**: one mutex-guarded vector swapped out once. Every scheduled job produces exactly one
   completion and `drain_completions()` alone decrements `jobsInFlight`, whatever became of the slot; a
   completion whose `generation` no longer matches (the slot was reset by `shutdown()` or taken over by
   `load_blocking`) is dropped. On success the CPU output and header dependencies are stored, a handle is
   acquired for each dependency (`load(dep, same priority)` into `pendingDependencyHandles`), the load-time
   cycle check runs (iterative DFS that follows only slots whose stage has completed, so their dependency
   list is the one from the new blob, never slots still queued or in flight) and the slot becomes
   `FINALISING` with `awaitingFinalise`.
3. **Finalise** under `finaliseBudget`, at least one slot per call, `HIGH` first and FIFO within a level: a
   slot whose dependencies are all terminal gets `loader.finalise(finalise_context {id, dependencies,
   frameIndex, isReload}, cpuOutput)`; success publishes (6), failure fails the slot (or, on a reload, keeps
   the old payload, returns to `LOADED` and queues `RELOAD_FAILED`).
4. **Retire pass** (6).
5. **Events**: the queued `asset_event`s are delivered to subscribers in order.

`load_blocking` runs the same stages inline, dependencies first (depth first, cycle guarded), takes over a
`QUEUED` slot, waits up to `blockingTimeout` for a job that already owns the slot (`TIMEOUT`) and logs one WARN
per frame when called inside a frame (R23). A FAILED dependency never fails its dependent: the dependent
finalises, reads the fallback through its ref and one WARN is logged per slot.

## 6. Publish, retire and collect

`publish(slot, data, payload)`: move the payload into `slot_data::payload`, `object.store(ptr, release)`,
`version++`, state `LOADED`, swap `pendingDependencyHandles` into `dependencyHandles`, push the previous payload
(if any) onto `retired` with the current `frameIndex`, queue `LOADED` or `RELOADED`. The retire pass destroys
entries with `retireFrame + RETIRE_FRAMES <= frameIndex`, so a pointer from `get()` is valid until the end of
the next frame (D4) and the test with `counted_asset` pins the exact frame.

`collect()` loops until no change: every `LOADED` slot with `strongRefs == 0` and not `pinned` gets
`object = nullptr`, `version++`, `UNLOADED`, its payload retired and its dependency handles released (which can
make the dependencies collectable in the next iteration). `UNLOADED` events fire on the following `update()`.
Fallbacks are created lazily from `loader.create_fallback()` (or at registration with `PINNED_FALLBACK`
through `asset_type_registry_hooks::onRegistered`), live outside the slot table and die with the type.
`shutdown()` refuses new work (`SHUTTING_DOWN`), waits for jobs in flight, discards their completions and
returns queued slots to `UNLOADED`; the destructor asserts that no external handle is alive (debug) and logs
and continues in Distribution.

## 7. Threading

One rule (D13): every mutating call on `asset_manager`, `asset_database`, `hot_reload_controller`,
`asset_cooker`, the registries and the watcher is main-thread only (asserted).

| Shared state | Guard |
|---|---|
| slot table | stable addresses; creation main thread only; reads lock-free through the atomics in `asset_slot` |
| completion vector | one `std::mutex`, swapped once per `update()` |
| source list | `std::shared_mutex` held shared for the whole `asset_source::read`, exclusive by `swap_source` / `add_source`, so a source is never destroyed under a running read (a job that has not started resolves the new source) |
| database | `std::shared_mutex`; lookups anywhere, mutations main thread; the stale check reads blob headers outside the lock |
| `sub_asset_table` | `std::mutex`; new uuids allocated by importer jobs, merged on the main thread |
| watcher result table and ready list | `std::mutex`; `add_root` waits for a pass in flight |
| `importer_registry::mImportsInFlight` | atomic; `unregister_importer` refused while non-zero |

Safe from any thread: `asset_ref` copy / `get()` / `state()` / `version()`, `asset_handle` copy / move /
destroy, `asset_type_registry::find*` and `importer_registry::find*` between registrations,
`asset_source::read`, `asset_database` lookups, `asset_cache` reads and writes. Jobs run the blob read plus
`load_cpu_stage`, importer `import()`, source hashing and the watcher stat pass. No `std::thread` is created
here; everything goes through `core::jobs::job_system` (0 workers = inline, which most tests use).

## 8. Hot reload and the watcher job

`polling_file_watcher::poll()` schedules one stat pass on the job system when `interval` has elapsed since the
last pass started (interval 0 means every poll) and applies the previous pass's table: a path whose
`(size, writeTime)` differs from the snapshot is moved to `mPending`; it is reported `MODIFIED` / `ADDED` only
when the **next** pass sees the same pending value (one stable interval of debounce), and a pending value that
changes again restarts the wait. Removals go through `mPendingRemoval` the same way, and a change reverted
before it is stable is never reported. The first `poll()` without a `snapshot()` only records the baseline.
Temporaries (`path_filters.h`) are never tracked. `scan_now()` runs a pass synchronously for tests.

`hot_reload_controller::update()` (after `asset_manager::update()`): `poll()`, classify each change (a `.meta`
change re-reads the meta and marks the owner dirty when settings or importer changed; a source change marks it
dirty; a side file change marks every owner through `owners_of_side_file`; `ADDED` calls `add_file`;
`REMOVED` calls `mark_source_missing`, and a later `ADDED` of the same path calls `mark_source_present`), add
the slots `FAILED` with `CACHE_MISS` (`asset_manager::failed_ids`), `database.import(dirty)` synchronously,
then `collect_dependents(changed)` and, in that depth order, `manager.reload(id)` for every `LOADED` slot that
changed or whose type has `RELOAD_WITH_DEPENDENCIES`. A changed asset whose slot is still in flight is
deferred (`hot_reload_report::deferred`) and reloaded by a later `update()` once it is `LOADED`.

## 9. Cooker and pack determinism

`asset_cooker::cook`: validate settings (`INVALID_ARGUMENT` for an empty output directory, `NOT_FOUND` for an
unknown root), `scan()`, roots = `roots` + `alwaysInclude` + every `engine://` record (or all records with
`includeAll`), then **alternate** `dependency_closure` and `import` until the closure stops growing, because a
source's dependencies are only known once it has been imported. Drop `EXCLUDE_FROM_PACK` types, fail with
`IMPORT_FAILED` and write nothing if any member failed (every failure in the report; the good imports stay in
the cache), otherwise `pack_writer::add` every blob (re-verifying `payloadHash`), `write` the pack and the
manifest, both `tmp + rename`.

`pack_writer::build` sorts entries by uuid, deduplicates payloads by `(payloadHash, payloadSize, bytes)`,
aligns each payload to 16 bytes and writes the 64-byte header, the 72-byte TOC entries, the dependency table,
the string table and the payloads; `contentHash` covers every byte after the header. There are no timestamps
and no absolute paths, so two cooks of the same content are byte-identical (`test_asset_cooker`,
`test_asset_pack`). `pack_reader::open` validates magic, format version, every table range in subtraction form,
TOC order and the content hash before serving anything; payloads are then read positionally from the file (or
from the buffer after `open_bytes`) and hash-checked per `read`.

## 10. Extension points

| Point | How |
|---|---|
| New asset type | `CONJURE_DECLARE_ASSET_TYPE` + a `typed_asset_loader` + `register_type(make_asset_type_desc<T>(...))`; see 📘 API: [Database, importers and hot reload](../../api/assets/database-and-import.md) |
| New source format | an `asset_importer`; declare dependencies through `find_asset_by_path` / `sub_asset_id`, side files through `read_side_file` |
| New blob provider | derive `asset_source` (`contains`, `type_of`, `find_by_path`, `read`; `read` must be thread-safe) and pass it to `swap_source` / `add_source` |
| Native file watching | derive `file_watcher` (`add_root`, `snapshot`, `poll`); the controller only needs debounced changes |
| Plugin modules | `importer_extension_desc` / `asset_type_extension_desc` records (R37) instantiated by the engine's assets consumer through `register_*_extension` |
| Manager plumbing | `asset_type_registry::set_hooks` (`isInUse`, `onRegistered`, `onUnregistered`) is installed by the manager; not for general use |

## 11. Failure handling

Nothing throws and no failure crashes a frame (D14). The full table is in `05_assets.md` section 6; the
cases that shape the code:

| Situation | Behaviour |
|---|---|
| unknown uuid | `FAILED(NOT_FOUND)`, one ERROR per uuid (`notFoundLogged`) |
| blob missing in the editor | `FAILED(CACHE_MISS)`; the controller re-imports and reloads on its next update |
| blob or pack corrupt | cache: stale, re-imported, WARN; pack: `open()` fails `PACK_CORRUPT`, the runtime refuses to start |
| `bind<T>` vs recorded type, or blob type vs slot type | unbound ref + ERROR once per `(uuid, type)`; or `FAILED(TYPE_MISMATCH)` |
| loader failure | `FAILED(LOAD_FAILED` / `FINALISE_FAILED)`, WARN, event `FAILED`; `get_or_fallback` serves the fallback |
| failed dependency | dependent still finalises; WARN once |
| cycle | every member `CYCLIC_DEPENDENCY` at import (Tarjan) and again at load (DFS) |
| reload failure | old payload stays, `LOADED`, `RELOAD_FAILED` |
| crash mid-write | `*.tmp` ignored by scan and watcher, removed on the next `scan()` |
| handles alive at manager destruction | assert in Debug / Development, ERROR log in Distribution |

## 12. Tests

```
python tools/build.py --build-dir build/mod_assets --target conjure_assets_tests --test -R assets
```

`tests/assets/` builds `conjure_assets_tests` (`conjure_add_test_module(assets)`); one `test_<header>.cpp`
mirrors every public header and `test_asset_handle_lifetime`, `test_asset_manager_load`, `test_import_cache`
and `test_export_roundtrip` cover the cross-cutting topics. Everything is headless over
`core::vfs::memory_file_system` with a 0-worker (inline) job system and, for the cases tagged `[parallel]`,
4 workers. `assets_test_support.h` provides the deterministic `test_asset` type (a line-based text format:
`dep <uuid>`, `dep-path <asset path>`, `sub <name>`, `side <relative path>`, `fail`, `warn <text>`),
`test_importer`, `failing_importer`, loaders that can be told to fail or block (`controls()`), a
`counted_asset` that counts live instances so retirement can be observed, `make_test_blob`, `test_id`,
`settle` and the `pipeline_fixture` (file system, registries, database, cache, manager over a
`cache_asset_source`).

| File | Proves |
|---|---|
| `test_asset_ref`, `test_asset_handle_lifetime`, `test_asset_slot` | 24-byte layout and the reflect contract, strong count exactness, unbound behaviour, the destructor assert |
| `test_asset_manager`, `test_asset_manager_load` | bind rules, priorities, budget (`finaliseBudget = 0` finalises exactly one per update), dependency order, failed dependency, cycles (both paths), batches, collect, retirement after exactly `RETIRE_FRAMES`, events, `flush`, `shutdown`, 1,000 blobs through 4 workers; two regressions on reversed-dependency reloads |
| `test_asset_database`, `test_import_cache` | scan, duplicate policy, moves, `MISSING_SOURCE`, refused uuid change, CSR graphs, a 200-member Tarjan component, side files, snapshot round trip, parallel import of 1,000 sources equal to serial byte for byte, staleness on each input |
| `test_asset_cache`, `test_cooked_blob` | path layout, `is_valid` per field, truncation, flipped bytes, mislabelled uuid, the wrap-around regressions |
| `test_file_watcher`, `test_hot_reload` | debounce, temporaries, `add_root` during a pass; leaf / flagged / unflagged propagation, settings and importer changes, side files, removal and return, deferral while in flight, `CACHE_MISS` self-heal |
| `test_asset_source`, `test_asset_pack`, `test_asset_cooker`, `test_export_roundtrip` | the source contract over all four sources, `swap_source` under a parked read, pack validation and dedup, closure rules, all-or-nothing, byte-identical cooks, pack loads equal to cache loads |
| `test_asset_extension_desc`, `test_binary_asset`, `test_asset_type_registry`, `test_asset_importer`, `test_asset_loader`, `test_asset_meta`, `test_asset_payload`, `test_asset_types`, `test_assets_error` | the remaining headers |

## 13. Open items

Native OS watchers behind `file_watcher` (for projects above ~50k files); zero-copy payload aliasing and
memory-mapped packs; LRU eviction (today `collect()` is explicit); stripping database, importer and cooker
sources from Distribution builds; streaming loads; several packs per export; the editor commands
"regenerate uuid" / "repair meta" over the functions this module exposes. The integration tests named in the
design (`test_assets_renderer_headless`, `test_assets_scene`) and the benchmarks (`bench_asset_ref_get`,
`bench_asset_load`, `bench_pack_lookup`) land with the modules they need.

## Related

- design records: [`05_assets.md`](../../design/05_assets.md) (binding),
  [`09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md) R10, R23-R25, R33-R37, R43-R45
- README: `engine/src/assets/README.md`
- 📘 API: [Assets](../../api/assets/index.md), [Referencing and loading](../../api/assets/referencing-and-loading.md),
  [Database, importers and hot reload](../../api/assets/database-and-import.md),
  [Cooking, caches and packs](../../api/assets/cooking-and-packs.md)
- handbook: [Core module](core.md) (`stable_vector`, `flat_hash_map`, jobs, vfs, reflect),
  [Module system](../module-system/index.md) (extension records), [Architecture](../architecture.md),
  [Testing](../testing.md)
