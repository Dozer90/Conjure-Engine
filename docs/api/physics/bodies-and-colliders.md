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

> [!TIP]
> Set the horizontal part of `velocity` every step from the input and leave the vertical part to the controller
> unless you jump; the controller snaps it to the ground's velocity while grounded.

## Collision meshes

`collision_mesh_asset` (`conjure.physics.collision_mesh`) is positions plus triangle indices with bounds,
imported from `.gltf`, `.glb` or `.obj` by the importer `register_asset_types` adds (node transforms baked,
vertices welded, one asset per file). `from_geometry(positions, indices)`, `from_payload(bytes)` and
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
