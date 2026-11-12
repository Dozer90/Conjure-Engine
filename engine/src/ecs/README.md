# ecs

The entity component system: archetype storage in 16 KiB SoA chunks, a name-keyed component registry,
queries and typed views, deferred structural changes through command buffers, a stage-packing system
scheduler, typed in-memory archives and JSON serialisation. Static library `conjure::ecs`, namespace
`conjure::ecs`, depends on `conjure::core` only. Binding design: `docs/design/01_ecs.md`; the frame
phases and the module extension records follow `docs/design/09_cross_area_resolutions.md` R1/R2.

## Public headers (`conjure/ecs/`)

| Header | Main types |
|---|---|
| `ecs.h` | umbrella include |
| `log_category.h` | `LOG_ECS` |
| `ecs_error.h` | `ecs_error`, `ecs_error_name`, `make_error` |
| `entity.h` | `entity`, `NULL_ENTITY`, `ARCHIVE_LOCAL_GENERATION`, `entity_remap`, `std::hash<entity>`, `std::formatter<entity>`, the `reflect_kind_override<entity>` (kind `ENTITY`) |
| `component.h` | `component_id`, `component_flags`, `component_ops`, `component_serializer`, `component_info`, `component_mask`, `component_name<T>` / `CONJURE_COMPONENT`, `component_type_cache<T>`, `make_component_ops<T>`, `derive_component_flags<T>`, `default_component_bytes<T>`, `MAX_COMPONENTS` (1024), `MAX_COMPONENT_SIZE` (4096), `MAX_COMPONENT_ALIGNMENT` (64) |
| `component_registry.h` | `component_registry`, `dynamic_component_desc`, `dynamic_field_desc` |
| `archetype.h` | `archetype` (read-only, `spare_chunk_count()` for the parked chunk metadata), `chunk_layout`, `CHUNK_SIZE`, `CHUNK_ALIGNMENT`, `COLUMN_ALIGNMENT` |
| `chunk_view.h` | `chunk_view` |
| `query.h` | `query_desc`, `query`, `QUERY_INLINE_MATCHES` |
| `view.h` | `view<ComponentTypes...>`, `maybe<T>`, `view_access` |
| `command_buffer.h` | `command_buffer` (`clear()` keeps its room, `shrink_to_fit()` returns it), `flush_report` |
| `observer.h` | `component_event`, `component_observer_fn`, `entity_observer_fn`, `observer_id` |
| `world.h` | `world`, `world_desc`, `world_stats`, `iteration_scope` |
| `entity_handle.h` | `entity_handle`, `component_ref<T>` |
| `system_phase.h` | `system_phase` (the R1 list), `frame_info` |
| `system.h` | `system_flags`, `system_desc`, `system_scratch`, `system_context`, `make_system<Ts...>` |
| `component_type_desc.h` | `component_type_desc`, `system_extension_desc`, `describe_component<T>()` |
| `world_hash.h` | `world_hash(const world &)` |
| `scheduler.h` | `scheduler`, `scheduler_desc`, `system_id`, `stage_info`, `system_timing` |
| `entity_archive.h` | `entity_archive`, `archive_entry`, `archive_component`, `archive_options` |
| `serialization.h` | `unresolved_components`, `make_default_serializer<T>`, `write_component_json` / `read_component_json`, `write_archive_json` / `read_archive_json`, `component_to_json` / `component_from_json` |

