# The physics world and queries
<!-- doc-template:wiki -->
[API](../index.md) → [Physics](index.md)

<!-- doc-content:purpose:start -->
How to create and step the `physics_world`, push it around with forces, impulses and teleports, read collision
events, cast rays and shapes, tune it and draw it. Headers: `conjure/physics/physics_world.h`,
`conjure/physics/physics_settings.h`, `conjure/physics/queries.h`, `conjure/physics/contact_events.h` and
`conjure/physics/physics_debug_draw.h`. Everything is in `namespace conjure::physics`.
<!-- doc-content:purpose:end -->

|---|---|
| Name | The physics world and queries |
| Module | Physics |
| `#include` | `conjure/physics/contact_events.h`, `conjure/physics/physics_debug_draw.h`, `conjure/physics/physics_settings.h`, `conjure/physics/physics_world.h`, `conjure/physics/queries.h` |
| Engineering Handbook | [Engineering handbook](../../handbook/engine/physics.md) |

## Syntax

<!-- doc-content:syntax:start -->
<a id="quick-example"></a>

```cpp
#include "conjure/physics/physics_world.h"

using namespace conjure;

void report_hits(const physics::physics_world &physics, ecs::entity player) {
    physics::query_filter filter = {};
    filter.ignore = player;                                        // never hit the caster
    filter.layers = physics::physics_layer_mask::all().without(3);  // skip layer 3 (debris)
    const math::ray forward = {math::vec3(0.0f, 1.0f, 0.0f), math::vec3(0.0f, 0.0f, -1.0f)};
    const std::optional<physics::raycast_hit> hit = physics.raycast(forward, 50.0f, filter);
    if (hit.has_value()) {
        CONJURE_LOG_INFO(LOG_PHYSICS, "hit {} at {} m", hit->entity, hit->distance);
    }

    for (const physics::contact_event &event : physics.contact_events()) {
        if (event.kind == physics::contact_kind::ENTER && (event.a == player || event.b == player)) {
            CONJURE_LOG_INFO(LOG_PHYSICS, "player touched something with impulse {}", event.impulse);
        }
    }
}
```
<!-- doc-content:syntax:end -->

## At a glance

<!-- doc-content:at_a_glance:start -->
| You want to... | Use |
|---|---|
| Create the world | `physics_world(settings, jobs)` then `attach(world)`; `detach()` or destruction removes every body and the `scene::interpolated_transform` physics added |
| Step by hand (tools, tests) | `push_transforms(dt)`, `step(dt)`, `pull_transforms()`, `collect_contact_events()` |
| Push a body | `add_force`, `add_force_at`, `add_impulse`, `add_impulse_at`, `add_torque`, `add_angular_impulse` |
| Set a velocity | `set_linear_velocity`, `set_angular_velocity` |
| Move a body instantly / smoothly | `teleport(entity, pose)` / `move_kinematic(entity, pose)` (kinematic bodies that do not follow their transform) |
| Sleep and wake | `wake`, `sleep`, `is_sleeping` |
| Change gravity | `set_gravity(vec3)`, `gravity()` |
| Move a character | `character_move(entity, velocity)`, `character_ground(entity)` |
| Read collisions | `contact_events()` after `collect_contact_events()` |
| Cast a ray | `raycast(ray, maxDistance, filter)`, `raycast_all(ray, maxDistance, out, filter)` |
| Find what overlaps a volume | `overlap_sphere(center, radius, out, filter)`, `overlap_box(center, halfExtents, rotation, out, filter)` |
| Sweep a shape | `shape_cast(shape, start, direction, filter)` |
| Read a body's pose or the world's bounds | `body_pose(entity)`, `bounds()` |
| Counters | `stats()`, `body_count()`, `active_body_count()` |
| Draw it | `debug_draw(sink, flags)` with your `physics_debug_draw` implementation |
<!-- doc-content:at_a_glance:end -->

## Variables and Properties

<!-- doc-content:variables:start -->
Types, fields, and state are described in the reference sections below.
<!-- doc-content:variables:end -->

<!-- doc-content:body:start -->
## Creating and stepping

