#pragma once

#include "conjure/core/api.h"
#include "conjure/core/math/aabb.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/math/plane.h"
#include "conjure/core/types.h"

#include <optional>

namespace conjure::math {

    /// <summary>
    /// A half-line origin + t * direction for t >= 0. Callers keep direction normalised (the
    /// factories do) so that t is a distance. Every intersection returns the smallest t >= 0 at
    /// which the ray is on or inside the shape, or nullopt when it misses; a ray starting inside a
    /// closed volume therefore hits at t = 0.
    /// </summary>
    struct CONJURE_API ray {
        vec3 origin = vec3(0.0f);
        vec3 direction = vec3(0.0f, 0.0f, -1.0f);

        /// <summary>
        /// The point at parameter t.
        /// </summary>
        [[nodiscard]] vec3 at(f32 t) const noexcept {
            return origin + direction * t;
        }

        /// <summary>
        /// Slab test against a box. Returns 0 when the origin is inside, nullopt for a miss, an
        /// invalid box or a box entirely behind the origin. Rays parallel to a slab are handled.
        /// </summary>
        [[nodiscard]] std::optional<f32> intersect(const aabb &box) const noexcept;

        /// <summary>
        /// Nearest hit with a sphere. Returns 0 when the origin is inside; nullopt for a miss, a
        /// sphere behind the origin or a negative radius.
        /// </summary>
        [[nodiscard]] std::optional<f32> intersect_sphere(vec3 center, f32 radius) const noexcept;

        /// <summary>
        /// Hit with an infinite plane from either side. Returns nullopt when the ray is parallel to
        /// the plane (including when it lies in it) or the plane is behind the origin.
        /// </summary>
        [[nodiscard]] std::optional<f32> intersect_plane(const plane &surface) const noexcept;

        /// <summary>
        /// Moller-Trumbore triangle test. Triangles are counter-clockwise when seen from the front;
        /// with cullBackFaces the back side is ignored. Degenerate (collinear) triangles and rays
        /// parallel to the triangle never hit; the test is relative to the triangle size and the
        /// direction length, so millimetre triangles and grazing angles work. Edges and vertices
        /// count as hits.
        /// </summary>
        [[nodiscard]] std::optional<f32> intersect_triangle(
          vec3 a,
          vec3 b,
          vec3 c,
          bool cullBackFaces = true
        ) const noexcept;

        /// <summary>
        /// The ray from a to b with a normalised direction; a == b yields the default direction.
        /// </summary>
        [[nodiscard]] static ray from_points(vec3 a, vec3 b) noexcept {
            ray result = {};
            result.origin = a;
            result.direction = safe_normalize(b - a);
            return result;
        }

        /// <summary>
        /// The world-space pick ray through a viewport pixel (origin top-left, y down; pixel
        /// centres are at +0.5). The origin lies on the near plane (depth 0). Works for
        /// perspective, infinite and orthographic projections. A zero-sized viewport asserts and
        /// uses the viewport centre.
        /// </summary>
        [[nodiscard]] static ray from_viewport(
          vec2 pixel,
          uvec2 viewportSize,
          const mat4 &inverseViewProjection
        ) noexcept;

        [[nodiscard]] bool operator==(const ray &other) const = default;
    };
}
