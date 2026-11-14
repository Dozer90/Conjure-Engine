# physics

Rigid-body simulation over the ECS (`conjure::physics`, target `conjure_physics`, wave 3): Jolt Physics 5.3
fully encapsulated behind `physics_world`, with ECS components for bodies, colliders, materials and a
character controller, a fixed-step push / step / pull / contact pipeline, layer-filtered queries, cooked
collision meshes with a glTF / OBJ importer, and a debug draw sink. Binding design: `docs/design/14_physics.md`;
the cross-area decisions it cites are R6, R44, R47-R49 and R53 of `docs/design/09_cross_area_resolutions.md`
and `ARCHITECTURE.md` 4.5. Public dependencies `conjure::core`, `conjure::ecs`, `conjure::scene` and
`conjure::assets`; `conjure::thirdparty::jolt` and `conjure::thirdparty::cgltf` are PRIVATE, so no Jolt
or cgltf symbol is reachable from a public header.

## Public headers (`engine/include/conjure/physics/`)

| Header | Contents |
|---|---|
| `log_category.h` | `LOG_PHYSICS` (`"physics"`) |
| `physics_error.h` | `physics_error` (21 codes), `physics_error_name`, category `"physics"` |
| `physics_layers.h` | `MAX_PHYSICS_LAYERS` (32), `physics_layer_mask` (`all`, `none`, `only`, `with`, `without`, `includes`), `physics_layer_table` (`from_json`, `to_json`, `add_layer`, `set_collides`, `collides`, `collision_mask`, `find`, `name`, `count`, `validate`) |
| `physics_settings.h` | `physics_settings` (Jolt sizing, solver, sleeping, contact and command budgets, `layers`), `validate`, `from_json(physicsSection, layersSection)` |
| `components.h` | `motion_type`, `rigid_body_flags`, `rigid_body`, `box_collider`, `sphere_collider`, `capsule_collider`, `convex_collider`, `mesh_collider`, `physics_material` (reflected, registered as `conjure.physics.<struct name>`) |
| `character_controller.h` | `ground_state`, `character_flags`, `character_controller` (68 bytes) |
| `contact_events.h` | `contact_kind`, `to_string`, `contact_event` (48 bytes) |
| `queries.h` | `query_filter`, `raycast_hit`, `shape_cast_hit`, `sphere_shape_desc`, `box_shape_desc`, `capsule_shape_desc`, `cast_shape_desc` |
| `collision_mesh_asset.h` | `COLLISION_MESH_ASSET_TYPE`, `COLLISION_MESH_IMPORTER_ID`, `collision_mesh_asset` (`from_geometry`, `from_payload`, `to_payload`, positions, indices, bounds, opaque Jolt shapes) |
| `physics_debug_draw.h` | `physics_debug_draw_flags`, abstract `physics_debug_draw` (`line`, `triangle`, `text`) |
| `physics_world.h` | `physics_stats`, `physics_world`: `attach` / `detach`, `push_transforms`, `step`, `pull_transforms`, `collect_contact_events`, `contact_events`, forces / impulses / velocities / `teleport` / `move_kinematic` / `wake` / `sleep` / `set_gravity`, `character_move`, `character_ground`, `raycast`, `raycast_all`, `overlap_sphere`, `overlap_box`, `shape_cast`, `body_pose`, `bounds`, `stats`, `debug_draw`, `on_asset_reloaded`, `validate` |
| `register.h` | `register_components`, `register_systems`, `register_asset_types`, the four system names |
| `physics.h` | umbrella |

Private (`engine/src/physics/`): `jolt_include.h` (the one place Jolt's `Jolt.h` is included, with the
warning pragmas), `jolt_runtime.{h,cpp}` (reference-counted process state: memory hooks, trace and assert
handlers, `Factory`, `RegisterTypes`), `jolt_memory.{h,cpp}` (Jolt's allocation hooks over
`core::memory::tracked_allocate` with a 16-byte header so allocation counters and `allocation_scope` see
Jolt), `jolt_job_system.{h,cpp}` (`JPH::JobSystemWithBarrier` adapter over `core::jobs`), `jolt_layers.{h,cpp}`
(object and broad-phase layer tables, query filters), `jolt_conversions.h`, `body_record.h` (the row table),
`shape_builder.{h,cpp}` and `shape_cache.{h,cpp}` (collider gathering, compound shapes, shared Jolt shapes),
`contact_collector.{h,cpp}` (the `ContactListener` and `BodyActivationListener`), `collision_mesh_internal.h`,
`collision_mesh_loader.{h,cpp}`, `collision_mesh_importer.{h,cpp}` (cgltf and OBJ readers, welding),
`physics_world_internal.h` (`physics_world::state`) and one `physics_world_*.cpp` per concern (bodies,
characters, sync, contacts, queries, debug) plus `physics_systems.cpp` (the registrations).

