#pragma once

#include "conjure/testing/doctest.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <format>
#include <string>

// Approximate comparison of math types and their doctest stringification.
//
//   CONJURE_CHECK_APPROX(body.position(), vec3(0.0f, 1.0f, 0.0f));
//
// Quaternions q and -q encode the same rotation and compare equal here, unlike math::nearly_equal.

namespace conjure::testing {

    /// <summary>
    /// Tolerance of the approx_equal overloads when none is given. Looser than math::EPSILON because
    /// tests compare results of whole computations, not single operations.
    /// </summary>
    inline constexpr f32 EPSILON = 1.0e-5f;

    /// <summary>
    /// True when |left - right| <= epsilon.
    /// </summary>
    [[nodiscard]] bool approx_equal(f32 left, f32 right, f32 epsilon = EPSILON) noexcept;

    /// <summary>
    /// True when every component is within epsilon.
    /// </summary>
    [[nodiscard]] bool approx_equal(const math::vec2 &left, const math::vec2 &right, f32 epsilon = EPSILON) noexcept;
    [[nodiscard]] bool approx_equal(const math::vec3 &left, const math::vec3 &right, f32 epsilon = EPSILON) noexcept;
    [[nodiscard]] bool approx_equal(const math::vec4 &left, const math::vec4 &right, f32 epsilon = EPSILON) noexcept;

    /// <summary>
    /// True when the quaternions encode the same rotation within epsilon: q and -q compare equal.
    /// </summary>
    [[nodiscard]] bool approx_equal(const math::quat &left, const math::quat &right, f32 epsilon = EPSILON) noexcept;

    /// <summary>
    /// True when every element is within epsilon.
    /// </summary>
    [[nodiscard]] bool approx_equal(const math::mat3 &left, const math::mat3 &right, f32 epsilon = EPSILON) noexcept;
    [[nodiscard]] bool approx_equal(const math::mat4 &left, const math::mat4 &right, f32 epsilon = EPSILON) noexcept;

    /// <summary>
    /// True when both corners are within epsilon.
    /// </summary>
    [[nodiscard]] bool approx_equal(const math::aabb &left, const math::aabb &right, f32 epsilon = EPSILON) noexcept;

    /// <summary>
    /// True when position and scale are within epsilon and the rotations encode the same rotation.
    /// </summary>
    [[nodiscard]] bool approx_equal(
      const math::transform &left,
      const math::transform &right,
      f32 epsilon = EPSILON
    ) noexcept;
}

// Both operands are evaluated exactly once: CHECK_MESSAGE captures its message by reference and would
// otherwise re-evaluate the expressions when the comparison fails.
#define CONJURE_CHECK_APPROX(actual, expected)                                                              \
    do {                                                                                                    \
        auto &&CONJURE_UNIQUE_NAME(conjureApproxActual_) = (actual);                                        \
        auto &&CONJURE_UNIQUE_NAME(conjureApproxExpected_) = (expected);                                    \
        CHECK_MESSAGE(                                                                                      \
          ::conjure::testing::approx_equal(                                                                 \
            CONJURE_UNIQUE_NAME(conjureApproxActual_),                                                      \
            CONJURE_UNIQUE_NAME(conjureApproxExpected_)                                                     \
          ),                                                                                                \
          "approx: " << CONJURE_UNIQUE_NAME(conjureApproxActual_) << " vs "                                 \
                     << CONJURE_UNIQUE_NAME(conjureApproxExpected_)                                         \
        );                                                                                                  \
    } while (false)

// doctest::StringMaker specialisations through the engine's std::formatter specialisations.
namespace doctest {

    #define CONJURE_TESTING_MATH_STRING_MAKER(type)                                                         \
        template <>                                                                                         \
        struct StringMaker<type> {                                                                          \
            static String convert(const type &value) {                                                      \
                const std::string text = std::format("{}", value);                                          \
                return String(text.c_str());                                                                \
            }                                                                                               \
        }

    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::vec2);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::vec3);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::vec4);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::ivec2);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::ivec3);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::ivec4);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::uvec2);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::uvec3);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::uvec4);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::quat);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::mat3);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::mat4);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::transform);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::aabb);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::plane);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::rect);
    CONJURE_TESTING_MATH_STRING_MAKER(conjure::math::urect);

    #undef CONJURE_TESTING_MATH_STRING_MAKER
}
