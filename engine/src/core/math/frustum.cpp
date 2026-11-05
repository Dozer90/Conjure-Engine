#include "conjure/core/math/frustum.h"

#include "conjure/core/math/math_utils.h"
#include "conjure/core/assert.h"

#include <array>

using namespace conjure;
using namespace conjure::math;

/*===========================================================================*/
/*===========================================================================*/
// Local helpers

namespace {
    // glm matrices are column-major: m[column][row]. Gribb-Hartmann works on rows.
    vec4 matrix_row(const mat4 &matrix, glm::length_t row) noexcept {
        return vec4(matrix[0][row], matrix[1][row], matrix[2][row], matrix[3][row]);
    }

    plane plane_from_row(const vec4 &row) noexcept {
        plane result = {};
        result.normal = vec3(row);
        result.distance = row.w;
        result.normalize();
        return result;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Public methods

frustum frustum::from_view_projection(const mat4 &viewProjection) noexcept {
    const vec4 rowX = matrix_row(viewProjection, 0);
    const vec4 rowY = matrix_row(viewProjection, 1);
    const vec4 rowZ = matrix_row(viewProjection, 2);
    const vec4 rowW = matrix_row(viewProjection, 3);

    // Clip-space half-spaces: -w <= x (left), x <= w (right), -w <= y (bottom), y <= w (top),
    // 0 <= z (near, zero-to-one depth) and z <= w (far). Each inequality rearranged to
    // dot(plane, point) >= 0 gives the plane below.
    frustum result = {};
    result.planes[static_cast<usize>(frustum_plane::LEFT)] = plane_from_row(rowW + rowX);
    result.planes[static_cast<usize>(frustum_plane::RIGHT)] = plane_from_row(rowW - rowX);
    result.planes[static_cast<usize>(frustum_plane::BOTTOM)] = plane_from_row(rowW + rowY);
    result.planes[static_cast<usize>(frustum_plane::TOP)] = plane_from_row(rowW - rowY);
    result.planes[static_cast<usize>(frustum_plane::NEAR_PLANE)] = plane_from_row(rowZ);
    result.planes[static_cast<usize>(frustum_plane::FAR_PLANE)] = plane_from_row(rowW - rowZ);
    return result;
}

std::array<vec3, 8> frustum::corners(const mat4 &inverseViewProjection) noexcept {
    static constexpr std::array<vec2, 4> CORNER_SIGNS = {
      vec2(-1.0f, -1.0f),
      vec2(1.0f, -1.0f),
      vec2(1.0f, 1.0f),
      vec2(-1.0f, 1.0f)
    };

    std::array<vec3, 8> result = {};
    bool atInfinity = false;
    for (usize index = 0; index < result.size(); ++index) {
        const vec2 signs = CORNER_SIGNS[index % CORNER_SIGNS.size()];
        const f32 depth = index < CORNER_SIGNS.size() ? 0.0f : 1.0f;
        const vec4 clip = inverseViewProjection * vec4(signs.x, signs.y, depth, 1.0f);
        if (nearly_zero(clip.w)) {
            // The far plane of an infinite projection maps to w = 0: there is no finite corner.
            atInfinity = true;
            result[index] = vec3(clip);
            continue;
        }
        result[index] = vec3(clip) / clip.w;
    }
    CONJURE_ASSERT(!atInfinity, "frustum::corners: the projection has corners at infinity (infinite far plane)");
    return result;
}

bool frustum::intersects(const aabb &box) const noexcept {
    if (!box.is_valid()) {
        return false;
    }

    for (const plane &side : planes) {
        // The p-vertex is the corner furthest along the normal; if even it is behind the plane the
        // whole box is.
        const vec3 positiveVertex = vec3(
          side.normal.x >= 0.0f ? box.max.x : box.min.x,
          side.normal.y >= 0.0f ? box.max.y : box.min.y,
          side.normal.z >= 0.0f ? box.max.z : box.min.z
        );
        if (side.signed_distance(positiveVertex) < 0.0f) {
            return false;
        }
    }
    return true;
}

bool frustum::contains(const aabb &box) const noexcept {
    if (!box.is_valid()) {
        return false;
    }

    for (const plane &side : planes) {
        // The n-vertex is the corner furthest against the normal; the box is fully inside a plane
        // when even that corner is in front of it.
        const vec3 negativeVertex = vec3(
          side.normal.x >= 0.0f ? box.min.x : box.max.x,
          side.normal.y >= 0.0f ? box.min.y : box.max.y,
          side.normal.z >= 0.0f ? box.min.z : box.max.z
        );
        if (side.signed_distance(negativeVertex) < 0.0f) {
            return false;
        }
    }
    return true;
}

bool frustum::intersects_sphere(vec3 center, f32 radius) const noexcept {
    const f32 safeRadius = radius < 0.0f ? 0.0f : radius;
    for (const plane &side : planes) {
        if (side.signed_distance(center) < -safeRadius) {
            return false;
        }
    }
    return true;
}

bool frustum::contains(vec3 point) const noexcept {
    for (const plane &side : planes) {
        if (side.signed_distance(point) < 0.0f) {
            return false;
        }
    }
    return true;
}