`physics_world(settings, jobs)` builds the Jolt system from validated `physics_settings` (call `validate()`
first; an invalid value is clamped with one ERROR) and runs Jolt's jobs on the engine `job_system` (0 workers =
everything inline, same results). `attach(ecsWorld)` installs the component observers and queues every entity
that already carries a body component; it fails with `ALREADY_ATTACHED`, with `INVALID_STATE` when the registry
lacks the scene or physics components, and with `WRONG_THREAD` (after an assert) off the thread that built the
world. `detach()` removes the observers, destroys every body and removes the `scene::interpolated_transform`
physics added to dynamic and kinematic bodies and characters (one you added to a static body stays); call it
while the ECS world is not locked.

Each fixed step is four calls, which the four systems `register_systems` adds make for you:

| Call | What it does |
|---|---|
| `push_transforms(dt, deferred = nullptr)` | creates, rebuilds and destroys bodies from the components, re-poses teleported ones, applies the queued commands |
| `step(dt)` | runs the characters and `PhysicsSystem::Update` |
| `pull_transforms(context = nullptr)` | writes poses back into `local_transform` (and `scene::interpolated_transform`), velocities and sleep state into the components; parallel over chunks when given a system context |
| `collect_contact_events()` | merges and sorts the step's contacts into `contact_events()` |

> [!WARNING]
> Every command and `attach` / `push` / `step` / `collect` belong to the thread that owns the ECS world. The
> queries run from any thread, but not while `push_transforms`, `step` or `pull_transforms` is in progress
> (`STEP_IN_PROGRESS` asserts and the query yields nothing); in the engine that means outside the `PHYSICS_STEP`
> and `FIXED_END` phases. Structural ECS changes on the owner thread (adding or removing body components or
> `scene::parent`, destroying a body entity) run the physics observers, which edit the same tables, so queries
> from other threads must not overlap them either.

## Commands

Forces, impulses, velocities, teleports, `move_kinematic`, `wake`, `sleep`, `set_gravity` and `character_move`
are queued and applied in the next `push_transforms`; a command on an entity whose body is still pending is
kept for one push. `teleport` on a dynamic body also resets its interpolation so rendering does not blend across
the jump.

A command is refused at the call, and never queued, for the first of these that holds:

| Error | When |
|---|---|
| `WRONG_THREAD` (after an assert) | the call is not on the thread that built the world |
| `INVALID_ARGUMENT` (a core error) | a vector or rotation holds a NaN or an infinity |
| `NO_BODY` | the entity has no body and none is pending (`character_move` reports `NOT_A_CHARACTER`) |
| `WRONG_BODY_KIND` | the entity has a body, but not of a kind the command acts on (`character_move` reports `NOT_A_CHARACTER`) |
| `COMMAND_QUEUE_FULL` | `physics_settings::maxQueuedCommands` commands are already queued |

Which kind of body takes which command:

| Command | Static (also colliders without a `rigid_body`) | Kinematic | Kinematic with `KINEMATIC_MOVES_WITH_TRANSFORM` | Dynamic | Character |
|---|---|---|---|---|---|
| `add_force`, `add_force_at`, `add_impulse`, `add_impulse_at`, `add_torque`, `add_angular_impulse` | refused | refused | refused | yes | refused |
| `set_linear_velocity`, `set_angular_velocity` | refused | yes | yes, for one step | yes | refused |
| `move_kinematic` | refused | yes | refused | refused | refused |
| `teleport` | yes | yes | yes | yes | yes |
| `wake`, `sleep` | succeeds, does nothing | yes | yes | yes | succeeds, does nothing |
| `character_move` | `NOT_A_CHARACTER` | `NOT_A_CHARACTER` | `NOT_A_CHARACTER` | `NOT_A_CHARACTER` | yes |

A kinematic body that follows its transform is moved back to its `local_transform` by every push, so a velocity
set on it lasts one step; move it by writing `local_transform`.

The kind is read from the entity's components at the call (`character_controller`, else `rigid_body::motionType`
and its flags, else colliders alone), not from the body the last push left. The next push applies those
components before the commands, so you can set `motionType = DYNAMIC` and call `add_impulse` in the same frame.
The call therefore reads the target's `rigid_body` and `character_controller`: a system that sends body commands
declares both as reads (or is `EXCLUSIVE`), so the scheduler keeps it apart from a system that writes them.

