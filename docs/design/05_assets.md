# Design E - Asset pipeline (`conjure::assets`)

Status: **final**. Judge's synthesis of the performance-first and robustness-first proposals.
Module: `conjure_assets`, wave 2, namespace `conjure::assets`, depends on `conjure_core` only.
Owning modules (renderer, audio, scene, scripting, plugins) register their asset types *into* this
module; it never includes their headers.

---

## 1. Overview and decisions

### 1.1 What the module does

Gives every source file and sub-asset a stable uuid (`.meta` sidecar); imports sources into
**cooked binary blobs** cached under `.conjure/cache` (editor/dev only; loading only ever reads
blobs, in editor and runtime alike); loads blobs asynchronously through `core::jobs` with
main-thread finalisation under a time budget; exposes `asset_ref<T>` (non-owning, O(1) `get()`) and
`asset_handle` (owning); hot reloads with dependency propagation; cooks a deterministic pack.

### 1.2 Binding decisions

| # | Decision | Rationale |
|---|---|---|
| D1 | Identity is `asset_id = core::uuid` (v4). Paths are editor metadata. Sub-assets (glTF meshes, materials, embedded textures) get their own uuid, persisted in the owner's `.meta` under a stable `subPath` (`mesh/Body`). | Renames and moves never break references; re-import never silently changes references. |
| D2 | `asset_ref<T>` is **non-owning**: `{asset_id, const asset_slot *}` (24 bytes, trivially copyable). `asset_handle` is the **owning**, type-erased, ref-counted RAII type. | Components live in ECS chunks and are memcpy'd, cloned and serialised 10,000 at a time; an atomic RMW per copy is unacceptable. Residency is owned by scene manager, renderer and editor through handles. |
| D3 | Slots are allocated once per uuid and **never freed before `asset_manager` shutdown**. `get()` is one atomic pointer load (`mSlot->mObject`), null unless LOADED. No generation counters. | Removes the ABA/generation class of bugs; 128 bytes per touched uuid is negligible. |
| D4 | Publishing a payload (first load or reload) **swaps the pointer and bumps a per-slot `mVersion`**; the previous payload is retired for `RETIRE_FRAMES = 2` frames, then destroyed. A pointer from `get()` is valid until the end of the next frame; anything longer-lived holds the `asset_ref`. | No per-type in-place swap code (the biggest implementation risk in the performance-first proposal). Dependents detect change with one `uint32_t` compare during work they already do. |
| D5 | Unloading is **explicit**: `asset_manager::collect()` retires payloads of LOADED slots with zero strong refs and no pin. Nothing is unloaded implicitly mid-frame. | Predictable; a system that forgot a handle gets a deterministic failure at scene unload, not a random one two frames later. |
| D6 | Two plugin interfaces: `asset_importer` (source bytes -> cooked blobs, worker thread, deterministic, stateless) and `asset_loader` (blob -> payload in two stages: `load_cpu` on a worker, `finalise` on the main thread). `typed_asset_loader<T, Intermediate>` does the type erasure. | Export cooking and dev loading share the importer; GPU upload is isolated to one function on one thread; implementers write two typed functions. |
| D7 | The assets module knows no concrete asset type. Types are registered as `asset_type_desc` (64-bit FNV-1a id of the type name, version, loader). Only built-in type: `binary_asset`. | Respects the waves; the module is fully testable alone. |
| D8 | Cache validity is **content hash based**: source hash (+ side files), canonical settings hash, importer version, type version, blob format version, platform. mtime+size is only a fast path to skip hashing. Every blob carries a payload hash; every cache/pack/meta write is `tmp + rename`. Anything failing validation is stale and re-imported, never trusted. | mtime lies (git checkout, copies, clock skew). A crash mid-write can never produce a half-loaded asset. |
| D9 | The manager **auto-loads dependencies** listed in a blob header and does not finalise a dependent until every dependency is terminal (LOADED or FAILED). A slot holds handles to its dependencies while loaded. Cycles are rejected at import time and again at load time. | Materials finalise binding sets against loaded textures; residency of a dependency tree follows the root handle; wave-3 implementers never write ordering code. |
| D10 | Hot reload = polling `file_watcher` (stat pass on a job every 500 ms, change emitted after one stable interval) + `hot_reload_controller` that re-imports dirty sources and **reloads dependents through the normal pipeline** in dependency-depth order. Only types flagged `ASSET_TYPE_RELOAD_WITH_DEPENDENCIES` are reloaded transitively; others observe `version()` of their refs. | Identical on all three OSes, unit-testable with the memory VFS, one code path for reload. |
| D11 | Export is **all-or-nothing**: closure over the forward graph from roots, every blob valid, one `content.cpak` + `manifest.json`, uuid-sorted TOC with binary search, 16-byte aligned payloads, identical payloads stored once, no timestamps or absolute paths. Two cooks of identical input are byte-identical (tested). | Reproducible, diffable exports; no partial packs. |
| D12 | Blob providers are `asset_source` implementations: `cache_asset_source` (editor/dev), `pack_asset_source` (runtime), `memory_asset_source` (tests), `composite_asset_source` (plugin packs). The load pipeline is identical in all modes. | The runtime path is exercised by the same tests as the editor path. |
| D13 | Threading rule, one sentence: **every `asset_manager`, `asset_database`, `hot_reload_controller` and `asset_cooker` mutating call is main-thread only**; callable from any thread are `asset_ref` copy/`get()`/`version()`/`state()`, `asset_handle` copy/move/destroy (atomics), `asset_type_registry::find*` after start-up, `asset_source::read`, and `asset_database` lookups (shared lock). | One documented rule instead of a lock per method. |
| D14 | Failure never throws and never crashes a frame. A FAILED slot returns `nullptr` from `get()`; `asset_manager::get_or_fallback()` returns the type's registered fallback (magenta texture, default material, silent clip). Importer failures keep the previous blob. | Fixed decision (no exceptions); the editor stays usable while an artist fixes a file. |
| D15 | Mount roots: `assets://` (project `assets/`) and `engine://` (engine built-ins). Asset path strings always carry the mount and optional sub path: `assets://models/robot.gltf#mesh/Body`. | Engine and project content share one namespace without colliding. |
| D16 | Everything is compiled in every configuration; `conjure_runtime` simply never constructs the database, watcher, cooker or importers. | No `#ifdef` in public headers; stripping dev code from Distribution is a later size optimisation. |

---

## 2. Data model

Naming (cross-area resolution R31): the `m`-prefixed fields shown on plain data structs in this section
(`asset_slot`, `asset_type_desc`, `asset_meta`, `asset_record`, `cooked_blob_header`, `pack_header`, the
settings/report structs) are implemented as unprefixed camelCase (`object`, `version`, `id`, ...), matching the
other area documents; classes (`asset_ref`, `asset_handle`, `asset_manager`, ...) keep `mCamelCase`. The listings
below were left as written to keep the review history intact.

### 2.1 Identity and type identity (`asset_types.h`)

