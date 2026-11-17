# Bodies, colliders and characters

📘 **Public API wiki** · 🛠 Handbook: [Physics module](../../handbook/modules/physics.md)

How to make an entity a physical body, which shapes it can have, how layers decide what collides, how the
character controller moves, and how collision meshes come from files. Headers:
`conjure/physics/components.h`, `conjure/physics/character_controller.h`, `conjure/physics/physics_layers.h`,
`conjure/physics/collision_mesh_asset.h` and `conjure/physics/register.h`. Everything is in `namespace
conjure::physics`.

## At a glance

| You want to... | Use |
|---|---|
| Register the physics components once | `register_components(registry)` (after `scene::register_components`) |
| A falling box | `rigid_body` (default `DYNAMIC`) + `box_collider` on an entity with a `scene::local_transform` |
| A floor or wall | a collider without a `rigid_body` (static), or `rigid_body` with `motion_type::STATIC` |
| A moving platform you drive | `rigid_body` with `motion_type::KINEMATIC` and `KINEMATIC_MOVES_WITH_TRANSFORM`, then edit `local_transform` |
| A sensor volume | any collider with `isTrigger = true` |
| Friction and bounce | `physics_material {friction, restitution}` on the entity |
| A convex hull or a triangle mesh from a file | `convex_collider` / `mesh_collider` with an `asset_ref<collision_mesh_asset>` |
| A walking player | `character_controller` (capsule) and `physics_world::character_move(entity, velocity)` each step |
| Name the layers and who collides with whom | `physics_layer_table` from `project.json` `extensions.physics_layers` |
| Add the four physics systems to the scheduler | `register_systems(scheduler, world)` |
| Register the collision mesh asset type and importer | `register_asset_types(types, importers)` |

## Quick example

```cpp
#include "conjure/physics/components.h"

using namespace conjure;

core::result<void> make_ball(ecs::world &world, ecs::entity ball, u8 ballLayer) {
    physics::rigid_body body = {};
    body.mass = 0.5f;                                    // kg; 0 would derive it from the shape
    body.layer = ballLayer;
    body.flags = physics::rigid_body_flags::ALLOW_SLEEPING | physics::rigid_body_flags::CONTINUOUS_COLLISION;
    CONJURE_TRY(world.add<physics::rigid_body>(ball, body));

    physics::sphere_collider shape = {};
    shape.radius = 0.25f;
    CONJURE_TRY(world.add<physics::sphere_collider>(ball, shape));

    physics::physics_material rubber = {};
    rubber.restitution = 0.8f;
    return world.add<physics::physics_material>(ball, rubber);
}
```

The body appears at the next `push_transforms`; from then on the `rigid_body` fields marked runtime
(`linearVelocity`, `angularVelocity`, `isSleeping`) are written back every step, and editing the authored ones
(mass, damping, flags, layer, motion type) is picked up by the next push.

Dynamic and kinematic bodies and characters get a `scene::interpolated_transform`, which the renderer blends
between the last two fixed steps. Physics adds it (starting at the pose the entity has when its body component
arrives), keeps it current and removes it again; a static body never has one, and a body that falls asleep
holds still in it.

## The components

All components are plain reflected structs registered under `"conjure.physics.<struct name>"` (so
`physics_material` is `conjure.physics.physics_material`); the C# mirrors share their layout.

> [!NOTE]
> `physics_material` was registered as `conjure.physics.material` before. A scene saved with that key keeps the
> old entry in `unresolved_components` when it loads (saved back untouched, but no longer a material on the
> body); add the material again to restore it.

| Component | Fields | Notes |
|---|---|---|
| `rigid_body` | `motionType`, `layer`, `flags`, `mass`, `linearDamping`, `angularDamping`, `gravityFactor`, `maxLinearVelocity`, `linearVelocity`, `angularVelocity`, `isSleeping` | 52 bytes; mass 0 derives from the shape volume (a shape without volume, a mesh, gets 1 kg) |
| `box_collider` | `halfExtents`, `offset`, `rotation`, `isTrigger` | entity space |
| `sphere_collider` | `radius`, `offset`, `isTrigger` | |
| `capsule_collider` | `radius`, `halfHeight`, `offset`, `rotation`, `isTrigger` | along the entity's Y axis; total height `2 * (halfHeight + radius)` |
| `convex_collider` | `mesh` (`asset_ref<collision_mesh_asset>`), `offset`, `rotation`, `convexRadius`, `isTrigger` | the hull of the asset's points |
| `mesh_collider` | `mesh`, `isTrigger` | the asset's triangles; `STATIC` and `KINEMATIC` bodies only |
| `physics_material` | `friction` (0.5), `restitution` (0) | combined as `sqrt(f1 * f2)` and `max(r1, r2)` |
| `character_controller` | see below | |

