# Conjure ECS - Final Design (Area A: ECS internals)

Status: **binding**. Module `conjure::ecs` (wave 2). Headers in `engine/include/conjure/ecs/`, sources in
`engine/src/ecs/`, tests in `tests/ecs/`. Depends only on `conjure::core`. No third-party dependency.

This document merges two panel proposals. The performance-first proposal supplies the storage backbone,
the dense bit-mask component ids, the conflict-free stage scheduler and the one-move-per-entity command
flush. The robustness-first proposal supplies the error model (stale handles are recoverable errors,
contract violations assert *and* return an error), owner-thread capture, observers that only enqueue,
deterministic per-grain command merging, `world::validate()`/`world_stats`, name-keyed component ids,
typed in-memory archives and `unresolved_components` preservation. Every open question is resolved
below; implementers decide nothing architectural.

---

## 1. Overview & decisions

| # | Decision | Rationale |
|---|---|---|
| D1 | **Archetype storage, fixed 16 KiB SoA chunks, dense `entity_record` location table.** No sparse sets, no hybrid. Every live entity lives in an archetype, including the empty one (mask 0). | Fixed by the brief ("contiguous per archetype"). One storage model = one set of invariants. The record table gives O(1) random access for the OOP facade and C# `Entity`. Chunks are the job granule. |
| D2 | **`entity` = `{uint32_t index; uint32_t generation;}`**, 8 bytes, trivially copyable. Generation 0 is never issued, so the all-zero value is `NULL_ENTITY`. An index whose generation would wrap to 0 is retired. `to_u64()` = `(generation << 32) \| index` for C#. | Zero-initialised memory is null for free (C# `default(Entity)`, memset blobs). 32-bit generations never alias in practice; two named fields debug better than a packed word. |
| D3 | **Dense `uint16_t` component ids, `MAX_COMPONENTS = 1024`, `component_mask` = 16 x `uint64_t` (128 bytes).** Ids are assigned per *name* in registration order and never reused for the registry's lifetime; re-registering a name yields the same id. | Archetype matching is 16 AND/compare ops. Name-keyed ids are identical in every shared library and every world. Never reusing ids means a stale id can never alias another type; 1024 leaves room for C# type renames across a long editor session. |
| D4 | **One explicit `component_registry` object; worlds hold a non-owning reference. No singleton.** C++ types are named with `CONJURE_COMPONENT(Type, "module.name")` and registered explicitly (module entry, test setup); `id_of<T>()` is cached per type in an atomic `(registrySerial, id)` word. | Tests build a registry on the stack with no cross-test interference. Ids agree across editor world, play world and archives because they share the object. The cache is a per-DLL static but only caches a lookup result, so DLLs always agree. |
| D5 | **Chunk-granular change detection with a monotonic `uint64_t` world version.** Non-const span acquisition stamps `(chunk, column)` once; a row moving into a chunk stamps all its columns. Filter is `changed<T>` against a since-version. | No per-row bookkeeping, no false negatives, race-free because stamps are relaxed atomic stores. |
| D6 | **Structural changes are refused while the world is locked for iteration** (`CONJURE_ASSERT` + `STRUCTURE_LOCKED` error). Systems and observers use `command_buffer`; `command_buffer::create_entity()` returns a real id reserved immediately (64-id blocks, one mutex). Flush does one archetype move per touched entity. | Removes iterator-invalidation bugs and placeholder-id remapping. Real ids let a deferred create be used in later commands and handed to C# at once. |
| D7 | **Scheduler packs systems into conflict-free stages per phase from declared read/write masks and `after`/`before` edges; a stage's systems run as jobs; each system fans chunks out with `parallel_for`; per-grain command buffers are merged in grain order; buffers flush at stage end.** `scheduler_desc` can disable system-level and chunk-level parallelism independently. | Declared masks make conflicts a static, testable property. The serial mode is the same code path with stages executed inline, so headless tests and the parallel engine share semantics. |
| D8 | **Non-trivial components are supported via `component_ops` function pointers (nullptr = memcpy path).** Dynamic (C#) components are always trivial and described by a runtime field list that feeds core reflection and the default serializer. | `scene::children` needs a `small_vector`; the trivial path stays the fast path, chosen once per column. |
| D9 | **Hierarchy stays in `scene`.** ECS offers observers (which may only enqueue), `observe_destroyed`, reflection-driven entity-field remap, `changed<T>`, O(1) random access and batch instantiation. `destroy_entity` never cascades. | ECS cannot get hierarchy semantics wrong if it has none. |
| D10 | **Prefab base = `entity_archive`:** an in-memory, typed copy of N entities (component bytes produced by `ops.copy`), entity references inside the set stored as local indices. JSON (de)serialisation operates on archives. `instantiate_batch` places entities directly in their final archetype. | One pipeline: world <-> archive <-> JSON serves prefab assets, scene files, copy/paste, undo and C# `Spawn()`. |
| D11 | **Error model:** operations on dead/stale entities return `core::result` errors; contract violations (structural call while locked, wrong thread, observer mutating directly) `CONJURE_ASSERT` *and* return an error so Distribution stays defined. Unknown component names on load are preserved verbatim in `unresolved_components`. | Scripts hit stale handles routinely; a crash there is unacceptable. Data authored with an unloaded module must survive a round trip. |
| D12 | **Accessors that refresh or rebuild lazily are non-const, with no `mutable` caches.** `query::chunk_count`, `entity_count` and `matches` (and `view::entity_count`, which forwards) call `refresh()` first, which appends to the cached matches; `scheduler::stages` rebuilds a dirty phase first, which allocates and can fail. The query.h and scheduler.h summaries and the API page say so. | `const` keeps meaning "no state changes, safe to call from several threads at once"; `mutable` caches would make two threads reading one const query race on its match list. No caller needs a const variant (code sweep 2026-11-12, F636). |
| D13 | **A system name is unique within its phase.** `scheduler::add_system` returns `NAME_CONFLICT` and adds nothing when a live system of the same phase already carries the same non-null name. Unnamed systems (the null `string_id`), the same name in another phase and a name freed by `remove_system` never conflict; a disabled system keeps its name. `scheduler::find_system(phase, name)` is the lookup, and the engine's `ecs_module_consumer::validate` uses it (and compares the records of one module with each other) so a clashing module fails to load with `REGISTRATION_CONFLICT` instead of losing the system at attach. That check covers the systems the scheduler holds when the module loads: the engine adds its own systems at startup step 18, after the modules loaded at startup (step 15), so a startup module that takes an engine system's name is not refused and the engine's later `add_system` is the one that fails with `NAME_CONFLICT` (an `ERROR` from the subsystem; the scene, physics and scripting registrations stop at the first refused system, and startup continues). Module authors prefix system names with the module name, as engine systems do (`scene.propagate_transforms`), and never use an engine prefix. | The name is the system's identity for `after`/`before` edges, timings and the profiler. With duplicates an edge bound to whichever system was registered first, and a duplicate is almost always an accidental second registration that runs every system twice; `NAME_CONFLICT` is already the answer to the same mistake on component names (code sweep 2026-11-12, F259). |
| D14 | **`write_archive_json` writes each entity's component keys sorted by name**, in the byte order of the UTF-8 text (the order of `core::json`'s canonical form), with the preserved unknown components of `unresolved_components` merged into the same order; an unknown name a file listed twice keeps its file order. Entities stay in archive order and the fields inside a component in declaration order. `read_archive_json` accepts any key order. | Component ids follow registration order (D3), which changes with the features compiled in and the order modules load, so id order made the same scene saved by two builds differ. Name order makes scene and prefab files stable and diffable (`13_scene.md` S8) and matches the cooked form. Every file written before reorders once on its next save (code sweep 2026-11-12, F606). |
| D15 | **A system's access set follows the component registry.** A system that resolves its `reads` / `writes` by type or by name does so through `system_desc::resolveAccess`: `make_system` fills it from the pack, and the engine's `ecs_module_consumer` fills it from the read and write name lists of a module's `system_extension_desc`. `add_system` calls the hook, and `finalise()`, `run_phase` and `stages()` call every live system's hook again once the registry holds more component types than at the last pass (`component_registry::count()` against a stored count, one atomic load when nothing changed); a phase is marked for a rebuild only when one of its systems gained a bit. The hook only ever sets bits. A name that is unknown when a module attaches does not fail the load: the consumer logs one `WARN` per such name and the system runs without that access until a component of the name is registered. A misspelt name stays a warning; the Debug view and chunk-view asserts still catch the access itself. | A module may name a component of an optional dependency that is not loaded (`07_modules.md`), so refusing the load is wrong, and dropping the name silently and for good left the system with a smaller access set once that dependency did load: it shared a stage with a writer of the component, a data race no check reported. Ids are append-only (D3), so the count is a complete change signal and resolving again can only add bits. `make_system` systems added before their component types were registered had the same gap and get the same fix (code sweep 2026-11-12, F077). |

Rejected: sparse sets (violates the brief, multi-component iteration not contiguous); 32-bit packed ids (8-bit
generations wrap in seconds); per-row dirty bits (state to keep consistent across swap-removes); a singleton
registry (tests interfere, hidden global); lazy registration on first `id()` call (hidden side effects from any
thread, breaks module unload); fully serial systems (leaves cores idle once systems multiply); binary archive
blobs in v1 (JSON is enough for 10k entities; a binary form is additive later).

---

## 2. Data model

