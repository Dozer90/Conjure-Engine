#pragma once

// Shared helpers for the core math tests: tolerances, a few reference transforms and CHECK_NEARLY, an
// approximate-equality check that prints both values on failure. The doctest stringification of the
// math types comes from the support library (conjure/testing/approx.h).

#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "conjure/core/math/math.h"
#include "conjure/core/types.h"

namespace math_test {

    /// <summary>
    /// Tolerance for results of one or two floating point operations.
    /// </summary>
    inline constexpr conjure::f32 TIGHT_EPSILON = 1.0e-5f;

    /// <summary>
    /// Tolerance for results of longer chains (matrix products, decompositions, inverses).
    /// </summary>
    inline constexpr conjure::f32 LOOSE_EPSILON = 1.0e-3f;

    /// <summary>
    /// A unit quaternion rotating by the angle (radians) about the axis, which need not be normalised.
    /// </summary>
    [[nodiscard]] inline conjure::math::quat rotation_about(const conjure::math::vec3 &axis, conjure::f32 angle) {
        return glm::angleAxis(angle, glm::normalize(axis));
    }

    /// <summary>
    /// A translated, rotated and non-uniformly scaled transform with no zero or negative scale.
    /// </summary>
    [[nodiscard]] inline conjure::math::transform sample_transform() {
        conjure::math::transform value = {};
        value.position = conjure::math::vec3(1.5f, -2.0f, 3.25f);
        value.rotation = rotation_about(conjure::math::vec3(1.0f, 2.0f, 3.0f), 0.7f);
        value.scale = conjure::math::vec3(2.0f, 0.5f, 3.0f);
        return value;
    }

    /// <summary>
    /// A translated and rotated transform with uniform scale, for which every composition is exact.
    /// </summary>
    [[nodiscard]] inline conjure::math::transform uniform_transform() {
        conjure::math::transform value = {};
        value.position = conjure::math::vec3(-4.0f, 1.0f, 0.5f);
        value.rotation = rotation_about(conjure::math::vec3(0.0f, 1.0f, 1.0f), -1.1f);
        value.scale = conjure::math::vec3(2.0f);
        return value;
    }
}

// Approximate equality through math::nearly_equal, printing both values when it fails. The operands
// are bound once: CHECK_MESSAGE captures its message by reference and would otherwise re-evaluate
// them on failure.
#define CHECK_NEARLY(actual, expected, epsilon)                                                             \
    do {                                                                                                    \
        auto &&CONJURE_UNIQUE_NAME(mathNearlyActual_) = (actual);                                           \
        auto &&CONJURE_UNIQUE_NAME(mathNearlyExpected_) = (expected);                                       \
        CHECK_MESSAGE(                                                                                      \
          ::conjure::math::nearly_equal(                                                                    \
            CONJURE_UNIQUE_NAME(mathNearlyActual_),                                                         \
            CONJURE_UNIQUE_NAME(mathNearlyExpected_),                                                       \
            (epsilon)                                                                                       \
          ),                                                                                                \
          "nearly: " << CONJURE_UNIQUE_NAME(mathNearlyActual_) << " vs "                                    \
                     << CONJURE_UNIQUE_NAME(mathNearlyExpected_)                                            \
        );                                                                                                  \
    } while (false)
