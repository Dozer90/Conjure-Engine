#pragma once

#include "conjure/core/math/math_utils.h"
#include "conjure/core/types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <format>
#include <span>

// The engine's vector, quaternion and matrix types are glm's. This is the single public header
// through which glm reaches the rest of the engine (design rule C6); glm is configured by the build
// with GLM_FORCE_RADIANS, GLM_FORCE_DEPTH_ZERO_TO_ONE, GLM_ENABLE_EXPERIMENTAL and
// GLM_FORCE_EXPLICIT_CTOR, so write vec3(0.0f), never vec3 value = 0.0f.
//
// Conventions: right-handed, Y up, -Z forward (glTF). Matrices are column-major and act on column
// vectors (matrix * vector). Quaternions are constructed as quat(w, x, y, z) and stored in memory as
// x, y, z, w (glm's default layout, mirrored by the C# side).

namespace conjure::math {

    using vec2 = glm::vec2;
    using vec3 = glm::vec3;
    using vec4 = glm::vec4;
    using ivec2 = glm::ivec2;
    using ivec3 = glm::ivec3;
    using ivec4 = glm::ivec4;
    using uvec2 = glm::uvec2;
    using uvec3 = glm::uvec3;
    using uvec4 = glm::uvec4;
    using quat = glm::quat;
    using mat3 = glm::mat3;
    using mat4 = glm::mat4;

    static_assert(sizeof(vec2) == 8 && sizeof(vec3) == 12 && sizeof(vec4) == 16, "glm vectors must be unpadded");
    static_assert(sizeof(quat) == 16 && sizeof(mat3) == 36 && sizeof(mat4) == 64, "glm types must be unpadded");

    /*===========================================================================*/
    /*===========================================================================*/
    // Approximate comparison

    /// <summary>
    /// True when every component of a is within epsilon of the matching component of b.
    /// </summary>
    [[nodiscard]] inline bool nearly_equal(const vec2 &a, const vec2 &b, f32 epsilon = EPSILON) noexcept {
        return nearly_equal(a.x, b.x, epsilon) && nearly_equal(a.y, b.y, epsilon);
    }

    /// <summary>
    /// True when every component of a is within epsilon of the matching component of b.
    /// </summary>
    [[nodiscard]] inline bool nearly_equal(const vec3 &a, const vec3 &b, f32 epsilon = EPSILON) noexcept {
        return nearly_equal(a.x, b.x, epsilon) && nearly_equal(a.y, b.y, epsilon) && nearly_equal(a.z, b.z, epsilon);
    }

    /// <summary>
    /// True when every component of a is within epsilon of the matching component of b.
    /// </summary>
    [[nodiscard]] inline bool nearly_equal(const vec4 &a, const vec4 &b, f32 epsilon = EPSILON) noexcept {
        return nearly_equal(a.x, b.x, epsilon) && nearly_equal(a.y, b.y, epsilon) && nearly_equal(a.z, b.z, epsilon)
          && nearly_equal(a.w, b.w, epsilon);
    }

    /// <summary>
    /// True when every component of a is within epsilon of the matching component of b. Note that
    /// q and -q are different quaternions here although they encode the same rotation; use
    /// same_rotation() to compare rotations.
    /// </summary>
    [[nodiscard]] inline bool nearly_equal(const quat &a, const quat &b, f32 epsilon = EPSILON) noexcept {
        return nearly_equal(a.x, b.x, epsilon) && nearly_equal(a.y, b.y, epsilon) && nearly_equal(a.z, b.z, epsilon)
          && nearly_equal(a.w, b.w, epsilon);
    }

    /// <summary>
    /// True when every element of a is within epsilon of the matching element of b.
    /// </summary>
    [[nodiscard]] inline bool nearly_equal(const mat3 &a, const mat3 &b, f32 epsilon = EPSILON) noexcept {
        for (glm::length_t column = 0; column < 3; ++column) {
            if (!nearly_equal(a[column], b[column], epsilon)) {
                return false;
            }
        }
        return true;
    }

