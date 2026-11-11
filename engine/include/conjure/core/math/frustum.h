#pragma once

#include "conjure/core/api.h"
#include "conjure/core/math/aabb.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/math/plane.h"
#include "conjure/core/types.h"

#include <array>

namespace conjure::math {

    /// <summary>
    /// Index of each frustum plane in frustum::planes. NEAR and FAR are Windows macros, hence the
    /// _PLANE suffix.
    /// </summary>
    enum class frustum_plane : u8 {
        LEFT = 0,
        RIGHT,
        BOTTOM,
        TOP,
        NEAR_PLANE,
        FAR_PLANE,
        COUNT
    };

    inline constexpr usize FRUSTUM_PLANE_COUNT = static_cast<usize>(frustum_plane::COUNT);

    /// <summary>
    /// Six inward-facing, normalised planes describing a view volume. Points with a non-negative
    /// signed distance to every plane are inside. Built from a view-projection matrix with the
    /// engine's 0..1 depth convention.
    /// </summary>
    struct CONJURE_API frustum {
        std::array<plane, FRUSTUM_PLANE_COUNT> planes = {};

        /// <summary>
        /// Extracts the planes from a view-projection (or projection) matrix using the
        /// Gribb-Hartmann method for clip space -w <= x <= w, -w <= y <= w, 0 <= z <= w
        /// (GLM_FORCE_DEPTH_ZERO_TO_ONE). Works for perspective, infinite and orthographic
        /// projections; planes are normalised.
        /// </summary>
        [[nodiscard]] static frustum from_view_projection(const mat4 &viewProjection) noexcept;

        /// <summary>
        /// The eight corners of the volume described by an inverse view-projection matrix, in the
        /// order near bottom-left, near bottom-right, near top-right, near top-left, then the same
        /// for the far plane. Requires a finite projection: an infinite projection has no finite
        /// far corners, so the call asserts once and leaves the affected corners at their undivided
        /// clip-space value.
        /// </summary>
        [[nodiscard]] static std::array<vec3, 8> corners(const mat4 &inverseViewProjection) noexcept;

        /// <summary>
        /// The plane with the given index.
        /// </summary>
        [[nodiscard]] const plane &get(frustum_plane which) const noexcept {
            return planes[static_cast<usize>(which)];
        }

        /// <summary>
        /// Conservative box test using the p-vertex (the corner furthest along each plane normal):
        /// false only when the box is entirely behind at least one plane, so boxes near an edge
        /// may report an intersection they do not have. Invalid boxes never intersect.
        /// </summary>
        [[nodiscard]] bool intersects(const aabb &box) const noexcept;

        /// <summary>
        /// True when the box is entirely inside or touching all six planes (n-vertex test).
        /// </summary>
        [[nodiscard]] bool contains(const aabb &box) const noexcept;

        /// <summary>
        /// True when the sphere is not entirely behind any plane (exact for the planes, so
        /// conservative near edges like intersects(aabb)). Negative radii are treated as 0.
        /// </summary>
        [[nodiscard]] bool intersects_sphere(vec3 center, f32 radius) const noexcept;

        /// <summary>
        /// True when the point is inside or on every plane.
        /// </summary>
        [[nodiscard]] bool contains(vec3 point) const noexcept;

        [[nodiscard]] bool operator==(const frustum &other) const = default;
    };
}