Naming note: classes use `m`-prefixed camelCase members; plain data `struct`s use unprefixed camelCase fields,
as in the style guide's `my_struct` example.

### 2.1 Entity and records

```cpp
struct entity {
    uint32_t index = 0;
    uint32_t generation = 0;

    [[nodiscard]] constexpr bool is_null() const noexcept;
    [[nodiscard]] constexpr uint64_t to_u64() const noexcept;
    [[nodiscard]] static constexpr entity from_u64(uint64_t packed) noexcept;
    constexpr bool operator==(const entity &other) const noexcept = default;
};

inline constexpr entity NULL_ENTITY = {};
inline constexpr uint32_t ARCHIVE_LOCAL_GENERATION = 0xFFFFFFFF;    // never issued by the allocator
```

`entity_record` (private): `archetype *archetype` (nullptr when not alive), `uint32_t chunkIndex`,
`uint32_t row`, `uint32_t generation`, `entity_state state` (`FREE`, `RESERVED`, `ALIVE`). 24 bytes, stored
in `std::vector<entity_record>` indexed by `entity.index`. 10k entities = 240 KB.

`entity_allocator` (private): `std::mutex mMutex`, `std::vector<free_slot{uint32_t index; uint32_t
generation;}> mFreeList` (LIFO), `uint32_t mNextIndex`. `reserve(std::span<entity> out)` is the only
thread-safe method: it pops free slots or bumps `mNextIndex`; it never touches the record vector. Records for
new indices are appended by the owner thread (direct create, or flush). `release(entity)` (owner thread) bumps
the generation, skips 0, retires the index if the generation would wrap, pushes the slot. `return_unused(span)`
pushes reserved-but-never-placed slots back without bumping.

### 2.2 Components

```cpp
inline constexpr uint32_t MAX_COMPONENTS = 1024;
inline constexpr uint32_t MAX_COMPONENT_SIZE = 4096;       // larger data belongs behind an asset handle
inline constexpr uint32_t MAX_COMPONENT_ALIGNMENT = 64;

struct component_id {
    static constexpr uint16_t INVALID = 0xFFFF;
    uint16_t value = INVALID;

    [[nodiscard]] constexpr bool is_valid() const noexcept;
    constexpr bool operator==(const component_id &other) const noexcept = default;
};

enum class component_flags : uint32_t {
    NONE = 0,
    TAG = 1 << 0,                       // size 0, lives only in the mask
    TRIVIAL = 1 << 1,                   // memcpy relocate/copy, no destructor
    DYNAMIC = 1 << 2,                   // registered at runtime (C#), fields described by data
    TRANSIENT = 1 << 3,                 // never serialised, no reflection required
    EDITOR_HIDDEN = 1 << 4              // internal types (unresolved_components); area H calls this INTERNAL (R32)
};
CONJURE_ENUM_FLAGS(component_flags);

struct component_ops {
    void (*constructDefault)(void *destination, uint32_t count) = nullptr;   // null => zero-fill / default bytes
    void (*destruct)(void *object, uint32_t count) = nullptr;                // null => no-op
    void (*relocate)(void *destination, void *source, uint32_t count) = nullptr;       // move + destruct src
    void (*copy)(void *destination, const void *source, uint32_t count) = nullptr;     // null => memcpy
};

struct component_info {
    component_id id = {};
    core::string_id name = {};
    uint32_t size = 0;
    uint32_t alignment = 1;
    component_flags flags = {};
    component_ops ops = {};
    component_serializer serializer = {};
    const core::reflect::type_info *reflection = nullptr;
    std::vector<uint32_t> entityFieldOffsets = {};        // byte offsets of every entity-typed field
    std::vector<uint32_t> assetRefFieldOffsets = {};      // byte offsets of every assets::asset_ref field (R25)
    std::vector<std::byte> defaultValue = {};             // dynamic types; empty = zero-fill
};
```

`component_mask` wraps `std::array<uint64_t, MAX_COMPONENTS / 64>` and offers `set`, `clear`, `test`,
`contains_all`, `contains_any`, `none`, `count`, `hash`, `for_each_set(callable(component_id))`, `|`, `&`, `~`,
`==` and a nested `hasher` for `std::unordered_map`.

C++ types are named with `CONJURE_COMPONENT(Type, "conjure.scene.local_transform")`, which specialises
`component_name<Type>::VALUE` (a `std::string_view`). Registration is explicit; the name's `core::string_id` is
the key. `registry.id_of<T>()` consults `component_type_cache<T>`: one `static std::atomic<uint64_t>` packing
`registrySerial << 16 | id`; on a serial mismatch it performs the name lookup and stores the packed word. A
registry serial is a process-unique `uint32_t` taken from an atomic counter at construction.

Dynamic descriptors (from scripting):

```cpp
struct dynamic_field_desc {
    core::string_id name = {};
    core::reflect::type_id type = {};       // primitive, math type, or type_id_of<entity>()
    uint32_t offset = 0;
};

struct dynamic_component_desc {
    core::string_id name = {};
    uint32_t size = 0;
    uint32_t alignment = 1;                 // power of two in [1, 16]
    std::span<const dynamic_field_desc> fields = {};
    std::span<const std::byte> defaultValue = {};
    component_flags flags = {};             // DYNAMIC | TRIVIAL are forced on
};
```

The registry constructor registers `unresolved_components` (id 0, non-trivial, `EDITOR_HIDDEN`) so every
registry can preserve unknown data.

### 2.3 Archetypes, chunks, layout

Constants (`archetype.h`): `CHUNK_SIZE = 16 * 1024`, `CHUNK_ALIGNMENT = 64`, `COLUMN_ALIGNMENT = 16` (minimum
column alignment for SIMD-friendly starts). `chunk_layout` (public struct) holds `uint32_t capacity` (rows per
chunk), `small_vector<component_id, 16> components` (column -> id, non-tag only), `small_vector<uint32_t, 16>
columnOffsets` and `columnSizes`, `std::array<uint16_t, MAX_COMPONENTS> columnIndex` (id -> column, `0xFFFF` if
absent) and `static core::result<chunk_layout> compute(const component_mask &mask, const component_registry
&registry)`.

Layout algorithm: `rowBytes = sizeof(entity) + sum(size_i)`; `capacity = CHUNK_SIZE / rowBytes`; the entity
column sits at offset 0; remaining columns are ordered by descending alignment, each at
`align_up(previousEnd, max(alignment_i, COLUMN_ALIGNMENT))`. If the final end exceeds `CHUNK_SIZE`, decrement
`capacity` and retry. `capacity == 0` yields `ARCHETYPE_TOO_LARGE`; `capacity < 8` logs one `WARN` per
archetype. Capacity is therefore maximal: `capacity + 1` never fits (tested). Examples: empty archetype 2048
rows; `{transform 64 B}` 227 rows; `{transform, sprite 32 B, velocity 12 B, health 8 B}` 132 rows, so 10k such
entities occupy 76 chunks.

`chunk` (private): `void *mBlock` from the world's `chunk_pool` (RAII wrapper over
`core::memory::pool_allocator`, 16 KiB blocks, 64-byte aligned), `uint32_t mCount`,
`std::vector<std::atomic<uint64_t>> mColumnVersions` (one per column, relaxed stores). The block is pure data;
all metadata is out of line, so layout maths never deals with header padding.

`archetype` (public class, read-only API; `world` is a friend and the sole mutator): `component_mask mMask`,
`chunk_layout mLayout`, `std::vector<std::unique_ptr<chunk>> mChunks`, `std::vector<archetype_edge> mEdges`
(`{component_id id; archetype *withAdded; archetype *withRemoved;}`, linear search, falls back to the world's
mask map), `uint32_t mEntityCount`. Rows are packed: only the last chunk may be partial; one drained trailing
chunk is kept as a spare, further spares go back to the pool. The world's `std::vector<std::unique_ptr<archetype>>`
is append-only, so `archetype *` is stable for the world's lifetime and queries cache it.

### 2.4 Queries and views

```cpp
struct query_desc {
    component_mask with = {};
    component_mask without = {};
    component_mask optional = {};
    component_mask writes = {};                  // subset of with | optional; drives stamping + validation
    component_mask changed = {};                 // chunk passes if any listed column changed since sinceVersion
    uint64_t sinceVersion = 0;
};
```

`query` is a value type holding `world *`, `query_desc`, a cached `std::vector<archetype *>` and
`uint32_t mSeenArchetypeCount` for incremental refresh. `view<ComponentTypes...>` is a typed facade over a
`query`: `const T` in the pack = read, `T` = write, `maybe<T>` = optional (callback receives `T *`, may be
null). Builder methods `with<Tag>()`, `without<T>()`, `changed<T>(sinceVersion)`. `chunk_view` is the untyped
per-chunk accessor and is what C# `Span<T>` is built on.

### 2.5 Command buffer

Records live in a `core::memory::arena` (stable addresses until `reset()`): `{opcode, entity, component_id,
payloadSize, payloadAlignment}` followed by the payload, which is relocated in via `ops.relocate` (or memcpy)
and destructed on apply or `clear()`. Opcodes: `CREATE`, `DESTROY`, `ADD`, `SET`, `REMOVE`, `INSTANTIATE`
(holds a `std::shared_ptr<const entity_archive>` and the pre-reserved ids). Each buffer owns a block of up to
64 reserved ids refilled from the allocator; unused ids survive `clear()` and are returned on destruction.

