#pragma once

#include "conjure/core/api.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/types.h"

#include <array>
#include <format>

namespace conjure::math {

    /// <summary>
    /// Axis-aligned bounding box. The default box is inverted (min = +FLT_MAX, max = -FLT_MAX) so
    /// that expanding it with the first point makes it valid; is_valid() is false until then.
    /// Containment and intersection tests are inclusive: touching counts.
    /// </summary>
    struct CONJURE_API aabb {
        vec3 min = vec3(F32_MAX);
        vec3 max = vec3(-F32_MAX);

        /// <summary>
        /// True when min <= max on every axis (a point or a degenerate flat box is valid).
        /// </summary>
        [[nodiscard]] bool is_valid() const noexcept {
            return min.x <= max.x && min.y <= max.y && min.z <= max.z;
        }

        /// <summary>
        /// Midpoint of min and max.
        /// </summary>
        [[nodiscard]] vec3 center() const noexcept {
            return (min + max) * 0.5f;
        }

        /// <summary>
        /// Half the size on each axis.
        /// </summary>
        [[nodiscard]] vec3 extents() const noexcept {
            return (max - min) * 0.5f;
        }

        /// <summary>
        /// max - min.
        /// </summary>
        [[nodiscard]] vec3 size() const noexcept {
            return max - min;
        }

        /// <summary>
        /// Grows the box to include a point.
        /// </summary>
        void expand(vec3 point) noexcept {
            min = glm::min(min, point);
            max = glm::max(max, point);
        }

        /// <summary>
        /// Grows the box to include another box. An invalid other box is ignored.
        /// </summary>
        void expand(const aabb &other) noexcept {
            if (!other.is_valid()) {
                return;
            }
            min = glm::min(min, other.min);
            max = glm::max(max, other.max);
        }

        /// <summary>
        /// Grows the box by amount on every side (shrinks for negative amounts).
        /// </summary>
        [[nodiscard]] aabb inflated(f32 amount) const noexcept {
            aabb result = *this;
            result.min -= vec3(amount);
            result.max += vec3(amount);
            return result;
        }

        /// <summary>
        /// True when the point lies inside or on the box. Always false for an invalid box.
        /// </summary>
        [[nodiscard]] bool contains(vec3 point) const noexcept {
            return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y && point.z >= min.z
              && point.z <= max.z;
        }

        /// <summary>
        /// True when other lies entirely inside or on this box. False when either box is invalid.
        /// </summary>
        [[nodiscard]] bool contains(const aabb &other) const noexcept {
            return other.is_valid() && contains(other.min) && contains(other.max);
        }

        /// <summary>
        /// True when the boxes overlap or touch. False when either box is invalid.
        /// </summary>
        [[nodiscard]] bool intersects(const aabb &other) const noexcept {
            if (!is_valid() || !other.is_valid()) {
                return false;
            }
            return min.x <= other.max.x && max.x >= other.min.x && min.y <= other.max.y && max.y >= other.min.y
              && min.z <= other.max.z && max.z >= other.min.z;
        }

        /// <summary>
        /// The overlap of both boxes; invalid when they do not intersect.
        /// </summary>
        [[nodiscard]] aabb intersection(const aabb &other) const noexcept {
            aabb result = {};
            result.min = glm::max(min, other.min);
            result.max = glm::min(max, other.max);
            return result;
        }

        /// <summary>
        /// The point inside or on the box nearest to point (point itself when inside). An invalid
        /// box has no surface and returns point unchanged.
        /// </summary>
        [[nodiscard]] vec3 closest_point(vec3 point) const noexcept {
            if (!is_valid()) {
                return point;
            }
            return glm::clamp(point, min, max);
        }

        /// <summary>
        /// Squared distance from point to the box surface, 0 when inside. 0 for an invalid box
        /// (like surface_area and volume).
        /// </summary>
        [[nodiscard]] f32 distance_squared(vec3 point) const noexcept {
            if (!is_valid()) {
                return 0.0f;
            }
            const vec3 offset = closest_point(point) - point;
            return glm::dot(offset, offset);
        }

        /// <summary>
        /// The eight corners, in the order min, +x, +y, +xy, +z, +xz, +yz, max
        /// (bit 0 = x at max, bit 1 = y at max, bit 2 = z at max).
        /// </summary>
        [[nodiscard]] std::array<vec3, 8> corners() const noexcept;

        /// <summary>
        /// The axis-aligned box enclosing all eight transformed corners (conservative under
        /// rotation). Affine matrices only: the bottom row is ignored (no perspective divide), so
        /// a projection matrix yields a meaningless box; use frustum::corners for view volumes. An
        /// invalid box stays invalid.
        /// </summary>
        [[nodiscard]] aabb transformed(const mat4 &matrix) const noexcept;

        /// <summary>
        /// Total area of the six faces; 0 for an invalid box.
        /// </summary>
        [[nodiscard]] f32 surface_area() const noexcept {
            if (!is_valid()) {
                return 0.0f;
            }
            const vec3 extent = size();
            return 2.0f * (extent.x * extent.y + extent.y * extent.z + extent.z * extent.x);
        }

        /// <summary>
        /// Enclosed volume; 0 for an invalid box.
        /// </summary>
        [[nodiscard]] f32 volume() const noexcept {
            if (!is_valid()) {
                return 0.0f;
            }
            const vec3 extent = size();
            return extent.x * extent.y * extent.z;
        }

        /// <summary>
        /// A box with the given center and half-size on each axis (negative extents are made
        /// positive).
        /// </summary>
        [[nodiscard]] static aabb from_center_extents(vec3 centerPoint, vec3 halfExtents) noexcept {
            const vec3 halfSize = glm::abs(halfExtents);
            aabb result = {};
            result.min = centerPoint - halfSize;
            result.max = centerPoint + halfSize;
            return result;
        }

        /// <summary>
        /// A box from two arbitrary corners (they need not be ordered).
        /// </summary>
        [[nodiscard]] static aabb from_points(vec3 a, vec3 b) noexcept {
            aabb result = {};
            result.min = glm::min(a, b);
            result.max = glm::max(a, b);
            return result;
        }

        [[nodiscard]] bool operator==(const aabb &other) const = default;
    };

    /// <summary>
    /// True when both corners are within epsilon.
    /// </summary>
    [[nodiscard]] inline bool nearly_equal(const aabb &a, const aabb &b, f32 epsilon = EPSILON) noexcept {
        return nearly_equal(a.min, b.min, epsilon) && nearly_equal(a.max, b.max, epsilon);
    }
}

template <>
struct std::formatter<conjure::math::aabb> : std::formatter<float> {
    template <typename FormatContext>
    auto format(const conjure::math::aabb &value, FormatContext &context) const {
        const float minimum[3] = {value.min.x, value.min.y, value.min.z};
        const float maximum[3] = {value.max.x, value.max.y, value.max.z};

        typename FormatContext::iterator out = std::format_to(context.out(), "{{min=");
        context.advance_to(out);
        out = conjure::math::detail::format_components<float>(*this, minimum, context);
        out = std::format_to(out, ", max=");
        context.advance_to(out);
        out = conjure::math::detail::format_components<float>(*this, maximum, context);
        *out++ = '}';
        return out;
    }
};