Private files: `entity_allocator.{h,cpp}` (id reservation, the module's only mutex), `chunk.{h,cpp}`
(one block plus out-of-line metadata and column versions), `chunk_pool.{h,cpp}` (RAII over
`core::memory::pool_allocator`, 16 KiB blocks, 64-byte aligned), `observer_table.{h,cpp}`,
`archetype.cpp`, `component_mask.cpp`, `component_registry.cpp`, `query.cpp`, `chunk_view.cpp`,
`world.cpp` (guards, lookups, archetype map, observers, stats, validate, clear),
`world_structural.cpp` (rows, create/destroy/add/remove, flush), `world_archive.cpp` (instantiation),
`command_buffer.cpp`, `system.cpp` (`system_context`, `parallel_for_chunks`), `scheduler.cpp` (execution),
`scheduler_stages.cpp` (edges, topological sort, packing), `scheduler_internal.h`, `entity_archive.cpp`,
`serialization.cpp`, `world_hash.cpp`, `system_phase.cpp`, `ecs_error.cpp`, `log_category.cpp`.

## Using it

```cpp
struct position { f32 x = 0.0f; f32 y = 0.0f; f32 z = 0.0f; };
CONJURE_REFLECT_STRUCT(position, "game.position", CONJURE_FIELD(x), CONJURE_FIELD(y), CONJURE_FIELD(z));
CONJURE_COMPONENT(position, "game.position");

component_registry registry = {};
CONJURE_TRY(registry.register_component<position>());
world world(registry);
CONJURE_TRY_ASSIGN(const entity player, world.create_entity());
CONJURE_TRY(world.add<position>(player, position{0.0f, 1.0f, 0.0f}));

world.view<const velocity, position>().for_each([](entity, const velocity &v, position &p) { p.x += v.x; });

scheduler scheduler(world, &jobs);
CONJURE_TRY(scheduler.add_system(make_system<const velocity, position>(core::string_id("game.integrate"),
    system_phase::UPDATE, [](system_context &context) { ... context.commands().destroy_entity(id); ... })));
CONJURE_TRY(scheduler.finalise());
scheduler.run_phase(system_phase::UPDATE, frame);
```

A component type is a plain struct named with `CONJURE_COMPONENT` and described with
`CONJURE_REFLECT_STRUCT` (required unless it is a tag, `TRANSIENT`, or registered as an empty type).
Registration is explicit and per registry; ids are assigned per name in registration order and never
reused, so a stale id can never alias another type and every world on the same registry agrees.

## Behaviour worth knowing

- **Entities.** `entity` is `{u32 index; u32 generation;}`; generation 0 is never issued so all-zero
  memory is `NULL_ENTITY`. Released indices go on a LIFO free list with a bumped generation; an index
  whose next generation would reach `ARCHIVE_LOCAL_GENERATION` is retired for good. `is_alive`,
  `has`, `get` const and `mask_of` on a dead or stale entity return false / nullptr / empty and never
  assert; `destroy`, `add`, `remove`, `set`, `add_raw`, `remove_raw` return `ENTITY_NOT_ALIVE`.
- **Storage.** Every live entity sits in an archetype (the empty one included). A chunk is pure data:
  the entity column at offset 0, then the non-tag columns ordered by descending alignment, each at
  `align_up(previousEnd, max(alignment, 16))`; capacity is the largest row count that fits
  (`chunk_layout::compute`, `ARCHETYPE_TOO_LARGE` when not even one row fits, one `WARN` below 8
  rows). Rows are packed by swap-remove; one drained trailing chunk is kept as a spare, and a new row
  fills the partial chunk ahead of the spare before the spare itself, so only the last used chunk is
  ever partial. Every further drained chunk returns its block to the pool and parks its metadata (the
  `chunk` object with its column versions) on the archetype (`spare_chunk_count()`), where `reserve_row`
  re-attaches it to the next block before allocating a new object, so create/destroy churn allocates no
  chunk metadata once it has reached its peak. Default construction zeroes the cell first so padding
  bytes are deterministic. Archetypes are append-only, so `archetype *` stays valid for the world's
  lifetime and queries cache them (inline up to `QUERY_INLINE_MATCHES` = 16 archetypes, so a query or
  view built per system run allocates nothing in the common case). `world_desc::maxChunkSlabs` caps the
  chunk pool (0 = unlimited); a structural call that needs a chunk beyond the cap fails with
  `OUT_OF_MEMORY` and changes nothing.
- **Structural rule.** Create, destroy, add, remove, `remove_from_all`, `flush`, `clear`, observer
  registration and `scheduler::run_phase` run on the owner thread while the world is unlocked.
  Elsewhere they `CONJURE_ASSERT` and return `WRONG_THREAD`; under an `iteration_scope` (every
  `for_each`, `for_each_chunk`, system stage), inside an observer or inside another structural call
  (the only foreign code there is a component's constructor, destructor or relocation, so one that
  calls back into its world is refused) they assert and return `STRUCTURE_LOCKED`. `set<T>`, `get<T>`
  and the untyped getters are not structural.
- **Observers** fire synchronously inside the structural call: `ADDED` after the value is in place
  (also after a flush payload or an archive copy), `REMOVED` before the data is destroyed and only
  once the move can no longer fail (the destination row is reserved first), destroyed observers
  before the components go. They may only enqueue into the `command_buffer &deferred` they receive;
  the world drains that buffer in a loop once the outermost structural call has finished (1024 passes
  assert: a cycle in caller data), so destroy chains are loops over flushes. An observer may register
  or remove observers: one registered during a delivery is first invoked by the next one. The observer
  table keeps a per-event interest mask, so an event for a component nobody observes costs one bit test
  instead of a scan over every observer slot. `add<T>` / `add_raw` return nullptr when an `ADDED`
  observer's deferred work removed the component again or destroyed the entity.
- **Change detection** is chunk-granular. The world version starts at 1 and is bumped at the start of
  every structural call, every flush and every system run. Non-const spans from `chunk_view`,
  `world::get<T>()`, `get_raw(..., true)`, `component_ref::write()`, `set` and `component_from_json`
  stamp the column; a row moving into or out of a chunk stamps all columns of the chunks involved.
  `changed<T>(since)` visits a chunk when its column version is greater than `since`; a system's
  `since_version()` is the version observed at the start of its previous run, and the chunk views of
  its `context.view<...>()` and `parallel_for_chunks` stamp that run's version (`run_version()`, not the
  current world version, which a structural call or a sibling system may bump mid-run), so it never
  sees its own previous view writes. Its writes through `context.world()` (`get<T>()`, `set`,
  `component_ref::write()`) stamp the current version and may pass its next `since_version()`.
  `world::get<const T>()` on a non-const world does not stamp.