### 2.6 Entity archive

`archive_component {component_id id; std::vector<std::byte> bytes;}` (bytes produced by `ops.copy`, destructed
by the archive), `archive_entry {component_mask mask; std::vector<archive_component> components;}`,
`archive_options {bool keepExternalReferences = false;}` (true for editor copy/paste inside one world).
Entity-typed fields that point inside the captured set hold `{localIndex, ARCHIVE_LOCAL_GENERATION}`;
references outside the set become `NULL_ENTITY` unless `keepExternalReferences` is set. Entry 0 is the root.

---

## 3. Algorithms / flow

### 3.1 Entity lifecycle (owner thread, world unlocked)

- `create_entity(mask)`: reserve one id; append a record if the index is new; `archetype::push_row` (allocate
  a chunk when the last one is full); default-construct each column (`defaultValue`/zero for trivial,
  `ops.constructDefault` otherwise); stamp every column version of that chunk with `version()`; write the
  record (`ALIVE`); fire `ADDED` observers for each set bit. `create_entities(mask, span out)` fills chunk by
  chunk (one loop per chunk) and is what `instantiate_batch` uses.
- `destroy_entity(e)`: not alive -> `ENTITY_NOT_ALIVE`; fire `REMOVED` observers for each column then
  `observe_destroyed` callbacks (observers may only enqueue into the deferred buffer); destruct non-trivial
  columns; `remove_row`; `release(e)`; set `archetype = nullptr`; then drain the deferred buffer until empty
  (Debug guard: assert after 1024 drain passes, which means a cycle in caller data).
- `remove_row(a, c, r)`: if `(c, r)` is not the archetype's last row, relocate every column of the last row
  into the hole, copy its entity, update that entity's record, and stamp all column versions of chunk `c`.
  Decrement the last chunk's count; keep one empty spare, return any other.
- `move_entity(e, dst)`: push a row in `dst`; relocate shared columns; default-construct added columns;
  destruct removed columns (after `REMOVED` observers have run); `remove_row` in `src`; update the record;
  stamp the destination chunk; fire `ADDED` observers for added columns.
- `add_raw(e, id, bytes)`: validate alive, id active, not present (`COMPONENT_ALREADY_PRESENT`); resolve the
  edge; `move_entity`; copy the payload with `ops.copy` or memcpy; return the pointer.

### 3.2 Query matching and iteration

`query::refresh()` scans archetypes `[mSeenArchetypeCount, world.archetype_count())` and keeps those with
`mask.contains_all(with) && !mask.contains_any(without)`; cost is O(new archetypes). Iteration skips empty
archetypes and chunks. With a `changed` mask, a chunk is visited only if
`max(columnVersion[c] for c in changed) > sinceVersion`. Iteration holds an `iteration_scope` for its duration.

### 3.3 Change detection

`world::mVersion` starts at 1 and `bump_version()` increments it at the start of every system run, every
flush and every direct structural call. `chunk_view::get<T>()` with non-const `T`, `column_data(id, true)`,
`world::get<T>()` (non-const) and `component_ref::write()` store `version()` into the column's atomic. A system
records the version observed at the start of its previous run as `sinceVersion`; because the world version is
bumped before each run, a system never sees its own previous writes but does see every later writer.
Granularity is the chunk: up to one chunk of false positives per real write, never a false negative.

### 3.4 Command buffer flush

`world::flush(buffer)` (owner thread, unlocked): `bump_version()`, append records for reserved indices
`>= mRecords.size()`, then:

1. **Plan.** Walk the records once. Per touched entity (dense scratch vector, indices via a flush-stamped
   `std::vector<int32_t>` slot table indexed by `entity.index`) accumulate: `destroy` flag, `addMask`,
   `removeMask`, list of `SET`/`ADD` payload offsets. Rules, in record order: `ADD` sets the add bit, clears the
   remove bit; `REMOVE` sets the remove bit, clears the add bit and drops pending payloads for that component;
   `SET` appends a payload; `DESTROY` wins over every other command for that entity in the buffer. Commands on
   entities that are not alive and not `RESERVED` are skipped, logged at `DEBUG`, counted in
   `flush_report::skipped`.
2. **Apply.** For each touched entity: `finalMask = (currentMask | addMask) & ~removeMask`; reserved entities
   are placed directly into `finalMask`'s archetype (no empty-archetype hop); live entities take one
   `move_entity` if the mask changed. Observers fire only for the delta between current and final masks.
   Payloads apply in record order: `ADD` payload copies into the fresh column; `SET` on an existing non-trivial
   component is destruct + copy-construct, memcpy for trivial. `INSTANTIATE` runs 3.6 with its pre-reserved
   ids. Destroys run last.
3. `buffer.clear()` resets the arena (destructing unconsumed payloads) and keeps unused reserved ids.

### 3.5 Scheduler

```text
run_phase(p, dt):
    if dirty[p]: build_stages(p)
    for stage in stages[p]:
        if stage.exclusive: bump_version(); run on calling thread, world unlocked; flush; continue
        scope = world.lock_iteration()
        for system in stage (main-thread systems last, on the calling thread):
            parallelSystems ? jobs.schedule(run_system) : run_system inline
        jobs.wait(stage handle)              // no-op in inline mode
        release scope
        for system in stage order: world.flush(system.commands)
```

The per-stage bookkeeping (the job closures and the main-thread list) lives in the phase's state and is kept
at capacity, so a phase that has run once allocates nothing of its own afterwards (with `parallelSystems` and
a job system, `jobs.schedule` still allocates its completion counter once per stage). The job closures reach
the frame through the scheduler, which saves and restores that pointer around the phase so an `EXCLUSIVE`
system may run another phase from inside this one.

`build_stages(p)`: resolve `after`/`before` names into edges (unknown names are ignored with one `DEBUG` log
each, because the referenced module may be compiled out); Kahn topological sort with registration order as the
tiebreak (a cycle is detected incrementally in `add_system`, which fails with `SYSTEM_CYCLE` and does not add
the system); then greedy append-only packing: a system joins the *current* stage if every predecessor is in an
earlier stage, it is not `EXCLUSIVE`, the stage is not exclusive, and for every member `s`:
`(writes & (s.reads | s.writes)).none() && (reads & s.writes).none()`. Otherwise a new stage opens. Stages are
rebuilt lazily after `add_system`/`remove_system`/`set_enabled`, and after an access set grew (below).

Access sets follow the registry (D15). `run_phase`, `finalise()` and `stages()` start with `refresh_access()`:
it compares `registry.count()` with the count stored at the last pass and returns when they are equal. Otherwise
it calls `resolveAccess(registry, reads, writes)` of every live system that has one (disabled systems included,
they can be enabled again), marks the phase of each system whose masks changed as dirty with one `DEBUG` log,
and stores the count it read before the pass, so a type another thread registers during the pass triggers the
next one. Only the dirty flag is set: an `EXCLUSIVE` system may register a component and run another phase while
its own phase is walking its stages, and that phase is then rebuilt before its next run. A component registered
while a phase runs therefore takes effect from the next `run_phase` of each phase. The pass also waits while
the world is locked for iteration (`finalise()` or `stages()` called from inside a system): the systems of a
running stage read their masks on other threads. Systems with hand-filled masks and no hook are never touched.

A name stands for exactly one system of a phase (D13): `add_system` looks the name up among the live systems of
the phase (`find_system`, disabled systems included, the null name never) and fails with `NAME_CONFLICT` before
it takes a record, so edge resolution never has to choose between two systems of one name. Unnamed systems may
repeat, so a null entry in `after` / `before` binds to none of them: it is resolved as an unknown name. Whoever
adds the name first keeps it: the module consumer's `validate` refuses a module whose system clashes with one
already added, and a system added later (the engine's own, at startup step 18, when a startup module holds its
name) is refused by `add_system` itself.

`run_system`: `bump_version()`; `system_context ctx{world, dt, sinceVersion, system buffer, jobs, desc}`;
`run(ctx)`; record `sinceVersion = version at start` and the elapsed time. `ctx.parallel_for_chunks(query,
fn)`: flatten matches to `(archetype*, chunkIndex)` pairs; `grain = max(1, minEntitiesPerJob / capacity)`
chunks per job; take one `command_buffer` per grain from the context's buffer pool (grown on demand, arenas
retained across frames); `jobs->parallel_for(grainCount, 1, job)` or an inline loop when `jobs == nullptr`
or `parallelChunks == false`; after the join, append grain buffers into the system buffer **in grain order**.
Command application order is thus deterministic regardless of which worker ran which grain. Entity id *values*
reserved from parallel code are not stable across runs; gameplay must not depend on them (tests that compare
ids set `parallelChunks = false`).

Debug-only validation: a `view`/`query` created through `system_context` must have `with | optional` inside
`reads | writes` and `writes` inside the declared `writes`; `chunk_view::get<T>()` non-const inside a system
asserts if `T` is not declared written. Violations assert and log the system name.

### 3.6 Archive capture and instantiation

`entity_archive::capture(world, span<entity>, options)`: one entry per entity in order; each non-`TRANSIENT`
component copied with `ops.copy` into `bytes`; then every `entityFieldOffsets` entry is rewritten: inside the
set -> `{localIndex, ARCHIVE_LOCAL_GENERATION}`, outside -> `NULL_ENTITY` or kept per options.
`world::instantiate_batch(archive, outRoots, commands)`: group entries by mask; resolve each archetype once;
reserve `count * n` ids in one call; `create_entities` per group; copy payloads; build an `entity_remap` (flat
`std::vector<entity>` local -> real) per copy and rewrite entity fields. A local index `>= n` fails with
`ARCHIVE_CORRUPT` before any entity is created (archives are validated on construction and after JSON load).
With a command buffer the call records `INSTANTIATE` (ids reserved now, roots returned now).

