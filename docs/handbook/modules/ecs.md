# ECS module

🛠 **Engineering handbook** · 📘 API: [Entities, components and systems](../../api/ecs/index.md)

`conjure::ecs` is the entity component system: archetype storage in 16 KiB structure-of-arrays chunks, a
name-keyed component registry, queries and typed views, deferred structural changes through command buffers, a
stage-packing system scheduler, typed in-memory archives and JSON serialisation. This chapter is for people
changing the module or building on its internals (scene, scripting bindings, the engine loop): how the data is
laid out, the algorithms behind each public operation, the threading contract, the extension points and the
tests. Headers live under `engine/include/conjure/ecs/`, sources under `engine/src/ecs/`, tests under
`tests/ecs/`; the binding design is [`../../design/01_ecs.md`](../../design/01_ecs.md) and the finer behavioural
notes are in `engine/src/ecs/README.md`. The code wins where any of them disagree.

## Contents

- [1. Shape of the module](#1-shape-of-the-module)
- [2. Data layout](#2-data-layout)
- [3. Algorithms](#3-algorithms)
- [4. Observers](#4-observers)
- [5. Threading model](#5-threading-model)
- [6. Extension points](#6-extension-points)
- [7. Failure handling](#7-failure-handling)
- [8. Tests](#8-tests)
- [9. Open items](#9-open-items)
- [Related](#related)

## 1. Shape of the module

Static library `conjure_ecs` (alias `conjure::ecs`), namespace `conjure::ecs`, `PUBLIC_DEPS conjure::core` and
nothing else; registered with `conjure_register_static_library()` so it is whole-archived into `conjure_shared`
([Module system](../module-system/index.md) 1.1). Log category `LOG_ECS`, error category `"ecs"`
(`ecs_error`, `make_error`).

| Header | Main types | 📘 Wiki page |
|---|---|---|
| `entity.h` | `entity`, `NULL_ENTITY`, `ARCHIVE_LOCAL_GENERATION`, `entity_remap`, `std::hash` / `std::formatter` / `reflect_kind_override` specialisations | [Entities and components](../../api/ecs/entities-and-components.md) |
| `component.h` | `component_id`, `component_flags`, `component_ops`, `component_serializer`, `component_info`, `component_mask`, `component_name<T>` / `CONJURE_COMPONENT`, `component_type_cache<T>`, `make_component_ops<T>`, `derive_component_flags<T>`, `default_component_bytes<T>`, the limits | same |
| `component_registry.h` | `component_registry`, `dynamic_component_desc`, `dynamic_field_desc` | same |
| `world.h` | `world`, `world_desc`, `world_stats`, `iteration_scope` | same |
| `entity_handle.h` | `entity_handle`, `component_ref<T>` | same |
| `component_type_desc.h` | `component_type_desc`, `system_extension_desc`, `describe_component<T>()` | same |
| `archetype.h` | `archetype` (read-only), `chunk_layout`, `CHUNK_SIZE`, `CHUNK_ALIGNMENT`, `COLUMN_ALIGNMENT`, `SMALL_CAPACITY_WARNING` | [Archives and serialisation](../../api/ecs/archives-and-serialization.md) |
| `chunk_view.h`, `query.h`, `view.h` | `chunk_view`; `query_desc`, `query`; `view<Ts...>`, `maybe<T>`, `view_access` | [Queries, views and systems](../../api/ecs/queries-views-and-systems.md) |
| `system_phase.h`, `system.h`, `scheduler.h` | `system_phase`, `frame_info`; `system_flags`, `system_desc`, `system_scratch`, `system_context`, `make_system<Ts...>`; `scheduler`, `scheduler_desc`, `system_id`, `stage_info`, `system_timing` | same |
| `command_buffer.h`, `observer.h` | `command_buffer`, `flush_report`; `component_event`, `component_observer_fn`, `entity_observer_fn`, `observer_id` | [Command buffers and observers](../../api/ecs/command-buffers-and-observers.md) |
| `world_hash.h` | `world_hash(const world &)` | same |
| `entity_archive.h`, `serialization.h` | `entity_archive`, `archive_entry`, `archive_component`, `archive_options`; `unresolved_components`, `make_default_serializer<T>`, `write_/read_component_json`, `write_/read_archive_json`, `component_to_json` / `component_from_json` | [Archives and serialisation](../../api/ecs/archives-and-serialization.md) |
| `ecs.h`, `ecs_error.h`, `log_category.h` | umbrella include, error codes, `LOG_ECS` | not user-facing |

Private files: `entity_allocator.{h,cpp}` (id reservation, the module's only mutex), `chunk.{h,cpp}` (one block
plus out-of-line metadata and column versions), `chunk_pool.{h,cpp}` (RAII over `core::memory::pool_allocator`),
`observer_table.{h,cpp}`, `scheduler_internal.h`, and the translation units named per header, with `world`
split into `world.cpp` (guards, lookups, archetype map, observers, stats, validate, clear),
`world_structural.cpp` (rows, create / destroy / add / remove, flush) and `world_archive.cpp` (instantiation).

## 2. Data layout

### 2.1 Entities and records

`entity` is `{u32 index; u32 generation;}`. Generation 0 is never issued, so all-zero memory is `NULL_ENTITY`;
`ARCHIVE_LOCAL_GENERATION` (`0xFFFFFFFF`) marks an archive-local index and is never issued either. The world's
`std::vector<entity_record>` is indexed by `entity.index`: `archetype *owner`, `chunkIndex`, `row`,
`generation` (the current occupant's when `ALIVE`, the next to issue when `FREE`) and `entity_state`. The
`entity_allocator` keeps a LIFO free list of `{index, generation}` plus `mNextIndex`; `reserve(span)` is its only
locked method and never touches the record vector, `release` bumps the generation and retires an index whose next
generation would reach `ARCHIVE_LOCAL_GENERATION`, `return_unused` pushes reserved-but-unplaced slots back
unchanged. Records for new indices are appended on the owner thread (direct create or flush).

### 2.2 Component ids, masks and infos

Ids are dense `u16` (`MAX_COMPONENTS = 1024`), assigned per **name** in registration order and never reused for
the registry's lifetime, so a stale id cannot alias another type and every world on one registry agrees. Id 0 is
always `unresolved_components`. `component_mask` is `std::array<u64, 16>` (128 bytes): matching is 16 AND /
compare steps; `hash()` is FNV-1a 64 over the words. `component_info` (stable address for the registry's
lifetime: the vector is reserved to `MAX_COMPONENTS` and append-only, guarded by an atomic count) carries size,
alignment, flags, `component_ops`, `component_serializer`, the reflection `type_info *`,
`entityFieldOffsets`, `assetRefFieldOffsets` and `defaultValue` bytes. `register_component<T>` derives `TAG` for
empty types and `TRIVIAL` for trivially copyable ones (`derive_component_flags`), builds ops from the special
members only for non-trivial types (`make_component_ops`), and captures `default_component_bytes<T>` for trivial
types whose default constructor is not trivial (built in zeroed, type-aligned storage so padding is zero).
`id_of<T>()` reads `component_type_cache<T>`, one static atomic packing `registrySerial << 16 | id`; a serial
mismatch means "look the name up again". The static is per shared library but caches only a lookup result (R17).

### 2.3 Archetypes and chunk layout

Every live entity, the component-less ones included, sits in exactly one `archetype`, found through the
world's `unordered_map<component_mask, archetype *>` or the archetype's edge list (`{id, withAdded,
withRemoved}`, linear search, filled lazily). The archetype list is append-only, so `archetype *` is stable for
the world's lifetime and queries cache it.

A chunk is one 16 KiB, 64-byte aligned block from the `chunk_pool` (a `pool_allocator` with
`world_desc::chunksPerSlab` blocks per slab, capped by `maxChunkSlabs`) containing pure data: the entity column
at offset 0, then one column per non-tag component ordered by descending alignment, each at
`align_up(previousEnd, max(alignment, COLUMN_ALIGNMENT))`. Metadata (`mCount`, one relaxed-atomic version per
column) lives out of line in `chunk`, so layout maths never deals with a header. `chunk_layout::compute` starts
at `capacity = CHUNK_SIZE / rowBytes` and decrements until the last column fits; `capacity == 0` is
`ARCHETYPE_TOO_LARGE`, below `SMALL_CAPACITY_WARNING` logs once. Capacity is maximal: `capacity + 1` never
fits (tested). Rows are packed by swap-remove; only the last used chunk is partial; one drained trailing chunk is
kept as a spare and a new row fills a partial chunk ahead of the spare before the spare itself (`open_chunk`).
Default construction zeroes the cell first so padding bytes are deterministic.

| Archetype | Rows per chunk |
|---|---|
| empty | 2048 |
| `{transform 64 B}` | 227 |
| `{transform 64 B, sprite 32 B, velocity 12 B, health 8 B}` | 132 (10 000 entities = 76 chunks) |

## 3. Algorithms

### 3.1 Entity lifecycle and moves

`create_entity(mask)`: reserve one id, append a record if the index is new, `archetype::reserve_row` (the part
that can fail: `ARCHETYPE_TOO_LARGE`, `OUT_OF_MEMORY`), `push_row`, default-construct every column
(`defaultValue` or zero for trivial types, `ops.constructDefault` otherwise), stamp the chunk, write the record,
fire `ADDED` per set bit. `destroy_entity`: fire `REMOVED` per column then destroyed observers, destruct
non-trivial columns, `remove_row`, `release`, drain the deferred buffer. `remove_row(a, c, r)` relocates the
archetype's last row into the hole, updates that entity's record and stamps every column of chunk `c`.
`move_entity(e, dst)` reserves the destination row first (so `REMOVED` observers run only once the move can no
longer fail), relocates shared columns, default-constructs added ones, destructs removed ones, `remove_row`s the
source, stamps the destination chunk and fires `ADDED` for the added columns. `add_storage` / `finish_add` split
`add` so the typed template can construct the value in place between them.

### 3.2 Queries and change detection

`query::refresh()` scans `[mSeenArchetypeCount, archetype_count())` and keeps archetypes with
`mask.contains_all(with) && !mask.contains_any(without)`: O(new archetypes). Iteration skips empty archetypes and
chunks, holds an `iteration_scope` and, with a `changed` mask, visits a chunk only when one listed column's
version exceeds `sinceVersion` (`chunk_passes`). The world version starts at 1 and is bumped at the start of
every structural call, flush and system run. Non-const `chunk_view::get<T>`, `column_data(id, true)`,
`world::get<T>()`, `get_raw(.., true)`, `component_ref::write()`, `set` and `component_from_json` stamp the
column with the version current when the view was created (`stamp_version`); a row moving in or out stamps all
columns of the chunks involved. A system's `since_version()` is the version observed at the start of its previous
run, so it never sees its own previous writes. Granularity is the chunk: no false negatives, at most one chunk of
false positives per write. A `view` whose pack or builder call names an unregistered type asserts and sets
`with = ~component_mask{}` so it matches nothing; an unregistered `maybe<T>` is simply absent.

### 3.3 Command buffer flush

Records live in a `core::memory::arena` as an intrusive list `{opcode, payloadLive, target, id, payloadSize,
instantiateIndex, payload *, next}`; payloads are move-constructed in and destroyed on apply or `clear()`.
Each buffer owns up to `RESERVE_BLOCK` (64) reserved ids refilled from the allocator; unused ids survive `clear()`
and return on destruction. `world::flush` (owner thread, unlocked): `bump_version`, extend the record vector for
reserved indices, then:

1. **Plan.** One pass over the records. The flush-stamped slot table `mFlushSlots` (`{stamp, plan}` indexed by
   entity index, never cleared) maps each touched entity to a dense plan entry accumulating a destroy flag,
   `addMask`, `removeMask` and the payload list. `ADD` sets add and clears remove; `REMOVE` sets remove, clears
   add and drops pending payloads of that component; `SET` appends a payload; `DESTROY` wins. Commands on
   entities neither alive nor reserved are skipped (`DEBUG` log, `flush_report::skipped`).
2. **Apply.** `INSTANTIATE` records first (so later commands on their roots find live entities), then per entity
   `finalMask = (current | add) & ~remove`: a reserved entity is placed directly (no empty-archetype hop), a live
   one takes one `move_entity` when the mask changed, observers fire for the delta only, payloads apply in record
   order (`ADD` copies into the fresh column, `SET` on a non-trivial component is destruct + copy-construct).
   Destroys run last. A placement that fails counts all of that entity's commands as skipped and releases a
   reserved-only id with a bumped generation.
3. `clear()` resets the arena.

### 3.4 Scheduler

`scheduler_stages.cpp`: `resolve_edges` turns `after` / `before` names into predecessor lists (unknown names
ignored with one `DEBUG` log each), `topological_order` is Kahn's algorithm with registration order as the
tiebreak (a cycle is detected incrementally in `add_system`, which fails with `SYSTEM_CYCLE` and adds nothing),
then greedy append-only packing: a system joins the current stage when no predecessor is in it, it is not
`EXCLUSIVE`, the stage is not exclusive, and for every member `s`: `(writes & (s.reads | s.writes)).none() &&
(reads & s.writes).none()`. Stages are rebuilt lazily after `add_system`, `remove_system` and `set_enabled`;
`finalise()` builds every phase now. `system_desc::resolveAccess` (filled by `make_system`) is called at
`add_system` to merge pack-derived masks into `reads` / `writes`, so a `system_desc` needs no registry at
construction.

`scheduler.cpp`: `run_phase` walks the stages; an exclusive stage bumps the version and runs on the caller
with the world unlocked, then flushes; every other stage takes an `iteration_scope`, schedules its systems as
jobs (main-thread systems last, on the caller; everything inline when `jobs == nullptr` or
`parallelSystems == false`), waits, releases the scope and flushes each system's buffer in stage order.
`run_system` bumps the version, builds a `system_context` (world, frame, `sinceVersion`, `runVersion`, the
system's buffer, jobs, desc, `system_scratch`, chunk parallelism switches), runs, records `sinceVersion` and the
elapsed microseconds. Debug validation: `system_context::view()` checks `with | optional` against
`reads | writes` and `writes` against the declared writes; the query carries the context (`bind_context`) so
every `chunk_view` it hands out asserts on a non-const span of an undeclared component, naming the system.

`system_context::dispatch_chunks` (`system.cpp`) flattens the matched chunks into `system_scratch::chunks`,
groups them into grains of `max(1, minEntitiesPerJob / capacity)` chunks, takes one `command_buffer` per grain
from `system_scratch::grainBuffers` (grown on demand, arenas retained across frames), runs
`jobs->parallel_for` or an inline loop, and appends the grain buffers to the system buffer **in grain order**
after the join. Application order is therefore independent of the worker count; entity id values reserved inside
grains are not.

### 3.5 Archives, serialisation and the hash

`entity_archive::capture` copies every non-`TRANSIENT` component with `ops.copy` (or `memcpy`) and rewrites
each `entityFieldOffsets` entry: inside the set to `entity::archive_local(index)`, outside to `NULL_ENTITY`
unless `keepExternalReferences`. `world::instantiate_batch` validates first (`ARCHIVE_CORRUPT` before anything is
created), resolves each entry's archetype once, reserves `copies * entries` ids in one call, places all copies of
an entry together (`place_rows`), copies payloads, builds an `entity_remap` per copy and rewrites entity fields
(custom `remapEntities` hooks included), then fires `ADDED` last. A placement that fails part-way discards the
rows placed so far (`discard_placed`) and returns every id unused. With a command buffer the ids are reserved and
the roots returned at record time; the copy runs first in the flush.

`serialization.cpp` writes `{"entities":[{"components":{"<name>":{...}}}]}`. The default serialiser walks
`core::reflect::type_info`: scalars, strings, `string_id`, `uuid`, math types, enums and `std::array` through
`core::reflect`; nested structs, entity fields (local index / `null` / `{"external":"i:g"}`) and `ASSET_REF`
fields (uuid in the first 16 bytes of the handle, rest zeroed on read; the scene bind pass resolves them, R25)
here. Unknown names on load go into `unresolved_components` verbatim and are re-emitted; malformed JSON fails the
whole load with `SERIALIZATION_FAILED` and an `<entity index>/<component>/<field path>: detail` message. Dynamic
types serialise through their runtime description with the same walker.

`world_hash` visits archetypes in ascending order of mask hash and entities in chunk order; per entity it hashes
the id, then per component in id order the name hash combined with: the reflected fields for trivial types with a
description (padding excluded), the raw bytes for trivial types without one, the canonical JSON for non-trivial
types, nothing more for tags. An `ASSET_REF` field of a described type wider than a uuid contributes its leading
uuid alone, never its bound slot pointer, so the hash is the same in every process and whether or not references
are bound; an undescribed trivial type hashes every byte, pointers included. Entity ids and TRANSIENT components
(`transform_state::lastUpdateRun` among them) are hashed, so recorded hashes compare only at equal frame counts and
when no id came from a command buffer on a parallel grain or a concurrently running system. Allocation-free below
64 archetypes for trivial worlds.

## 4. Observers

`observer_table` stores component observers per `(id, event)` and destroyed observers, each slot generational
(`observer_id{index, generation}`, generation 0 never issued). Delivery is synchronous on the owner thread
inside the structural call: `ADDED` after the value is in place (also after a flush payload or an archive copy),
`REMOVED` before the data is destroyed and only once the move can no longer fail, destroyed observers before the
components go. `mInObserver` makes any direct structural call assert and return `STRUCTURE_LOCKED`. Observers
enqueue into `mDeferred`; `end_structural` of the outermost call (`mStructuralDepth == 0`) swaps it with
`mDeferredFlushing` and flushes in a loop until nothing new was enqueued (1024 passes assert: a cycle in caller
data). An observer may register or remove observers: one registered during a delivery is first invoked by the
next one. `clear()` fires observers; the destructor does not.

## 5. Threading model

| Rule | Detail |
|---|---|
| One owner thread per world | captured at construction, changed with `set_owner_thread()` while unlocked; structural calls, `flush`, `observe`, `run_phase`, `clear` belong to it (`WRONG_THREAD` + assert elsewhere) |
| `iteration_scope` | an atomic depth counter taken from any thread; while locked any thread may read records (`is_alive`, `has`, `mask_of`, `get` const) and read or write columns of chunks it was handed; structural calls are refused (`STRUCTURE_LOCKED`) |
| Version stamps | relaxed atomic stores, so concurrent stamping of one chunk is defined |
| The only mutex | `entity_allocator::reserve`, hit once per 64 ids by command buffers and once per archive instantiation |
| `command_buffer` | single-threaded; `parallel_for_chunks` gives every grain its own and merges in grain order |
| `component_registry` | every method takes its mutex except `info`, `is_registered`, `count`, `types` (atomic count over the pre-reserved vector); `id_of<T>` is a lock-free cache hit after the first lookup; `on_registered` fires on the registering thread after the lock is released |
| Systems | `run_system` may run on a job thread; it touches the world only through the locked-iteration API and its own buffer |
| Unlocked readers | `world_hash`, `entity_archive::capture`, `validate()`, `stats()` read without locking: call them while no structural change is in flight |

Ownership: the registry outlives every world (worlds attach and detach themselves); the world owns records,
archetypes, chunks (through the pool) and observers; `command_buffer` owns payloads and reserved ids;
`entity_archive` owns component bytes; the scheduler owns `system_record`s (desc, buffer, scratch, timings) and
holds non-owning `world &` and `job_system *`; `query`, `view`, `entity_handle` and `component_ref` are
non-owning values that re-validate on use. `std::shared_ptr` appears only for archives referenced by buffers.

## 6. Extension points

| Point | Mechanism |
|---|---|
| C++ component types | `CONJURE_COMPONENT` + `CONJURE_REFLECT_STRUCT`, `register_component<T>(flags)`; non-trivial types get ops from their special members |
| Custom lifecycle or serialisation | `component_ops` / `component_serializer` function pointers in `component_info` (today filled by the registry; a custom-hook registration path is additive) |
| Module extension records | `component_type_desc` (`"ecs.component"`, `describe_component<T>()`, applied by `register_described`, reflection deep-copied so the module may unload) and `system_extension_desc` (`"ecs.system"`, name lists resolved by the engine's `ecs_module_consumer` into `add_system`); detach uses `remove_system` / `remove_from_all` ([Module system](../module-system/index.md)) |
| Script-defined types | `register_dynamic` (always `DYNAMIC \| TRIVIAL`, alignment 1..16, fields of primitive, math or `entity` kinds, the registry owns the `type_info`); `redefine_dynamic` keeps the id, refuses while `has_instances` (`COMPONENT_IN_USE`) or on a name change, and re-lays out the now empty archetypes containing the type (an archetype whose rows no longer fit gets capacity 0). Hot reload: serialise (`component_to_json`) -> `remove_from_all` -> `redefine_dynamic` -> `add_raw` -> `component_from_json` |
| Bindings | `add_raw` / `set_raw` / `get_raw` / `remove_raw` / `has_raw`, `query::chunk(i)` + `chunk_view::column_data` under `lock_iteration()`, `version()` to invalidate cached spans, `entity::to_u64` / `from_u64` |
| Frame integration | `system_phase` (list owned by the engine loop design, header by this module, R1), `frame_info` (R2), `run_phase_filtered` with `RUNS_WHILE_PAUSED`, `scheduler::finalise()` at startup and after module loads |
| Hierarchy and cascades | not in the ECS (D9): scene builds them from `observe(parent, REMOVED)`, `observe_destroyed`, the deferred buffer and `changed<T>` |

## 7. Failure handling

Contract violations assert in Debug and Development and still return their code; every path stays defined in
Distribution. `world::validate()` checks records against rows, chunk counts, packing (every chunk before the last
non-empty one is full, at most one spare), column alignment and bounds, free-list consistency, the archetype map
and the pool's live block count, naming the first violation.

| Situation | Behaviour |
|---|---|
| dead or null entity to `is_alive` / `has` / `get` / `mask_of` | false / nullptr / empty, no assert |
| dead entity to `destroy` / `add` / `remove` / `set` / `add_raw` / `remove_raw` | `ENTITY_NOT_ALIVE` |
| add present / remove missing / unknown id | `COMPONENT_ALREADY_PRESENT` / `COMPONENT_NOT_PRESENT` / `UNKNOWN_COMPONENT` |
| structural call while locked, off the owner thread, or from an observer | assert + `STRUCTURE_LOCKED` / `WRONG_THREAD` |
| registration: duplicate name with another layout, 1025th type, > 4096 bytes, bad alignment, unreflected field kind | `NAME_CONFLICT`, `COMPONENT_LIMIT_REACHED`, `COMPONENT_TOO_LARGE`, `BAD_ALIGNMENT`, `UNSUPPORTED_FIELD` |
| `redefine_dynamic` with live instances | `COMPONENT_IN_USE` |
| archetype capacity 0 / pool cap reached | `ARCHETYPE_TOO_LARGE` / `OUT_OF_MEMORY` from the structural call; nothing changes |
| command on an entity dead at flush | skipped, `DEBUG` log, `flush_report::skipped` |
| system cycle / unknown name in `after` or `before` | `SYSTEM_CYCLE`, not added / ignored with a `DEBUG` log |
| undeclared write inside a system (Debug) | assert naming system and component |
| archive local index out of range / foreign registry | `ARCHIVE_CORRUPT`, nothing created |
| unknown component name on load / malformed JSON | preserved in `unresolved_components` with an `INFO` log / `SERIALIZATION_FAILED` with the field path, whole load fails |
| `command_buffer` destroyed non-empty | `WARN`, payloads destroyed, ids returned |

## 8. Tests

`tests/ecs/` builds `conjure_ecs_tests`: one `test_<header>.cpp` per public header plus the topic files
`test_entity_allocator`, `test_component_mask`, `test_chunk_layout`, `test_world_entities`,
`test_world_components`, `test_change_detection`, `test_parallel_iteration` and `test_dynamic_components`.
`ecs_test_fixture.h` provides the test component types (`position`, `velocity`, `health`, tags, the non-trivial
`name_component` and `tracked`, the `alignas(8)` `link`, `nested_component`, `wide_component`,
`huge_component`), `ecs_fixture` (registry + world, `validate()` in the destructor), `make_zero_capacity_archetype`
(an archetype laid out with capacity 0 through `redefine_dynamic`, for the failure paths of placement, flush and
instantiation) and `for_each_job_mode` (SUBCASEs `inline` with `jobs == nullptr` and `parallel` on at least four
workers). Pool exhaustion is tested through `world_desc::maxChunkSlabs`; death tests use
`conjure::testing::expect_assert`; the parallel cases assert order independence through `world_hash`. Review
fixes carry `regression - review <n> ...` cases. Benchmarks live in `tests/benchmarks/bench_ecs.cpp`
(`ecs.iterate_100k`, `ecs.spawn_destroy_10k`, `ecs.query_changed_10k`) under the excluded label `benchmark`.

```
python tools/build.py --build-dir build/mod_ecs --target conjure_ecs_tests --test -R ecs
build/mod_ecs/bin/conjure_ecs_tests
```

## 9. Open items

From `01_ecs.md` 11: a binary archive format for cooked scenes (additive through `component_serializer`);
entity-granular dirty bits (scene uses a tag where chunk granularity costs too much); cross-frame system
pipelining or render-thread hand-off (the engine loop decides, stages expose the metadata); first-fit packing
into earlier stages (append-only greedy keeps ordering intuitive); reusing component ids after a registry-wide
zero-instance check; shared or global components, relationships beyond scene's parent and children, reactive
queries; deterministic entity id values from parallel spawning (needs a per-grain reservation protocol).

## Related

- design records: [`../../design/01_ecs.md`](../../design/01_ecs.md) (binding),
  [`../../design/09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md) R1, R2, R17, R19, R25,
  R32, R40, [`../../design/08_testing.md`](../../design/08_testing.md) 3.4 (`world_hash` ordering)
- module README: `engine/src/ecs/README.md`
- handbook: [Data-oriented design](../data-oriented-design.md), [Module system](../module-system/index.md),
  [Core module](core.md), [Testing](../testing.md)
- 📘 API: [Entities, components and systems](../../api/ecs/index.md) and its four topic pages