## How it works

- **Jolt stays inside.** `physics_world` owns a `JPH::PhysicsSystem`, a `TempAllocatorImpl`, the layer
  tables and the job adapter through a pimpl `state`. Every public call translates entities to
  `JPH::BodyID`s through the row table (`body_record`, indexed by body index, grown in 1024-row blocks):
  `rowOfEntity` knows every entity the world tracks (live rows and the pending markers) and
  `liveRowByEntityIndex`, a dense table indexed by `entity.index` and generation-checked against the row's
  entity, is the one load `find_row` makes for a live row (the pull resolves every body through it each
  step). That table is the one container not reserved at construction: it is sized by entity index, not
  body count, so `claim_row` grows it in 1024-index blocks when a body lands beyond it (body creation, where
  Jolt allocates anyway) and the pull never grows it; a world whose few bodies sit on high entity indices
  pays 4 bytes per index up to the highest one. Jolt's own allocations go through the engine's tracked
  allocator, so the "allocation free step" tests cover Jolt too.
- **Jobs.** `core::jobs::job_system::schedule` allocates a counter per call, which would break P15, so
  the adapter uses `parallel_for`: `run_parallel` dispatches `worker_count()` helper ranges that pop Jolt
  jobs from a fixed ring while range 0 (always the caller) runs `PhysicsSystem::Update`; helpers wait on a
  generation counter with `std::atomic::wait`. With 0 workers every job runs inline inside `QueueJob`,
  like `JobSystemSingleThreaded`, and both paths are bit-identical (`test_determinism.cpp`).
- **Layers.** Object layer = `(moving ? 32 : 0) + layer`: two broad-phase layers (static, moving), a
  64-entry `ObjectLayerPairFilterTable` derived from `physics_layer_table` (static-static never enabled),
  and mask filters for queries. The table comes from `project.json` `extensions.physics_layers`
  (`{"names": [...], "ignore": [[a, b], ...]}`, canonical `to_json`).
- **Bodies follow components.** ADDED / REMOVED observers on the eight physics components and
  `scene::parent`, plus `observe_destroyed`, keep a pending-create list and per-row rebuild flags; the
  bodies are created, rebuilt and destroyed inside `push_transforms` (never from an observer, except that
  destruction frees the Jolt body at once so the entity is gone from queries). A pending entity whose
  body components go away again is tombstoned in `rowOfEntity` (`WITHDRAWN_ROW`) and dropped by the next
  flush instead of being erased from the list; a stale rebuild entry is skipped because the row's own
  `dirty` flags are the truth; `body_count()` is a counter kept by `claim_row` / `destroy_body`.
  Colliders without a `rigid_body` make an implicit static body whose rebuilds compare against a
  STATIC description (adding a collider to a resting floor is a `SetShape`, not a new body);
  `rigid_body` without colliders stays pending (`stats().bodiesWithoutShape`); `rigid_body` +
  `character_controller` warns `CONFLICTING_BODY_COMPONENTS` and the character wins, also when the
  `character_controller` arrives after the body exists. `interpolated_transform` is added to dynamic and
  kinematic bodies and characters through the deferred command buffer (R47), seeded with the entity's
  composed world pose so it never renders at the origin before the first pull; `create_body` removes it
  from a body that turns out STATIC (one queued while the entity was pending as the default DYNAMIC
  `rigid_body`), and `detach` removes every one physics added. Shapes are gathered from all colliders of
  the entity (offset, rotation, world scale), shared through `shape_cache` by value, and wrapped in a
  `StaticCompoundShape` when there is more than one. A convex or mesh collider whose asset is not loaded
  makes `build_shape` return `pendingAsset` before it builds or logs anything: a new body stays pending
  and an existing one keeps its shape while its SHAPE rebuild is re-queued every push.