- **Command buffers** record into an arena (payloads moved in, destroyed on apply or `clear()`) and
  own a block of 64 reserved ids, so `create_entity()` returns a real id usable in later commands and
  from job threads. `flush` plans one archetype move per touched entity (add then remove cancels,
  remove then add keeps the new payload, destroy wins), applies archive instantiations first so later
  commands on their roots find live entities, runs destroys last, and skips commands on dead or stale
  entities (`DEBUG` log, `flush_report::skipped`). An entity whose placement or move fails at flush
  (`ARCHETYPE_TOO_LARGE`, `OUT_OF_MEMORY`) has all of its commands counted as skipped and, when it was
  only reserved, its id released with a bumped generation. The plan uses a flush-stamped slot table
  kept across flushes and the world's member plan vector `mFlushPlan` (`world::flush_entry`, payload
  records inline up to `FLUSH_INLINE_PAYLOADS` = 4), used in place and cleared on exit, never moved
  into a local (a vector object costs an allocation under MSVC's debug iterators); `place_rows` uses
  its member undo list `mPlacedScratch` the same way. Both rely on the paths never nesting:
  `check_structural` refuses structural calls from observers, under the iteration lock and from inside
  an open structural call (a component constructor or destructor calling back into its world), and the
  deferred buffer drains only after the outermost structural call has returned. So a warmed-up flush
  allocates nothing of its own (`tests/ecs/test_world_structural.cpp` gates the 7.1 spawn/destroy churn
  at <= 2). `clear()` keeps room for the records as well: a recording that outgrew the arena's 16 KiB
  block leaves the buffer with a single block spanning the blocks the recording used (its bytes rounded
  up to whole 16 KiB blocks, vector-style: at most one block more than it needed; one allocation on the
  next recording, none afterwards). Typed `add<T>`/`set<T>`/`remove<T>` of an unregistered type assert
  and record nothing. Unused reserved ids survive `clear()` and go back to the allocator when the buffer
  is destroyed; a buffer destroyed with pending commands logs `WARN`. The ids of an instantiation that
  never applied (cleared, destroyed, or skipped by a failed flush) were already handed out as roots, so
  `clear()` releases them through `world::release_reserved` with a bumped generation (growing the
  record table first when the world never saw the index) instead of returning them unused; off the owner
  thread `clear()` asserts and leaves them reserved rather than racing on the record table. An untyped
  `add_raw` with no payload records no value: a component the entity already holds at flush keeps its
  value and fires no observer. A record table that cannot grow skips (and counts) every command.
- **Retention.** The scratch above is sized by the peak it has seen and kept for the owner's lifetime:
  the world's flush plan (about 330 bytes per entity the largest flush touched) and placement list, every
  archetype's parked chunk metadata (`spare_chunk_count()`, bounded by its peak chunk count) and every
  command buffer's arena room. `command_buffer::shrink_to_fit()` returns a buffer's room (arena blocks,
  instantiation list) when it is empty and keeps its reserved ids; `scheduler::remove_system` calls it
  on the removed system's buffer and drops its grain buffers and chunk scratch, and `world::clear()`
  releases the plan, the placement list, every archetype's spares and the deferred buffers' room next to
  the chunk pool shrink. Nothing else shrinks: a world that flushed a large command buffer once keeps
  that plan until it is cleared or destroyed.
- **Scheduler.** Systems are packed per phase into stages: Kahn's topological sort over the
  `after`/`before` edges (registration order as the tiebreak; a cycle is `SYSTEM_CYCLE` at `add_system`
  and the system is not added; unknown names are ignored with one `DEBUG` log each, written by
  `add_system` for the system being added and never by later additions or rebuilds), then greedy append-only
  packing where a system joins the current stage when no predecessor is in it, it is not `EXCLUSIVE`,
  and its `writes` touch no member's reads or writes and its `reads` touch no member's writes. A stage
  runs its systems as jobs over a locked world (main-thread systems last on the caller, everything
  inline without a job system or with `parallelSystems = false`), then flushes each system's buffer in
  stage order. `EXCLUSIVE` systems run alone with an unlocked world. `finalise()` builds every phase
  now; `run_phase_filtered` runs only systems whose flags contain the required bits. The per-stage job
  and main-thread lists live in the phase state (kept at capacity) and the stage jobs complete on a
  `core::jobs::job_fence` held on `run_phase`'s stack (`job_system::schedule(jobs, fence)`, no shared
  counter), so a phase that has run once allocates nothing of its own afterwards, `parallelSystems`
  with a job system included (`test_scheduler.cpp` gates both modes at zero).
