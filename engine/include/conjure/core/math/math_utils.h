#pragma once

#include "conjure/core/assert.h"
#include "conjure/core/types.h"

#include <bit>
#include <cmath>
#include <concepts>
#include <limits>
#include <type_traits>

// Scalar helpers that need no glm: constants, angle conversion, approximate comparison,
// interpolation and integer alignment / power-of-two utilities. Everything here is pure, constexpr
// where the standard library allows it, and safe to call from any thread.

namespace conjure::math {

    /*===========================================================================*/
    /*===========================================================================*/
    // Constants

    inline constexpr f32 PI = 3.14159265358979323846f;
    inline constexpr f32 TWO_PI = 2.0f * PI;
    inline constexpr f32 HALF_PI = 0.5f * PI;
    inline constexpr f32 QUARTER_PI = 0.25f * PI;
    inline constexpr f32 INVERSE_PI = 1.0f / PI;
    inline constexpr f32 EPSILON = 1.0e-6f;
    inline constexpr f64 EPSILON_F64 = 1.0e-12;
    inline constexpr f32 DEGREES_TO_RADIANS = PI / 180.0f;
    inline constexpr f32 RADIANS_TO_DEGREES = 180.0f / PI;
    inline constexpr f32 F32_MAX = std::numeric_limits<f32>::max();
    inline constexpr f32 F32_INFINITY = std::numeric_limits<f32>::infinity();

    /*===========================================================================*/
    /*===========================================================================*/
    // Angles

    /// <summary>
    /// Converts degrees to radians.
    /// </summary>
    [[nodiscard]] constexpr f32 radians(f32 angleDegrees) noexcept {
        return angleDegrees * DEGREES_TO_RADIANS;
    }

    /// <summary>
    /// Converts radians to degrees.
    /// </summary>
    [[nodiscard]] constexpr f32 degrees(f32 angleRadians) noexcept {
        return angleRadians * RADIANS_TO_DEGREES;
    }

