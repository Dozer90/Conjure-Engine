#pragma once

#include "conjure/core/math/glm_types.h"
#include "conjure/core/types.h"

#include <format>

namespace conjure::math {

    /// <summary>
    /// An infinite plane in the form dot(normal, point) + distance = 0. The positive half-space
    /// is the side the normal points to. Normalise before trusting signed distances as metric
    /// distances; frustum extraction and the factories below already do.
    /// </summary>
    struct plane {
        vec3 normal = vec3(0.0f, 1.0f, 0.0f);
        f32 distance = 0.0f;

        /// <summary>
        /// dot(normal, point) + distance: positive in front of the plane, negative behind, zero on
        /// it. A metric distance only when the normal has unit length.
        /// </summary>
        [[nodiscard]] f32 signed_distance(vec3 point) const noexcept {
            return glm::dot(normal, point) + distance;
        }

        /// <summary>
        /// Scales the equation so that the normal has unit length. A zero normal is left as is.
        /// </summary>
        void normalize() noexcept {
            const f32 length = glm::length(normal);
            if (length <= EPSILON) {
                return;
            }
            normal /= length;
            distance /= length;
        }

        /// <summary>
        /// A copy with normalize() applied.
        /// </summary>
        [[nodiscard]] plane normalized() const noexcept {
            plane result = *this;
            result.normalize();
            return result;
        }

        /// <summary>
        /// The orthogonal projection of a point onto the plane (requires a unit normal).
        /// </summary>
        [[nodiscard]] vec3 project(vec3 point) const noexcept {
            return point - normal * signed_distance(point);
        }

        /// <summary>
        /// The plane through point with the given normal, normalised. A zero normal yields the
        /// default (Y up) plane through the point.
        /// </summary>
        [[nodiscard]] static plane from_point_normal(vec3 point, vec3 direction) noexcept {
            plane result = {};
            result.normal = safe_normalize(direction, vec3(0.0f, 1.0f, 0.0f));
            result.distance = -glm::dot(result.normal, point);
            return result;
        }

        /// <summary>
        /// The plane through three counter-clockwise points (normal = cross(b - a, c - a)),
        /// normalised. Collinear points yield the default (Y up) plane through a.
        /// </summary>
        [[nodiscard]] static plane from_points(vec3 a, vec3 b, vec3 c) noexcept {
            return from_point_normal(a, glm::cross(b - a, c - a));
        }

        [[nodiscard]] bool operator==(const plane &other) const = default;
    };

    /// <summary>
    /// True when normal and distance are each within epsilon.
    /// </summary>
    [[nodiscard]] inline bool nearly_equal(const plane &a, const plane &b, f32 epsilon = EPSILON) noexcept {
        return nearly_equal(a.normal, b.normal, epsilon) && nearly_equal(a.distance, b.distance, epsilon);
    }
}

template <>
struct std::formatter<conjure::math::plane> : std::formatter<float> {
    template <typename FormatContext>
    auto format(const conjure::math::plane &value, FormatContext &context) const {
        const float normal[3] = {value.normal.x, value.normal.y, value.normal.z};

        typename FormatContext::iterator out = std::format_to(context.out(), "{{normal=");
        context.advance_to(out);
        out = conjure::math::detail::format_components<float>(*this, normal, context);
        out = std::format_to(out, ", distance=");
        context.advance_to(out);
        out = std::formatter<float>::format(value.distance, context);
        *out++ = '}';
        return out;
    }
};