### 3.7 Serialisation

`write_archive_json(archive, writer)` (the archive carries its registry) emits
`{"entities":[{"components":{"<name>":{...}}}]}` with entity fields as integer local index, `null`, or
`{"external":"<index>:<generation>"}` when kept. `read_archive_json(value, registry)` resolves names; unknown
names go into an `unresolved_components` component (`std::vector<unresolved_component{std::string name;
std::string json;}>`) and are re-emitted verbatim by `write_archive_json`. The keys of one entity's `components`
object are written sorted by name, in the byte order of the UTF-8 text, preserved unknown names included (D14):
the writer gathers the keys of an entry into a buffer it reuses across the archive, sorts it and writes in that
order, so the text is independent of component ids. The default serializer walks
`core::reflect::type_info` fields: bool, integers, floats, `std::string`, `string_id`, `uuid`, math types
(arrays), `entity`, `assets::asset_ref<T>` (reflection kind `ASSET_REF`, written as the uuid string; the slot
pointer is left null on read and bound afterwards by the scene module's bind pass over
`component_info::assetRefFieldOffsets` with `assets::asset_manager::bind_slot`, so `ecs` still depends on `core`
only, R25), nested reflected structs, `std::array<T, N>`. Any other field kind fails registration with
`UNSUPPORTED_FIELD` unless the component supplies custom hooks or is `TRANSIENT`.

---

## 4. Public API

| Header | Main types |
|---|---|
| `ecs.h` | umbrella include |
| `log_category.h` | `ecs` log category declaration |
| `ecs_error.h` | `ecs_error`, `make_error(ecs_error, std::string_view)` |
| `entity.h` | `entity`, `NULL_ENTITY`, `ARCHIVE_LOCAL_GENERATION`, `entity_remap`, `std::hash<entity>` |
| `component.h` | `component_id`, `component_flags`, `component_ops`, `component_info`, `component_mask`, `component_name<T>`, `CONJURE_COMPONENT`, limits |
| `component_registry.h` | `component_registry`, `dynamic_component_desc`, `dynamic_field_desc` |
| `archetype.h` | `archetype` (read-only), `chunk_layout`, `CHUNK_SIZE`, `CHUNK_ALIGNMENT`, `COLUMN_ALIGNMENT` |
| `chunk_view.h` | `chunk_view` |
| `query.h` | `query_desc`, `query` |
| `view.h` | `view<ComponentTypes...>`, `maybe<T>`, `view_access` |
| `command_buffer.h` | `command_buffer`, `flush_report` |
| `observer.h` | `component_event`, `component_observer_fn`, `entity_observer_fn`, `observer_id` |
| `world.h` | `world`, `world_desc`, `world_stats`, `iteration_scope` |
| `entity_handle.h` | `entity_handle`, `component_ref<T>` |
| `system_phase.h` | `system_phase` (the list is owned by area F, `06_engine_loop.md` 2.4; R1), `frame_info` |
| `system.h` | `system_flags`, `system_desc`, `system_id`, `system_context`, `make_system<Ts...>` |
| `component_type_desc.h` | `component_type_desc`, `system_extension_desc`, `describe_component<T>()` (trivially copyable module extension records, R2/R19) |
| `world_hash.h` | `world_hash(const world &)` (deterministic state hash for tests; ordering rules in `08_testing.md` 3.4; R32) |
| `scheduler.h` | `scheduler`, `scheduler_desc`, `stage_info`, `system_timing` |
| `entity_archive.h` | `entity_archive`, `archive_entry`, `archive_component`, `archive_options` |
| `serialization.h` | `component_serializer`, `make_default_serializer<T>()`, `unresolved_components`, `write_archive_json`, `read_archive_json` |

Private files in `engine/src/ecs/`: `entity_allocator.{h,cpp}`, `chunk.{h,cpp}`, `chunk_pool.{h,cpp}`,
`archetype.cpp`, `component_mask.cpp`, `component_registry.cpp`, `query.cpp`, `chunk_view.cpp`, `world.cpp`,
`world_structural.cpp` (move/destroy/flush), `command_buffer.cpp`, `scheduler.cpp`, `scheduler_stages.cpp`,
`entity_archive.cpp`, `serialization.cpp`, `log_category.cpp`, `README.md`. Every public method carries a
`/// <summary>` block in the real header; the listings below show signatures only and omit private members
except where they document ownership.

### 4.1 `ecs_error.h`

`enum class ecs_error : uint16_t` with values `NONE = 0`, `ENTITY_NOT_ALIVE`, `ENTITY_LIMIT_REACHED`,
`COMPONENT_ALREADY_PRESENT`, `COMPONENT_NOT_PRESENT`, `UNKNOWN_COMPONENT`, `COMPONENT_LIMIT_REACHED`,
`COMPONENT_IN_USE`, `COMPONENT_TOO_LARGE`, `BAD_ALIGNMENT`, `NAME_CONFLICT`, `UNSUPPORTED_FIELD`,
`ARCHETYPE_TOO_LARGE`, `STRUCTURE_LOCKED`, `WRONG_THREAD`, `OUT_OF_MEMORY`, `ARCHIVE_CORRUPT`,
`SERIALIZATION_FAILED`, `SYSTEM_CYCLE`, `SYSTEM_NOT_FOUND` (one per line in the header), plus
`[[nodiscard]] core::error make_error(ecs_error code, std::string_view detail = {});`.

### 4.2 `component_registry.h`

```cpp
// Members: serial, std::vector<component_info> (reserved to MAX_COMPONENTS, append-only), atomic count,
// name -> id map, owned dynamic type_infos, live world list, mutex, on_registered event. Non-copyable.
class component_registry {
    public:
        component_registry();
        ~component_registry();

    public:
        template <typename ComponentType>
        [[nodiscard]] core::result<component_id> register_component(component_flags flags = {});
        [[nodiscard]] core::result<component_id> register_dynamic(const dynamic_component_desc &desc);
        [[nodiscard]] core::result<void> redefine_dynamic(component_id id, const dynamic_component_desc &desc);
        template <typename ComponentType> [[nodiscard]] component_id id_of() const noexcept;   // INVALID if absent
        [[nodiscard]] component_id find(core::string_id name) const noexcept;
        [[nodiscard]] const component_info &info(component_id id) const noexcept;
        [[nodiscard]] uint32_t count() const noexcept;
        [[nodiscard]] std::span<const component_info> types() const noexcept;   // editor / test enumeration (R32)
        [[nodiscard]] uint32_t serial() const noexcept;
        [[nodiscard]] bool has_instances(component_id id) const;    // sums over live worlds
        [[nodiscard]] core::event<const component_info &> &on_registered() noexcept;
};
```

`register_component<T>` fills size/alignment from the type, `COMPONENT_TAG` for empty types,
`COMPONENT_TRIVIAL` when `std::is_trivially_copyable_v<T>`, otherwise generates `component_ops` from the
type's special members; `reflection = core::reflect::type_info_of<T>()`, required unless `TRANSIENT` or tag;
`entityFieldOffsets` and the default serializer derive from reflection. `register_dynamic` builds a runtime
`type_info`, forces `DYNAMIC | TRIVIAL`, accepts alignment in {1,2,4,8,16}. `redefine_dynamic` fails with
`COMPONENT_IN_USE` while `has_instances(id)`; the id is kept. Limits: `COMPONENT_LIMIT_REACHED` at 1024,
`COMPONENT_TOO_LARGE` above 4096 bytes, `BAD_ALIGNMENT` for non-power-of-two or > 64.

### 4.3 `world.h`

`world_desc {std::string name = "world"; uint32_t initialEntityCapacity = 1024; uint32_t chunksPerSlab = 64;}`,
`world_stats {uint32_t aliveEntities; uint32_t archetypes; uint32_t chunks; uint64_t chunkBytes; uint64_t
version;}`, `iteration_scope` (RAII, movable; increments `world::mLockDepth` on construction, decrements on
destruction). `world` members: `const component_registry *mRegistry`, `std::thread::id mOwnerThread`,
`std::vector<entity_record> mRecords`, `entity_allocator mAllocator`, `chunk_pool mChunkPool`,
`std::vector<std::unique_ptr<archetype>> mArchetypes`, `std::unordered_map<component_mask, archetype *,
component_mask::hasher> mArchetypeByMask`, `archetype *mEmptyArchetype`, `std::atomic<int32_t> mLockDepth`,
`std::atomic<uint64_t> mVersion = 1`, `bool mInObserver`, `observer_table mObservers`, `command_buffer
mDeferredFromObservers`. Non-copyable and non-movable (handles store `world *`).