An entity may carry several colliders; they become one compound shape scaled by the entity's world scale.
`rigid_body_flags`: `CONTINUOUS_COLLISION`, `KINEMATIC_MOVES_WITH_TRANSFORM`, `ALLOW_SLEEPING` (default),
`START_ASLEEP`, `LOCK_ROTATION`, `LOCK_ROTATION_X/Y/Z`, `LOCK_TRANSLATION_X/Y/Z`, `USE_GYROSCOPIC_FORCE`.

> [!WARNING]
> A `rigid_body` without any collider is a pending body (`physics_stats::bodiesWithoutShape`) until a collider
> arrives, and a `rigid_body` next to a `character_controller` logs `CONFLICTING_BODY_COMPONENTS` and the
> character wins. A `mesh_collider` on a `DYNAMIC` body is ignored with `UNSUPPORTED_SHAPE_FOR_MOTION`: use a convex hull.

Bodies follow the scene hierarchy: a body under a moving parent is re-posed when the parent moves, and the pull
writes the body's pose back as a local transform relative to its parent.

## Motion types and kinematics

`motion_type::STATIC` bodies never move (a changed `local_transform` is a teleport). `KINEMATIC` bodies push
dynamic ones and are driven either by your edits of `local_transform` (`KINEMATIC_MOVES_WITH_TRANSFORM`, the
world turns the edit into a smooth `MoveKinematic` over the step and interpolates the rendered pose between your
last two transforms) or by velocities you set
(`physics_world::set_linear_velocity` or `move_kinematic(entity, pose)`). `DYNAMIC` bodies are simulated; write
their pose only through `physics_world::teleport` (an edited `local_transform` of a dynamic body is treated as a
teleport too).

An edit of `local_transform` counts as a teleport once it moves the body by more than 0.1 mm along an axis or
turns it by more than about 0.02 degrees (4e-4 rad); a turn below about 0.01 degrees never counts, and in between
it depends on the axis. A smaller edit is ignored, and the next pull overwrites it for a body the pull writes.
For a body under a parent the position tolerance grows with the size of the coordinates involved (16 float
epsilons of the largest one), so far from the origin an edit must be larger to count.