    /// <summary>
    /// True when every element of a is within epsilon of the matching element of b.
    /// </summary>
    [[nodiscard]] inline bool nearly_equal(const mat4 &a, const mat4 &b, f32 epsilon = EPSILON) noexcept {
        for (glm::length_t column = 0; column < 4; ++column) {
            if (!nearly_equal(a[column], b[column], epsilon)) {
                return false;
            }
        }
        return true;
    }

    /// <summary>
    /// True when two unit quaternions encode the same rotation (q and -q both qualify): the
    /// absolute value of their dot product is within epsilon of 1.
    /// </summary>
    [[nodiscard]] inline bool same_rotation(const quat &a, const quat &b, f32 epsilon = EPSILON) noexcept {
        return absolute(glm::dot(a, b)) >= 1.0f - epsilon;
    }

    /// <summary>
    /// True when the vector's length is at most epsilon.
    /// </summary>
    [[nodiscard]] inline bool nearly_zero(const vec3 &value, f32 epsilon = EPSILON) noexcept {
        return glm::dot(value, value) <= epsilon * epsilon;
    }

    /// <summary>
    /// True when the vector's length is at most epsilon.
    /// </summary>
    [[nodiscard]] inline bool nearly_zero(const vec2 &value, f32 epsilon = EPSILON) noexcept {
        return glm::dot(value, value) <= epsilon * epsilon;
    }

    /// <summary>
    /// Normalises the vector, or returns fallback when its length is (nearly) zero. glm::normalize
    /// would produce NaN there.
    /// </summary>
    [[nodiscard]] inline vec3 safe_normalize(
      const vec3 &value,
      const vec3 &fallback = vec3(0.0f, 0.0f, -1.0f)
    ) noexcept {
        const f32 lengthSquared = glm::dot(value, value);
        if (lengthSquared <= EPSILON * EPSILON) {
            return fallback;
        }
        return value / std::sqrt(lengthSquared);
    }