```cpp
class world {
    public:
        explicit world(const component_registry &registry, world_desc desc = {});
        ~world();

    public:
        // Entities. Owner thread only, world unlocked.
        [[nodiscard]] core::result<entity> create_entity(const component_mask &mask = {});
        [[nodiscard]] core::result<void> create_entities(const component_mask &mask, std::span<entity> out);
        [[nodiscard]] core::result<void> destroy_entity(entity target);
        [[nodiscard]] bool is_alive(entity target) const noexcept;         // any thread
        [[nodiscard]] uint32_t entity_count() const noexcept;
        [[nodiscard]] component_mask mask_of(entity target) const noexcept;

        // Typed components. Pointers are valid until the next structural change.
        template <typename ComponentType>
        [[nodiscard]] core::result<ComponentType *> add(entity target, ComponentType value = {});
        template <typename ComponentType> [[nodiscard]] core::result<void> remove(entity target);
        template <typename ComponentType> [[nodiscard]] core::result<void> set(entity target, ComponentType value);
        template <typename ComponentType> [[nodiscard]] ComponentType *get(entity target) noexcept;   // stamps
        template <typename ComponentType> [[nodiscard]] const ComponentType *get(entity target) const noexcept;
        template <typename ComponentType> [[nodiscard]] bool has(entity target) const noexcept;

        // Untyped components (bindings, serialisation).
        [[nodiscard]] core::result<void *> add_raw(
          entity target,
          component_id id,
          std::span<const std::byte> initial = {}
        );
        [[nodiscard]] core::result<void> remove_raw(entity target, component_id id);
        [[nodiscard]] void *get_raw(entity target, component_id id, bool forWrite) noexcept;
        [[nodiscard]] bool has_raw(entity target, component_id id) const noexcept;
        [[nodiscard]] uint32_t instance_count(component_id id) const noexcept;
        [[nodiscard]] core::result<void> remove_from_all(component_id id);

        // Queries.
        template <typename... ComponentTypes> [[nodiscard]] view<ComponentTypes...> view();
        [[nodiscard]] query create_query(const query_desc &desc);
        [[nodiscard]] std::span<archetype *const> archetypes() const noexcept;

        // Deferred mutation, locking, versions.
        [[nodiscard]] flush_report flush(command_buffer &commands);
        [[nodiscard]] iteration_scope lock_iteration() noexcept;
        [[nodiscard]] bool is_locked() const noexcept;
        [[nodiscard]] uint64_t version() const noexcept;
        uint64_t bump_version() noexcept;

        // Archives.
        [[nodiscard]] core::result<entity> instantiate(
          const std::shared_ptr<const entity_archive> &archive,
          command_buffer *commands = nullptr
        );
        [[nodiscard]] core::result<void> instantiate_batch(
          const std::shared_ptr<const entity_archive> &archive,
          std::span<entity> outRoots,
          command_buffer *commands = nullptr,
          std::vector<entity> *outEntities = nullptr     // every created id, copy-major in entry order; empty after a failure
        );

        // Observers.
        [[nodiscard]] observer_id observe(component_id id, component_event event, component_observer_fn fn);
        [[nodiscard]] observer_id observe_destroyed(entity_observer_fn fn);
        void unobserve(observer_id id);

        // Misc.
        [[nodiscard]] entity_handle handle(entity target) noexcept;
        [[nodiscard]] const component_registry &registry() const noexcept;
        [[nodiscard]] std::thread::id owner_thread() const noexcept;
        [[nodiscard]] core::result<void> set_owner_thread(std::thread::id thread);   // only while unlocked
        [[nodiscard]] world_stats stats() const noexcept;
        [[nodiscard]] core::result<void> validate() const;                    // full invariant check
        void clear();                                                         // destroys all, fires observers
};
```

### 4.4 `chunk_view.h`, `query.h`, `view.h` (DOD API)

```cpp
class chunk_view {                 // holds archetype *, chunk *, stamp version, optional system_context *
    public:
        [[nodiscard]] uint32_t count() const noexcept;
        [[nodiscard]] std::span<const entity> entities() const noexcept;
        template <typename ComponentType>                       // const T => span<const T>, no stamp
        [[nodiscard]] std::span<ComponentType> get() noexcept;  // non-const stamps the column version once
        template <typename ComponentType>
        [[nodiscard]] std::span<ComponentType> get_optional() noexcept;    // empty span if absent
        [[nodiscard]] bool has(component_id id) const noexcept;
        [[nodiscard]] void *column_data(component_id id, bool forWrite) noexcept;   // nullptr if absent
        [[nodiscard]] uint32_t column_stride(component_id id) const noexcept;
        [[nodiscard]] uint64_t column_version(component_id id) const noexcept;
};

class query {                      // value type; query(world &, query_desc); caches archetype * matches
    public:
        template <typename Callable> void for_each_chunk(Callable &&callable);     // callable(chunk_view &)
        [[nodiscard]] uint32_t chunk_count();                   // refreshes first, hence non-const (D12)
        [[nodiscard]] uint32_t entity_count();                  // refreshes first, hence non-const (D12)
        [[nodiscard]] chunk_view chunk(uint32_t index);         // flat index over matches (bindings)
        [[nodiscard]] std::span<archetype *const> matches();    // refreshes first, hence non-const (D12)
        [[nodiscard]] const query_desc &desc() const noexcept;
        void refresh();
};

template <typename ComponentType> struct maybe {};               // optional component marker
struct view_access { component_mask reads = {}; component_mask writes = {}; };

template <typename... ComponentTypes>
class view {                       // owns one query; explicit view(world &)
    public:
        template <typename TagType> [[nodiscard]] view &with();
        template <typename OtherType> [[nodiscard]] view &without();
        template <typename OtherType> [[nodiscard]] view &changed(uint64_t sinceVersion);
        template <typename Callable> void for_each(Callable &&callable);          // (entity, ComponentTypes&...)
        template <typename Callable> void for_each_chunk(Callable &&callable);    // (chunk_view &)
        template <typename Callable>                                              // (chunk_view &, command_buffer &)
        void parallel_for_each_chunk(system_context &context, Callable &&callable);
        [[nodiscard]] uint32_t entity_count();
        [[nodiscard]] view_access access() const noexcept;
        [[nodiscard]] query &underlying() noexcept;
};
```

### 4.5 `entity_handle.h` (OOP API)

```cpp
class entity_handle {              // world * + entity; entity_handle(world &, entity); re-validates on every call
    public:
        [[nodiscard]] entity id() const noexcept;
        [[nodiscard]] bool is_alive() const noexcept;
        template <typename ComponentType>
        [[nodiscard]] core::result<component_ref<ComponentType>> add(ComponentType value = {});
        template <typename ComponentType> [[nodiscard]] component_ref<ComponentType> component() const noexcept;
        template <typename ComponentType> [[nodiscard]] bool has() const noexcept;
        template <typename ComponentType> [[nodiscard]] core::result<void> remove();
        [[nodiscard]] core::result<void> destroy();
};

template <typename ComponentType>
class component_ref {              // world * + entity; component_ref(world &, entity); re-resolves on every access
    public:
        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] const ComponentType *read() const noexcept;    // nullptr if dead or missing
        [[nodiscard]] ComponentType *write() noexcept;               // stamps the chunk column
        [[nodiscard]] core::result<void> set(ComponentType value);
};
```

### 4.6 `command_buffer.h` and `observer.h`

```cpp
struct flush_report {
    uint32_t applied = 0;
    uint32_t skipped = 0;
};

class command_buffer {             // world *, core::memory::arena, std::array<entity, 64> reserved ids
    public:
        explicit command_buffer(world &target);
        ~command_buffer();                       // WARN + discard if non-empty; returns unused ids
        command_buffer(command_buffer &&other) noexcept;
        command_buffer &operator=(command_buffer &&other) noexcept;

    public:
        [[nodiscard]] entity create_entity();                        // real id; usable in later commands
        void destroy_entity(entity target);
        template <typename ComponentType> void add(entity target, ComponentType value = {});
        template <typename ComponentType> void set(entity target, ComponentType value);
        template <typename ComponentType> void remove(entity target);
        void add_raw(entity target, component_id id, std::span<const std::byte> payload);
        void set_raw(entity target, component_id id, std::span<const std::byte> payload);
        void remove_raw(entity target, component_id id);
        [[nodiscard]] core::result<entity> instantiate(std::shared_ptr<const entity_archive> archive);
        void append(command_buffer &&other);                         // preserves order, moves payloads
        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] uint32_t command_count() const noexcept;
        void clear() noexcept;
};
```

`observer.h`: `enum class component_event : uint8_t { ADDED, REMOVED }`; `component_observer_fn =
std::function<void(world &owner, entity target, component_id id, command_buffer &deferred)>`;
`entity_observer_fn = std::function<void(world &owner, entity target, command_buffer &deferred)>`;
`observer_id {uint32_t index; uint32_t generation;}`.

### 4.7 `system.h` and `scheduler.h`

`enum class system_phase : uint8_t { FRAME_BEGIN, FIXED_UPDATE, PHYSICS_STEP, FIXED_END, UPDATE, LATE_UPDATE,
TRANSFORM, SYNC, EXTRACT, FRAME_END, COUNT }` lives in `conjure/ecs/system_phase.h` (one per line). The *list* is
owned by area F (`06_engine_loop.md` 2.4), the header by this module (cross-area resolution R1). The same header
declares `struct frame_info { float deltaTime; float fixedDeltaTime; float interpolationAlpha; double time;
uint64_t frameIndex; bool inFixedStep; }`, filled by the engine before every `run_phase` (R2).
`system_id {uint32_t index; uint32_t generation;}`,
`system_timing {core::string_id name; uint64_t microseconds;}`, `stage_info {std::vector<system_id> systems;
bool exclusive;}`.