> [!NOTE]
> An entity whose body is still pending (its components were added since the last push, or its colliders or
> collision assets are not there yet) has no kind to refuse by, so every command on it is accepted. The push
> that creates the body applies the command, or drops it and counts it in `physics_stats::droppedCommands` when
> the new body cannot take it. The same late, counted drop meets a command whose entity changed kind between
> the call and the push. A command whose entity still has no body at the second push after the call is dropped
> and counted too.

## Contact events

`collect_contact_events()` folds the step's raw contacts into one `contact_event` per body pair, sorted by body
pair, so the order is the same whatever the worker count: `a` and `b` (the entities), `point` and `normal`
(world space, from `a` towards `b`, zero on exits), `impulse` (estimated on `ENTER` when
`estimateContactImpulses` is on), `kind` (`ENTER`, `STAY`, `EXIT`, `TRIGGER_ENTER`, `TRIGGER_EXIT`),
`contactCount`, `layerA`, `layerB`. The span lives until the next `push_transforms`; destroying a body in contact
synthesises its exits. The engine feeds this span to scripts one fixed step later (`OnCollisionEnter` and
friends).

> [!WARNING]
> An `EXIT` or `TRIGGER_EXIT` synthesised because an entity was destroyed still names that entity, which is no
> longer alive: check `world::is_alive` before touching `a` or `b`. The id keeps its generation, so it never
> matches an entity that later reuses the slot, and no later event references it. Consumers keyed on entities (a
> trigger zone counting its occupants) use it to clean up.

