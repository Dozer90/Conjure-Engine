#pragma once

#include "conjure/core/math/glm_types.h"
#include "conjure/core/types.h"

#include <format>
#include <type_traits>

namespace conjure::math {

    /// <summary>
    /// Position, rotation and scale (TRS). Right-handed, Y up, -Z forward. Trivially copyable
    /// and exactly 10 floats (position xyz, rotation xyzw, scale xyz) so the C# side mirrors it.
    ///
    /// A TRS cannot represent shear, so composing or inverting transforms with non-uniform scale
    /// under rotation is an approximation; the documented behaviours below say exactly which
    /// identities hold.
    /// </summary>
    struct transform {
        vec3 position = vec3(0.0f);
        quat rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);   // identity (w, x, y, z)
        vec3 scale = vec3(1.0f);

        /// <summary>
        /// The 4x4 matrix translate(position) * rotate(rotation) * scale(scale).
        /// </summary>
        [[nodiscard]] mat4 to_matrix() const noexcept;

        /// <summary>
        /// Decomposes a TRS matrix (no shear). A negative determinant (mirroring) is expressed as a
        /// negative x scale; a zero-length axis yields a zero scale on that axis and the identity
        /// direction for its rotation column.
        /// </summary>
        [[nodiscard]] static transform from_matrix(const mat4 &matrix) noexcept;

        /// <summary>
        /// Composes parent * child: the child expressed in the parent's space is moved into the
        /// parent's parent space. Equals from_matrix(to_matrix() * child.to_matrix()) exactly when
        /// the parent scale is uniform or the child rotation is the identity.
        /// </summary>
        [[nodiscard]] transform operator*(const transform &child) const noexcept;

        /// <summary>
        /// The transform that undoes this one: inverse() * (*this) is the identity for every
        /// transform, and inverse().to_matrix() equals glm::inverse(to_matrix()) when the scale
        /// is uniform. A (nearly) zero scale component inverts to zero instead of infinity.
        /// </summary>
        [[nodiscard]] transform inverse() const noexcept;

        /// <summary>
        /// The rotated -Z axis.
        /// </summary>
        [[nodiscard]] vec3 forward() const noexcept {
            return rotation * vec3(0.0f, 0.0f, -1.0f);
        }

        /// <summary>
        /// The rotated +X axis.
        /// </summary>
        [[nodiscard]] vec3 right() const noexcept {
            return rotation * vec3(1.0f, 0.0f, 0.0f);
        }

        /// <summary>
        /// The rotated +Y axis.
        /// </summary>
        [[nodiscard]] vec3 up() const noexcept {
            return rotation * vec3(0.0f, 1.0f, 0.0f);
        }

        /// <summary>
        /// Applies scale, rotation and translation to a point in this transform's local space.
        /// </summary>
        [[nodiscard]] vec3 transform_point(vec3 point) const noexcept {
            return position + rotation * (scale * point);
        }

        /// <summary>
        /// Applies only the rotation to a direction (no scale, no translation).
        /// </summary>
        [[nodiscard]] vec3 transform_direction(vec3 direction) const noexcept {
            return rotation * direction;
        }

        /// <summary>
        /// Maps a point from the parent space back into this transform's local space; exact for
        /// every scale (the exact inverse of transform_point, including non-uniform scale).
        /// </summary>
        [[nodiscard]] vec3 inverse_transform_point(vec3 point) const noexcept;

        /// <summary>
        /// Rotates a direction from the parent space back into local space.
        /// </summary>
        [[nodiscard]] vec3 inverse_transform_direction(vec3 direction) const noexcept {
            return glm::conjugate(rotation) * direction;
        }

        /// <summary>
        /// A transform at eye whose forward() points at target with the given up hint and unit
        /// scale. A target equal to eye keeps the identity rotation; an up hint parallel to the
        /// view direction (or zero) is replaced by the world axis least aligned with the direction
        /// so the result is always a valid rotation.
        /// </summary>
        [[nodiscard]] static transform look_at(vec3 eye, vec3 target, vec3 upHint) noexcept;

        /// <summary>
        /// Interpolates position and scale linearly and the rotation by shortest-path slerp. t is
        /// not clamped (like math::lerp): values outside [0, 1] extrapolate.
        /// </summary>
        [[nodiscard]] static transform lerp(const transform &a, const transform &b, f32 t) noexcept;

        /// <summary>
        /// A transform with only a position.
        /// </summary>
        [[nodiscard]] static transform from_position(vec3 value) noexcept {
            transform result = {};
            result.position = value;
            return result;
        }

        /// <summary>
        /// A transform with only a rotation.
        /// </summary>
        [[nodiscard]] static transform from_rotation(quat value) noexcept {
            transform result = {};
            result.rotation = value;
            return result;
        }

        /// <summary>
        /// A transform with only a scale.
        /// </summary>
        [[nodiscard]] static transform from_scale(vec3 value) noexcept {
            transform result = {};
            result.scale = value;
            return result;
        }

        [[nodiscard]] bool operator==(const transform &other) const = default;
    };

    static_assert(sizeof(transform) == 40, "transform must be 10 floats for the C# mirror");
    static_assert(std::is_trivially_copyable_v<transform>, "transform must be trivially copyable");

    /// <summary>
    /// True when position, rotation (component-wise) and scale are each within epsilon.
    /// </summary>
    [[nodiscard]] inline bool nearly_equal(const transform &a, const transform &b, f32 epsilon = EPSILON) noexcept {
        return nearly_equal(a.position, b.position, epsilon) && nearly_equal(a.rotation, b.rotation, epsilon)
          && nearly_equal(a.scale, b.scale, epsilon);
    }
}

template <>
struct std::formatter<conjure::math::transform> : std::formatter<float> {
    template <typename FormatContext>
    auto format(const conjure::math::transform &value, FormatContext &context) const {
        const float position[3] = {value.position.x, value.position.y, value.position.z};
        const float rotation[4] = {value.rotation.w, value.rotation.x, value.rotation.y, value.rotation.z};
        const float scale[3] = {value.scale.x, value.scale.y, value.scale.z};

        typename FormatContext::iterator out = std::format_to(context.out(), "{{position=");
        context.advance_to(out);
        out = conjure::math::detail::format_components<float>(*this, position, context);
        out = std::format_to(out, ", rotation=");
        context.advance_to(out);
        out = conjure::math::detail::format_components<float>(*this, rotation, context);
        out = std::format_to(out, ", scale=");
        context.advance_to(out);
        out = conjure::math::detail::format_components<float>(*this, scale, context);
        *out++ = '}';
        return out;
    }
};