```cpp
enum class system_flags : uint32_t {
    NONE = 0,
    MAIN_THREAD = 1 << 0,            // thread mode: runs on the run_phase caller thread (neither bit = PARALLEL)
    EXCLUSIVE = 1 << 1,              // thread mode: alone in its stage, receives an unlocked world
    DISABLED = 1 << 2,
    RUNS_WHILE_PAUSED = 1 << 3       // selected by run_phase_filtered while the engine is PAUSED (R2)
};
CONJURE_ENUM_FLAGS(system_flags);

struct system_desc {
    core::string_id name = {};
    system_phase phase = system_phase::UPDATE;
    system_flags flags = system_flags::NONE;
    component_mask reads = {};
    component_mask writes = {};
    core::small_vector<core::string_id, 4> after = {};
    core::small_vector<core::string_id, 4> before = {};
    std::function<void(system_context &context)> run = {};

    /// <summary>
    /// Optional: sets the bits of the component types the registry knows. Called by add_system and again
    /// whenever the registry has gained types (D15); only ever sets bits.
    /// </summary>
    std::function<void(const component_registry &registry, component_mask &reads, component_mask &writes)>
      resolveAccess = {};
};

class system_context {
    public:
        [[nodiscard]] ecs::world &world() noexcept;
        [[nodiscard]] float delta_time() const noexcept;             // frame().deltaTime
        [[nodiscard]] const frame_info &frame() const noexcept;      // full per-frame timing block (R2)
        [[nodiscard]] uint64_t since_version() const noexcept;      // version at the start of the previous run
        [[nodiscard]] command_buffer &commands() noexcept;           // system buffer; system thread only
        [[nodiscard]] core::jobs::job_system *jobs() noexcept;      // nullptr => inline
        [[nodiscard]] const system_desc &desc() const noexcept;
        template <typename... ComponentTypes> [[nodiscard]] ecs::view<ComponentTypes...> view();   // validated
        template <typename Callable>                                // callable(chunk_view &, command_buffer &)
        void parallel_for_chunks(query &target, Callable &&callable);
};

template <typename... ComponentTypes>
[[nodiscard]] system_desc make_system(
  core::string_id name,
  system_phase target,
  std::function<void(system_context &context)> run
);

struct scheduler_desc {
    bool parallelSystems = true;
    bool parallelChunks = true;
    uint32_t minEntitiesPerJob = 1024;
};

class scheduler {                  // world *, job_system *, unique_ptr<system_record> list, stages per phase
    public:
        scheduler(ecs::world &target, core::jobs::job_system *jobs, scheduler_desc desc = {});
        ~scheduler();

    public:
        [[nodiscard]] core::result<system_id> add_system(system_desc desc);    // NAME_CONFLICT, SYSTEM_CYCLE
        [[nodiscard]] core::result<void> remove_system(system_id id);
        [[nodiscard]] core::result<void> set_enabled(system_id id, bool enabled);

        /// <summary>The live system of a phase named 'name'; an invalid id when there is none (D13).</summary>
        [[nodiscard]] system_id find_system(system_phase target, core::string_id name) const noexcept;

        /// <summary>
        /// Builds every phase's stages now and reports SYSTEM_CYCLE and Debug access conflicts (R2). Resolves
        /// the access sets again first when the registry has grown (D15). Optional: run_phase builds lazily;
        /// the engine calls it at startup step 18 and after every module load so configuration errors
        /// surface before the first frame instead of inside it.
        /// </summary>
        [[nodiscard]] core::result<void> finalise();
        void run_phase(system_phase target, const frame_info &frame);

        /// <summary>Runs only the systems of the phase whose flags contain every bit of required (R2).</summary>
        void run_phase_filtered(system_phase target, const frame_info &frame, system_flags required);
        [[nodiscard]] std::span<const stage_info> stages(system_phase target);   // builds if dirty (D12)
        [[nodiscard]] std::span<const system_timing> last_timings(system_phase target) const noexcept;
};
```

`make_system<Ts...>` derives `reads`/`writes` from the pack (`const T` read, `T` write, `maybe<T>` by
constness) so the common one-view system declares nothing by hand. It does so through `resolveAccess`, so a
pack type that is registered after the system was added joins the access set then (D15).

Module extension records (`component_type_desc.h`, R2/R19). Plugins cannot hand the module system a `system_desc`
(it holds `std::function` and `small_vector`), so two trivially copyable mirrors exist for
`modules::module_registrar::register_extension`:

```cpp
struct component_type_desc {
    static constexpr core::string_id EXTENSION_POINT{"ecs.component"};
    const char *name = nullptr;                 // "com.studio.spinner"
    uint32_t size = 0;
    uint32_t alignment = 1;
    component_flags flags = component_flags::NONE;
    component_ops ops = {};
    component_serializer serializer = {};       // null for a reflected type: the registry's walker serialises the deep copy
    const core::reflect::type_info *reflection = nullptr;   // deep-copied by the registry at attach
};
template <typename ComponentType> [[nodiscard]] component_type_desc describe_component(component_flags flags = {});

struct system_extension_desc {
    static constexpr core::string_id EXTENSION_POINT{"ecs.system"};
    const char *name = nullptr;
    system_phase phase = system_phase::UPDATE;
    system_flags flags = system_flags::NONE;
    const core::string_id *reads = nullptr;     uint32_t readCount = 0;      // component names
    const core::string_id *writes = nullptr;    uint32_t writeCount = 0;
    const core::string_id *after = nullptr;     uint32_t afterCount = 0;     // system labels
    const core::string_id *before = nullptr;    uint32_t beforeCount = 0;
    void (*run)(system_context &context) = nullptr;
};
```

The engine's `ecs_module_consumer` turns them into `register_described` / `add_system` calls on attach and into
`remove_system` / `remove_from_all` on detach. The read and write name lists are copied into the system's
`resolveAccess` hook, so the scheduler resolves them by name when the system is added and again when the
registry has grown (D15). A name that is not registered once the module's own components are is logged with one
`WARN` (module, system, name, list and position) and does not fail the load: it is a component of an optional
dependency that is not loaded, or a typo, and only the first ever resolves.
`describe_component<T>` leaves `serializer` null for a reflected type so that no function pointer into the
module reaches the registry: the registry serialises the type through its own deep copy of the reflection
description, which is what lets data authored with the module survive its unload (D11). A record without
reflection (scripting's `script_component`, R29) must supply `writeJson` and `readJson` (`UNSUPPORTED_FIELD`
otherwise); `register_described` copies the hooks as given.

### 4.8 `entity_archive.h` and `serialization.h`

```cpp
class entity_archive {             // registry *, vector<archive_entry>; move-only; dtor destructs payloads
    public:
        [[nodiscard]] static core::result<entity_archive> capture(
          const world &source,
          std::span<const entity> entities,
          archive_options options = {}
        );
        [[nodiscard]] uint32_t entity_count() const noexcept;
        [[nodiscard]] std::span<const archive_entry> entries() const noexcept;
        [[nodiscard]] core::result<void> validate() const;           // local indices in range, ids active
};

struct component_serializer {
    core::result<void> (*writeJson)(const void *component, core::json::writer &out) = nullptr;
    core::result<void> (*readJson)(void *component, const core::json::value &in) = nullptr;
    void (*remapEntities)(void *component, const entity_remap &map) = nullptr;
};

template <typename ComponentType> [[nodiscard]] component_serializer make_default_serializer();
[[nodiscard]] core::result<void> write_archive_json(const entity_archive &archive, core::json::writer &out);
[[nodiscard]] core::result<entity_archive> read_archive_json(
  const core::json::value &in,
  const component_registry &registry
);

// Single-component helpers for the editor inspector and the AI protocol (R40): run the component's serializer
// on the live value; the editor patches the JSON at a reflection path and writes it back.
[[nodiscard]] core::result<void> component_to_json(
  const world &source,
  entity target,
  component_id id,
  core::json::writer &out
);
[[nodiscard]] core::result<void> component_from_json(
  world &target,
  entity owner,
  component_id id,
  const core::json::value &in
);
```

---

## 5. Threading & ownership

1. A `world` has one owner thread, captured at construction, changeable with `set_owner_thread()` only while
   unlocked. Structural operations, `flush`, observer registration and `scheduler::run_phase` happen there;
   elsewhere they `CONJURE_ASSERT` and return `WRONG_THREAD`.
2. `iteration_scope` marks the world locked (atomic depth counter). While locked: any thread may read records
   (`is_alive`, `has`, `mask_of`, `get<T>() const`) and read/write component columns of chunks it was handed, or
   of other entities when the system guarantees disjoint access (hierarchy subtrees). Column version stamps are
   relaxed atomic stores, so concurrent stamping of a shared chunk is defined. Structural calls are refused.
3. `command_buffer` is single-threaded. `entity_allocator::reserve` is the module's only mutex and is hit once
   per 64 ids. Observers run synchronously on the owner thread inside structural operations and may only
   enqueue into the `deferred` buffer (`mInObserver` assert otherwise).
4. `component_registry` methods take its mutex except `info(id)` and `count()`, which read the pre-reserved,
   append-only vector guarded by the atomic count. Registration is expected on the owner thread but is safe
   anywhere. A registered `component_info &` stays valid for the registry's lifetime.
