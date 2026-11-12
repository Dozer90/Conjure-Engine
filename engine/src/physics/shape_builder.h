#pragma once

#include "physics/jolt_include.h"
#include "physics/shape_cache.h"

#include "conjure/physics/components.h"

#include "conjure/core/math/glm_types.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/entity.h"

#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

// Collider components to one Jolt shape (docs/design/14_physics.md 3.3): leaves in a fixed order (box,
// sphere, capsule, convex, mesh), a single leaf at the entity origin stays the leaf, anything else
// becomes a StaticCompoundShape; leaves are shared through the shape_cache, the world scale wraps the
// result in a ScaledShape. Every collider problem degrades with a WARN (the body is built without the
// offending collider). A convex or mesh collider whose asset is not loaded sets pendingAsset instead: nothing
// is built or logged until the asset is there (14_physics.md 3.3, unloaded asset_refs keep the body pending).

namespace conjure::physics {

    /// <summary>
    /// The colliders of one entity (null when absent) and the context the build needs.
    /// </summary>
    struct shape_build_input {
        ecs::entity entity = {};
        motion_type motion = motion_type::STATIC;
        math::vec3 scale = math::vec3(1.0f);
        const box_collider *box = nullptr;
        const sphere_collider *sphere = nullptr;
        const capsule_collider *capsule = nullptr;
        const convex_collider *convex = nullptr;
        const mesh_collider *mesh = nullptr;
    };

    /// <summary>
    /// What build_shape produced.
    /// </summary>
    struct shape_build_result {
        JPH::RefConst<JPH::Shape> shape = {};     // null when no collider could be built
        bool hasCollider = false;                 // at least one collider component is present
        // A convex / mesh collider that would be built references an asset that is not loaded: the shape is
        // null and nothing else was built or logged.
        bool pendingAsset = false;
        bool isTrigger = false;
        u64 leafKey = 0;                          // cache key of a single shared leaf, else 0
        core::uuid convexAsset = {};
        core::uuid meshAsset = {};
    };

    /// <summary>
    /// Builds the shape for the colliders. The result's shape is null when nothing could be built
    /// (no collider, every collider failed, or an asset is pending, which is checked before anything is built).
    /// </summary>
    [[nodiscard]] shape_build_result build_shape(const shape_build_input &input, shape_cache &cache);
}
