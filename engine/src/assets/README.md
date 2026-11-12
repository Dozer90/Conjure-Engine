# assets

The asset pipeline (`conjure::assets`, target `conjure_assets`, wave 2): stable uuid identity with
`.meta` sidecars, importers that cook sources into hash-validated blobs, an asynchronous loader with
main-thread finalisation, non-owning `asset_ref<T>` / owning `asset_handle`, explicit unloading,
hot reload with dependency propagation and a deterministic export pack. The binding design is
`docs/design/05_assets.md`; the cross-area decisions it cites are in
`docs/design/09_cross_area_resolutions.md` (R10, R23-R25, R33-R37, R43-R45). The module depends on
`conjure::core` only and knows no concrete asset type beyond `binary_asset`.

## Types

| Header | Types | Purpose |
|---|---|---|
| `assets_error.h` | `asset_error_code`, `make_asset_error` | the "assets" error category |
| `asset_types.h` | `asset_id`, `asset_type_id`, `make_asset_type_id`, `asset_state`, `asset_load_priority`, `asset_type_flags`, `asset_type_traits`, `CONJURE_DECLARE_ASSET_TYPE`, constants | identity and shared vocabulary |
| `asset_payload.h` | `asset_payload`, `make_payload`, `make_null_payload`, `payload_as` | the one type erasure of the module |
| `asset_slot.h` | `asset_slot` | lock-free per-uuid residency record (never freed before shutdown) |
| `asset_ref.h` | `asset_ref<T>` (24 bytes, trivially copyable; uuid first, slot pointer second), `asset_handle`, `ASSET_REF_TYPE_NAME` + `reflect_kind_override<asset_ref<T>>` (kind `ASSET_REF`, R25) | non-owning hot-path reference; owning, ref-counted handle; reflected component fields serialise the uuid and read back unbound |
| `asset_loader.h` | `load_context`, `finalise_context`, `asset_loader`, `typed_asset_loader<T, Intermediate>` | blob -> payload in two stages |
| `asset_type_registry.h` | `asset_type_desc`, `make_asset_type_desc<T>`, `asset_type_registry`, `asset_type_registry_hooks` | type registration (max 256) |
| `asset_importer.h` | `sub_asset_table`, `import_context`, `imported_asset`, `import_output`, `asset_importer`, `importer_registry` | source bytes -> cooked payloads |
| `asset_extension_desc.h` | `importer_extension_desc`, `asset_type_extension_desc`, `register_*_extension` | trivially copyable plugin records (R37) |
| `asset_meta.h` | `sub_asset_entry`, `asset_meta`, `parse/serialize/read/write_asset_meta`, `canonical_settings_hash`, `meta_path_of` | the `.meta` sidecar |
| `cooked_blob.h` | `cooked_blob_header` (80 bytes), `cooked_blob`, `make/write/parse_cooked_blob`, `hash_bytes` | the cooked format |
| `asset_cache.h` | `cache_expectation`, `asset_cache` | `<cacheRoot>/<platform>/blobs/<xx>/<uuid>.cab`, tmp + rename |
| `asset_source.h` | `asset_source`, `memory_asset_source`, `cache_asset_source`, `pack_asset_source`, `composite_asset_source` | blob providers behind one interface |
| `asset_pack.h` | `pack_header`, `pack_toc_entry`, `pack_stats`, `pack_writer`, `pack_reader` | `content.cpak` + `manifest.json` |
| `asset_database.h` | `asset_record`, `asset_record_status`, `asset_mount`, `split_asset_path`, `make_asset_path`, `scan_report`, `import_report`, `asset_database` | mounts, scan, import, graphs, snapshot, editing |
| `asset_manager.h` | `asset_manager_desc`, `asset_event*`, `asset_subscription`, `asset_load_batch`, `asset_manager_stats`, `asset_manager` | the runtime manager |
| `file_watcher.h` | `file_change`, `file_watcher`, `polling_file_watcher` | debounced change detection |
| `hot_reload.h` | `hot_reload_report`, `hot_reload_controller` | watcher -> re-import -> reload dependents |
| `asset_cooker.h` | `cook_settings`, `cook_failure`, `cook_report`, `asset_cooker` | all-or-nothing export |
| `binary_asset.h` | `binary_asset`, `binary_importer`, `binary_loader`, `register_binary_asset_type` | the built-in type |
| `assets.h`, `log_category.h` | umbrella, `LOG_ASSETS` | |

Private: `asset_slot_table.{h,cpp}` (slots + `slot_data`), `completion_queue.h`, `load_pipeline.{h,cpp}`
(`load_cpu_stage`, shared by jobs and `load_blocking`), `database_snapshot.{h,cpp}` (`db.bin`),
`binary_io.h` (little-endian field I/O for every on-disk format), `fnv1a.h`, `path_filters.h`
(`is_temporary_path`: `*.tmp` and `*.tmp-<uuid>` atomic-write leftovers are never sources, never watched).

`importer_registry::find_for_path(path)` picks the importer whose registered extension is the longest
suffix of the file name (a `.scene.json` importer wins over a `.json` one, R45); the database uses it at
scan, at `add_file` and for a meta without an importer, while `find_for_extension` keeps matching one
extension exactly.

Deviations from `05_assets.md` worth knowing: `asset_type_registry::types()` returns
`std::span<const asset_type_desc *const>` (descriptions live in their own heap blocks so a `find()`
pointer survives the removal of any other type, as the header promises); `hot_reload_controller`
defers the reload of an asset whose slot is still in flight until it is LOADED; the database offers
`mark_source_missing` / `mark_source_present` so a removed file becomes `MISSING_SOURCE` in a running
session, and `contains` / `type_of` / `status_of` as allocation-free lookups for the load path.