- **`parallel_for_chunks`** flattens the matched chunks, groups them into grains of
  `max(1, minEntitiesPerJob / capacity)` chunks, runs one job per grain with its own command buffer
  (inline without a job system or with `parallelChunks = false`) and appends the grain buffers to the
  system buffer in grain order after the join, so command application order never depends on the
  worker count. Entity id *values* reserved inside grains do depend on it.
- **Debug validation.** A view created through `system_context` must have `with | optional` inside
  the declared `reads | writes` and its writes inside the declared `writes`; a `chunk_view` handed out by
  `parallel_for_chunks` or by a system view's `for_each` / `for_each_chunk` (the view's query carries the
  context, `query::bind_context`) asserts on a non-const span (or `column_data(id, true)`) of a component
  the system did not declare as written. Both name the system. Both checks compile out in Distribution
  (`CONJURE_ASSERTS_ENABLED`); the context binding stays, because the views stamp its run version. A
  view that requires an unregistered type (pack or builder call) asserts naming the type and matches
  nothing; an unregistered `maybe<T>` is simply absent everywhere. A tag in a view pack (plain or
  `maybe<Tag>`) and `chunk_view::get<Tag>()` / `get_optional<Tag>()` do not compile: a tag has no
  column, so it is required with `with<Tag>()`.