- **Change detection.** `push_transforms` visits the chunks whose `rigid_body`, `character_controller` or
  `local_transform` column was stamped since the previous push (persistent `ecs::query` objects with a
  `changed` filter), diffs each row against the mirror kept in its record (velocities, motion type,
  layer, flags, mass, properties, scale) and applies only what changed: a mass or lock-flag change
  re-derives the Jolt mass properties from the shape (`ScaleToMass` + `SetMassProperties` with the
  allowed degrees of freedom, the derivation `BodyCreationSettings` uses), damping, gravity factor,
  motion quality, sleeping and gyroscopic force follow their flags, and a shape rebuild re-applies the
  same mass properties after `SetShape`. A pose that differs from the last pulled pose is a teleport
  (`SetPositionAndRotation`, `interpolated_transform.previous` reset). The push bumps the world version
  itself so that writes made between pushes always stamp above the remembered version. The chunk filter
  only sees a row's own columns, so `push_hierarchy_rows` walks the records every push for the rows the
  filter cannot cover: `KINEMATIC_MOVES_WITH_TRANSFORM` rows get `MoveKinematic` and their
  `interpolated_transform` written (the pull never visits them), and parented bodies and characters are
  composed through their ancestors and teleported when an ancestor moved (a design gap of `14_physics.md`
  3.4, which only names the chunk filter). A parented row's position tolerance grows with its coordinates
  (`TELEPORT_RELATIVE_EPSILON`): the pull's local = parent^-1 * world round trip loses more than the
  absolute 1e-4 m far from the origin, which would otherwise teleport and wake the body every push. Root
  rows keep the absolute tolerance; the pull writes them bit-exactly. An ancestor that moves less than that
  tolerance in one push does not carry an awake parented body (the pull re-expresses the body in the moved
  parent's frame): 16 * `FLT_EPSILON` times the largest coordinate, roughly 6 mm at 3 km.
- **Pull.** `pull_transforms` acquires a chunk's non-const spans at the first row that needs writing
  (awake, first pull, teleported), so chunks of sleeping bodies are left unstamped and every row resolves
  its record once; the pull that first sees a row asleep writes `previous = current`, so a sleeping body's
  render pose stands still; the body is fetched once per row through `BodyLockInterfaceNoLock::TryGetBody`
  (no step runs during the pull) and its pose and velocities read from the `JPH::Body`. Parented bodies and
  characters are written in a serial tail (local pose = parent world pose inverse times body pose).
  `pull_transforms(system_context *)` is the scheduled form: the persistent pull query (the match list,
  nothing more) is fanned out through `system_context::parallel_for_chunks`, so the step allocates
  nothing (a `context.view<>()` per run would allocate its match list every time); the dispatching
  `system_context` builds every chunk view with itself as the validating context, so the Debug write
  validation checks the pull's writes against the system's declaration without binding the query.
- **Contacts (R6).** The listener records raw added / persisted / removed contacts into per-thread
  buffers (global budget `maxContactEventsPerStep`); `collect_contact_events` merges them, sorts twice
  with the stable `core::radix_sort_u64` (sub-shape pair, then body pair) and folds each body pair into
  one `ENTER` / `STAY` / `EXIT` (or `TRIGGER_ENTER` / `TRIGGER_EXIT`) per step using a pair table, so the
  span is sorted by body id pair and identical whatever thread recorded what. `ENTER` takes its point and
  normal from the run's first ADDED record, `STAY` from its first live (ADDED or PERSISTED) one, never from
  a REMOVED record, whose point and normal are zero. Destroying a body in contact synthesises its exits,
  which the next collect reports. The span dies at the next `push_transforms`.
- **Commands (P9).** Forces, impulses, velocities, teleports, wake / sleep, gravity and `character_move`
  are queued (`COMMAND_QUEUE_FULL` when `maxQueuedCommands` is reached; core `INVALID_ARGUMENT`, never
  queued, for a non-finite vector or rotation) and applied in the next push; a command on a pending entity
  is kept for one push. `character_move` lands in the row (`desiredVelocity`) and is consumed by the next
  `step`, so the push never writes `character_controller` (its system declares the component as a read). A
  `teleport` of a row the pull never visits (static, `KINEMATIC_MOVES_WITH_TRANSFORM`) writes the entity's
  `local_transform` at once so later edits of it are measured against the teleported pose.
- **Characters.** `JPH::CharacterVirtual` with an inner kinematic body (so raycasts hit it and it shows
  up in contact events); `step()` runs `ExtendedUpdate` per character in table order before the Jolt
  update: horizontal velocity from the component, gravity integrated in the air, the vertical component
  snapped to the ground's while grounded unless the script set an upward (jump) velocity,
  `CancelVelocityTowardsSteepSlopes`, stair walking and floor sticking by `stepHeight`. The pull writes
  back `velocity`, `ground`, `groundNormal` and `groundEntity`. Authored field changes rebuild the
  character in place with the same inner body id.
- **Queries.** `raycast`, `raycast_all` (the nearest hits that fit the span, sorted by distance: once the
  span is full a nearer hit replaces the farthest kept and the collector's early-out fraction follows the
  farthest kept), `overlap_sphere`, `overlap_box`, `shape_cast` (sphere / box / capsule) through the
  narrow-phase query with layer mask, static, trigger and ignore filters. Rays are clamped to 1e6 m (an
  infinite `maxDistance` means unlimited) and a non-finite origin, rotation or direction yields nothing.
  Calling one (or `body_pose` / `debug_draw`) while `push_transforms`, `step` or `pull_transforms` runs
  asserts (`STEP_IN_PROGRESS`) and returns nothing: the `busy` flag covers every mutation of the row
  tables, not only the Jolt update.
- **Collision meshes.** Payload `{u32 vertexCount; u32 indexCount; u32 reserved[2]; f32 positions[][3];
  (16-aligned) u32 indices[]}`; the loader builds the `MeshShape` on the load job; convex hulls are built
  per (asset, version, convexRadius) in the shape cache. The importer claims `.obj` by extension only: a
  glTF is a render model by extension and reaches this importer when its `.meta` names it (code sweep
  F137). It reads glTF / GLB (cgltf with the
  engine's memory and VFS hooks) baking node transforms, and OBJ (quads, relative indices), welds
  vertices (`weld_distance`, default 1e-4 m; every welded vertex is chained into its grid cell, the lowest
  index within reach wins) and emits one primary asset; the same bytes import byte-identically. glTF input
  is validated first: `cgltf_validate` after the buffers load, the side-file hook rejects an external
  buffer shorter than its declared `byteLength` (cgltf only validates against the declared length), every
  triangle primitive's index count must be a multiple of three, and a node with a negative determinant has
  its triangles' winding reversed. `collision_mesh_importer::VERSION` is 2 since those checks. Jolt
  simulates against the front face of a triangle (counter-clockwise seen from the side the body
  approaches). `on_asset_reloaded` marks every body using the asset for a rebuild.
- **Settings.** `physics_settings::from_json` reads the snake_case keys of `project.json` `physics`;
  invalid settings are logged and replaced by validated defaults at construction. `maxJobs` must be a
  power of two (the job pool is a `JPH::FixedSizeFreeList` paged by it) large enough for one update's
  jobs; `maxBodyPairs` and `maxContactConstraints` must be between 4 and 1 << 24 (Jolt's caches assert
  below four entries) and `maxCharacters` at least 1. Jolt's Development
  configuration gets `JPH_DEBUG_RENDERER` and `JPH_ENABLE_ASSERTS` PUBLIC on the Jolt target from
  `third_party/CMakeLists.txt` (Jolt's generator expressions cover only Debug and Release); the engine's
  `JPH_*` feature defines are the ones `Jolt/Core/Core.h` checks through `JPH_VERSION_ID` at
  `RegisterTypes`. `CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC` (root option, OFF) maps to Jolt's
  `CROSS_PLATFORM_DETERMINISTIC` and adds `/fp:precise` or `-ffp-contract=off` to this module's objects.

## Deviations from `14_physics.md` worth knowing

- `push_transforms(f32 fixedDeltaTime, ecs::command_buffer *deferred = nullptr)` takes the deferred
  buffer the system passes (R47); outside a system the world's own buffer is flushed at the end of the
  push.
- `physics_settings::penetrationSlop` (default 0.005 m, `penetration_slop` in JSON) is an extra field:
  Jolt's default 0.02 m leaves a resting box 1.5 cm deep, which the design's "within 1 cm" tests reject.
  A ten-box stack still compresses about 1.3 mm per contact whatever the slop or iteration count (Jolt's
  solver residual), so `test_physics_stack.cpp` allows 2 cm at the top.
- `character_controller` is 68 bytes (R48 says 72): `velocity` at offset 32, `groundEntity` at 60,
  pinned by `static_assert`; the C# mirror follows `type_info::layout_hash()`.
- The free-fall and `collisionSteps` tests compare against Jolt's semi-implicit Euler closed form
  (`g dt^2 n(n+1)/2`), not `0.5 g t^2`, and say so in the test.
- The Jolt job adapter runs helper ranges through `parallel_for` instead of `schedule` (see "Jobs").
- Parented rows are composed every push (see "Change detection"); 3.4 of the design only has the chunk
  filter, under which a child of a moved parent would never be re-posed.
- Queries are refused during the push and the pull as well as during the step (see "Queries"); 3.8
  only names the step.
- `physics_settings::validate` also rejects a non-power-of-two `maxJobs`, a `maxBodyPairs` or
  `maxContactConstraints` outside 4 .. 1 << 24 and a zero `maxCharacters` (2.1 lists none of them).
- The only `new` outside Jolt's factory is `jolt::make_jolt_ref` (`jolt_include.h`) for the
  `CharacterVirtual`, a Jolt ref-counted object without a factory; its `JPH_OVERRIDE_NEW_DELETE` keeps
  the allocation on the tracked allocator.
- `test_collision_mesh_asset.cpp` generates its GLB and OBJ fixtures in memory instead of reading
  `tests/assets/physics/ramp.glb` and `hull.obj`, so the module needs no binary test assets.
- A character's `velocity` is written back every pull (Jolt leaves it as set, so walking into a wall
  keeps the pushed velocity while the position stops), and the grounded vertical reset described above
  is the engine's, not Jolt's.
- `push_transforms` applies rebuilds before it flushes pending creations (3.4 lists the flush first): a
  rebuild that replaces a body (a `rigid_body` removed so the colliders make an implicit static body, an
  implicit static body given a `rigid_body`, a shape left without colliders) queues the replacement, and
  the flush creates it in the same push. An implicit static body whose static `rigid_body` turns dynamic
  or kinematic is replaced by the change detection after that flush, so the push flushes once more; no
  fixed step runs without the body.
- The Jolt-free unit test of the job adapter (`14_physics.md` 9 step 2: 10k dependent jobs through a
  barrier with 0 and 4 workers) does not exist: the physics tests include no Jolt header. The adapter is
  covered through whole-world steps (`test_determinism.cpp`, 0 versus 4 workers); writing that test needs a
  test target with Jolt's include paths.

## Cross-module requests

- Resolved at wave 3 integration: the Development `JPH_DEBUG_RENDERER` / `JPH_ENABLE_ASSERTS` definitions
  live next to the Jolt declaration in `third_party/CMakeLists.txt` and
  `CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC` maps to Jolt's `CROSS_PLATFORM_DETERMINISTIC`.
- `core::jobs`: an allocation-free fire-and-forget `schedule` (a counter owned by the caller) would let
  the adapter drop the `parallel_for` helper trick.
- `tests/benchmarks/bench_physics.cpp` (10k sleeping bodies, 2k active, raycast batches); the wiki pages
  under `docs/api/physics/` and `docs/handbook/modules/physics.md` exist since wave 3 integration.

## Threading

`attach`, `detach`, `push_transforms`, `step`, `collect_contact_events` and the command queue are
owner-thread only (the thread that constructed the world; asserted, and `attach` and the commands return
`WRONG_THREAD`). `pull_transforms` with a `system_context` runs its chunk pass on the job system; the
queries run from any thread while none of `push_transforms`, `step` and `pull_transforms` is in progress
(Jolt's `NarrowPhaseQuery` locks per body; the row tables are not locked, so the `busy` flag refuses
queries for the whole of those three calls, which in the engine means outside the PHYSICS_STEP and
FIXED_END phases); `debug_draw` draws nothing meanwhile. Structural ECS changes on the owner thread run
the physics observers, which edit the same tables, so queries from other threads must not overlap those
either. The contact listener runs on Jolt's job threads and writes only to the buffer of
`job_system::thread_index()`.

## Testing

`tests/physics/` (`conjure_physics_tests`, headless, no Jolt header): one `test_<header>.cpp` per public
header plus the design's topic files (`test_layers`, `test_rigid_body`, `test_colliders`, `test_fixed_step`,
`test_component_sync`, `test_physics_stack`, `test_determinism`). `physics_test_fixture.h` builds a
registry, world and attached physics world with small budgets and runs push / step / pull / collect;
`for_each_job_mode` runs a body with 0 and 4 workers; `scheduled_physics` is the engine's view of the four
systems (an `ecs::scheduler` with `register_systems`, the PARALLEL pull over chunks) and
`check_pull_allocation_free` the allocation gate of that pull at a body count. The `[slow]` cases (the
0-versus-4-workers determinism run over `determinism_scenario.h`, the 10,000-body allocation gate) live in
`tests/physics/slow/` as the second executable `conjure_physics_slow_tests` (labels `physics;slow`), which
the default test presets skip and the nightly presets run; the default executable keeps the same gate at
2,000 bodies. Build and run with
`python tools/build.py --build-dir <dir> --target conjure_physics_tests --test -R physics` (add
`--target conjure_physics_slow_tests -L slow` for the slow cases).