## Data flow

```
source + <source>.meta --scan--> asset_record (hash, importer, settings) --import (job)--> cache blob
cache / pack / memory source --load() job: read + load_cpu--> completion --update(): finalise--> publish
asset_slot.object (atomic) <-- asset_ref<T>::get()            events LOADED/RELOADED/FAILED/UNLOADED
watcher poll -> hot_reload_controller -> database.import(dirty) -> manager.reload(changed + flagged dependents)
asset_cooker: scan -> closure -> import stale -> pack_writer (uuid-sorted TOC, dedup payloads) -> content.cpak
```

Load states: `UNLOADED -> QUEUED -> LOADING -> FINALISING -> LOADED` (or `FAILED`); a reload goes
`LOADED -> RELOADING -> LOADED` with the old payload published until the new one finalises; a
failed reload keeps the old payload and fires `RELOAD_FAILED`. A dependent finalises only once every
dependency is terminal; a FAILED dependency does not fail it (one WARN; read the fallback through the
ref). `update()` finalises at least one slot per call and stops once `finaliseBudget` is spent.
Payloads replaced by a publish or dropped by `collect()` live `RETIRE_FRAMES` (2) more frames, so a
pointer from `get()` is valid until the end of the next frame.

Cache validity is content based: `sourceHash` (source bytes plus side files, sorted by path),
`settingsHash` (canonical JSON), importer version, type version and blob format version are all
written into the blob header and compared at scan; mtime + size only decides whether to re-hash
(a source with side files is always re-hashed, since their changes are invisible in its own stat).
Every cache, meta, pack and snapshot write is `tmp + rename`. The cooker alternates
`dependency_closure` and `import` until no member of the closure is left to import (each source at
most once per cook), because a source's dependencies are only known once it has been imported; only
members of the final closure can fail the cook.

Records without a usable uuid (corrupt, duplicate or unwritable meta) carry a transient uuid that is
never persisted, so they are browsable (`records()`, `find_by_path`) and loads of them fail with
`META_INVALID` / `DUPLICATE_UUID`. A uuid edited in an existing `.meta` is refused at scan (ERROR,
in-memory uuid kept), except for a meta the scan itself created and for the uuid of a known asset
that left its old place (moved here). Removed sources become `MISSING_SOURCE` (at scan, or at once
through the hot reload controller); loaded payloads stay loaded and fresh loads fail `NOT_FOUND`
until the file is back. The importer named in the `.meta` is authoritative at import time: an importer change made
by hand re-imports with the new importer (an unknown name makes the record `NO_IMPORTER` until the
meta is fixed). A blob header names no importer, so an imported primary switched to another importer
(`write_meta()`, or a meta that differs from the previous record at scan) stays stale until imported. A
meta the scan creates over a known asset's path logs a WARN naming both uuids. Blob and pack readers validate every declared range in subtraction form, so a corrupt
header cannot wrap a sum past the buffer and reach an allocation or a wild read; the cycle checks
(database Tarjan pass, manager load-time DFS) are iterative, so dependency depth is not bounded by
the stack.

## Threading

One rule (D13): every mutating call on `asset_manager`, `asset_database`, `hot_reload_controller`,
`asset_cooker`, the registries and the watcher is main-thread only (asserted). Safe from any thread:
`asset_ref` copy / `get()` / `state()` / `version()`, `asset_handle` copy / move / destroy (atomic
counter), `asset_type_registry::find*` and `importer_registry::find*` between registrations,
`asset_source::read`, `asset_database` lookups (shared lock), `asset_cache` reads and writes. Jobs
run the blob read + `load_cpu_stage`, importer `import()`, source hashing and the watcher stat pass.
Shared state and its guard: the completion vector (mutex, swapped once per `update()`), the source
list (shared mutex, held for the whole `asset_source::read` so `swap_source` / `add_source` can never
destroy a source under a running read), the watcher result table (mutex; `add_root` waits for a pass
in flight), the database (shared mutex; the stale check reads blob headers outside it), `sub_asset_table`
(mutex). Slots never move and are never freed, so `asset_ref` reads need no lock. In-flight accounting
belongs to `drain_completions()` alone: every scheduled job produces one completion, which is counted
whatever became of its slot; the load-time cycle check only follows slots whose stage has completed
(their dependency list is the one from the new blob), never slots still queued or in flight.

`load_blocking` runs the pipeline inline (dependencies first, cycle guarded) and waits for a job
that already owns the slot up to `blockingTimeout` (`TIMEOUT`); inside a frame it logs one WARN per
frame (R23). `shutdown()` waits for in-flight jobs and discards their completions; the destructor
asserts that no external `asset_handle` is alive.

## Testing

`tests/assets/` builds `conjure_assets_tests`:

```
python tools/build.py --build-dir build/mod_assets --target conjure_assets_tests --test -R assets
```

Everything runs headless over `core::vfs::memory_file_system` with a `job_system` of 0 workers
(inline) and, for the asynchronous cases, 4 workers. `tests/assets/assets_test_support.h` provides
the deterministic `test_asset` type (`test_importer`, `test_loader`, dependency declarations through
a tiny text format, a failing importer, a loader that can be told to fail or block) and the
`pipeline_fixture` (file system, registries, database, cache, manager). One test file mirrors every
public header; `test_asset_handle_lifetime`, `test_asset_manager_load`, `test_import_cache` and
`test_export_roundtrip` cover the cross-cutting topics named in the design. `test_asset_cooker`
proves that two cooks of the same content are byte-identical.
