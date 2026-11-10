#pragma once

#include "conjure/core/types.h"

#include <cmath>
#include <concepts>

// A minimal HLSL subset for compiling engine/shaders/shared/tonemap.hlsli natively
// (docs/design/02_renderer.md 10 "test_tonemap_reference"): float3, float3x3 (row-major literal
// order, mul(matrix, vector) as HLSL), saturate, max, pow, log2 and dot. The shader file is included
// inside namespace conjure::testing::hlsl by test_tonemap_reference.cpp, so these names resolve the
// way the HLSL compiler resolves its intrinsics. Scalar operands accept double so the shader's
// unsuffixed literals compile unchanged.

namespace conjure::testing::hlsl {

    template <typename T>
    concept scalar = std::integral<T> || std::floating_point<T>;

    struct float3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        constexpr float3() noexcept = default;

        template <scalar A, scalar B, scalar C>
        constexpr float3(A a, B b, C c) noexcept :
            x(static_cast<float>(a)),
            y(static_cast<float>(b)),
            z(static_cast<float>(c)) {}

        template <scalar A>
        constexpr explicit float3(A all) noexcept :
            x(static_cast<float>(all)),
            y(static_cast<float>(all)),
            z(static_cast<float>(all)) {}
    };

    /// <summary>
    /// Three rows, exactly the literal order of an HLSL float3x3(...) constructor.
    /// </summary>
    struct float3x3 {
        float3 rows[3] = {};

        template <scalar... Values>
            requires(sizeof...(Values) == 9)
        constexpr explicit float3x3(Values... values) noexcept {
            const float flat[9] = {static_cast<float>(values)...};
            rows[0] = float3(flat[0], flat[1], flat[2]);
            rows[1] = float3(flat[3], flat[4], flat[5]);
            rows[2] = float3(flat[6], flat[7], flat[8]);
        }
    };

    [[nodiscard]] constexpr float3 operator+(const float3 &a, const float3 &b) noexcept {
        return float3(a.x + b.x, a.y + b.y, a.z + b.z);
    }

    [[nodiscard]] constexpr float3 operator-(const float3 &a, const float3 &b) noexcept {
        return float3(a.x - b.x, a.y - b.y, a.z - b.z);
    }

    [[nodiscard]] constexpr float3 operator*(const float3 &a, const float3 &b) noexcept {
        return float3(a.x * b.x, a.y * b.y, a.z * b.z);
    }

    [[nodiscard]] constexpr float3 operator/(const float3 &a, const float3 &b) noexcept {
        return float3(a.x / b.x, a.y / b.y, a.z / b.z);
    }

    template <scalar S>
    [[nodiscard]] constexpr float3 operator+(const float3 &a, S b) noexcept {
        const float s = static_cast<float>(b);
        return float3(a.x + s, a.y + s, a.z + s);
    }

    template <scalar S>
    [[nodiscard]] constexpr float3 operator+(S a, const float3 &b) noexcept {
        return b + a;
    }

    template <scalar S>
    [[nodiscard]] constexpr float3 operator-(const float3 &a, S b) noexcept {
        const float s = static_cast<float>(b);
        return float3(a.x - s, a.y - s, a.z - s);
    }

    template <scalar S>
    [[nodiscard]] constexpr float3 operator-(S a, const float3 &b) noexcept {
        const float s = static_cast<float>(a);
        return float3(s - b.x, s - b.y, s - b.z);
    }

    template <scalar S>
    [[nodiscard]] constexpr float3 operator*(const float3 &a, S b) noexcept {
        const float s = static_cast<float>(b);
        return float3(a.x * s, a.y * s, a.z * s);
    }

    template <scalar S>
    [[nodiscard]] constexpr float3 operator*(S a, const float3 &b) noexcept {
        return b * a;
    }

    template <scalar S>
    [[nodiscard]] constexpr float3 operator/(const float3 &a, S b) noexcept {
        const float s = static_cast<float>(b);
        return float3(a.x / s, a.y / s, a.z / s);
    }

    constexpr float3 &operator+=(float3 &a, const float3 &b) noexcept {
        a = a + b;
        return a;
    }

    [[nodiscard]] constexpr float saturate(float value) noexcept {
        return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    }

    [[nodiscard]] constexpr float3 saturate(const float3 &value) noexcept {
        return float3(saturate(value.x), saturate(value.y), saturate(value.z));
    }

    template <scalar A, scalar B>
    [[nodiscard]] constexpr float max(A a, B b) noexcept {
        const float left = static_cast<float>(a);
        const float right = static_cast<float>(b);
        return left > right ? left : right;
    }

    template <scalar S>
    [[nodiscard]] constexpr float3 max(const float3 &a, S b) noexcept {
        return float3(max(a.x, b), max(a.y, b), max(a.z, b));
    }

    [[nodiscard]] constexpr float3 max(const float3 &a, const float3 &b) noexcept {
        return float3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
    }

    [[nodiscard]] inline float3 pow(const float3 &base, const float3 &exponent) noexcept {
        return float3(std::pow(base.x, exponent.x), std::pow(base.y, exponent.y), std::pow(base.z, exponent.z));
    }

    template <scalar S>
    [[nodiscard]] inline float3 pow(const float3 &base, S exponent) noexcept {
        const float e = static_cast<float>(exponent);
        return float3(std::pow(base.x, e), std::pow(base.y, e), std::pow(base.z, e));
    }

    [[nodiscard]] inline float3 log2(const float3 &value) noexcept {
        return float3(std::log2(value.x), std::log2(value.y), std::log2(value.z));
    }

    [[nodiscard]] constexpr float dot(const float3 &a, const float3 &b) noexcept {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    /// <summary>
    /// HLSL mul(matrix, vector): each row dotted with the vector.
    /// </summary>
    [[nodiscard]] constexpr float3 mul(const float3x3 &matrix, const float3 &vector) noexcept {
        return float3(dot(matrix.rows[0], vector), dot(matrix.rows[1], vector), dot(matrix.rows[2], vector));
    }

    /// <summary>
    /// HLSL mul(vector, matrix): the vector's components weight the rows.
    /// </summary>
    [[nodiscard]] constexpr float3 mul(const float3 &vector, const float3x3 &matrix) noexcept {
        return matrix.rows[0] * vector.x + matrix.rows[1] * vector.y + matrix.rows[2] * vector.z;
    }
}