    /// <summary>
    /// Normalises the vector, or returns fallback when its length is (nearly) zero.
    /// </summary>
    [[nodiscard]] inline vec2 safe_normalize(const vec2 &value, const vec2 &fallback = vec2(1.0f, 0.0f)) noexcept {
        const f32 lengthSquared = glm::dot(value, value);
        if (lengthSquared <= EPSILON * EPSILON) {
            return fallback;
        }
        return value / std::sqrt(lengthSquared);
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Formatting support shared by the std::formatter specialisations below

    namespace detail {
        /// <summary>
        /// Writes "(c0, c1, ...)" applying one component formatter (which already parsed the format
        /// spec) to every component.
        /// </summary>
        template <typename ComponentType, typename FormatContext>
        typename FormatContext::iterator format_components(
          const std::formatter<ComponentType> &componentFormatter,
          std::span<const ComponentType> components,
          FormatContext &context
        ) {
            typename FormatContext::iterator out = context.out();
            *out++ = '(';
            for (usize index = 0; index < components.size(); ++index) {
                if (index != 0) {
                    *out++ = ',';
                    *out++ = ' ';
                }
                context.advance_to(out);
                out = componentFormatter.format(components[index], context);
            }
            *out++ = ')';
            return out;
        }
    }
}

// Formatters print components in parentheses and forward the format spec to every component, so
// "{:.2f}" formats each component with two decimals. Quaternions print as (w, x, y, z), matching
// the constructor order; matrices print their columns in order.

template <>
struct std::formatter<conjure::math::vec2> : std::formatter<float> {
    template <typename FormatContext>
    auto format(const conjure::math::vec2 &value, FormatContext &context) const {
        const float components[2] = {value.x, value.y};
        return conjure::math::detail::format_components<float>(*this, components, context);
    }
};

template <>
struct std::formatter<conjure::math::vec3> : std::formatter<float> {
    template <typename FormatContext>
    auto format(const conjure::math::vec3 &value, FormatContext &context) const {
        const float components[3] = {value.x, value.y, value.z};
        return conjure::math::detail::format_components<float>(*this, components, context);
    }
};

template <>
struct std::formatter<conjure::math::vec4> : std::formatter<float> {
    template <typename FormatContext>
    auto format(const conjure::math::vec4 &value, FormatContext &context) const {
        const float components[4] = {value.x, value.y, value.z, value.w};
        return conjure::math::detail::format_components<float>(*this, components, context);
    }
};

template <>
struct std::formatter<conjure::math::ivec2> : std::formatter<conjure::i32> {
    template <typename FormatContext>
    auto format(const conjure::math::ivec2 &value, FormatContext &context) const {
        const conjure::i32 components[2] = {value.x, value.y};
        return conjure::math::detail::format_components<conjure::i32>(*this, components, context);
    }
};

template <>
struct std::formatter<conjure::math::ivec3> : std::formatter<conjure::i32> {
    template <typename FormatContext>
    auto format(const conjure::math::ivec3 &value, FormatContext &context) const {
        const conjure::i32 components[3] = {value.x, value.y, value.z};
        return conjure::math::detail::format_components<conjure::i32>(*this, components, context);
    }
};

template <>
struct std::formatter<conjure::math::ivec4> : std::formatter<conjure::i32> {
    template <typename FormatContext>
    auto format(const conjure::math::ivec4 &value, FormatContext &context) const {
        const conjure::i32 components[4] = {value.x, value.y, value.z, value.w};
        return conjure::math::detail::format_components<conjure::i32>(*this, components, context);
    }
};

template <>
struct std::formatter<conjure::math::uvec2> : std::formatter<conjure::u32> {
    template <typename FormatContext>
    auto format(const conjure::math::uvec2 &value, FormatContext &context) const {
        const conjure::u32 components[2] = {value.x, value.y};
        return conjure::math::detail::format_components<conjure::u32>(*this, components, context);
    }
};

template <>
struct std::formatter<conjure::math::uvec3> : std::formatter<conjure::u32> {
    template <typename FormatContext>
    auto format(const conjure::math::uvec3 &value, FormatContext &context) const {
        const conjure::u32 components[3] = {value.x, value.y, value.z};
        return conjure::math::detail::format_components<conjure::u32>(*this, components, context);
    }
};

template <>
struct std::formatter<conjure::math::uvec4> : std::formatter<conjure::u32> {
    template <typename FormatContext>
    auto format(const conjure::math::uvec4 &value, FormatContext &context) const {
        const conjure::u32 components[4] = {value.x, value.y, value.z, value.w};
        return conjure::math::detail::format_components<conjure::u32>(*this, components, context);
    }
};

template <>
struct std::formatter<conjure::math::quat> : std::formatter<float> {
    template <typename FormatContext>
    auto format(const conjure::math::quat &value, FormatContext &context) const {
        const float components[4] = {value.w, value.x, value.y, value.z};
        return conjure::math::detail::format_components<float>(*this, components, context);
    }
};

template <>
struct std::formatter<conjure::math::mat3> : std::formatter<float> {
    template <typename FormatContext>
    auto format(const conjure::math::mat3 &value, FormatContext &context) const {
        typename FormatContext::iterator out = context.out();
        *out++ = '[';
        for (glm::length_t column = 0; column < 3; ++column) {
            if (column != 0) {
                *out++ = ',';
                *out++ = ' ';
            }
            const float components[3] = {value[column].x, value[column].y, value[column].z};
            context.advance_to(out);
            out = conjure::math::detail::format_components<float>(*this, components, context);
        }
        *out++ = ']';
        return out;
    }
};

template <>
struct std::formatter<conjure::math::mat4> : std::formatter<float> {
    template <typename FormatContext>
    auto format(const conjure::math::mat4 &value, FormatContext &context) const {
        typename FormatContext::iterator out = context.out();
        *out++ = '[';
        for (glm::length_t column = 0; column < 4; ++column) {
            if (column != 0) {
                *out++ = ',';
                *out++ = ' ';
            }
            const float components[4] = {value[column].x, value[column].y, value[column].z, value[column].w};
            context.advance_to(out);
            out = conjure::math::detail::format_components<float>(*this, components, context);
        }
        *out++ = ']';
        return out;
    }
};