- `using asset_id = core::uuid;` `asset_id{}` is NIL and means "no asset".
- `enum class asset_type_id : uint64_t { INVALID = 0 };` produced by
  `make_asset_type_id("conjure.renderer.texture")`, a `constexpr` 64-bit FNV-1a implemented inside
  the assets module (no dependency on core's hashing shape). Stored in blob headers and pack TOCs,
  so it must never depend on compiler or platform.
- `asset_state : uint8_t`: `UNLOADED, QUEUED, LOADING, FINALISING, LOADED, RELOADING, FAILED`.
  Terminal states for dependency gating: `LOADED`, `FAILED`.
- `asset_load_priority : uint8_t`: `LOW, NORMAL, HIGH`. FIFO within a level.
- `asset_type_flags`: `ASSET_TYPE_NONE`, `ASSET_TYPE_RELOAD_WITH_DEPENDENCIES` (material only),
  `ASSET_TYPE_EXCLUDE_FROM_PACK` (script source), `ASSET_TYPE_PINNED_FALLBACK` (fallback created
  at registration, not lazily).
- `asset_error_code : uint16_t`: `NOT_FOUND, NO_IMPORTER, NO_LOADER, META_INVALID, DUPLICATE_UUID,
  IO_ERROR, IMPORT_FAILED, CACHE_MISS, CACHE_CORRUPT, LOAD_FAILED, FINALISE_FAILED, TYPE_MISMATCH,
  TYPE_UNKNOWN, TYPE_DUPLICATE, CYCLIC_DEPENDENCY, PACK_CORRUPT, UNBOUND, SHUTTING_DOWN, TIMEOUT`;
  `make_asset_error(code, message)` builds a `core::error` in the assets category.
- Constants: `RETIRE_FRAMES = 2`, `MAX_ASSET_TYPES = 256`, `DEFAULT_MAX_JOBS_IN_FLIGHT = 8`,
  `DEFAULT_WATCH_INTERVAL_MS = 500`, `COOKED_BLOB_FORMAT_VERSION`, `PACK_FORMAT_VERSION`,
  `META_SCHEMA_VERSION` (all 1).

### 2.2 Slot (`asset_slot.h`, public, read-only from outside the module)

```cpp
struct asset_slot {
    std::atomic<void *> mObject = nullptr;           // Published payload; nullptr unless LOADED.
    std::atomic<uint32_t> mVersion = 0;              // Bumped on every publish and every unload.
    std::atomic<asset_state> mState = asset_state::UNLOADED;
    std::atomic<int32_t> mStrongRefs = 0;            // asset_handle count.
    asset_type_id mType = asset_type_id::INVALID;    // Set at bind; checked against the blob header.
    asset_id mId = {};
    uint32_t mIndex = 0;                             // Into the manager's private slot_data array.
};
```

Slots live in `core::stable_vector<asset_slot>` (chunked, addresses never move). Main-thread-only
state (payload owner, cpu output, dependency handles, last error, pinned flag, reload generation)
lives in the private `slot_data` struct (`engine/src/assets/asset_slot_table.h`), indexed by `mIndex`.

### 2.3 `asset_ref<T>` and `asset_handle` (`asset_ref.h`)

`asset_ref<T>` is `{asset_id mId; const asset_slot *mSlot}` (`static_assert(sizeof == 24)`,
trivially copyable), serialises as the uuid string only, returns `nullptr` while unbound and is
bound through `asset_manager::bind<T>(id)`. `asset_handle` is `{asset_slot *mSlot}`: copy
increments `mStrongRefs`, destroy decrements, type-erased so batches, bindings and the scene
serialiser handle any asset; `ref<T>()` / `get<T>()` check `asset_type_traits<T>::id() == mType`.

### 2.4 Type registration (`asset_type_registry.h`)

```cpp
template <typename AssetType>
struct asset_type_traits;        // Specialised by CONJURE_DECLARE_ASSET_TYPE(type, "conjure.x.y")

struct asset_type_desc {
    asset_type_id mId = asset_type_id::INVALID;
    std::string mName = {};                          // "conjure.renderer.texture"
    uint32_t mVersion = 1;                           // Bump to invalidate every cooked blob of the type.
    asset_type_flags mFlags = ASSET_TYPE_NONE;
    std::unique_ptr<asset_loader> mLoader = nullptr;
};
```

`CONJURE_DECLARE_ASSET_TYPE(conjure::renderer::texture, "conjure.renderer.texture")` expands to the
traits specialisation with `static constexpr std::string_view NAME` and
`static constexpr asset_type_id id()`.

### 2.5 Payload type erasure (`asset_payload.h`)

```cpp
using asset_payload = std::unique_ptr<void, void (*)(void *)>;

template <typename ValueType>
[[nodiscard]] asset_payload make_payload(ValueType &&value);   // std::make_unique + erased deleter
```

This is the only type erasure in the module: no base class on asset types, no `shared_ptr`, no
virtual call on the hot path.

### 2.6 `.meta` sidecar (`asset_meta.h`)

`<source>.meta`, JSON, next to the source, committed to version control, written only in editor mode:

```json
{
  "schema": 1,
  "uuid": "7f3c1d2e-5d2b-4e7a-9c1f-2b6d8e4a7f01",
  "importer": "conjure.renderer.gltf",
  "importerVersion": 3,
  "settings": { "scale": 1.0, "generateTangents": true },
  "subAssets": { "material/Skin": "c3d4...", "mesh/Body": "a1b2..." }
}
```

`asset_meta { asset_id mId; std::string mImporterId; uint32_t mImporterVersion; core::json::value
mSettings; std::vector<sub_asset_entry> mSubAssets; core::json::value mUnknown; }`. `settings` is
opaque to the module (hashed canonically: sorted keys, minified). `subAssets` is append-only and
sorted by `subPath` on write. Unknown keys are preserved. A corrupt `.meta` is **never**
overwritten; the record is flagged `META_INVALID`. A uuid change in an existing `.meta` is refused
(ERROR log, in-memory uuid kept) because it would orphan references.

### 2.7 Database record (`asset_database.h`, editor/dev)

`asset_record { asset_id mId; asset_id mParentId (primary asset for sub-assets, NIL otherwise);
asset_type_id mType; uint32_t mMountIndex; std::string mRelativePath (forward slashes, relative to
the mount root); std::string mSubPath ("" for the primary); std::string mImporterId; uint64_t
mSourceSize; int64_t mSourceWriteTime (fast-path hint only); uint64_t mSourceHash (source bytes +
side files, sorted by path); uint64_t mSettingsHash; asset_record_status mStatus }` with
`asset_record_status : uint8_t { OK, MISSING_SOURCE, META_INVALID, DUPLICATE_UUID, IMPORT_FAILED,
NO_IMPORTER }`.

Indexes: `uuid -> record`, `"mount://path#sub" -> uuid`, forward and reverse dependency graphs as
CSR arrays (`mFirstEdge`/`mEdgeCount` per record, rebuilt after each import batch), and
`side file path -> owners`. One `std::shared_mutex`: lookups anywhere, mutations main thread only.
A snapshot is persisted to `.conjure/cache/db.bin` (records, string table, edges, versioned header)
so start-up does not hash every source; it is purely a cache, rebuilt if missing or stale.

### 2.8 Cooked blob (`cooked_blob.h`)

```
cooked_blob_header (80 bytes, little-endian, written verbatim), in order:
  char mMagic[4] = "CJAB"   u32 mFormatVersion   u64 mType (asset_type_id)   u32 mTypeVersion
  u32 mImporterVersion      u64 mSourceHash      u64 mSettingsHash           u64 mPayloadHash
  u64 mPayloadSize          u32 mDependencyCount u32 mFlags                  asset_id mId (16)
followed by asset_id[mDependencyCount], then the payload.
```

Payload starts at `align_up(80 + 16 * mDependencyCount, 16)`. `cooked_blob { mHeader,
mDependencies, mPayload }` owns its bytes. Payload layout belongs to the owning module; the
convention is a fixed header struct followed by raw arrays.

### 2.9 Cache layout

`<project>/.conjure/cache/db.bin` (snapshot, rebuildable), `<platform>/blobs/<xx>/<uuid>.cab` (one
blob per asset, `xx` = first two hex chars) and `import_log.txt` (last import errors per source).

### 2.10 Pack (`asset_pack.h`)

```
pack_header (64 bytes) : magic "CPAK", formatVersion u32, flags u32, entryCount u32, tocOffset u64,
                         dependencyTableOffset u64, stringTableOffset u64, blobsOffset u64,
                         contentHash u64 (hash64 of every byte after the header)
toc[N] (72 bytes each) : id(16) type(8) typeVersion(4) flags(4) blobOffset(8) blobSize(8)
                         payloadHash(8) dependencyStart(4) dependencyCount(4) nameOffset(4)
                         nameLength(4), sorted by id
dependency table       : asset_id[]
string table           : UTF-8 asset paths "assets://models/robot.gltf#mesh/Body"
blobs                  : payloads only, each 16-byte aligned, identical payloads stored once
```

`manifest.json` next to the pack lists `{uuid, type, name, size, hash, dependencies[]}` sorted by
uuid, for tooling and the AI command server. The runtime never needs it.

---

## 3. Algorithms and flow

### 3.1 Hot path: `asset_ref<T>::get()`

`get()` is `mSlot == nullptr ? nullptr : static_cast<T *>(mSlot->mObject.load(acquire))`. The
publish on the main thread is a `release` store, which makes the payload visible to worker threads
reading refs during parallel systems. Benchmark `bench_asset_ref_get`: 10,000 calls under 20 us.

### 3.2 Binding

`asset_manager::bind<T>(id)` (main thread): find or create the slot; if `mType` is INVALID, take the
type from `mSource->type_of(id)` when known, else from `T`; if the recorded type differs from `T`,
log ERROR once per uuid and return an unbound ref. Binding never loads.

### 3.3 Async load pipeline (identical in editor and runtime)

```
load(id, priority) -> asset_handle                                           [main]
    slot = find_or_create_slot(id); handle = asset_handle(slot)
    if state == UNLOADED: state = QUEUED; push to queue      (FAILED requires retry_failed())
    return handle

update()                                                                     [main]
    1. dispatch: while inFlight < mMaxJobsInFlight and queue not empty:
           pop highest priority; state = LOADING; schedule job read_and_load_cpu(slot)
    2. drain completions (mutex-guarded vector, swapped once):
           error -> fail(slot, error)
           ok    -> slot_data.cpuOutput = output; record header dependencies;
                    for each dependency: slot_data.dependencyHandles += load(dep, same priority)
                    cycle check (DFS over in-flight slots) -> fail(CYCLIC_DEPENDENCY)
                    state = FINALISING
    3. finalise pass (budget mFinaliseBudget, HIGH first, FIFO within level):
           for each FINALISING slot whose dependencies are all terminal:
               loader.finalise(context, move(cpuOutput)) -> payload or error
               ok    -> publish(slot, payload)                                (3.4)
               error -> fail(slot, error); on reload keep the old payload, state back to LOADED
    4. destroy retired payloads whose retireFrame + RETIRE_FRAMES <= frameIndex
    5. fire queued events to subscribers (main thread, in order)

read_and_load_cpu(slot)                                                      [worker]
    blob = source.read(id)                      // validates magic, versions, payload hash
    if blob.header.mType != slot.mType          -> TYPE_MISMATCH
    loader = registry.find_loader(type)         -> NO_LOADER if missing
    output = loader.load_cpu(load_context{id, type, blob.dependencies}, blob.payload)
    post completion {slot, header, dependencies, output or error}
```

Blob bytes are valid only during `load_cpu`; the CPU output must own its data. A FAILED dependency
does not fail its dependent: the dependent finalises and reads the fallback through its ref (WARN
once). `load_blocking(id)` (main thread only, asserted) runs the same stages inline, dependencies
first (depth-first, cycle guarded), with no job system involvement; it is used by the cooker, tools
and most unit tests. Inside a frame it is permitted on the main thread but logs one WARN per frame in
Development builds: it is a hitch, not a hazard, and the editor relies on it for `prefab.instantiate` /
`scene.open` when an asset is not yet resident (R23).

A slot in FINALISING or LOADED holds an `asset_handle` to each dependency, released when the slot
is unloaded, so residency of a whole material/texture tree follows the root handle. `RELOADING` is
not terminal, which gives hot reload its natural dependencies-first order (3.7).

### 3.4 Publish and retirement

`publish(slot, payload)`: move the new payload into `slot_data`, `mObject.store(ptr, release)`,
`mVersion++`, state LOADED, push the old payload (if any) to `mRetired` with the frame index, queue
the LOADED or RELOADED event.

Rule for all users: a pointer from `get()` is valid until the end of the next frame. Jobs that
outlive a frame hold the `asset_ref`, not the pointer. Caches keyed by `(id, version)` invalidate
themselves: renderer mesh instances compare their cached material version against
`ref.version()` while extraction already walks the instances. No subscriptions, no allocation.

### 3.5 Unload

`collect()` (main thread; engine calls it at scene unload, the editor on "unload unused"): every
LOADED slot with `mStrongRefs == 0` and not pinned gets `mObject = nullptr`, `mVersion++`, state
UNLOADED, payload retired, dependency handles released; the pass loops until no change. Fallbacks
and `engine://` built-ins are pinned. Slots themselves are never freed.

### 3.6 Import and cache (editor/dev)

```
asset_database::scan()                                                      [main orchestrates]
    load db.bin if valid
    for each mount, walk in sorted order; every regular non-.meta file:
        meta = read or create <file>.meta (new uuid, importer by extension; NO_IMPORTER keeps a uuid)
        duplicate uuid across two metas -> first in sorted path order wins, second DUPLICATE_UUID
        if (size, mtime) unchanged vs snapshot: keep hashes; else hash source + side files on a job
    stale = blob header missing, or any of sourceHash/settingsHash/importerVersion/typeVersion/
            formatVersion mismatched
    delete leftover *.tmp in the cache; save db.bin

asset_database::import(ids, jobs)                                           [parallel_for, one job per source]
    output = importer.import(import_context{vfs, record, settings, platform, sub-asset map, database})
    for each imported_asset: build header, hash payload, write <uuid>.cab.tmp, rename
    main thread merge: new sub-asset ids into meta (rewrite sorted), record types, CSR graphs
        (DFS cycle check: members flagged CYCLIC_DEPENDENCY), side-file owners, import_log.txt
    failure: record IMPORT_FAILED, previous blob kept, message logged with source path
```

`import_context::sub_asset_id(subPath)` is the only shared-state mutation an importer performs:
existing ids come from the meta, new ones are allocated under a mutex and merged on the main
thread. Importers must be deterministic (no clocks, no hash-map iteration order in output).
Loads never import: a missing blob at load time is `CACHE_MISS`, which the hot reload controller
repairs on its next update (re-import, then reload).

### 3.7 Hot reload

```
polling_file_watcher   (job every interval): stat every file under each root; diff vs snapshot;
                        a change is emitted when (mtime, size) differs from the snapshot AND equals
                        the value seen in the previous poll (one stable interval = debounce)

hot_reload_controller::update()                                             [main, after asset_manager::update]
    changes = watcher.poll()
    classify: .meta MODIFIED -> re-read, settings/importer change marks the owner dirty;
              source MODIFIED -> dirty; side file MODIFIED -> dirty every owner;
              ADDED -> database.add_file(); REMOVED -> MISSING_SOURCE (loaded payload stays loaded)
    dirty += slots currently FAILED(CACHE_MISS)
    database.import(dirty)                      // synchronous, small set
    changed = produced blobs whose payloadHash differs from before (+ newly produced)
    ordered = database.collect_dependents(changed)   // BFS over reverse CSR, depth ascending, dedup
    for uuid in ordered with a LOADED slot:
        if uuid in changed or type has ASSET_TYPE_RELOAD_WITH_DEPENDENCIES: manager.reload(uuid)
```

`manager.reload(id)`: LOADED -> RELOADING, schedule the normal job; the old payload stays published
until the new one finalises (dependents in RELOADING wait for it, so the material sees the new
texture). Failure keeps the old payload, returns to LOADED, fires `RELOAD_FAILED`.

### 3.8 Cook for export

```
asset_cooker::cook(settings) -> result<cook_report>
    database.scan(); roots = mRoots + mAlwaysInclude + every engine:// asset (all records if mIncludeAll)
    closure = database.dependency_closure(roots) minus ASSET_TYPE_EXCLUDE_FROM_PACK types
    import stale members in parallel; any failure -> error(IMPORT_FAILED), report.mFailures, write nothing
    pack_writer: add each blob (re-verify payloadHash), sort by uuid, dedup payloads by
                 (payloadHash, payloadSize), write content.cpak.tmp, rename; write manifest.json
```

---

## 4. Public API (`engine/include/conjure/assets/`)

Every header includes only `conjure/core/...` and the standard library.

| Header | Main types |
|---|---|
| `assets.h` | umbrella include |
| `log_category.h` | `CONJURE_LOG_CATEGORY(ASSETS)` |
| `asset_types.h` | `asset_id`, `asset_type_id`, `make_asset_type_id`, `asset_state`, `asset_load_priority`, `asset_type_flags`, `asset_error_code`, `make_asset_error`, constants |
| `asset_payload.h` | `asset_payload`, `make_payload<T>` |
| `asset_slot.h` | `asset_slot` |
| `asset_ref.h` | `asset_ref<T>`, `asset_handle` |
| `asset_type_registry.h` | `asset_type_traits<T>`, `CONJURE_DECLARE_ASSET_TYPE`, `asset_type_desc`, `asset_type_registry` |
| `asset_loader.h` | `load_context`, `finalise_context`, `asset_loader`, `typed_asset_loader<T, Intermediate>` |
| `asset_importer.h` | `import_context`, `imported_asset`, `import_output`, `asset_importer`, `importer_registry` |
| `asset_meta.h` | `sub_asset_entry`, `asset_meta`, `read_asset_meta`, `write_asset_meta`, `canonical_settings_hash` |
| `cooked_blob.h` | `cooked_blob_header`, `cooked_blob`, `write_cooked_blob`, `parse_cooked_blob` |
| `asset_cache.h` | `cache_expectation`, `asset_cache` |
| `asset_source.h` | `asset_source`, `cache_asset_source`, `pack_asset_source`, `memory_asset_source`, `composite_asset_source` |
| `asset_pack.h` | `pack_header`, `pack_toc_entry`, `pack_stats`, `pack_reader`, `pack_writer` |
| `asset_database.h` | `asset_record_status`, `asset_record`, `asset_mount`, `scan_report`, `import_report`, `asset_database` |
| `asset_manager.h` | `asset_manager_desc`, `asset_event_kind`, `asset_event`, `asset_event_callback`, `asset_subscription`, `asset_load_batch`, `asset_manager_stats`, `asset_manager` |
| `file_watcher.h` | `file_change_kind`, `file_change`, `file_watcher`, `polling_file_watcher` |
| `hot_reload.h` | `hot_reload_controller` |
| `asset_cooker.h` | `cook_settings`, `cook_failure`, `cook_report`, `asset_cooker` |
| `binary_asset.h` | `binary_asset`, `binary_importer`, `binary_loader`, `register_binary_asset_type` |

Private (`engine/src/assets/`): one `.cpp` per public header plus `asset_slot_table.{h,cpp}`,
`load_pipeline.{h,cpp}`, `completion_queue.h`, `database_snapshot.{h,cpp}`, `fnv1a.h`, `README.md`.

### 4.1 `asset_ref.h`

```cpp
namespace conjure::assets {

    template <typename AssetType>
    class asset_ref {
        private:
            asset_id mId = {};
            const asset_slot *mSlot = nullptr;

        public:
            asset_ref() = default;
            explicit asset_ref(const asset_id &id) noexcept;
            asset_ref(const asset_id &id, const asset_slot *slot) noexcept;

        public:
            [[nodiscard]] const asset_id &id() const noexcept { return mId; }
            [[nodiscard]] bool is_bound() const noexcept { return mSlot != nullptr; }

            /// <summary>
            /// Hot path: one atomic load, no hashing, no locking. Returns nullptr unless LOADED.
            /// The pointer is valid until the end of the next frame.
            /// </summary>
            [[nodiscard]] AssetType *get() const noexcept;
            [[nodiscard]] asset_state state() const noexcept;
            [[nodiscard]] uint32_t version() const noexcept;    // 0 when unbound.
            [[nodiscard]] bool operator==(const asset_ref &other) const noexcept { return mId == other.mId; }
    };
}
```

`asset_handle` (same header): default, `explicit asset_handle(asset_slot *slot) noexcept`, copy
and move constructors/assignments (`noexcept`), destructor; `id()`, `type()`, `is_bound()`,
`state()`, `is_terminal()` (LOADED or FAILED), `version()`, `template <typename AssetType>
asset_ref<AssetType> ref() const noexcept` (unbound ref + ERROR once on type mismatch),
`template <typename AssetType> AssetType *get() const noexcept`, `reset() noexcept`.

### 4.2 `asset_type_registry.h`

```cpp
namespace conjure::assets {

    class asset_type_registry {
        public:
            asset_type_registry();
            ~asset_type_registry();

        public:
            /// <summary>Main thread, before systems run. Fails with TYPE_DUPLICATE if the id is taken.</summary>
            [[nodiscard]] core::result<void> register_type(asset_type_desc desc);

            /// <summary>Plugin unload. Refused while any slot of the type is loaded or in flight.</summary>
            [[nodiscard]] core::result<void> unregister_type(asset_type_id id);
            [[nodiscard]] const asset_type_desc *find(asset_type_id id) const noexcept;
            [[nodiscard]] const asset_loader *find_loader(asset_type_id id) const noexcept;
            [[nodiscard]] std::span<const asset_type_desc> types() const noexcept;
    };
}
```

### 4.3 `asset_loader.h`

`load_context { asset_id mId; asset_type_id mType; std::span<const asset_id> mDependencies }`
(same order as the blob header); `finalise_context { asset_id mId; std::span<const asset_handle>
mDependencies (all terminal when finalise runs); uint64_t mFrameIndex; bool mIsReload }`.

```cpp
namespace conjure::assets {

    class asset_loader {
        public:
            asset_loader();
            virtual ~asset_loader();

        public:
            [[nodiscard]] virtual asset_type_id type() const noexcept = 0;

            /// <summary>Worker thread. Bytes are valid only for the duration of the call.</summary>
            [[nodiscard]] virtual core::result<asset_payload> load_cpu(
              const load_context &context,
              std::span<const std::byte> payloadBytes
            ) const = 0;

            /// <summary>Main thread. GPU upload, audio device buffers; returns the published payload.</summary>
            [[nodiscard]] virtual core::result<asset_payload> finalise(
              const finalise_context &context,
              asset_payload &&cpuOutput
            ) const = 0;

            /// <summary>Main thread, once per type, lazily. Default returns nullptr (no fallback).</summary>
            [[nodiscard]] virtual core::result<asset_payload> create_fallback() const;
    };

    /// <summary>
    /// Typed convenience base: implements type(), load_cpu() and finalise() as final overrides that
    /// erase through make_payload(); implementers write the two typed functions below.
    /// </summary>
    template <typename AssetType, typename Intermediate>
    class typed_asset_loader : public asset_loader {
        protected:
            [[nodiscard]] virtual core::result<Intermediate> load_cpu_typed(
              const load_context &context,
              std::span<const std::byte> payloadBytes
            ) const = 0;
            [[nodiscard]] virtual core::result<AssetType> finalise_typed(
              const finalise_context &context,
              Intermediate &&intermediate
            ) const = 0;
    };
}
```

Loaders that need a device receive it through their own constructor (the audio clip loader may hold an
`audio::audio_engine &`), so the assets module needs no forward declarations of other modules. The renderer's
loaders are CPU-only: `load_cpu` decodes the cooked payload and `finalise` is trivial; GPU upload is driven by
the engine's `renderer_subsystem` from LOADED / RELOADED / UNLOADED events through `renderer::upload_*` /
`release_*`, which copy the payload bytes at enqueue time (R10).

### 4.4 `asset_importer.h`

```cpp
namespace conjure::assets {

    class import_context {
        public:
            [[nodiscard]] std::string_view source_path() const noexcept;       // "assets://models/robot.gltf"
            [[nodiscard]] std::span<const std::byte> source_bytes() const noexcept;
            [[nodiscard]] const core::json::value &settings() const noexcept;
            [[nodiscard]] std::string_view platform() const noexcept;        // "windows", "linux", "macos"
            [[nodiscard]] const asset_id &primary_id() const noexcept;

            /// <summary>Reads a file next to the source; records it as a side file (hashed, watched).</summary>
            [[nodiscard]] core::result<std::vector<std::byte>> read_side_file(std::string_view relativePath);

            /// <summary>Stable uuid for a named sub-asset; allocated on first use, persisted to the meta.</summary>
            [[nodiscard]] asset_id sub_asset_id(std::string_view subPath);

            /// <summary>Uuid of another source in the project (an external .png a glTF cites).</summary>
            [[nodiscard]] std::optional<asset_id> find_asset_by_path(std::string_view relativePath) const;
            void add_warning(std::string message);
    };

    class asset_importer {
        public:
            asset_importer();
            virtual ~asset_importer();

        public:
            [[nodiscard]] virtual std::string_view id() const noexcept = 0;        // "conjure.renderer.gltf"
            [[nodiscard]] virtual uint32_t version() const noexcept = 0;           // Bump to invalidate cache.
            [[nodiscard]] virtual std::span<const std::string_view> extensions() const noexcept = 0;
            [[nodiscard]] virtual core::json::value default_settings() const;

            /// <summary>Worker thread. Const, deterministic, touches no shared state beyond the context.</summary>
            [[nodiscard]] virtual core::result<import_output> import(import_context &context) const = 0;
    };
}
```

`imported_asset { std::string mSubPath ("" for the primary); std::string mDisplayName;
asset_type_id mType; std::vector<asset_id> mDependencies (sorted, unique, no self reference);
std::vector<std::byte> mPayload }`; `import_output { std::vector<imported_asset> mAssets }` with
exactly one entry whose `mSubPath` is empty. `importer_registry`:
`register_importer(std::unique_ptr<asset_importer>)` (duplicate id fails), `unregister_importer(id)`
(refused while imports are in flight), `find(id)`, `find_for_extension(ext)` (case-insensitive),
`importers()`.

### 4.5 `asset_manager.h`

`asset_manager_desc { core::jobs::job_system *mJobs; std::unique_ptr<asset_source> mSource;
uint32_t mMaxJobsInFlight = DEFAULT_MAX_JOBS_IN_FLIGHT; std::chrono::microseconds mFinaliseBudget
= 2000us; std::chrono::milliseconds mBlockingTimeout = 30000ms }`.
`asset_event_kind : uint8_t { LOADED, RELOADED, RELOAD_FAILED, FAILED, UNLOADED }`;
`asset_event { mKind, asset_id mId, asset_type_id mType, uint32_t mVersion, const core::error
*mError (FAILED / RELOAD_FAILED only) }`; `using asset_event_callback =
std::function<void(const asset_event &event)>`.

```cpp
namespace conjure::assets {

    class asset_manager {
        public:
            explicit asset_manager(asset_manager_desc desc);
            ~asset_manager();                                   // Asserts zero strong refs in debug.

        public:
            [[nodiscard]] asset_type_registry &types() noexcept;
            void add_source(std::unique_ptr<asset_source> source);      // Plugin packs, searched after the primary.
            void swap_source(std::unique_ptr<asset_source> source);     // Editor: cache -> pack preview.

            template <typename AssetType>
            [[nodiscard]] asset_ref<AssetType> bind(const asset_id &id);
            [[nodiscard]] asset_handle make_handle(const asset_id &id);    // Binds, never loads.
            [[nodiscard]] const asset_slot *bind_slot(const asset_id &id); // Untyped bind for the scene bind pass (R25).
            [[nodiscard]] std::optional<asset_id> find(std::string_view assetPath) const;

            [[nodiscard]] asset_handle load(
              const asset_id &id,
              asset_load_priority priority = asset_load_priority::NORMAL
            );
            [[nodiscard]] asset_load_batch load_batch(
              std::span<const asset_id> ids,
              asset_load_priority priority = asset_load_priority::NORMAL
            );
            [[nodiscard]] core::result<asset_handle> load_blocking(const asset_id &id);
            void reload(const asset_id &id);                    // LOADED -> RELOADING through the pipeline.
            void retry_failed(const asset_id &id);
            void pin(const asset_id &id, bool pinned);

            template <typename AssetType>
            [[nodiscard]] AssetType *fallback() noexcept;
            template <typename AssetType>
            [[nodiscard]] AssetType *get_or_fallback(const asset_ref<AssetType> &ref) noexcept;
            [[nodiscard]] const core::error *last_error(const asset_id &id) const noexcept;
            [[nodiscard]] asset_state state_of(const asset_id &id) const noexcept;

            void begin_frame(uint64_t frameIndex);
            void update();                                      // Dispatch, completions, finalise, retire, events.
            void end_frame();
            void collect();                                     // Unload zero-ref, unpinned payloads.
            void flush();                                       // Blocks until every queued load is terminal.
            void shutdown();                                    // Drains jobs, discards completions.

            [[nodiscard]] asset_subscription subscribe(asset_event_callback callback);
            void unsubscribe(asset_subscription subscription);
            [[nodiscard]] asset_manager_stats stats() const noexcept;
    };
}
```

`asset_load_batch` holds a `std::vector<asset_handle>` and offers `is_complete()` (every handle
terminal), `progress()` and `handles()`. `asset_manager_stats` reports loaded, pending, failed and
retired counts plus bytes resident.

### 4.6 `asset_database.h`, `asset_cache.h`, `asset_source.h`, `asset_pack.h`

```cpp
namespace conjure::assets {

    class asset_database {
        public:
            asset_database(
              core::vfs::file_system &fileSystem,
              importer_registry &importers,
              const asset_type_registry &types
            );
            ~asset_database();

        public:
            [[nodiscard]] core::result<void> add_mount(std::string_view mountName, std::string_view rootPath);
            [[nodiscard]] core::result<void> open(std::string_view cacheRoot, std::string_view platform);
            [[nodiscard]] core::result<scan_report> scan();                      // Main thread.
            [[nodiscard]] core::result<import_report> import_stale(core::jobs::job_system &jobs);
            [[nodiscard]] core::result<import_report> import(
              std::span<const asset_id> ids,
              core::jobs::job_system &jobs
            );
            [[nodiscard]] core::result<asset_id> add_file(std::string_view assetPath);
            [[nodiscard]] core::result<void> move_asset(const asset_id &id, std::string_view newAssetPath);
            [[nodiscard]] core::result<void> delete_asset(const asset_id &id);

            // Lookups are thread-safe (shared lock).
            [[nodiscard]] std::optional<asset_record> find(const asset_id &id) const;
            [[nodiscard]] std::optional<asset_id> find_by_path(std::string_view assetPath) const;
            [[nodiscard]] std::vector<asset_id> dependencies_of(const asset_id &id) const;
            [[nodiscard]] std::vector<asset_id> dependents_of(const asset_id &id) const;
            [[nodiscard]] std::vector<asset_id> owners_of_side_file(std::string_view assetPath) const;
            [[nodiscard]] core::result<std::vector<asset_id>> dependency_closure(
              std::span<const asset_id> roots
            ) const;

            /// <summary>Transitive dependents including the roots, depth ascending, deduplicated.</summary>
            void collect_dependents(std::span<const asset_id> roots, std::vector<asset_id> &outOrdered) const;
    };

    class asset_source {
        public:
            asset_source();
            virtual ~asset_source();

        public:
            [[nodiscard]] virtual bool contains(const asset_id &id) const noexcept = 0;
            [[nodiscard]] virtual std::optional<asset_type_id> type_of(const asset_id &id) const noexcept = 0;
            [[nodiscard]] virtual std::optional<asset_id> find_by_path(std::string_view assetPath) const = 0;

            /// <summary>Any thread. Returns the validated blob (header, dependencies, payload).</summary>
            [[nodiscard]] virtual core::result<cooked_blob> read(const asset_id &id) const = 0;
    };
}
```

Further `asset_database` members: `create_asset(assetPath, importerId, initialBytes)`,
`save_snapshot()`, `records()`, `read_meta(id)`, `write_meta(id, meta)`.
`asset_cache(fileSystem, cacheRoot, platform)`: `read_header(id)`, `read(id)` (validates every
field), `write(blob)` (tmp + rename), `is_valid(header, cache_expectation)`, `remove(id)`,
`remove_temporaries()`, `clear()`. `pack_writer`: `add(blob, assetPath)`, `write(fileSystem,
packPath) -> result<pack_stats>` (sorts, dedups, tmp + rename), `write_manifest(fileSystem, path)`.
`pack_reader`: `open(fileSystem, packPath)` (validates header and content hash), `find(id) -> const
pack_toc_entry *` (binary search), `read(entry) -> result<cooked_blob>` (positional read), `toc()`,
`name_of(entry)`, `content_hash()`. `cache_asset_source(cache, database)`,
`pack_asset_source(reader)`, `memory_asset_source` (tests add blobs directly) and
`composite_asset_source` (resolves in insertion order) implement `asset_source`.

### 4.7 `file_watcher.h`, `hot_reload.h`, `asset_cooker.h`

```cpp
namespace conjure::assets {

    class file_watcher {
        public:
            file_watcher();
            virtual ~file_watcher();

        public:
            virtual void add_root(std::string_view rootPath) = 0;
            [[nodiscard]] virtual core::result<void> snapshot() = 0;           // Baseline, emits nothing.

            /// <summary>Main thread. Moves debounced changes since the last call into outChanges.</summary>
            virtual void poll(std::vector<file_change> &outChanges) = 0;
    };

    class hot_reload_controller {
        public:
            hot_reload_controller(
              asset_database &database,
              asset_manager &manager,
              file_watcher &watcher,
              core::jobs::job_system &jobs
            );
            ~hot_reload_controller();

        public:
            void update();                                      // Main thread, after asset_manager::update().
            [[nodiscard]] uint32_t reloads_this_frame() const noexcept;
    };
}
```

`file_change { std::string mPath; file_change_kind mKind }` with `file_change_kind : uint8_t
{ ADDED, MODIFIED, REMOVED }`. `polling_file_watcher final : file_watcher` is constructed with
`(core::vfs::file_system &, core::jobs::job_system &, std::chrono::milliseconds interval)` and adds
`scan_now()` (tests: one synchronous pass). `cook_settings { std::string mOutputDirectory;
std::string mPlatform; std::vector<asset_id> mRoots; std::vector<asset_id> mAlwaysInclude; bool
mIncludeAll = false; bool mWriteManifest = true }`; `cook_report { mAssetCount, mPackBytes,
mDuplicatesRemoved, mContentHash, std::vector<cook_failure> mFailures, std::vector<std::string>
mWarnings }`. `asset_cooker(asset_database &, asset_cache &, core::vfs::file_system &,
core::jobs::job_system &)` offers `[[nodiscard]] core::result<cook_report> cook(const cook_settings
&settings)` (all or nothing).

---

## 5. Threading and ownership

| Thread | May do |
|---|---|
| Main | every `asset_manager` mutation (`bind`, `load`, `reload`, `collect`, `update`, `subscribe`), finalise, publish, events and callbacks, every `asset_database` mutation and `.meta` write, `hot_reload_controller::update`, cooking |
| Job workers | `read_and_load_cpu` bodies (`asset_source::read`, `asset_loader::load_cpu`), importer `import()`, cache writes, source hashing, the watcher stat pass |
| Any | `asset_ref` copy / `get()` / `version()` / `state()`, `asset_handle` copy/move/destroy, `asset_type_registry::find*` after start-up, `asset_database` lookups (shared lock), `asset_source::read` |

Shared state and its guard: slot table (stable addresses, lock-free reads, creation main thread
only), completion vector (one `std::mutex`, swapped once per `update()`), watcher change list
(mutex), database (`std::shared_mutex`), `sub_asset_id` allocation (mutex). No other locks, no
`std::thread`. Ownership: `engine::application` owns the single `asset_manager` and destroys it
after every world/scene and the renderer/audio systems, so no handle outlives it (debug assert:
total strong refs == 0; release: log and continue). Slots are owned by the manager; payloads by
`asset_payload` in `slot_data` (plus the retirement list); importers and loaders by the registries.
`asset_ref` never owns anything; a dangling ref after manager destruction is a programmer error.

---

## 6. Failure handling

| Situation | Behaviour |
|---|---|
| Unknown uuid | `load()` returns a handle whose slot is FAILED(`NOT_FOUND`); ERROR once per uuid per session |
| No importer for extension | Record exists (browsable); loads FAIL with `NO_IMPORTER` |
| `.meta` corrupt | Never overwritten; record `META_INVALID`; editor shows it; loads FAIL |
| Duplicate uuid | First by sorted path wins; second `DUPLICATE_UUID`, excluded, never auto-rewritten (editor command "regenerate uuid") |
| Importer error | Record `IMPORT_FAILED`, message in `import_log.txt` and console; previous blob kept; running assets untouched |
| Blob missing at load (editor) | FAILED(`CACHE_MISS`); the hot reload controller re-imports on its next update and reloads |
| Blob corrupt / hash mismatch | Cache: treated as stale, re-imported, WARN. Pack: `pack_reader::open` fails `PACK_CORRUPT`; runtime refuses to start |
| Crash during cache or meta write | `*.tmp` ignored and removed on the next `scan()` |
| Type mismatch (`bind<T>` vs recorded type, or blob header vs slot type) | Unbound ref + ERROR once; or FAILED(`TYPE_MISMATCH`) |
| `load_cpu` / `finalise` error | FAILED; `get()` -> nullptr; `get_or_fallback()` -> fallback; event FAILED |
| Dependency failed | Dependent still finalises; reads fallback through its ref; WARN once |
| Dependency cycle | All members FAILED(`CYCLIC_DEPENDENCY`) at import; defensive check again at load |
| Reload fails | Old payload stays published, state back to LOADED, event RELOAD_FAILED |
| Source removed while loaded | Payload stays loaded; record `MISSING_SOURCE`; future loads FAIL `NOT_FOUND` |
| Cook: any import failure | Nothing written; `cook_report::mFailures` lists every failing asset |
| `load_blocking` exceeds timeout | FAILED(`TIMEOUT`) |
| Shutdown with in-flight jobs | `shutdown()` sets SHUTTING_DOWN, waits for jobs, discards completions |
| Manager destroyed with live handles | Debug assert; release log |

---

## 7. Headless / null behaviour

The module has no GPU, window or audio dependency; everything here runs in CI as-is. Loaders get
their devices through their own constructors (or, for the renderer, the engine's event-driven `upload_*`
creates the GPU mirror, R10), so with the `null` RHI null resources are created and the full pipeline (import -> cache -> load_cpu -> finalise -> hot reload
-> cook -> pack load) is exercised headless. Unit tests inject `core::vfs::memory_file_system`, a
single-threaded `core::jobs::job_system`, the built-in `binary_asset` type and a deterministic
`test_asset` type (plus a `failing_importer`). `conjure_editor --headless --export` runs the cooker
through exactly this path.

---

## 8. Integration points

**Needs from `core` (wave 1).** `uuid` (`generate()` thread-safe, `is_nil()`, `to_string()`,
`parse()`, `std::hash`, `operator<=>`), `result<T>` / `error` (code + category + message; assets
registers its own category), `hash::fnv1a64(span<const byte>)`, `flat_hash_map<K, V>`, `stable_vector<T>`,
`jobs::job_system` (`schedule(fn, priority) -> job_handle`, `wait(job_handle)`,
`parallel_for(count, grain, fn(begin, end))`, worker count 0 = inline for tests; names per R30),
`vfs::file_system` (`read_file`,
`write_file_atomic` (tmp + rename), `stat -> {size, writeTime}`, `enumerate(dir, recursive, callback)`,
`rename`, `remove`, `create_directories`, `open_read -> file` with `read_at(offset, span)`,
`memory_file_system`), `json::value` / `parse` / `to_string(value, canonical)` (sorted keys,
minified), `CONJURE_LOG_*`, `CONJURE_ASSERT`, `platform_detect.h` for the platform string, and a `core::reflect`
field kind `ASSET_REF` so `asset_ref<T>` fields serialise as the uuid string. Binding happens after the read:
the ECS records `assetRefFieldOffsets` per component type and the scene module runs a bind pass over them
with `asset_manager::bind_slot` after scene load / prefab instantiation (R25), so neither `core` nor `ecs`
needs an `asset_manager *` in a deserialisation context.

**Needs from `modules` (wave 2, area G).** Plugins register trivially copyable extension records
`assets::importer_extension_desc { static constexpr core::string_id EXTENSION_POINT{"assets.importer"};
const char *id; std::unique_ptr<asset_importer> (*create)(); }` and `assets::asset_type_extension_desc {
EXTENSION_POINT{"assets.type"}; const char *name; uint32_t version; asset_type_flags flags;
std::unique_ptr<asset_loader> (*createLoader)(); }` (R37). The engine's `assets_module_consumer` calls the
factories and `register_type` / `register_importer` on attach, mounts the manifest's content directories as
`modules://<name>/` (R24) and calls `asset_manager::add_source` for module packs; detach calls
`unregister_type` / `unregister_importer`, which are refused while work is in flight or slots of the type are
loaded.

**Offers to `renderer` (wave 3).** Registers `conjure.renderer.texture` (`.png .jpg .tga .bmp`;
settings sRGB, mips, compression), `conjure.renderer.environment` (`.hdr`; the importer bakes the cubemap, SH9 and prefiltered specular on
the CPU so the payload is deterministic, R34), `conjure.renderer.mesh`, `conjure.renderer.material` (flag
`RELOAD_WITH_DEPENDENCIES`), `conjure.renderer.model` (glTF importer `conjure.renderer.gltf`: model primary +
mesh/material/texture sub-assets; external images resolve to their own assets via `find_asset_by_path`,
`.bin` via `read_side_file`). Registration happens through `renderer::register_asset_types(types, importers)`,
called by the engine's `renderer_subsystem`. Renderer loaders are CPU-only; the engine's `renderer_subsystem`
subscribes to LOADED / RELOADED / UNLOADED and drives `renderer::upload_*` / `release_*` (R10). Mesh instances
compare cached material `version()` during extraction.

**Offers to `audio`.** `conjure.audio.clip` (`.wav .ogg .mp3`; setting: decode to PCM or keep compressed).

**Offers to `scene`.** `conjure.scene.scene` (`.scene`) and `conjure.scene.prefab` (`.prefab`): the
importer canonicalises the JSON and extracts every `asset_ref` uuid as a dependency via reflection,
so hot reload and the cooking closure reach everything a scene uses. `scene_manager` holds the
`asset_handle`s that define residency, gates activation on `asset_load_batch::is_complete()`, and
calls `collect()` after unloading a scene.

**Offers to `scripting`.** `conjure.scripting.script_source` (`.cs`, flag `EXCLUDE_FROM_PACK`,
payload = content hash). Scripting subscribes to RELOADED for this type to rebuild the assembly.

**Offers to `engine` (wave 4).** `asset_manager` constructed with `cache_asset_source` (editor/dev)
or `pack_asset_source` (runtime, from `project.json`); `begin_frame` / `update` once per frame in
`assets_subsystem::begin_frame` (step 5 of `06_engine_loop.md` 3.1, before FRAME_BEGIN) and `end_frame` in
`assets_subsystem::end_frame` (step 13); `hot_reload_controller::update` right after `update` in editor
builds (R23); `asset_database::scan()` + `import_stale()` in `post_startup`, after every importer is registered
(R33); `asset_cooker` as step 2 of the engine's `export_pipeline` (R21).

**Offers to `editor`.** `asset_database` queries for the asset browser; `records()`, `last_error`,
`reload`, `create_asset` / `move_asset` / `delete_asset`, `write_meta` as undoable commands
reachable from the AI JSON protocol; `import_log.txt` and events for console and agent feedback.

**Offers to `bindings`.** `asset_ref<T>` marshals as a 16-byte uuid plus 64-bit type id; C#
`AssetRef<T>` resolves through a native `asset_ref_get(uuid, typeId)` that hits the same slot path;
`make_handle`, `find(path)`, `load`, `state_of` for the managed `Assets` API.

**Build.** `conjure_add_module(assets SOURCES ... PUBLIC_DEPS conjure_core)`; tests via
`conjure_add_test_module(assets)`.

---

## 9. Implementation plan

1. `asset_types.h`, `fnv1a.h`, `log_category.h`, `asset_payload.h`, `asset_slot.h`. Tests:
   `test_asset_types`.
2. `asset_type_registry.h`, `asset_loader.h` (`typed_asset_loader`), `CONJURE_DECLARE_ASSET_TYPE`.
   Tests: `test_asset_type_registry`.
3. `cooked_blob.h`: layout, write/parse, alignment, hash validation. Tests: `test_cooked_blob`.
4. `asset_source.h` + `memory_asset_source`; `asset_slot_table`, `asset_ref.h`. Tests:
   `test_asset_ref`, `test_asset_handle_lifetime`.
5. `asset_manager` core: bind, load, `load_pipeline` (dispatch, completions, finalise budget,
   dependency gating, cycle check, publish, retirement), `collect`, `flush`, `load_blocking`,
   events, fallbacks, stats, `shutdown`; `binary_asset`. Tests: `test_asset_manager_load`,
   `test_binary_asset`, `bench_asset_ref_get`, `bench_asset_load`.
6. `asset_meta.h`. Tests: `test_asset_meta`.
7. `asset_importer.h`, `importer_registry`, `import_context`. Tests: `test_importer_registry`.
8. `asset_cache.h`. Tests: `test_asset_cache`.
9. `asset_database.h` (mounts, scan, duplicate policy, hashing on jobs, CSR graphs, cycle detection,
   side files, import, snapshot, create/move/delete) + `cache_asset_source`. Tests:
   `test_asset_database`, `test_import_cache`.
10. `file_watcher.h`, `hot_reload.h`. Tests: `test_file_watcher`, `test_hot_reload`.
11. `asset_pack.h`, `pack_asset_source`, `composite_asset_source`. Tests: `test_asset_pack`,
    `test_asset_source` (contract suite over all sources).
12. `asset_cooker.h`. Tests: `test_asset_cooker`, `test_export_roundtrip`.
13. `assets.h`, `engine/src/assets/README.md`, CMake registration, CI green on all three platforms
    with ASan/UBSan.

Steps 1-5 are the runtime-critical core and unblock wave 3; 6-12 are editor/dev tooling and proceed
in parallel after step 5 (6-8 independent; 9 needs 6-8; 10 needs 9; 11 needs 3-4; 12 needs 9, 11).

---

## 10. Tests (`tests/assets/`, target `conjure_assets_tests`)

| File | Cases |
|---|---|
| `test_asset_types.cpp` | `make_asset_type_id` constexpr, stable, distinct per name; flag operators; `make_asset_error` carries code and message |
| `test_asset_type_registry.cpp` | register/find/`find_loader`; duplicate id fails; unregister refused while a slot is loaded; `MAX_ASSET_TYPES` overflow fails; `CONJURE_DECLARE_ASSET_TYPE` traits id matches the desc |
| `test_cooked_blob.cpp` | write/parse round trip; payload 16-byte aligned; bad magic, format version, payload hash, truncated file rejected; dependency list order |
| `test_asset_ref.cpp` | `sizeof == 24` and trivially copyable; default unbound; `get()` nullptr before load; bound ref reads payload after publish; `version()` increments on publish and unload; equality by id; uuid string round trip |
| `test_asset_handle_lifetime.cpp` | copy/move adjust `mStrongRefs` exactly; destructor releases; `ref<T>()` type mismatch yields unbound; handle outliving manager asserts (debug death test) |
| `test_asset_manager_load.cpp` | async load reaches LOADED through `update()`; budget stops finalisation mid-queue; idempotent re-request; `load_blocking` succeeds / times out / refused in frame; unknown uuid -> `NOT_FOUND`; corrupt blob -> FAILED; fallback served; type mismatch; priorities respected; dependencies finalise before dependents; failed dependency does not fail dependent; cycle -> `CYCLIC_DEPENDENCY`; `load_batch` progress/complete; `collect()` unloads only zero-ref unpinned and releases dependency handles; retired payload destroyed after exactly `RETIRE_FRAMES`; events fire once each on the main thread; `flush()`; `shutdown()` with in-flight jobs |
| `test_binary_asset.cpp` | importer/loader round trip; empty file; 64 MiB file |
| `test_asset_meta.cpp` | round trip; missing optional fields; corrupt JSON -> error, file untouched; unknown keys preserved; sub-assets sorted on write; `canonical_settings_hash` key-order independent |
| `test_importer_registry.cpp` | register; extension lookup case-insensitive; duplicate id fails; unregister refused in flight |
| `test_asset_cache.cpp` | write/read round trip; `is_valid` fails on each of source hash, settings hash, importer version, type version, format version; truncated; flipped payload byte; `remove_temporaries`; `clear()` |
| `test_asset_database.cpp` | scan creates metas and uuids; rescan stable; path and sub-path lookup; duplicate uuid policy; moved file keeps uuid; removed -> `MISSING_SOURCE`; uuid change in meta refused; CSR deps/dependents; `collect_dependents` order and dedup; `dependency_closure`; cycle detection; side-file owners; snapshot save/load equals in-memory; lookups during scan are TSan-clean |
| `test_import_cache.cpp` | stale on source / settings / importer version / type version / platform change; unchanged input imports nothing; failed import keeps previous blob and flags record; `sub_asset_id` stable across two imports; parallel import of 1,000 sources equals serial byte for byte |
| `test_file_watcher.cpp` | baseline emits nothing; add/modify/remove detected via `scan_now()`; a file written in two steps is reported once, after it is stable; nested directories; unreadable directory returns error |
| `test_hot_reload.cpp` | texture -> material -> consumer fixture: modify leaf -> leaf RELOADED with version+1 and new pointer, material (flagged) reloaded once after the leaf, consumer (unflagged) untouched but sees new version; settings change in meta -> reimport; side-file change reloads owner; removal keeps payload; failed reload keeps old payload and fires RELOAD_FAILED; `a -> b -> c` each reloaded exactly once in order; `CACHE_MISS` self-heals |
| `test_asset_source.cpp` | contract suite over memory, cache, pack and composite sources (`contains`, `type_of`, `find_by_path`, `read`); composite resolves in order; `swap_source` while loaded |
| `test_asset_pack.cpp` | writer/reader round trip; TOC sorted; binary search hit/miss; identical payloads stored once; corrupt header, bad content hash, truncated blob rejected at `open()`; name table lookup; writing twice is byte-identical |
| `test_asset_cooker.cpp` | closure from roots excludes unreferenced assets; `mAlwaysInclude` and `mIncludeAll`; `EXCLUDE_FROM_PACK` honoured; stale blobs imported during cook; one failing asset -> error and no files written; manifest matches TOC; two cooks byte-identical |

Integration (`tests/integration/`): `test_assets_renderer_headless.cpp` (wave 3: PNG -> texture
through the null RHI; glTF -> model/mesh/material/texture sub-assets with external and embedded
images; texture reload propagates to material version), `test_assets_scene.cpp` (scene load acquires
handles, unload + `collect()` releases the whole tree), `test_export_roundtrip.cpp`.
Benchmarks (`tests/benchmarks/`): `bench_asset_ref_get.cpp` (10k `get()` < 20 us),
`bench_asset_load.cpp` (1,000 blobs through jobs, < 250 ms main-thread time), `bench_pack_lookup.cpp`.

---

## 11. Out of scope / future

- Native OS file watchers (ReadDirectoryChangesW, inotify, FSEvents) behind `file_watcher`, for
  projects above ~50k files.
- Zero-copy payload aliasing and memory-mapped packs; LRU eviction (today `collect()` is explicit).
- Stripping import/database/cooker sources from Distribution builds.
- Streaming loads (mips, audio); multiple packs per export (DLC); shared cache across machines.
- Editor UI for "regenerate uuid" / "repair meta" (area D defines the commands; this module exposes
  the functions).