5. Ownership: `component_registry` outlives every `world` (worlds register/unregister themselves); `world`
   owns records, archetypes, chunks (via `chunk_pool`), observers; `archetype` owns chunks; `command_buffer`
   owns pending payloads and reserved ids; `entity_archive` owns component bytes; `scheduler` owns
   `system_record`s (desc, buffer, grain buffer pool, timings) and holds non-owning `world &` and
   `job_system *`; `query`, `view`, `entity_handle`, `component_ref` are non-owning values that re-validate on
   use. No raw owning pointers anywhere; `std::shared_ptr` appears only for archives referenced by buffers.

---

## 6. Failure handling

| Situation | Behaviour |
|---|---|
| Dead/null entity to `is_alive/has/get/mask_of` | returns false / nullptr / empty, never asserts |
| Dead entity to `destroy/add/remove/set/add_raw/remove_raw` | `ENTITY_NOT_ALIVE` |
| Add present / remove missing component | `COMPONENT_ALREADY_PRESENT` / `COMPONENT_NOT_PRESENT` |
| Invalid or inactive component id | `UNKNOWN_COMPONENT` |
| Structural call while locked | `CONJURE_ASSERT` + `STRUCTURE_LOCKED` |
| Structural call off the owner thread | `CONJURE_ASSERT` + `WRONG_THREAD` |
| Observer calls a structural method directly | `CONJURE_ASSERT` + `STRUCTURE_LOCKED` |
| Duplicate name, different layout or flags | `NAME_CONFLICT` (same layout returns the existing id) |
| 1025th component / size > 4096 / bad alignment | `COMPONENT_LIMIT_REACHED` / `COMPONENT_TOO_LARGE` / `BAD_ALIGNMENT` |
| Unreflected field kind without custom hooks | `UNSUPPORTED_FIELD` at registration |
| `redefine_dynamic` with live instances | `COMPONENT_IN_USE` |
| Archetype capacity 0 | `ARCHETYPE_TOO_LARGE` from the structural call; nothing changes |
| Pool exhaustion | `OUT_OF_MEMORY` from the structural call; nothing changes |
| Command on an entity dead at flush | skipped, `DEBUG` log, counted in `flush_report::skipped` |
| System cycle via `after`/`before` | `SYSTEM_CYCLE` from `add_system`; system not added |
| System name its phase already holds | `NAME_CONFLICT` from `add_system`; system not added (D13) |
| Unknown system name in `after`/`before` | ignored, one `DEBUG` log (module may be compiled out) |
| Unknown component name in a module system's read or write list | one `WARN` per name at attach; the system is added without that access and gains it when a component of the name is registered (D15) |
| Undeclared write inside a system (Debug) | `CONJURE_ASSERT` naming the system and component |
| Archive local index out of range | `ARCHIVE_CORRUPT` from `validate`/`read_archive_json`; no entities created |
| Unknown component name on load | preserved in `unresolved_components`, `INFO` log |
| Malformed component JSON | `SERIALIZATION_FAILED` with `<entity index>/<component>/<field>` path; load fails whole |
| `command_buffer` destroyed non-empty | `WARN`, payloads destructed, reserved ids returned |

All asserts compile out in Distribution; every asserted path also returns an error so behaviour stays
defined. `world::validate()` checks: records <-> rows agree, chunk counts sum to archetype counts, only the
last chunk is partial, free-list indices are `FREE`, every archetype mask maps back to itself, column offsets
are aligned and inside the block.

---

## 7. Headless / null behaviour

The ECS has no backend and no device dependency. The only optional dependency is `core::jobs::job_system *`:
`nullptr` makes `parallel_for_chunks` loop inline and `run_phase` execute stage systems sequentially on the
calling thread, with identical stage semantics and flush order. `tests/ecs/ecs_test_fixture.h` provides
`for_each_job_mode(callable)` which runs a test body twice: `jobs == nullptr` and a 4-worker job system, as
doctest `SUBCASE`s named `inline` and `parallel`. Nothing else differs between headless and engine use.

---

## 8. Integration points

**Needs from `core` (wave 1)** - every item below is a cross-area assumption to confirm against the final
core design: `core::result<T>`/`core::result<void>`/`core::error` with per-module error construction
(`make_error`); `CONJURE_ASSERT`/`CONJURE_VERIFY` plus `core::set_assert_handler()`, over which the test support library builds
`conjure::testing::expect_assert` (R16); `core::memory::pool_allocator(blockSize, alignment, blocksPerSlab)` with
`allocate()` -> nullptr on exhaustion and `deallocate(void *)`; `core::memory::arena` with
`allocate(size, alignment)`, `reset()`, stable addresses until reset; `core::jobs::job_system` with
`schedule(callable) -> job_handle`, `wait(job_handle)`, `parallel_for(count, grain, callable(begin, end))`,
`worker_count()`; `core::reflect` with `type_id` (stable across shared libraries), `type_info{name, size,
alignment, fields}`, `field_info{name, type, offset, kind}`, `type_info_of<T>()`, `type_id_of<T>()`, and
runtime `type_info` construction from a field list; `core::string_id` (constexpr hash), `small_vector`,
`event`, `uuid`; `core::json::writer` (object/array/key/value and `write_raw_json(std::string_view)`) and
`core::json::value` read API; `core::log` with the declare/define category macro pair.

**Offers to `scene` (wave 3):** registration for `parent`, `children`, `local_transform`, `world_transform`,
`name`, `tag`, `layer`; `observe(parent, REMOVED)` + `observe_destroyed` with the deferred buffer so unlinking
and cascaded destroy are loops over flushes, not recursion; `changed<local_transform>` plus scene's own `transform_dirty` tag for per-entity precision; O(1)
`get<T>` for the `scene.propagate_transforms` `TRANSFORM`-phase system, which is the single owner of transform
propagation (R3): it walks the hierarchy one depth level at a time (`parallel_for_chunks` over each level's
entities, so a thousand children of one root still parallelise) and writes `world_transform` and
`render_world_transform` in the same pass (`06_engine_loop.md` 3.2); `entity_archive` + JSON for prefab assets and scene
files with automatic parent/child rewiring.

**Offers to `scripting` / `bindings` (waves 3-4):** `register_dynamic`/`redefine_dynamic`/`has_instances`
for C# struct components and hot reload (flow: serialise -> `remove_from_all` -> `redefine_dynamic` -> re-add
-> deserialise; same name keeps the same id); `add_raw/set_raw/get_raw/remove_raw/has_raw`; `query::chunk(i)`
+ `chunk_view::column_data/count` + `world::lock_iteration()` so a `Span<T>` handed to C# cannot be
invalidated while held; `version()` to invalidate cached spans; a `command_buffer` per script batch;
`entity::to_u64/from_u64` for the managed `Entity` struct; `type_id_of<entity>()` for entity fields.

**Offers to `physics`, `renderer`, `audio`:** component registration (physics registers its own types; renderer
and audio components are registered by the engine's subsystem adapters because those modules do not depend on
`ecs`, R7); systems in `PHYSICS_STEP`/`FIXED_END`/`SYNC`/`EXTRACT` (R1); `observe(ADDED/REMOVED)` to create/destroy Jolt bodies and audio sources (deferred
buffer for follow-up structural work); `view::for_each_chunk` for render extraction; const views are
parallel-safe with each other by the stage rules.

**Offers to `engine` (wave 4):** owns the registry, world, scheduler and job system; calls `run_phase` in loop
order (`FIXED_UPDATE`/`PHYSICS_STEP`/`FIXED_END` zero or more times per frame, `06_engine_loop.md` 3.1); always
passes its `job_system` (worker count 0 means inline inside `core::jobs`; a `nullptr` job system is for unit
tests only, R30);
`world_stats` + `last_timings` for the profiler.

**Offers to `editor` (wave 4):** `on_registered` + `component_info::reflection` for the inspector;
`entity_archive` as the payload of copy/paste/undo commands (`keepExternalReferences = true`); `mask_of`,
`archetypes()`, `validate()` as a debug action.

**Offers to `modules` (wave 2):** a module entry registers `component_type_desc` / `system_extension_desc`
records through `modules::module_registrar` (R19); the engine's `ecs_module_consumer` applies them on attach and,
on detach, refuses while `has_instances` is true under `REFUSE_IF_IN_USE` or calls `remove_from_all` under
`REMOVE_INSTANCES`. The per-type `component_type_cache<T>` static is an explicit exception to area G's
"no template statics" rule: it caches a name lookup keyed by registry serial and carries no identity (R17).

CMake: `conjure_add_module(ecs SOURCES ... PUBLIC_HEADERS ... PUBLIC_DEPS conjure::core)` and
`conjure_add_test_module(ecs SOURCES ...)`; benchmarks in `tests/benchmarks/bench_ecs.cpp` outside ctest.

---

## 9. Implementation plan

Each step lands with its tests green and `validate()` passing; steps 1-4 can proceed in parallel once the core
headers exist, 5 depends on 1-4, 6-12 follow in order (10 and 11 may run in parallel with 8-9).

1. **Skeleton.** `engine/src/ecs/CMakeLists.txt`, `ecs.h`, `log_category.{h,cpp}`, `ecs_error.h`,
   `entity.h`, `entity_allocator.{h,cpp}`. Tests: `test_entity.cpp`, `test_entity_allocator.cpp`.
2. **Components.** `component.h` (`component_id`, flags, ops, info, `component_mask`, `CONJURE_COMPONENT`,
   `component_type_cache`), `component_mask.cpp`. Test: `test_component_mask.cpp`.