- **Archives.** `entity_archive::capture` copies every non-`TRANSIENT` component with `ops.copy` (or
  memcpy) and rewrites entity fields: inside the set they become `entity::archive_local(index)`, outside
  `NULL_ENTITY` unless `keepExternalReferences`. A reflection-less type with a `serializer.remapEntities`
  hook (the script set, R29) rewrites its own references through `entity_remap::remap` with a table whose
  targets are the captured entities; capture calls `index_targets()` on that table, so each lookup of a
  hook is a binary search instead of a scan over the captured set. `instantiate_batch` hands the same
  hook the table of the copy, so one symmetric call serves both directions. `instantiate_batch` refuses a
  command buffer of another world (`INVALID_ARGUMENT`, asserted), validates once (`ARCHIVE_CORRUPT`
  before anything is created; `push_instantiate` trusts its callers), resolves every archetype, places
  all copies of an entry together, copies payloads, remaps entity fields through an `entity_remap` per
  copy and fires `ADDED` last. A placement that fails part-way discards the rows placed so far and
  returns every id unused, so the world and the id space are as before. With a command buffer the ids
  are reserved immediately and the roots returned now. The optional `outEntities` vector receives every
  created id, copy-major in entry order (entries without components included), in both paths; the
  scene's link pass relies on it.
- **Serialisation.** Archives serialise as `{"entities":[{"components":{"<name>":{...}}}]}`. Entity
  fields are the integer local index, `null`, or `{"external":"<index>:<generation>"}`; the live
  `component_to_json` uses the external form. Scalars, strings, `string_id`, `uuid`, math types, enums
  and arrays of those go through `core::reflect`; nested structs, entity fields and asset references
  (`ASSET_REF`: the uuid in the first 16 bytes of the handle, rest zeroed on read) are handled here.
  Unknown component names on load are kept verbatim in the built-in `unresolved_components` (id 0,
  `EDITOR_HIDDEN`) and re-emitted under their original names, except one whose name now resolves to a
  component the entity holds (the live component is written instead, so no key appears twice);
  malformed component JSON fails the whole load with `SERIALIZATION_FAILED` and an
  `<entity index>/<component>/<field path>: detail` message. A failed `write_archive_json` leaves the
  writer with partial, unbalanced output.
  Types without custom hooks serialise through their reflection description (dynamic types included);
  `make_default_serializer<T>` wraps the same walker for C++ types.
- **Reflection constraint.** `core::reflect` demands of an `entity` field the alignment the owning
  type has (capped at 4 for a 4-byte aligned component, since `entity` is `{u32, u32}`), so a
  reflected component such as `parent {entity value; u32 siblingIndex}` needs no `alignas(8)`; an
  entity at an offset that breaks that alignment fails registration with `UNSUPPORTED_FIELD` because
  the description cannot be built. Non-trivial components must not need more than 16-byte alignment
  (`BAD_ALIGNMENT`): archive payloads live in `std::vector<std::byte>` storage.