> [!NOTE]
> Sleep ends a solid contact, but not a visit to a static trigger. A pair of solid bodies is reported only while
> one of the two is awake: when a box resting on static ground falls asleep the pair reports `EXIT`, and `ENTER`
> again when the box wakes, although the two never parted. Clear `ALLOW_SLEEPING` on a body whose contacts must
> stay reported. A static trigger goes on holding a body that falls asleep inside it and reports
> `TRIGGER_EXIT` only when the body leaves; kinematic and dynamic triggers are described under
> [Triggers](bodies-and-colliders.md#triggers).

## Queries

A `query_filter` has `layers` (a `physics_layer_mask`, default all), `includeTriggers` (off), `includeStatic` (on)
and `ignore` (one entity, usually the caster).

| Query | Result |
|---|---|
| `raycast(ray, maxDistance, filter)` | the closest `raycast_hit {entity, point, normal, distance, fraction, layer, isTrigger}` |
| `raycast_all(ray, maxDistance, out, filter)` | the nearest `out.size()` hits, sorted by distance; returns the count |
| `overlap_sphere(center, radius, out, filter)` / `overlap_box(center, halfExtents, rotation, out, filter)` | the entities whose bodies overlap, one per body, at most `out.size()` |
| `shape_cast(shape, start, direction, filter)` | the closest `shape_cast_hit {entity, point, normal, distance, fraction, penetrationDepth, layer}`; `shape` is a `sphere_shape_desc`, `box_shape_desc` or `capsule_shape_desc` and `direction` includes the length |

Queries never allocate: you pass the span they fill. A ray is at most 1e6 m long (`maxDistance` is clamped, so
an infinite one means unlimited), and a ray or sweep whose origin, rotation or direction is not finite hits
nothing.

## Settings

`physics_settings` is what `project.json` `"physics"` authors (snake_case keys through `from_json(physicsSection,
layersSection)`): `gravity`, the Jolt sizes (`maxBodies`, `maxBodyPairs`, `maxContactConstraints`,
`tempAllocatorBytes`, `maxJobs`, `maxBarriers`), `collisionSteps`, the budgets (`maxContactEventsPerStep`,
`maxQueuedCommands`, `maxCharacters`), `deterministic`, sleeping (`allowSleeping`, `timeBeforeSleepSeconds`,
`pointVelocitySleepThreshold`), solver iterations (`numVelocitySteps`, `numPositionSteps`),
`speculativeContactDistance`, `penetrationSlop`, `estimateContactImpulses` and the `layers` table. `validate()`
names the offending field (`INVALID_SETTINGS`).

## Debug drawing

Implement `physics_debug_draw` (`line(from, to, rgba)`, `triangle(a, b, c, rgba)` and optionally `text(position, text, rgba, height)`) and call
`physics_world::debug_draw(sink, flags)` with `physics_debug_draw_flags` (`SHAPES`, `WIREFRAME`,
`BOUNDING_BOXES`, `CENTER_OF_MASS`, `VELOCITIES`, `SLEEP_STATE`, `CHARACTERS`, `CONTACTS`). Nothing is drawn
while a step is in progress. The renderer's `debug_draw_buffer` is the natural sink in the engine.
<!-- doc-content:body:end -->

## Functions

<!-- doc-content:functions:start -->
<a id="api-summary"></a>

| Signature (short) | Purpose |
|---|---|
| `physics_world(const physics_settings &, core::jobs::job_system &)` | construction (never fails) |
| `result<void> attach(ecs::world &)`, `void detach()`, `bool is_attached() const` | ECS binding; `detach` also removes the `interpolated_transform` physics added |
| `void push_transforms(f32 dt, ecs::command_buffer *deferred = nullptr)`, `void step(f32 dt)`, `void pull_transforms(ecs::system_context * = nullptr)`, `void collect_contact_events()` | the fixed step |
| `span<const contact_event> contact_events() const` | last step's contacts |
| `bool has_body(entity) const` | the body exists (false while it is pending) |
| `result<void> add_force(entity, const vec3 &)`, `add_force_at(entity, const vec3 &force, const vec3 &point)`, `add_impulse(...)`, `add_impulse_at(...)`, `add_torque(...)`, `add_angular_impulse(...)` | queued forces (dynamic bodies) |
| `result<void> set_linear_velocity(entity, const vec3 &)`, `set_angular_velocity(entity, const vec3 &)` | queued velocities (dynamic and kinematic bodies) |
| `result<void> teleport(entity, const math::transform &)`, `move_kinematic(entity, const math::transform &)` | poses (`teleport`: every kind; `move_kinematic`: kinematic bodies that do not follow their transform) |
| `result<void> set_gravity(const vec3 &)`, `vec3 gravity() const` | gravity |
| `result<void> wake(entity)`, `sleep(entity)`, `bool is_sleeping(entity) const` | sleeping (`wake` and `sleep` are no-ops on static bodies and characters; `is_sleeping` is true for every static body) |
| `result<void> character_move(entity, const vec3 &velocity)`, `optional<ground_state> character_ground(entity) const` | characters; the velocity is the character's own, [moving ground carries it](bodies-and-colliders.md#the-character-controller) |
| `optional<raycast_hit> raycast(const ray &, f32 maxDistance, const query_filter & = {}) const` | rays |
| `u32 raycast_all(const ray &, f32, span<raycast_hit> out, const query_filter & = {}) const` | the nearest hits |
| `u32 overlap_sphere(const vec3 &, f32, span<entity> out, const query_filter & = {}) const`, `u32 overlap_box(const vec3 &, const vec3 &, const quat &, span<entity> out, const query_filter & = {}) const` | overlaps |
| `optional<shape_cast_hit> shape_cast(const cast_shape_desc &, const math::transform &start, const vec3 &direction, const query_filter & = {}) const` | sweeps |
| `void on_asset_reloaded(assets::asset_id)`, `void collect_shapes()` | asset lifetime |
| `const physics_settings &settings() const`, `const physics_layer_table &layers() const`, `physics_stats stats() const`, `u32 body_count() const`, `u32 active_body_count() const` | introspection; `active_body_count()` and `physics_stats::activeBodies` count the awake dynamic and kinematic bodies (characters' inner bodies among them), no static body and so no static trigger |
| `optional<math::transform> body_pose(entity) const`, `math::aabb bounds() const` | poses and bounds |
| `void debug_draw(physics_debug_draw &, physics_debug_draw_flags) const` | drawing |
| `result<void> validate() const` | invariants (tests) |
| `result<void> physics_settings::validate() const`, `static result<physics_settings> physics_settings::from_json(const json::value &physics, const json::value &layers)` | settings |
<!-- doc-content:functions:end -->

------------------------------------------

**See also:**

<!-- doc-content:related:start -->
- 📘 API: [Bodies, colliders and characters](bodies-and-colliders.md), [Jobs and threading](../core/jobs.md)
- 🛠 Handbook: [Physics module](../../handbook/engine/physics.md)
<!-- doc-content:related:end -->

<!-- doc-legacy-anchors:start -->
<a id="see-also"></a>
<!-- doc-legacy-anchors:end -->