3. **Registry.** `component_registry.{h,cpp}` with C++ and dynamic registration, `unresolved_components`
   built-in, world list, `has_instances` (returns false until step 5 wires worlds). Test:
   `test_component_registry.cpp`.
4. **Layout and storage.** `chunk_layout::compute`, `chunk_pool`, `chunk`, `archetype` (push row, remove row,
   edges, spare retention). Tests: `test_chunk_layout.cpp`, `test_archetype.cpp`.
5. **World core.** `world.{h,cpp}`, `world_structural.cpp`: records, create/destroy, add/remove/set/get
   typed and raw, `move_entity`, observers + deferred buffer (buffer type stubbed to record-only), lock,
   version, owner thread, `stats`, `validate`, `clear`, `remove_from_all`. Tests: `test_world_entities.cpp`,
   `test_world_components.cpp`, `test_observer.cpp`.
6. **Queries and views.** `query.{h,cpp}`, `chunk_view.{h,cpp}`, `view.h` serial paths, change detection.
   Tests: `test_query.cpp`, `test_view.cpp`, `test_change_detection.cpp`.
7. **Command buffer and flush.** `command_buffer.{h,cpp}`, plan/apply flush, `flush_report`, reserved-id
   blocks, `append`. Test: `test_command_buffer.cpp`.
8. **Parallel iteration.** `system_context::parallel_for_chunks` with grain buffers, inline fallback,
   `view::parallel_for_each_chunk`. Test: `test_parallel_iteration.cpp` (both job modes).
9. **Scheduler.** `system.h`, `scheduler.{h,cpp}`, `scheduler_stages.cpp` (topological sort, packing,
   exclusive/main-thread, timings), Debug access validation, `make_system`. Test: `test_scheduler.cpp`.
10. **OOP facade.** `entity_handle.h`, `component_ref<T>`. Test: `test_entity_handle.cpp`.
11. **Archives.** `entity_archive.{h,cpp}`: capture, validate, `instantiate`, `instantiate_batch`, command
    buffer `INSTANTIATE`. Test: `test_entity_archive.cpp`.
12. **Serialisation.** `serialization.{h,cpp}`: default reflection serializer, dynamic type walker,
    `unresolved_components` round trip, archive JSON. Tests: `test_serialization.cpp`,
    `test_dynamic_components.cpp`.
13. **Finish.** `README.md` (purpose, types, threading model, how to test), `bench_ecs.cpp`, style review
    against `docs/CODE_STYLE.cpp`, sanitizer run on Linux.

---

## 10. Tests (`tests/ecs/`, target `conjure_ecs_tests`, all headless)

Every test body ends with `REQUIRE(world.validate())` via the fixture; files that iterate use
`for_each_job_mode`. doctest `TEST_CASE` names:

- `test_entity.cpp`: `null entity is all zero`, `u64 round trip`, `hash and equality`, `remap resolves local and leaves real`.
- `test_entity_allocator.cpp`: `reserve returns unique ids`, `release bumps generation and skips zero`, `generation wrap retires index`, `return_unused keeps generation`, `concurrent reserve from 8 threads is unique`.
- `test_component_mask.cpp`: `set clear test`, `contains_all and contains_any`, `count and none`, `equal masks hash equal`, `bit 1023 works`, `for_each_set visits ascending`.
- `test_component_registry.cpp`: `register trivial non trivial and tag`, `same name same layout returns same id`, `same name different layout is NAME_CONFLICT`, `find by name`, `id_of cache follows registry serial`, `register_dynamic happy path`, `rejects bad alignment oversize and limit`, `redefine_dynamic refused while instances exist`, `redefine_dynamic keeps id`, `unsupported field fails registration`, `on_registered fires once`, `unresolved_components is id zero`.
- `test_chunk_layout.cpp`: `empty archetype has 2048 rows`, `offsets respect column alignment`, `block never exceeds CHUNK_SIZE`, `capacity is maximal`, `tags have no column`, `capacity zero is ARCHETYPE_TOO_LARGE`, `mixed alignments 1 to 64`.
- `test_archetype.cpp`: `push and swap remove keep rows packed`, `moved entity record updated`, `one spare chunk retained`, `edge cache resolves same archetype`, `non trivial lifecycle counts balance across moves`.
- `test_world_entities.cpp`: `create destroy alive`, `destroy twice is ENTITY_NOT_ALIVE`, `stale id operations fail`, `index reuse after destroy`, `100k churn keeps record vector at peak`, `entity_count and stats`, `clear fires observers and resets`, `wrong thread is refused`, `validate after seeded random churn`.
- `test_world_components.cpp`: `add get has remove`, `add twice is COMPONENT_ALREADY_PRESENT`, `remove missing is COMPONENT_NOT_PRESENT`, `set on existing replaces`, `get on dead returns nullptr`, `mask_of`, `remove_from_all empties instance_count`, `tags occupy no bytes`, `pointer stable until structural change`.
- `test_observer.cpp`: `ADDED fires after data exists`, `REMOVED fires before data is destroyed`, `observers fire for direct api move and flush`, `observer enqueues deferred destroy chain`, `observer calling world directly asserts`, `unobserve stops delivery`.
- `test_query.cpp`: `with without optional matching`, `incremental refresh sees new archetypes`, `entity_count across chunks`, `empty archetypes skipped`, `chunk index flat over matches`, `10k entities visited exactly once`.
- `test_view.cpp`: `for_each const non const and maybe`, `builder chaining`, `access masks derived from pack`, `structural change during for_each is STRUCTURE_LOCKED`.
- `test_change_detection.cpp`: `non const span stamps chunk`, `const span does not stamp`, `changed filter sees chunk once`, `moved row counts as changed`, `handle write stamps`, `system does not see own previous writes`, `version is monotonic across flushes`.
- `test_command_buffer.cpp`: `deferred create alive after flush not before`, `create add set on same id`, `multiple add remove cause one move`, `add then remove cancels`, `remove then add keeps new payload`, `destroy wins over later add`, `stale generation skipped and counted`, `payload destructors run on clear`, `append preserves order`, `reservation across 200 creates`, `unused reserved ids returned on destruction`.
- `test_parallel_iteration.cpp`: `sum of written column equals inline result`, `each chunk visited once`, `grain buffers apply in grain order`, `create_entity from jobs yields unique ids`, `grain size follows minEntitiesPerJob`.
- `test_scheduler.cpp`: `after before ordering`, `cycle is SYSTEM_CYCLE and not added`, `unknown name ignored`, `two readers share a stage`, `reader and writer split stages`, `exclusive system alone with unlocked world`, `main thread flag runs on caller thread`, `disabled system skipped`, `commands visible from next stage`, `serial and parallel modes give same result`, `since_version excludes own writes`, `undeclared write asserts`, `timings recorded`, `a system name may repeat across phases, after removal and when it is null`, `find_system returns the live system of a phase by name`, `regression - code sweep F259 a duplicate system name in one phase is NAME_CONFLICT`, `regression - code sweep F077 a system gains the access of a component registered after it`, `resolveAccess runs again only when the registry has grown`, `a registry growth that changes no access set rebuilds and logs nothing`, `an exclusive system may register a component and run another phase`.
- `test_entity_handle.cpp`: `handle mirrors world`, `dead handle safe for get has is_alive`, `component_ref re-resolves after move`, `component_ref write stamps`.
- `test_entity_archive.cpp`: `capture three linked entities`, `instantiate remaps internal references`, `external reference becomes null`, `keepExternalReferences preserves`, `instantiate_batch 1000 lands in expected chunk count`, `instantiate via command buffer resolves at flush`, `corrupt local index is ARCHIVE_CORRUPT`, `non trivial components copied not aliased`.
- `test_serialization.cpp`: `round trip trivial nested entity and math fields`, `TRANSIENT skipped`, `unknown name preserved and rewritten verbatim`, `malformed json fails with field path`, `dynamic component fields serialised`, `preserved unknown components are written in the same name order`, `an unknown component listed twice keeps its file order`, `regression - code sweep F606 component keys are written in name order whatever the registration order`.
- `test_dynamic_components.cpp`: `register add by id bytes visible via column_data`, `default value applied`, `reflection fields enumerable`, `hot reload sequence keeps id and data`, `entity field remap for dynamic type`.
- `tests/benchmarks/bench_ecs.cpp` (not ctest): `iterate 10k transform velocity inline vs 4 workers`, `10k creates via command buffer`, `1k archetype moves`, `instantiate_batch 1k`, `1M random get`. Target: 10k transform integrations under 0.1 ms on 4 workers.

Death tests use `conjure::testing::expect_assert` over `core::set_assert_handler` (R16), since nothing throws and
doctest's `CHECK_THROWS` is unusable by design. `for_each_job_mode` takes its worker count from
`conjure::testing::environment().jobThreadCount`.

---

## 11. Out of scope / future

- Binary archive format for cooked scenes (JSON only in v1; additive via `component_serializer` fields).
- Entity-granular dirty bits (scene uses a tag for precision where chunk granularity costs too much).
- Cross-frame system pipelining or render-thread hand-off (area F decides; stages expose the metadata).
- First-fit packing into earlier stages (append-only greedy keeps ordering intuitive; revisit with profiling).
- Reusing component ids after a registry-wide zero-instance check (not needed below 1024 names).
- Shared/global components, entity relationships beyond scene's parent/children, reactive queries.
- Deterministic entity id values from parallel spawning (needs a per-grain reservation protocol).
