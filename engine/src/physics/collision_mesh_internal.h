#pragma once

#include "physics/jolt_include.h"

#include "conjure/physics/collision_mesh_asset.h"

#include "conjure/core/result.h"

#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

// The Jolt side of a collision mesh asset (docs/design/14_physics.md 2.7, P14): the mesh shape is built
// once on the load job and shared by every mesh_collider; convex hulls depend on the collider's convex
// radius and are built by the shape builder.

namespace conjure::physics::detail {

    struct collision_mesh_shapes {
        JPH::RefConst<JPH::Shape> meshShape = {};
    };

    /// <summary>
    /// Builds the JPH::MeshShape of a mesh. SHAPE_CREATE_FAILED with Jolt's message.
    /// </summary>
    [[nodiscard]] core::result<JPH::RefConst<JPH::Shape>> build_mesh_shape(const collision_mesh_asset &mesh);

    /// <summary>
    /// Builds a convex hull of the mesh's positions. SHAPE_CREATE_FAILED with Jolt's message.
    /// </summary>
    [[nodiscard]] core::result<JPH::RefConst<JPH::Shape>> build_convex_shape(
      const collision_mesh_asset &mesh,
      f32 convexRadius
    );
}
