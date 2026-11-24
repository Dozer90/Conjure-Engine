#pragma once

#include "conjure/physics/physics_layers.h"

#include "conjure/core/math/glm_types.h"
#include "conjure/core/types.h"
#include "conjure/ecs/entity.h"

#include <variant>

// Query inputs and outputs (docs/design/14_physics.md 2.5, 3.8). Queries return entities, never Jolt
// ids, and write into caller memory.

namespace conjure::physics {

    /// <summary>
    /// What a query may hit.
    /// </summary>
    struct query_filter {
        physics_layer_mask layers = physics_layer_mask::all();
        bool includeTriggers = false;                     // sensors are skipped unless set
        bool includeStatic = true;                        // false drops the NON_MOVING broad phase
        ecs::entity ignore = ecs::NULL_ENTITY;            // typically the caster

        [[nodiscard]] bool operator==(const query_filter &) const = default;
    };

    /// <summary>
    /// One ray hit.
    /// </summary>
    struct raycast_hit {
        ecs::entity entity = {};
        math::vec3 point = math::vec3(0.0f);
        math::vec3 normal = math::vec3(0.0f);
        f32 distance = 0.0f;                              // metres along the ray
        f32 fraction = 0.0f;                              // distance / maxDistance (clamped to 1e6 m)
        u8 layer = 0;
        bool isTrigger = false;

        [[nodiscard]] bool operator==(const raycast_hit &) const = default;
    };

    /// <summary>
    /// The closest hit of a shape cast.
    /// </summary>
    struct shape_cast_hit {
        ecs::entity entity = {};
        math::vec3 point = math::vec3(0.0f);              // contact point on the hit body
        math::vec3 normal = math::vec3(0.0f);             // points away from the hit body
        f32 distance = 0.0f;                              // metres the shape travelled along the direction
        f32 fraction = 0.0f;                              // distance / maxDistance (clamped to 1e6 m)
        f32 penetrationDepth = 0.0f;                      // 0 from a clear start, approximate (often 0) if overlapping
        u8 layer = 0;

        [[nodiscard]] bool operator==(const shape_cast_hit &) const = default;
    };

    struct sphere_shape_desc {
        f32 radius = 0.5f;

        [[nodiscard]] bool operator==(const sphere_shape_desc &) const = default;
    };

    struct box_shape_desc {
        math::vec3 halfExtents = math::vec3(0.5f);

        [[nodiscard]] bool operator==(const box_shape_desc &) const = default;
    };

    struct capsule_shape_desc {
        f32 radius = 0.5f;
        f32 halfHeight = 0.5f;                            // half the cylinder part; total 2 * (halfHeight + radius)

        [[nodiscard]] bool operator==(const capsule_shape_desc &) const = default;
    };

    /// <summary>
    /// The shape a shape_cast sweeps.
    /// </summary>
    using cast_shape_desc = std::variant<sphere_shape_desc, box_shape_desc, capsule_shape_desc>;
}