- **Dynamic types** (`register_dynamic`) are always `DYNAMIC | TRIVIAL`, accept alignment 1..16 and
  fields of the primitive, math and `entity` kinds (`UNSUPPORTED_FIELD` otherwise, including misaligned
  offsets); the registry owns their `type_info`. Their fields are part of the layout: re-registering a
  name with other fields (order, name, type, offset) is `NAME_CONFLICT` even at the same size, which
  sends scripting's hot reload to `redefine_dynamic`. A described record whose size and `TAG` flag
  disagree is `INVALID_ARGUMENT`. `redefine_dynamic` keeps the id, refuses while any
  live world holds an instance (`COMPONENT_IN_USE`) or when the name changes (`NAME_CONFLICT`), and
  re-lays out the now empty archetypes that contain the type (an archetype whose rows no longer fit
  gets capacity 0 and answers every placement with `ARCHETYPE_TOO_LARGE`). The hot-reload flow is
  serialise -> `remove_from_all` -> `redefine_dynamic` -> `add_raw` -> `component_from_json`.
  `register_described` deep-copies the module's description, nested struct and enum descriptions
  included, keeps the copy only when the registration succeeds, and takes the record's
  `component_serializer` as given. `describe_component<T>` leaves the hooks null for a reflected type:
  the registry's reflection walker serialises the deep copy, so a scene or prefab that names the
  component never calls into the module after it has unloaded (`make_default_serializer<T>` would point
  at the module's instantiation). A type without reflection must supply `writeJson` and `readJson`,
  which is how scripting's `script_component` serialises its store handle as the script set.
- **`world_hash`** visits archetypes in ascending order of their mask hash, entities in chunk order,
  and hashes the entity id plus every component (name hash combined with the bytes of every reflected
  field for trivial types, nested structs walked, so padding never takes part, and an `ASSET_REF` field
  wider than a uuid through its leading uuid alone, so the bound slot pointer never takes part and a
  world hashes the same in every process, bound or not; with the raw bytes for
  trivial types without a description, padding and pointers included, so such a type must not hold a
  bound reference or any other process-dependent value in a world whose hash is recorded; with the
  serialised text for non-trivial ones, written into one JSON writer reused across the whole hash, and
  a constant for one whose serializer fails; the name alone for tags). It allocates nothing below 64
  archetypes for trivial worlds. Row order inside an archetype takes part (a removed row is filled by
  the archetype's last one), so equal content reached through different structural histories can hash
  differently. Entity ids and TRANSIENT components are hashed too: ids created
  through command buffers from parallel grains or concurrently running systems come in thread-timing
  order, and bookkeeping such as `transform_state::lastUpdateRun` counts passes, so recorded hashes
  compare only for runs that create from EXCLUSIVE or MAIN_THREAD systems and at equal frame counts.
- **`world::clear()`** destroys every entity (observers fire), returns every chunk, the parked chunk
  metadata and the structural scratch (flush plan, placement list, the deferred buffers' arena room) to
  the heap and keeps the archetypes, the observers and the id space: destroyed indices come back with a
  bumped generation, so stale handles stay stale and ids still reserved by command buffers remain valid.
- **`world::validate()`** checks records against rows, chunk counts, packing (every chunk before the
  last non-empty one is full, at most one spare), column alignment and bounds, free-list consistency,
  the archetype map and the pool's live block count; the error message names the first violation.

## Threading model

- One owner thread per world, captured at construction and changeable with `set_owner_thread()` while
  unlocked. Structural operations and `run_phase` belong to it.
- `iteration_scope` is an atomic depth counter taken from any thread. While locked, any thread may
  read records (`is_alive`, `has`, `mask_of`, `get` const) and read or write columns of chunks it was
  handed (column version stamps are relaxed atomic stores). Structural calls are refused.
- `entity_allocator::reserve` is the module's only mutex (hit once per 64 ids by command buffers and
  once per archive instantiation). `command_buffer` itself is single-threaded; `parallel_for_chunks`
  gives every grain its own.
- `component_registry` methods take its mutex except `info(id)`, `is_registered(id)`, `count()` and
  `types()`, which read the pre-reserved append-only vector guarded by the atomic count; `id_of<T>()`
  is a lock-free cache hit after the first lookup per registry serial. `on_registered` fires on the
  registering thread after the lock is released.
- Observers run on the owner thread inside structural operations. `scheduler::run_system` may run on a
  job thread (parallel systems); it only touches the world through the locked-iteration API and its own
  command buffer.
- `world_hash`, `entity_archive::capture`, `validate()` and `stats()` read without locking: call them
  while no structural change is in flight.

## Testing

`tests/ecs/` builds `conjure_ecs_tests`: one `test_<header>.cpp` per public header plus the topic files
of `01_ecs.md` section 10 (`test_entity_allocator`, `test_component_mask`, `test_chunk_layout`,
`test_world_entities`, `test_world_components`, `test_change_detection`, `test_parallel_iteration`,
`test_dynamic_components`) and `test_world_structural` (the allocation gates of flush and placement
churn). `ecs_test_fixture.h` provides the test component types, `ecs_fixture`
(registry + world, validated in the destructor), `make_zero_capacity_archetype` (an archetype laid out
with capacity 0 through `redefine_dynamic`, for the failure paths of placement, flush and
instantiation) and `for_each_job_mode` (SUBCASEs `inline` and `parallel`, the latter on a worker pool
of at least four threads). Pool exhaustion is tested through `world_desc::maxChunkSlabs`. Review
fixes carry `regression - review <n> ...` cases. Everything is headless and deterministic; the
parallel cases assert order independence through `world_hash`.

```
python tools/build.py --build-dir build/mod_ecs --target conjure_ecs_tests --test -R ecs
build/mod_ecs/bin/conjure_ecs_tests
```
