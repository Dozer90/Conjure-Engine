#pragma once

// Umbrella header of the math area: glm aliases and formatters, scalar utilities, transform,
// aabb, plane, frustum, ray, rect and the projection helpers below. Include this (or
// conjure/core/math.h) rather than the individual headers unless compile time matters.

#include "conjure/core/math/aabb.h"
#include "conjure/core/math/frustum.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/math/math_utils.h"
#include "conjure/core/math/plane.h"
#include "conjure/core/math/ray.h"
#include "conjure/core/math/rect.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/types.h"

#include <optional>

namespace conjure::math {

    /*===========================================================================*/
    /*===========================================================================*/
    // Projection and view matrices (right-handed, depth 0..1)

    /// <summary>
    /// Perspective projection with a vertical field of view in radians, mapping -nearPlane to
    /// depth 0 and -farPlane to depth 1 (equals glm::perspective under the engine's glm
    /// configuration). Asserts on a non-positive field of view, aspect or near plane and on
    /// farPlane <= nearPlane.
    /// </summary>
    [[nodiscard]] mat4 perspective(f32 fovYRadians, f32 aspect, f32 nearPlane, f32 farPlane) noexcept;

    /// <summary>
    /// Perspective projection without a far plane: depth tends to 1 at infinity. Asserts on a
    /// non-positive field of view, aspect or near plane.
    /// </summary>
    [[nodiscard]] mat4 infinite_perspective(f32 fovYRadians, f32 aspect, f32 nearPlane) noexcept;

    /// <summary>
    /// Orthographic projection mapping x in [left, right], y in [bottom, top] and -z in
    /// [nearPlane, farPlane] to clip space with depth 0..1. Asserts when any range is empty.
    /// </summary>
    [[nodiscard]] mat4 orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 nearPlane, f32 farPlane) noexcept;

    /// <summary>
    /// A view matrix (world to camera) for a camera at eye looking at target: the inverse of
    /// transform::look_at(eye, target, up).to_matrix(), with the same handling of degenerate
    /// inputs.
    /// </summary>
    [[nodiscard]] mat4 look_at(vec3 eye, vec3 target, vec3 up) noexcept;

    /// <summary>
    /// Converts normalised device coordinates (x, y in [-1, 1], depth in [0, 1]) back to a point
    /// using an inverse view-projection matrix. Returns nullopt when the result is at infinity
    /// (clip w of zero, e.g. depth 1 of an infinite projection).
    /// </summary>
    [[nodiscard]] std::optional<vec3> unproject(
      vec3 normalizedDeviceCoordinates,
      const mat4 &inverseViewProjection
    ) noexcept;

    /// <summary>
    /// Projects a world point to normalised device coordinates (x, y in [-1, 1] on screen, depth
    /// in [0, 1] between the near and far planes). Returns nullopt for points on the camera plane
    /// (clip w of zero).
    /// </summary>
    [[nodiscard]] std::optional<vec3> project(vec3 point, const mat4 &viewProjection) noexcept;
}