Each command of the world acts on some kinds of body only: forces and impulses on `DYNAMIC` bodies, velocities on
`DYNAMIC` and `KINEMATIC` ones, `move_kinematic` on a `KINEMATIC` body without `KINEMATIC_MOVES_WITH_TRANSFORM`.
Sent to a body of another kind a command is refused with `WRONG_BODY_KIND`; the table is under
[Commands](physics-world-and-queries.md#commands).

## Triggers

A body whose colliders all set `isTrigger` is a trigger: it pushes nothing, queries skip it unless
`includeTriggers` is on, and its contacts are `TRIGGER_ENTER` and `TRIGGER_EXIT`. A static trigger (a `STATIC`
`rigid_body`, or trigger colliders without a `rigid_body`) reports the dynamic and kinematic bodies and the
characters inside it, never a static body and never another static trigger. It keeps a body that falls asleep
inside it, and reports one that is already asleep when the trigger is created or moved over it: a crate left on
a pressure plate stays on the plate until it really leaves. A kinematic or dynamic trigger also reports the
static bodies it overlaps, and a trigger whose `motionType` you change reports as its new kind from the next
step on (a static trigger turned kinematic enters the static bodies it stands in, and leaves them when it turns
static again). A kinematic or dynamic trigger sees sleeping bodies while it is awake itself; once awake it does
not fall asleep on its own, but one created with `START_ASLEEP` or put to sleep with `sleep` sees awake bodies
only until something wakes it. `START_ASLEEP` means nothing on a static trigger.

> [!NOTE]
> Every static trigger costs the step what an awake body costs, because that is how it sees sleeping bodies.
> `is_sleeping` still answers true for it and `active_body_count()` leaves it out, as for any static body. In
> every other way it is static too: velocities and forces are refused, `includeStatic = false` skips it, and
> nothing but a teleport moves it. A body inside a static trigger keeps one body pair and one cached contact,
> asleep or not, so many bodies resting in trigger volumes count against `maxBodyPairs` and
> `maxContactConstraints`. It takes none of the step's contact records after the one of its `TRIGGER_ENTER`, but
> like every pair in contact it holds one of the `maxContactEventsPerStep` entries of the world's pair table for
> as long as it is inside.

## Layers

`physics_layer_table` holds up to `MAX_PHYSICS_LAYERS` (32) named layers and a collision matrix. The engine builds
it from `project.json`:

```json
"extensions": { "physics_layers": { "names": ["default", "player", "debris"], "ignore": [["debris", "debris"]] } }
```

Everything collides unless ignored. In code: `from_json(section)`, `add_layer(name)`, `set_collides(a, b, on)`,
`collides(a, b)`, `collision_mask(layer)`, `find(name)`, `name(index)`, `count()`, `validate()`, `to_json(writer)`.
A `physics_layer_mask` is the 32-bit set queries filter with: `all()`, `none()`, `only(layer)`, `with(layer)`,
`without(layer)`, `includes(layer)`. Every body carries its layer index in `rigid_body::layer` or
`character_controller::layer`.

## The character controller

`character_controller` is a capsule (`radius`, `height` as the total height, feet at the entity origin) with
`maxSlopeAngleDegrees`, `stepHeight`, `mass` (how hard it pushes dynamic bodies), `maxStrength`, `padding`, a
`layer` and `character_flags` (`STICK_TO_FLOOR`, `WALK_STAIRS`, `APPLY_GRAVITY`, all on by default). You set
`velocity` (or call `physics_world::character_move(entity, velocity)`) before the step; after the pull the
component reports the achieved `velocity`, the `ground` state (`ON_GROUND`, `ON_STEEP_GROUND`, `NOT_SUPPORTED`,
`IN_AIR`), `groundNormal` and `groundEntity`. Gravity is integrated while in the air; an upward velocity you set
while grounded is a jump. Characters have an inner kinematic body, so rays hit them and contacts report them.
A character is not a rigid body for the world's commands: it takes `character_move` and `teleport`, `wake` and
`sleep` succeed and do nothing, and forces, velocities and `move_kinematic` are refused with `WRONG_BODY_KIND`.

`velocity` is the character's own velocity; moving ground carries the character on top of it. A step that
starts with the character `ON_GROUND` adds the horizontal velocity of the body under its feet (a kinematic
platform, however it is driven, or a dynamic body), and the pull takes the same amount out again. So on a
platform a velocity of zero stands still on the platform, what you write is relative to it, and what you read
back is what you wrote, not the speed over the world. In the air the character keeps what the ground last gave
it: a jump from a moving platform keeps the platform's speed, and `velocity` stays relative to that motion until
the character touches ground again. `ON_GROUND` it takes the velocity of the ground it landed on (none on static
ground); on ground that does not carry it (`ON_STEEP_GROUND`, `NOT_SUPPORTED`) the carrying ends at once.
Carrying asks for `ON_GROUND` at the start of the step, not for `APPLY_GRAVITY`: without the flag nothing but
your own downward velocity keeps the character on the ground.

- Only the horizontal part is carried and taken out again. With `APPLY_GRAVITY` the vertical part of a grounded
  character is set to the ground's vertical velocity unless yours is at least 0.1 m/s faster upwards than the
  ground, which is a jump; `velocity.y` then reads the ground's. On a lift that goes up, or down slower than
  0.1 m/s, the character rides and reads the lift's vertical velocity. On a lift that goes down faster, a
  character standing still is that much faster upwards than its ground, so the rule leaves it alone:
  `STICK_TO_FLOOR` steps it down after the lift, one step of the lift's motion behind, with `velocity.y`
  unchanged, and without that flag it falls after the lift. Without `APPLY_GRAVITY` the vertical part is yours
  alone.
- Ground that turns moves the character round with it but does not turn the character. You turn it by writing
  the rotation of its `local_transform`; that leaves the carrying alone.
- A teleport ends the carrying: `teleport`, or a new position written into the character's `local_transform`.
  The step after it adds no horizontal ground velocity, so a character teleported from one spot of a moving
  platform to another slips by one step of the platform's motion. The vertical part is not ended: the step after
  the teleport still takes the character for standing on the ground it left, so one teleported off a rising lift
  keeps the lift's vertical velocity and falls from there.
- A character parented to a moving entity follows its parent as a teleport in every step, and is therefore not
  carried a second time by a parent it stands on while that parent moves sideways or sinks. A parent that rises
  under its feet does move it twice (the parent's motion and the ground's vertical velocity): the character hops
  a few centimetres again and again.

> [!TIP]
> Set the horizontal part of `velocity` every step from the input and leave the vertical part to the controller
> unless you jump; the controller snaps it to the ground's velocity while grounded. Do not add a platform's
> velocity yourself and do not parent the character to the platform or lift it stands on: the step carries it.

## Collision meshes

`collision_mesh_asset` (`conjure.physics.collision_mesh`) is positions plus triangle indices with bounds,
imported from `.obj`, `.gltf` or `.glb` by the importer `register_asset_types` adds (node transforms baked,
vertices welded, one asset per file). An `.obj` is a collision mesh as soon as it is in the project. A glTF is a
render model by default, so name the importer to get collision geometry from one: keep the collision shape in its
own file (`level_collision.glb`) and import it with `asset.import` and
`"importer": "conjure.physics.collision_mesh_importer"`, or call `asset_database::set_importer(id,
physics::COLLISION_MESH_IMPORTER_ID)` in code. `from_geometry(positions, indices)`, `from_payload(bytes)` and
`to_payload()` are the in-code forms; `positions()`, `indices()`, `bounds()`, `vertex_count()` and
`triangle_count()` read it. The loader builds the Jolt shape on the load job, so a `mesh_collider` referencing a
loaded asset creates its body at the next push without a stall; convex hulls are built per asset, version and
`convexRadius` and shared between bodies. A body whose `convex_collider` or `mesh_collider` references an asset
that is not loaded yet waits for it: a new body stays pending (`physics_stats::bodiesWithoutShape`) and an existing
one keeps its current shape, both until the asset is there. When the asset reloads,
`physics_world::on_asset_reloaded(id)` marks every body using it for a rebuild.

The importer validates glTF sources before reading them (accessors and buffer views against their buffers, an
external buffer file against its declared length, every triangle primitive's index count a multiple of three),
so a malformed file is an import failure rather than a crash, and it keeps the triangles of a mirrored node (a
negative scale) facing the way they were authored.

## API summary

| Signature (short) | Purpose |
|---|---|
| `result<void> register_components(ecs::component_registry &)` | the eight physics components, after the scene's |
| `result<void> register_systems(ecs::scheduler &, physics_world &)` | `physics.push_transforms`, `physics.step` (PHYSICS_STEP, main thread), `physics.pull_transforms`, `physics.contact_events` (FIXED_END) |
| `result<void> register_asset_types(assets::asset_type_registry &, assets::importer_registry &)` | `conjure.physics.collision_mesh` and its importer |
| `static result<physics_layer_table> physics_layer_table::from_json(const json::value &)`, `result<void> to_json(json::writer &) const` | layer table I/O |
| `result<u8> physics_layer_table::add_layer(string_id)`, `void set_collides(u8, u8, bool)`, `bool collides(u8, u8) const`, `physics_layer_mask collision_mask(u8) const`, `optional<u8> find(string_id) const`, `string_id name(u8) const`, `u32 count() const`, `result<void> validate() const` | layers |
| `static result<collision_mesh_asset> collision_mesh_asset::from_geometry(span<const vec3>, span<const u32>)`, `from_payload(span<const byte>)`, `vector<byte> to_payload() const` | collision meshes |

## See also

- 📘 API: [The physics world and queries](physics-world-and-queries.md), [Scenes and the hierarchy](../scene/scenes-and-hierarchy.md), [Referencing and loading assets](../assets/referencing-and-loading.md)
- 🛠 Handbook: [Physics module](../../handbook/modules/physics.md)
