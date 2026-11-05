#include "conjure/core/math/ray.h"

#include "conjure/core/math/math.h"
#include "conjure/core/assert.h"

#include <algorithm>
#include <cmath>

using namespace conjure;
using namespace conjure::math;

/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::optional<f32> ray::intersect(const aabb &box) const noexcept {
    if (!box.is_valid()) {
        return std::nullopt;
    }

    f32 entry = 0.0f;         // the ray starts at t = 0; an origin inside the box hits there
    f32 exit = F32_INFINITY;
    for (glm::length_t axis = 0; axis < 3; ++axis) {
        if (nearly_zero(direction[axis])) {
            // Parallel to this slab: the ray hits only when the origin already lies within it.
            if (origin[axis] < box.min[axis] || origin[axis] > box.max[axis]) {
                return std::nullopt;
            }
            continue;
        }

        const f32 inverseDirection = 1.0f / direction[axis];
        f32 slabEntry = (box.min[axis] - origin[axis]) * inverseDirection;
        f32 slabExit = (box.max[axis] - origin[axis]) * inverseDirection;
        if (slabEntry > slabExit) {
            std::swap(slabEntry, slabExit);
        }
        entry = std::max(entry, slabEntry);
        exit = std::min(exit, slabExit);
        if (entry > exit) {
            return std::nullopt;
        }
    }
    return entry;
}

std::optional<f32> ray::intersect_sphere(vec3 center, f32 radius) const noexcept {
    if (radius < 0.0f) {
        return std::nullopt;
    }

    // Solve |origin + t * direction - center|^2 = radius^2 as a quadratic in t. The direction is
    // not assumed to be normalised so the quadratic coefficient is kept.
    const vec3 offset = origin - center;
    const f32 a = glm::dot(direction, direction);
    if (nearly_zero(a)) {
        return std::nullopt;
    }
    const f32 halfB = glm::dot(offset, direction);
    const f32 c = glm::dot(offset, offset) - radius * radius;
    if (c <= 0.0f) {
        return 0.0f;    // origin inside or on the sphere
    }

    const f32 discriminant = halfB * halfB - a * c;
    if (discriminant < 0.0f) {
        return std::nullopt;
    }

    const f32 root = std::sqrt(discriminant);
    const f32 nearest = (-halfB - root) / a;
    if (nearest >= 0.0f) {
        return nearest;
    }
    // The near root is behind; the far root can only be in front if the origin were inside, which
    // was handled above, so the sphere is behind the ray.
    return std::nullopt;
}

std::optional<f32> ray::intersect_plane(const plane &surface) const noexcept {
    const f32 denominator = glm::dot(surface.normal, direction);
    if (nearly_zero(denominator)) {
        return std::nullopt;
    }

    const f32 t = -surface.signed_distance(origin) / denominator;
    if (t < 0.0f) {
        return std::nullopt;
    }
    return t;
}

std::optional<f32> ray::intersect_triangle(vec3 a, vec3 b, vec3 c, bool cullBackFaces) const noexcept {
    const vec3 edgeAB = b - a;
    const vec3 edgeAC = c - a;
    const vec3 perpendicular = glm::cross(direction, edgeAC);
    const f32 determinant = glm::dot(edgeAB, perpendicular);

    // determinant < 0 means the ray approaches the back face (clockwise from the ray's point of
    // view); ~0 means parallel or degenerate. The determinant is |direction| * |AB| * |AC| times a
    // purely angular factor (sin of the edge angle times cos of the incidence angle), so the
    // degeneracy threshold must be relative to that product: an absolute epsilon would silently
    // cull every triangle smaller than about a millimetre.
    const f32 scale = glm::length(direction) * glm::length(edgeAB) * glm::length(edgeAC);
    const f32 threshold = EPSILON * scale;
    if (cullBackFaces ? determinant <= threshold : absolute(determinant) <= threshold) {
        return std::nullopt;
    }

    const f32 inverseDeterminant = 1.0f / determinant;
    const vec3 toOrigin = origin - a;
    const f32 u = glm::dot(toOrigin, perpendicular) * inverseDeterminant;
    if (u < -EPSILON || u > 1.0f + EPSILON) {
        return std::nullopt;
    }

    const vec3 crossOriginAB = glm::cross(toOrigin, edgeAB);
    const f32 v = glm::dot(direction, crossOriginAB) * inverseDeterminant;
    if (v < -EPSILON || u + v > 1.0f + EPSILON) {
        return std::nullopt;
    }

    const f32 t = glm::dot(edgeAC, crossOriginAB) * inverseDeterminant;
    if (t < 0.0f) {
        return std::nullopt;
    }
    return t;
}

ray ray::from_viewport(vec2 pixel, uvec2 viewportSize, const mat4 &inverseViewProjection) noexcept {
    vec2 normalized = vec2(0.0f);
    if (viewportSize.x == 0 || viewportSize.y == 0) {
        CONJURE_ASSERT(false, "ray::from_viewport: viewport size must not be zero ({})", viewportSize);
    } else {
        const vec2 size = vec2(static_cast<f32>(viewportSize.x), static_cast<f32>(viewportSize.y));
        normalized = vec2((pixel.x / size.x) * 2.0f - 1.0f, 1.0f - (pixel.y / size.y) * 2.0f);
    }

    // Depth 0 is the near plane. The second point sits halfway through depth rather than on the
    // far plane so that infinite projections (far plane at w = 0) still produce a direction.
    const std::optional<vec3> nearPoint = unproject(vec3(normalized, 0.0f), inverseViewProjection);
    const std::optional<vec3> midPoint = unproject(vec3(normalized, 0.5f), inverseViewProjection);

    ray result = {};
    if (!nearPoint.has_value() || !midPoint.has_value()) {
        CONJURE_ASSERT(false, "ray::from_viewport: the inverse view-projection matrix is singular");
        return result;
    }
    result.origin = *nearPoint;
    result.direction = safe_normalize(*midPoint - *nearPoint);
    return result;
}
