#pragma once

#include "conjure/core/math/glm_types.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/scene/scene.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/components.h"
#endif

#include <span>
#include <string_view>
#include <vector>

// Fluent world construction for tests and benchmarks (docs/design/08_testing.md 2.3, 4.5): bulk spawns in
// one archetype, the 10,000-entity hierarchy of the propagation scenarios, physics bodies, scripted
// entities and deterministic dirty marking. Every function runs on the owner thread of the world.
//
//   std::vector<ecs::entity> movers = spawn_many(world, 10000, position{}, velocity{1.0f, 0.0f, 0.0f});
//   std::vector<ecs::entity> tree = spawn_hierarchy(engine.active_scene(), hierarchy_shape{});
//   mark_dirty_fraction(engine.active_scene(), tree, 0.1f, environment().seed);

namespace conjure::testing {

    /// <summary>
    /// Shape of spawn_hierarchy's tree: rootCount roots, depth levels of children below them with
    /// childrenPerNode each, padded or trimmed at the last level to totalEntityCount entities.
    /// </summary>
    struct hierarchy_shape {
        u32 rootCount = 100;
        u32 depth = 3;
        u32 childrenPerNode = 4;
        u32 totalEntityCount = 10000;
    };

    /// <summary>
    /// Spawns count entities in one archetype through the command buffer with one flush; every entity
    /// receives a copy of each prototype. The returned ids are in creation order. An unregistered
    /// component type asserts inside the command buffer and the entities are created without it.
    /// </summary>
    template <typename... ComponentTypes>
    [[nodiscard]] std::vector<ecs::entity> spawn_many(
      ecs::world &world,
      u32 count,
      const ComponentTypes &...prototypes
    ) {
        std::vector<ecs::entity> entities = {};
        entities.reserve(count);

        ecs::command_buffer commands(world);
        for (u32 index = 0; index < count; ++index) {
            const ecs::entity spawned = commands.create_entity();
            if (spawned.is_null()) {
                break;
            }
            (commands.add<ComponentTypes>(spawned, prototypes), ...);
            entities.push_back(spawned);
        }
        CONJURE_UNUSED(world.flush(commands));
        return entities;
    }

    /// <summary>
    /// Builds the tree described by 'shape' with scene entities carrying a transform: level 0 holds the
    /// roots (named "root"), each deeper level childrenPerNode children (named "node") per parent. When
    /// the full tree falls short of totalEntityCount the last level is padded with extra children spread
    /// over its parents; when it overshoots, the last level is trimmed (the levels above it never are).
    /// Returns every entity in creation order (roots first, then level by level), exactly
    /// hierarchy_entity_count(shape) of them; empty, with a WARN, when the shape is unusable.
    /// </summary>
    [[nodiscard]] std::vector<ecs::entity> spawn_hierarchy(scene::scene &scene, const hierarchy_shape &shape);

    /// <summary>
    /// The number of entities spawn_hierarchy creates for a shape (after padding or trimming); 0 when the
    /// shape is unusable: no roots, more levels than the hierarchy allows (depth >= MAX_HIERARCHY_DEPTH),
    /// a depth above 0 with zero children per node, or a tree whose spawned entities would not fit a u32.
    /// Never allocates.
    /// </summary>
    [[nodiscard]] u32 hierarchy_entity_count(const hierarchy_shape &shape) noexcept;

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    /// <summary>
    /// A root entity named "box" at 'position' with a unit box collider and a rigid body of the given
    /// motion type (the physics subsystem creates the Jolt body at the next push_transforms). Fails with
    /// the scene's errors or ecs_error::UNKNOWN_COMPONENT when the physics components are not registered.
    /// </summary>
    [[nodiscard]] core::result<ecs::entity> spawn_box_body(
      scene::scene &scene,
      const math::vec3 &position,
      physics::motion_type motion
    );
#endif

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    /// <summary>
    /// A root entity with 'name' carrying one script of the C# class 'scriptType' (full type name, hashed
    /// as the managed side does) with default fields, added through the active script_runtime. Fails with
    /// INVALID_STATE when no runtime is active or the runtime refuses the entry.
    /// </summary>
    [[nodiscard]] core::result<ecs::entity> spawn_script_entity(
      scene::scene &scene,
      std::string_view name,
      std::string_view scriptType
    );
#endif

    /// <summary>
    /// Marks round(fraction * entities.size()) distinct entities, chosen deterministically from 'seed',
    /// as needing propagation by adding the scene's transform_dirty tag in one flush. Entities without a
    /// local_transform or already dirty are skipped (they still count towards the selection). Returns
    /// the entities that were tagged, in entity order.
    /// </summary>
    std::vector<ecs::entity> mark_dirty_fraction(
      scene::scene &scene,
      std::span<const ecs::entity> entities,
      f32 fraction,
      u64 seed
    );
}
