# Physics

📘 **Public API wiki** · 🛠 Handbook: [Physics module](../../handbook/modules/physics.md)

Rigid bodies, colliders, a character controller, layers, collision events and spatial queries, simulated by
Jolt Physics behind a small engine-side API. You describe bodies with components on scene entities; the
`physics_world` creates the Jolt bodies for you, steps them at the fixed rate and writes the results back into
the entities' transforms. Everything on these pages is in `namespace conjure::physics`; no Jolt type appears in
any header.

## The mental model in five sentences

1. An entity becomes a body when it carries a `rigid_body` (or a `character_controller`) plus one or more
   colliders (`box_collider`, `sphere_collider`, `capsule_collider`, `convex_collider`, `mesh_collider`). Colliders
   without a `rigid_body` make a static body.
2. The world is stepped four times per fixed step: `push_transforms` (create, rebuild and re-pose bodies from the
   components, apply queued commands), `step` (Jolt), `pull_transforms` (write poses and velocities back into
   `local_transform` and the components) and `collect_contact_events`. The engine's four systems do this for
   you; a tool or a test calls them by hand.
3. Every gameplay mutation (`add_force`, `set_linear_velocity`, `teleport`, `wake`, `character_move`, ...) is a
   queued command applied at the next push, so a frame of gameplay sees one consistent state. A command the
   entity's kind of body cannot take (a force on a static body) is refused at the call with `WRONG_BODY_KIND`.
4. Contacts come out of `contact_events()` as a span sorted by body pair: one `ENTER`, `STAY` or `EXIT` (or
   `TRIGGER_ENTER` / `TRIGGER_EXIT`) per pair per step, with point, normal and impulse.
5. Queries (`raycast`, `overlap_sphere`, `overlap_box`, `shape_cast`) are entity-keyed, layer-filtered and
   allocation-free: you pass the span to fill.

Layers are 32 named groups from `project.json` (`physics_layer_table`); a `query_filter` and every body
reference them by index or mask.

## Where to go next

| Page | Read it when you want to... |
|---|---|
| [Bodies, colliders and characters](bodies-and-colliders.md) | give an entity a body and shapes, choose motion types and flags, author materials, set up layers, use the character controller, use collision mesh assets |
| [The physics world and queries](physics-world-and-queries.md) | create and step the world, apply forces and impulses, teleport and move kinematics, read contact events, cast rays and shapes, tune `physics_settings`, draw debug shapes |

## Headers

| Header | What it holds |
|---|---|
| `conjure/physics/components.h` | `rigid_body`, `motion_type`, `rigid_body_flags`, `box_collider`, `sphere_collider`, `capsule_collider`, `convex_collider`, `mesh_collider`, `physics_material` |
| `conjure/physics/character_controller.h` | `character_controller`, `ground_state`, `character_flags` |
| `conjure/physics/physics_layers.h` | `physics_layer_mask`, `physics_layer_table`, `MAX_PHYSICS_LAYERS` |
| `conjure/physics/physics_settings.h` | `physics_settings` (`validate()`, `from_json()`) |
| `conjure/physics/physics_world.h` | `physics_world`, `physics_stats` |
| `conjure/physics/queries.h` | `query_filter`, `raycast_hit`, `shape_cast_hit`, the shape descriptions |
| `conjure/physics/contact_events.h` | `contact_event`, `contact_kind` |
| `conjure/physics/collision_mesh_asset.h` | `collision_mesh_asset` (`conjure.physics.collision_mesh`) |
| `conjure/physics/physics_debug_draw.h` | `physics_debug_draw` (the sink interface), `physics_debug_draw_flags` |
| `conjure/physics/register.h` | `register_components()`, `register_systems()`, `register_asset_types()`, the four system names |

## Quick tour

```cpp
#include "conjure/physics/physics_world.h"
#include "conjure/physics/register.h"

using namespace conjure;

core::result<void> drop_a_crate(ecs::world &world, core::jobs::job_system &jobs, ecs::entity crate) {
    physics::physics_settings settings = {};
    CONJURE_TRY(settings.validate());
    physics::physics_world physics(settings, jobs);
    CONJURE_TRY(physics.attach(world));                    // scene and physics components registered already

    CONJURE_TRY(world.add<physics::rigid_body>(crate));     // DYNAMIC, 1 kg, sleeping allowed
    physics::box_collider shape = {};
    shape.halfExtents = math::vec3(0.5f);
    CONJURE_TRY(world.add<physics::box_collider>(crate, shape));

    const f32 step = 1.0f / 60.0f;
    for (u32 index = 0; index < 60; ++index) {              // what physics.step and friends do per fixed step
        physics.push_transforms(step);
        physics.step(step);
        physics.pull_transforms();
        physics.collect_contact_events();
    }
    return core::result<void>::success();                  // local_transform of the crate has fallen
}
```

> [!IMPORTANT]
> Bodies need the scene's `local_transform` to exist on the entity (`scene::register_components` and the physics
> `register_components` both before `attach`), and bodies are created at the next `push_transforms`, not when you
> add the component. `physics_world::has_body(entity)` tells you when it exists.

## Local terms

| Term | Meaning |
|---|---|
| Fixed step | the physics tick; the engine runs the four physics systems once per fixed step in the `PHYSICS_STEP` and `FIXED_END` phases |
| Motion type | `STATIC` (never moves), `KINEMATIC` (moved by you, pushes others), `DYNAMIC` (simulated) |
| Layer | one of 32 named groups; the `physics_layer_table` says which pairs collide |
| Trigger | a collider with `isTrigger`: reports `TRIGGER_ENTER` / `TRIGGER_EXIT` and never pushes |
| Interpolation | `scene::interpolated_transform` holds the last two fixed-step poses so rendering can blend between them; physics adds and writes it |

## See also

- 📘 API: [Scenes and the hierarchy](../scene/scenes-and-hierarchy.md), [Jobs and threading](../core/jobs.md), [Math](../core/math.md)
- 🛠 Handbook: [Physics module](../../handbook/modules/physics.md)