    /// <summary>
    /// Wraps an angle in radians into the half-open range [-PI, PI).
    /// </summary>
    [[nodiscard]] inline f32 wrap_angle(f32 angleRadians) noexcept {
        const f32 wrapped = std::fmod(angleRadians + PI, TWO_PI);
        if (wrapped < 0.0f) {
            return wrapped + TWO_PI - PI;
        }
        return wrapped - PI;
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Approximate comparison and basic scalar helpers

    /// <summary>
    /// Absolute value usable in constant expressions.
    /// </summary>
    [[nodiscard]] constexpr f32 absolute(f32 value) noexcept {
        return value < 0.0f ? -value : value;
    }

    /// <summary>
    /// True when |a - b| is within epsilon (absolute tolerance). Exactly equal values compare equal
    /// even when infinite (inf - inf is NaN); NaN never compares equal.
    /// </summary>
    [[nodiscard]] constexpr bool nearly_equal(f32 a, f32 b, f32 epsilon = EPSILON) noexcept {
        return a == b || absolute(a - b) <= epsilon;
    }

    /// <summary>
    /// True when |a - b| is within epsilon (absolute tolerance). Exactly equal values compare equal
    /// even when infinite (inf - inf is NaN); NaN never compares equal.
    /// </summary>
    [[nodiscard]] constexpr bool nearly_equal(f64 a, f64 b, f64 epsilon = EPSILON_F64) noexcept {
        if (a == b) {
            return true;
        }
        const f64 difference = a - b;
        return (difference < 0.0 ? -difference : difference) <= epsilon;
    }

    /// <summary>
    /// True when |value| is within epsilon of zero.
    /// </summary>
    [[nodiscard]] constexpr bool nearly_zero(f32 value, f32 epsilon = EPSILON) noexcept {
        return absolute(value) <= epsilon;
    }

    /// <summary>
    /// -1, 0 or 1 depending on the sign of value (0 for zero and NaN).
    /// </summary>
    [[nodiscard]] constexpr f32 sign(f32 value) noexcept {
        if (value > 0.0f) {
            return 1.0f;
        }
        if (value < 0.0f) {
            return -1.0f;
        }
        return 0.0f;
    }

    /// <summary>
    /// value * value.
    /// </summary>
    [[nodiscard]] constexpr f32 square(f32 value) noexcept {
        return value * value;
    }

    /// <summary>
    /// Clamps value into [low, high]. Asserts when low > high.
    /// </summary>
    [[nodiscard]] constexpr f32 clamp(f32 value, f32 low, f32 high) noexcept {
        CONJURE_ASSERT(low <= high, "clamp: low ({}) must not exceed high ({})", low, high);
        if (value < low) {
            return low;
        }
        if (value > high) {
            return high;
        }
        return value;
    }

    /// <summary>
    /// Clamps value into [0, 1].
    /// </summary>
    [[nodiscard]] constexpr f32 saturate(f32 value) noexcept {
        return clamp(value, 0.0f, 1.0f);
    }

    /// <summary>
    /// Linear interpolation from a (t = 0) to b (t = 1); t is not clamped.
    /// </summary>
    [[nodiscard]] constexpr f32 lerp(f32 a, f32 b, f32 t) noexcept {
        return a + (b - a) * t;
    }

    /// <summary>
    /// The t for which lerp(a, b, t) == value. Returns 0 when a == b exactly; any non-zero range,
    /// however narrow, is divided through.
    /// </summary>
    [[nodiscard]] constexpr f32 inverse_lerp(f32 a, f32 b, f32 value) noexcept {
        if (a == b) {
            return 0.0f;
        }
        return (value - a) / (b - a);
    }

    /// <summary>
    /// Maps value from [inLow, inHigh] onto [outLow, outHigh] without clamping.
    /// </summary>
    [[nodiscard]] constexpr f32 remap(f32 value, f32 inLow, f32 inHigh, f32 outLow, f32 outHigh) noexcept {
        return lerp(outLow, outHigh, inverse_lerp(inLow, inHigh, value));
    }

    /// <summary>
    /// Hermite smooth step: 0 at or below edge0, 1 at or above edge1, smooth in between.
    /// </summary>
    [[nodiscard]] constexpr f32 smoothstep(f32 edge0, f32 edge1, f32 value) noexcept {
        const f32 t = saturate(inverse_lerp(edge0, edge1, value));
        return t * t * (3.0f - 2.0f * t);
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Integer helpers
    //
    // The templates below take unsigned values only (u8 .. u64, usize), never bool: sizes, offsets
    // and alignments are unsigned in the engine, and a signed/unsigned mix would hide a negative
    // index behind a conversion. Callers holding a signed index static_cast it first.

    /// <summary>
    /// An unsigned integer type other than bool: what the alignment and power-of-two helpers accept.
    /// </summary>
    template <typename IntegerType>
    concept unsigned_integer = std::unsigned_integral<IntegerType> && !std::same_as<IntegerType, bool>;

    /// <summary>
    /// True when value is a power of two. Zero is not a power of two.
    /// </summary>
    [[nodiscard]] constexpr bool is_power_of_two(u64 value) noexcept {
        return value != 0 && (value & (value - 1)) == 0;
    }

    /// <summary>
    /// Rounds value up to the next multiple of alignment, which must be a power of two
    /// (asserted; a bad alignment returns value unchanged). When the rounded value does not fit
    /// the type (value within alignment - 1 of its maximum) the function asserts and returns value
    /// unchanged rather than wrapping to 0.
    /// </summary>
    template <unsigned_integer IntegerType>
    [[nodiscard]] constexpr IntegerType align_up(
      IntegerType value,
      std::type_identity_t<IntegerType> alignment
    ) noexcept {
        if (!is_power_of_two(alignment)) {
            CONJURE_ASSERT(false, "align_up: alignment {} is not a power of two", alignment);
            return value;
        }
        const IntegerType mask = static_cast<IntegerType>(alignment - 1);
        if (value > static_cast<IntegerType>(std::numeric_limits<IntegerType>::max() - mask)) {
            CONJURE_ASSERT(false, "align_up: {} aligned to {} does not fit the type", value, alignment);
            return value;
        }
        return static_cast<IntegerType>((value + mask) & ~mask);
    }

    /// <summary>
    /// Rounds value down to the previous multiple of alignment, which must be a power of two
    /// (asserted; a bad alignment returns value unchanged).
    /// </summary>
    template <unsigned_integer IntegerType>
    [[nodiscard]] constexpr IntegerType align_down(
      IntegerType value,
      std::type_identity_t<IntegerType> alignment
    ) noexcept {
        if (!is_power_of_two(alignment)) {
            CONJURE_ASSERT(false, "align_down: alignment {} is not a power of two", alignment);
            return value;
        }
        const IntegerType mask = static_cast<IntegerType>(alignment - 1);
        return static_cast<IntegerType>(value & ~mask);
    }

    /// <summary>
    /// True when value is a multiple of alignment (a power of two; asserted, false otherwise).
    /// </summary>
    template <unsigned_integer IntegerType>
    [[nodiscard]] constexpr bool is_aligned(IntegerType value, std::type_identity_t<IntegerType> alignment) noexcept {
        if (!is_power_of_two(alignment)) {
            CONJURE_ASSERT(false, "is_aligned: alignment {} is not a power of two", alignment);
            return false;
        }
        return (value & static_cast<IntegerType>(alignment - 1)) == 0;
    }

    /// <summary>
    /// True when the pointer address is a multiple of alignment.
    /// </summary>
    [[nodiscard]] inline bool is_aligned(const void *pointer, usize alignment) noexcept {
        return is_aligned(reinterpret_cast<usize>(pointer), alignment);
    }

    /// <summary>
    /// The smallest power of two that is >= value; 1 for zero. Returns 0 when the result does not
    /// fit the type (value above the largest representable power of two).
    /// </summary>
    template <unsigned_integer IntegerType>
    [[nodiscard]] constexpr IntegerType next_power_of_two(IntegerType value) noexcept {
        constexpr int HIGHEST_BIT_INDEX = std::numeric_limits<IntegerType>::digits - 1;
        constexpr IntegerType HIGHEST_BIT = static_cast<IntegerType>(IntegerType{1} << HIGHEST_BIT_INDEX);
        if (value > HIGHEST_BIT) {
            return 0;
        }
        return std::bit_ceil(value);
    }

    /// <summary>
    /// The largest power of two that is <= value; 0 for zero.
    /// </summary>
    template <unsigned_integer IntegerType>
    [[nodiscard]] constexpr IntegerType previous_power_of_two(IntegerType value) noexcept {
        return std::bit_floor(value);
    }

    /// <summary>
    /// floor(log2(value)) for value > 0; asserts and returns 0 for zero.
    /// </summary>
    template <unsigned_integer IntegerType>
    [[nodiscard]] constexpr u32 log2_floor(IntegerType value) noexcept {
        if (value == 0) {
            CONJURE_ASSERT(false, "log2_floor: value must be positive");
            return 0;
        }
        return static_cast<u32>(std::bit_width(value) - 1);
    }

    /// <summary>
    /// ceil(numerator / denominator) for integers; asserts and returns 0 when denominator is 0.
    /// </summary>
    template <unsigned_integer IntegerType>
    [[nodiscard]] constexpr IntegerType divide_round_up(
      IntegerType numerator,
      std::type_identity_t<IntegerType> denominator
    ) noexcept {
        if (denominator == 0) {
            CONJURE_ASSERT(false, "divide_round_up: division by zero");
            return 0;
        }
        if (numerator == 0) {
            return 0;
        }
        return static_cast<IntegerType>(1 + (numerator - 1) / denominator);
    }
}
