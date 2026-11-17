# Physics module

🛠 **Engineering handbook** · 📘 API: [Physics](../../api/physics/index.md), [Bodies, colliders and characters](../../api/physics/bodies-and-colliders.md), [The physics world and queries](../../api/physics/physics-world-and-queries.md)

`conjure::physics` (target `conjure_physics`, alias `conjure::physics`) is the wave-3 module that runs Jolt
Physics 5.3 behind `physics_world`: ECS components for bodies, colliders, materials and a character controller,
a fixed-step push / step / pull / contact pipeline, layer-filtered queries, cooked collision meshes with a glTF /
OBJ importer and a debug draw sink. This chapter is for people changing the module or wiring it into the engine:
how Jolt is contained, how bodies follow components, change detection, the job adapter, contacts, threading
and tests. The binding design is [`14_physics.md`](../../design/14_physics.md); the cross-area decisions are R6,
R44, R47-R49 and R53 of [`09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md);
`engine/src/physics/README.md` carries the finer notes.

## Contents

- [1. Shape of the module](#1-shape-of-the-module)
- [2. Jolt stays inside](#2-jolt-stays-inside)
- [3. Bodies follow components](#3-bodies-follow-components)
- [4. The fixed step](#4-the-fixed-step)
- [5. Jobs and memory](#5-jobs-and-memory)
- [6. Contacts, commands and queries](#6-contacts-commands-and-queries)
- [7. Characters](#7-characters)
- [8. Collision meshes](#8-collision-meshes)
- [9. Settings and build options](#9-settings-and-build-options)
- [10. Threading model](#10-threading-model)
- [11. Tests](#11-tests)
- [12. Open items](#12-open-items)
- [Related](#related)

## 1. Shape of the module

`engine/src/physics/CMakeLists.txt`: `conjure_add_module(physics ... PUBLIC_DEPS conjure::core conjure::ecs
conjure::scene conjure::assets PRIVATE_DEPS conjure::thirdparty::jolt conjure::thirdparty::cgltf)`, guarded by
`CONJURE_WITH_PHYSICS` and the presence of the three engine targets. No Jolt or cgltf symbol reaches a public
header.

| Header | Main types | 📘 Wiki page |
|---|---|---|
| `components.h`, `character_controller.h` | `rigid_body`, the five colliders, `physics_material`, `character_controller`, the enumerations | [Bodies, colliders and characters](../../api/physics/bodies-and-colliders.md) |
| `physics_layers.h` | `physics_layer_mask`, `physics_layer_table` | same |
| `collision_mesh_asset.h`, `register.h` | `collision_mesh_asset`; `register_components`, `register_systems`, `register_asset_types` | same |
| `physics_world.h`, `physics_settings.h` | `physics_world`, `physics_stats`; `physics_settings` | [The physics world and queries](../../api/physics/physics-world-and-queries.md) |
| `queries.h`, `contact_events.h`, `physics_debug_draw.h` | `query_filter`, the hit records, the shape descriptions; `contact_event`; `physics_debug_draw` | same |
| `physics.h`, `physics_error.h`, `log_category.h` | umbrella, `physics_error` (22 codes), `LOG_PHYSICS` | not user-facing |

Private: `jolt_include.h` (the one place `Jolt.h` is included, with the warning pragmas), `jolt_runtime.{h,cpp}`
(reference-counted process state: memory hooks, trace and assert handlers, `Factory`, `RegisterTypes`),
`jolt_memory.{h,cpp}` (Jolt's allocation hooks over `core::memory::tracked_allocate` with a 16-byte header),
`jolt_job_system.{h,cpp}` (`JPH::JobSystemWithBarrier` adapter over `core::jobs`), `jolt_layers.{h,cpp}`,
`jolt_conversions.h`, `body_record.h` (the row table), `shape_builder.{h,cpp}` and `shape_cache.{h,cpp}`,
`contact_collector.{h,cpp}` (the `ContactListener` and `BodyActivationListener`), `collision_mesh_internal.h`,
`collision_mesh_loader.{h,cpp}`, `collision_mesh_importer.{h,cpp}` (cgltf and OBJ readers, welding),
`physics_world_internal.h` (`physics_world::state`) and one `physics_world_*.cpp` per concern (bodies,
characters, sync, contacts, queries, debug) plus `physics_systems.cpp`.

## 2. Jolt stays inside

`physics_world` owns a `JPH::PhysicsSystem`, a `TempAllocatorImpl`, the layer tables and the job adapter through a
pimpl `state`. Every public call translates entities to `JPH::BodyID`s through the row table (`body_record`,
indexed by body index, grown in 1024-row blocks) and `rowOfEntity`. Jolt's own allocations go through the
engine's tracked allocator, so the allocation-free step tests cover Jolt too. Object layer = `(moving ? 32 : 0) +
layer`: two broad-phase layers (static, moving), a 64-entry `ObjectLayerPairFilterTable` derived from
`physics_layer_table` (static-static never enabled), and mask filters for queries.

## 3. Bodies follow components

ADDED / REMOVED observers on the eight physics components and `scene::parent`, plus `observe_destroyed`, keep a
pending-create list and per-row rebuild flags; the bodies are created, rebuilt and destroyed inside
`push_transforms` (never from an observer, except that destruction frees the Jolt body at once so the entity is
gone from queries). A pending entity whose body components go away again is tombstoned (`WITHDRAWN_ROW`); a stale
rebuild entry is skipped because the row's own `dirty` flags are the truth. Colliders without a `rigid_body` make
an implicit static body (adding a collider to a resting floor is a `SetShape`, not a new body); `rigid_body`
without colliders stays pending (`stats().bodiesWithoutShape`); `rigid_body` + `character_controller` warns
`CONFLICTING_BODY_COMPONENTS` and the character wins. `interpolated_transform` is added to dynamic and kinematic
bodies and characters through the deferred command buffer (R47), seeded with the entity's world pose so nothing
renders at the origin before the first pull; a body that turns out static at creation loses one queued while it
was pending, and `detach` removes every one physics added. Shapes are gathered from all colliders of the entity
(offset, rotation, world scale), shared through `shape_cache` by value, and wrapped in a `StaticCompoundShape`
when there is more than one. A convex or mesh collider whose asset is not loaded keeps a new body pending and an
existing one on its current shape (`build_shape` reports `pendingAsset` before building or logging anything).
Rebuilds run before the pending creations, so a rebuild that replaces a body is recreated in the same push; an
implicit static body replaced later in the push (its static `rigid_body` turned dynamic) gets a second flush.

A static trigger is the one body whose Jolt motion type is not the row's (`14_physics.md` P21, code sweep F408):
Jolt holds it as a kinematic sensor, added active and without velocity, in the static object layer, so that it
keeps a body that falls asleep inside it and pairs with neither static geometry nor another static trigger.
`row.motion` stays `STATIC` and every engine rule keyed on it holds (no pull, no `interpolated_transform`,
commands refused, `is_sleeping` true); `row.staticSensor` and `state::staticSensors` record the difference, and
`active_body_count()` subtracts the count from Jolt's. `sync_jolt_motion(row)` is the only place that maps
`(row.motion, row.isTrigger)` onto Jolt after creation (motion type, sensor flag,
`mCollideKinematicVsNonDynamic`, activation, zeroed velocities); `apply_motion_type` and the `SHAPE` rebuild both
end in it, so call it from any new path that changes either input. It returns false for an implicit static
body that would have to become a trigger: that body has no motion properties, and the rebuild replaces it.
`validate()` checks the mark, the count and that each static trigger is an active kinematic sensor. The price:
a static trigger is stepped like an awake body, and a body inside one keeps its body pair and its cached manifold
while it sleeps (`maxBodyPairs`, `maxContactConstraints`) and one entry of the pair table (`activePairs`, capacity
`maxContactEventsPerStep`). It costs no raw contact record per step: Jolt reports the pair as persisted in every
step, and the listener drops persisted contacts of sensor pairs, which nothing reads.

## 4. The fixed step

`push_transforms` visits the chunks whose `rigid_body`, `character_controller` or `local_transform` column was
stamped since the previous push (persistent `ecs::query` objects with a `changed` filter), diffs each row against
the mirror kept in its record and applies only what changed: a mass or lock-flag change re-derives the Jolt mass
properties from the shape, damping, gravity factor, motion quality, sleeping and gyroscopic force follow their
flags, and a shape rebuild re-applies the same mass properties after `SetShape`. A pose that differs from the
last pulled pose is a teleport: `TELEPORT_EPSILON` (1e-4) per position component and, through
`math::same_rotation`, per quaternion component, which is a turn of 2e-4 to 4e-4 rad depending on the axis (it
was a cosine tolerance, 1.6 degrees, before code sweep F213). A character whose rotation alone differs is turned
in place and keeps what its ground carries it with (section 7). The chunk filter only sees a row's own columns, so `push_hierarchy_rows` walks the
records every push for the rows it cannot cover: `KINEMATIC_MOVES_WITH_TRANSFORM` rows get `MoveKinematic` and
their `interpolated_transform` written, and parented bodies and characters are composed through their ancestors
and teleported when an ancestor moved. The teleport tolerance of a parented row grows with its coordinates, since
the pull's local = parent^-1 * world round trip loses more than 1e-4 m far from the origin. An ancestor that moves
less than that tolerance in one push does not carry an awake parented body (the pull re-expresses the body in the
moved parent's frame): 16 * `FLT_EPSILON` times the largest coordinate, roughly 6 mm at 3 km.

`pull_transforms` acquires a chunk's non-const spans only when a row of it needs writing (awake, first pull,
teleported), so chunks of sleeping bodies are left unstamped, and the pull that first sees a row asleep writes
`previous = current` so its render pose holds still; parented bodies and characters are written in a serial
tail (local pose = parent world pose inverse times body pose). `pull_transforms(system_context *)` is the
scheduled form (parallel chunks).

## 5. Jobs and memory

`core::jobs::job_system::schedule` allocates a counter per call, which would break the allocation-free step, so
the adapter uses `parallel_for`: `run_parallel` dispatches `worker_count()` helper ranges that pop Jolt jobs from a
fixed ring while range 0 (always the caller) runs `PhysicsSystem::Update`; helpers wait on a generation counter
with `std::atomic::wait`. With 0 workers every job runs inline inside `QueueJob`, and both paths are
bit-identical (`test_determinism.cpp`). `maxJobs` must be a power of two (the pool is a
`JPH::FixedSizeFreeList` paged by it) large enough for one update's jobs.

## 6. Contacts, commands and queries

The listener records raw added / persisted / removed contacts into per-thread buffers (global budget
`maxContactEventsPerStep`; persisted contacts of sensor pairs are not recorded, a trigger pair has no `STAY`);
`collect_contact_events` merges them, sorts twice with the stable
`core::radix_sort_u64` (sub-shape pair, then body pair) and folds each body pair into one `ENTER` / `STAY` / `EXIT`
(or `TRIGGER_ENTER` / `TRIGGER_EXIT`) per step, so the span is sorted by body id pair and identical whatever
thread recorded what (R6). A `STAY` takes its point and normal from a live (added or persisted) record, never
from a removed one. Destroying a body in contact synthesises its exits. Jolt removes a contact when the last
active body of the pair falls asleep, so a solid pair reports `EXIT` at sleep and `ENTER` at wake (kept and
documented, P21); a static trigger, being an active sensor, keeps its pairs with sleeping bodies.

Forces, impulses, velocities, teleports, wake / sleep, gravity and `character_move` are queued
(`COMMAND_QUEUE_FULL` when `maxQueuedCommands` is reached, core `INVALID_ARGUMENT` for a non-finite value) and
applied in the next push; a command on a pending entity is kept for one push. Since code sweep F413 `queue()`
also refuses, with `WRONG_BODY_KIND`, a command the entity's kind of body cannot take (`14_physics.md` P18 and
the table of 3.4). `command_applies(kind, command_target)` in `physics_world_internal.h` is the one table: the
call asks it with `target_of_components` (the kind the components describe, which is what the row will be after
the next push: that push applies motion type and `rigid_body` changes before the commands), `apply_commands`
asks it with `target_of_row` and counts a mismatch in `droppedCommands` (a command queued for a pending entity,
or one whose target changed kind since the call). A new command or kind is one line in that table; never test
the kind in only one of the two places. Because the call reads `rigid_body` and `character_controller`, a system
that sends body commands declares both as reads (or is `EXCLUSIVE`). Queries go through the
narrow-phase query with layer mask, static, trigger and ignore filters; `raycast_all` keeps the nearest hits that
fit its span, rays are clamped to 1e6 m and non-finite input yields nothing. Calling a query (or `body_pose` /
`debug_draw`) while `push_transforms`, `step` or `pull_transforms` runs asserts (`STEP_IN_PROGRESS`) and returns
nothing, because the `busy` flag covers every mutation of the row tables, not only the Jolt update.

## 7. Characters

`JPH::CharacterVirtual` with an inner kinematic body (so raycasts hit it and it shows up in contact events);
`step()` runs `ExtendedUpdate` per character in table order before the Jolt update: horizontal velocity from the
component, gravity integrated in the air, the vertical component snapped to the ground's while grounded unless
the script set an upward (jump) velocity, `CancelVelocityTowardsSteepSlopes`, stair walking and floor sticking by
`stepHeight`. The pull writes back `velocity`, `ground`, `groundNormal` and `groundEntity`. Authored field
changes rebuild the character in place with the same inner body id.

Moving ground carries a character (`14_physics.md` P20, code sweep F421). A step that starts `ON_GROUND` calls
`UpdateGroundVelocity` and stores the horizontal part of the ground velocity in `row.carriedVelocity`; every
step adds that vector to the velocity it gives Jolt, and `pull_characters` subtracts it again, so the component
holds the character's own velocity and nothing compounds (Jolt keeps the velocity as set, it does not derive
one from the displacement). The vector is kept while the character is `IN_AIR`, which is what makes a jump keep
the platform's speed; a step that starts `ON_STEEP_GROUND` or `NOT_SUPPORTED` clears it, because ground that does
not carry the character must not push it along a slope. `reposition_character` (both teleports: the command and
a new position written into `local_transform`, the latter also being how a parented character follows a moving
parent) clears it and sets `row.skipGroundCarry`, so the next step does not refresh it from a ground state that
still describes the old place; without that a character parented to the platform under its feet would move
twice. Keep any new way of moving a character on that function. A rotation alone written into `local_transform`
is not a move: `teleport_row` applies it with `SetRotation` and the carrying goes on (gameplay turns a character
that way in every step). The skip is horizontal only. The vertical rule of the step after a teleport still reads
the stale ground: a character teleported off a rising lift keeps the lift's vertical velocity, and one parented
to a rising platform under its feet hops. A grounded character on ground that sinks faster than 0.1 m/s passes
the rule's jump test, so `STICK_TO_FLOOR`, not the rule, takes it down. The render scale is `row.lastScale`, refreshed by the push from
the entity's world scale; the capsule is never scaled, so a scale change needs no rebuild.

## 8. Collision meshes

Payload `{u32 vertexCount; u32 indexCount; u32 reserved[2]; f32 positions[][3]; (16-aligned) u32 indices[]}`;
the loader builds the `MeshShape` on the load job; convex hulls are built per (asset, version, convexRadius) in
the shape cache. The importer claims `.obj` by extension only: a glTF is a render model by extension and reaches
this importer when its `.meta` names it (code sweep F137). It reads glTF / GLB (cgltf with the engine's memory and
VFS hooks) baking node transforms, and OBJ (quads, relative indices), welds vertices (`weld_distance`, default 1e-4 m) and emits one
primary asset; the same bytes import byte-identically. glTF input is validated before it is read
(`cgltf_validate`, external buffers against their declared length, per-primitive triangle counts) and mirrored
nodes keep their winding (importer version 2). `on_asset_reloaded` marks every body using the asset for a
rebuild.

## 9. Settings and build options

`physics_settings::from_json` reads the snake_case keys of `project.json` `physics` and the layer table from
`extensions.physics_layers`; invalid settings are logged and replaced by validated defaults at construction.
`penetrationSlop` (default 0.005 m) is an addition to the design: Jolt's 0.02 m leaves a resting box 1.5 cm deep.

Jolt's feature defines travel PUBLIC on the `Jolt` target from `third_party/CMakeLists.txt`: Development gets
`JPH_DEBUG_RENDERER` and `JPH_ENABLE_ASSERTS` (Jolt's own generator expressions cover Debug and Release only),
Distribution neither; `Jolt/Core/Core.h` checks them at `RegisterTypes` through `JPH_VERSION_ID`.
`CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC` (OFF) maps to Jolt's `CROSS_PLATFORM_DETERMINISTIC` and adds
`/fp:precise` or `-ffp-contract=off` to the module's own objects (14 P16); CI never enables it.

## 10. Threading model

`attach`, `detach`, `push_transforms`, `step`, `collect_contact_events` and the command queue are owner-thread
only (the thread that constructed the world; asserted, `attach` and the commands return `WRONG_THREAD`). A
body command reads the target's `rigid_body` and `character_controller` at the call, so the system that sends it
declares both as reads. `pull_transforms` with a `system_context` runs its chunk pass on the job system; the queries run from any thread
while none of `push_transforms`, `step` and `pull_transforms` is in progress (Jolt's `NarrowPhaseQuery` locks per
body; the row tables are not locked), which in the engine means outside the PHYSICS_STEP and FIXED_END phases.
Structural ECS changes on the owner thread run the physics observers, which edit the same tables, so queries from
other threads must not overlap those either. The contact listener runs on Jolt's job threads and writes only to the
buffer of `job_system::thread_index()`.

## 11. Tests

`tests/physics/` (`conjure_physics_tests`, headless, no Jolt header): one `test_<header>.cpp` per public header
plus the design's topic files (`test_layers`, `test_rigid_body`, `test_colliders`, `test_fixed_step`,
`test_component_sync`, `test_physics_stack`, `test_determinism`). `physics_test_fixture.h` builds a registry,
world and attached physics world with small budgets and runs push / step / pull / collect; `for_each_job_mode`
runs a body with 0 and 4 workers. Spawn helpers and `trigger_tally` live in the fixture; a helper two test files
need goes there, never into both. The free-fall and `collisionSteps` tests compare against Jolt's semi-implicit
Euler closed form; the stack test allows 2 cm at the top (Jolt's solver residual); the GLB and OBJ fixtures of
`test_collision_mesh_asset.cpp` are generated in memory. `test_physics_world.cpp` holds the command tables of
P18: every command is sent to a body of every kind, once existing (queued or `WRONG_BODY_KIND`) and once
pending (accepted, the inapplicable ones dropped and counted), `character_move` included with its own code
(`NOT_A_CHARACTER`); a new command or kind extends its `command` list and `spawn_each_kind`.

```
python tools/build.py --build-dir build/mod_physics --target conjure_physics_tests --test -R physics
```

## 12. Open items

- `core::jobs`: an allocation-free fire-and-forget `schedule` (a counter owned by the caller) would let the
  adapter drop the `parallel_for` helper trick.
- The Jolt-free unit test of the job adapter (`14_physics.md` 9 step 2) does not exist: the physics tests include
  no Jolt header, so it needs a test target with Jolt's include paths. Whole-world steps with 0 and 4 workers
  (`test_determinism.cpp`) cover the adapter meanwhile.
- `tests/benchmarks/bench_physics.cpp` (`physics.step_10k_active`, `physics.step_10k_sleeping`,
  `physics.step_10k_sleeping_1k_triggers`, `physics.sync_10k`, `physics.step_2k_settling`) and
  `bench_full_frame_10k.cpp` (`engine.full_frame_10k_physics`) hold the physics scenarios; the raycast batch
  scenario of the design is not written yet, and the `[bench]` / `[slow]` cases in `tests/physics` cover the
  allocation-free behaviour.
- The pull writes `local_transform` of bodies directly; a "physics wrote this" flag on `scene::transform_state`
  would let the transform pass skip redundant dirtying.
- `physics_world::step - 600 empty steps allocate nothing` has been seen to fail once under heavy parallel load
  on the CI machine class (an allocation inside the job system's block pool under contention); it passes alone.

## Related

- design record: [`14_physics.md`](../../design/14_physics.md), [`09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md)
- 📘 API: [Physics](../../api/physics/index.md)
- 🛠 Handbook: [ECS module](ecs.md), [Scene module](scene.md), [Assets module](assets.md)
