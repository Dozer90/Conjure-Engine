#include "conjure/testing/doctest.h"

#include "math_test.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/math/math_utils.h"

#include <cstdint>
#include <limits>

using namespace conjure;
using namespace conjure::math;

namespace {
    // Compile-time usability of the constexpr helpers.
    static_assert(nearly_equal(radians(180.0f), PI));
    static_assert(nearly_equal(degrees(PI), 180.0f, 1.0e-4f));
    static_assert(align_up(u32{13}, 8) == 16);
    static_assert(align_up(u64{16}, 16) == 16);
    static_assert(align_down(usize{17}, 16) == 16);
    static_assert(is_power_of_two(1024) && !is_power_of_two(0) && !is_power_of_two(1000));
    static_assert(next_power_of_two(u32{5}) == 8);
    static_assert(previous_power_of_two(u32{5}) == 4);
    static_assert(log2_floor(u64{1} << 40) == 40);
    static_assert(divide_round_up(u32{10}, 3) == 4);
    static_assert(clamp(5.0f, 0.0f, 1.0f) == 1.0f);
    static_assert(lerp(2.0f, 4.0f, 0.5f) == 3.0f);
    static_assert(sign(-3.0f) == -1.0f);

    // Whether align_up accepts a value of the given type (a dependent requires-expression, so a
    // rejected type yields false instead of a compile error).
    template <typename ValueType>
    constexpr bool CAN_ALIGN_UP = requires(ValueType value) { align_up(value, 16); };
}

TEST_SUITE("core/math_utils") {

    TEST_CASE("math_utils - constants are consistent with each other") {
        CHECK(nearly_equal(TWO_PI, 2.0f * PI));
        CHECK(nearly_equal(HALF_PI, PI / 2.0f));
        CHECK(nearly_equal(QUARTER_PI, PI / 4.0f));
        CHECK(nearly_equal(INVERSE_PI * PI, 1.0f));
        CHECK(EPSILON == 1.0e-6f);
        CHECK(F32_MAX == std::numeric_limits<f32>::max());
        CHECK(F32_INFINITY > F32_MAX);
    }

    TEST_CASE("math_utils - radians and degrees convert and round trip") {
        CHECK(nearly_equal(radians(90.0f), HALF_PI));
        CHECK(nearly_equal(radians(-45.0f), -QUARTER_PI));
        CHECK(nearly_equal(degrees(TWO_PI), 360.0f, 1.0e-4f));
        CHECK(nearly_equal(degrees(radians(123.0f)), 123.0f, 1.0e-4f));
        CHECK(radians(0.0f) == 0.0f);
    }

    TEST_CASE("math_utils - wrap_angle maps into the half-open range -PI to PI") {
        CHECK(nearly_equal(wrap_angle(0.0f), 0.0f));
        CHECK(nearly_equal(wrap_angle(PI), -PI, 1.0e-5f));
        CHECK(nearly_equal(wrap_angle(-PI), -PI, 1.0e-5f));
        CHECK(nearly_equal(wrap_angle(TWO_PI), 0.0f, 1.0e-5f));
        CHECK(nearly_equal(wrap_angle(3.0f * PI), -PI, 1.0e-5f));
        CHECK(nearly_equal(wrap_angle(-1.5f * PI), HALF_PI, 1.0e-5f));
        CHECK(nearly_equal(wrap_angle(HALF_PI), HALF_PI, 1.0e-5f));
        CHECK(nearly_equal(wrap_angle(5.0f * TWO_PI + 0.5f), 0.5f, 1.0e-4f));
    }

    TEST_CASE("math_utils - absolute, nearly_equal and nearly_zero") {
        CHECK(absolute(-2.5f) == 2.5f);
        CHECK(absolute(2.5f) == 2.5f);
        CHECK(absolute(0.0f) == 0.0f);

        CHECK(nearly_equal(1.0f, 1.0f + 0.5f * EPSILON));
        CHECK_FALSE(nearly_equal(1.0f, 1.0f + 2.0f * EPSILON));
        CHECK(nearly_equal(1.0f, 1.1f, 0.2f));
        CHECK_FALSE(nearly_equal(1.0f, 1.3f, 0.2f));

        const f32 nan = std::numeric_limits<f32>::quiet_NaN();
        CHECK_FALSE(nearly_equal(nan, nan));
        CHECK_FALSE(nearly_equal(nan, 0.0f));
        CHECK_FALSE(nearly_equal(nan, F32_INFINITY));

        // Equal infinities compare equal although inf - inf is NaN; opposite infinities do not.
        CHECK(nearly_equal(F32_INFINITY, F32_INFINITY));
        CHECK(nearly_equal(-F32_INFINITY, -F32_INFINITY));
        CHECK_FALSE(nearly_equal(F32_INFINITY, -F32_INFINITY));
        CHECK_FALSE(nearly_equal(F32_INFINITY, F32_MAX));

        CHECK(nearly_equal(1.0, 1.0 + 1.0e-13));
        CHECK_FALSE(nearly_equal(1.0, 1.0 + 1.0e-9));
        CHECK(nearly_equal(1.0, 1.5, 0.6));
        const f64 infinity64 = std::numeric_limits<f64>::infinity();
        CHECK(nearly_equal(infinity64, infinity64));
        CHECK_FALSE(nearly_equal(infinity64, -infinity64));
        CHECK_FALSE(nearly_equal(std::numeric_limits<f64>::quiet_NaN(), std::numeric_limits<f64>::quiet_NaN()));

        CHECK(nearly_zero(0.0f));
        CHECK(nearly_zero(-0.5f * EPSILON));
        CHECK_FALSE(nearly_zero(2.0f * EPSILON));
        CHECK(nearly_zero(0.01f, 0.1f));
    }

    TEST_CASE("math_utils - sign and square") {
        CHECK(sign(3.0f) == 1.0f);
        CHECK(sign(-0.001f) == -1.0f);
        CHECK(sign(0.0f) == 0.0f);
        CHECK(sign(-0.0f) == 0.0f);
        CHECK(sign(std::numeric_limits<f32>::quiet_NaN()) == 0.0f);
        CHECK(square(-3.0f) == 9.0f);
        CHECK(square(0.5f) == 0.25f);
    }

    TEST_CASE("math_utils - clamp and saturate") {
        CHECK(clamp(-1.0f, 0.0f, 1.0f) == 0.0f);
        CHECK(clamp(0.5f, 0.0f, 1.0f) == 0.5f);
        CHECK(clamp(7.0f, 0.0f, 1.0f) == 1.0f);
        CHECK(clamp(3.0f, 3.0f, 3.0f) == 3.0f);
        CHECK(saturate(-5.0f) == 0.0f);
        CHECK(saturate(0.25f) == 0.25f);
        CHECK(saturate(5.0f) == 1.0f);
    }

    TEST_CASE("math_utils - clamp with low above high asserts") {
        testing::expect_assert asserts = {};
        const f32 result = clamp(0.5f, 1.0f, 0.0f);
        CHECK(asserts.count() == 1);
        CHECK(result == 1.0f);
    }

    TEST_CASE("math_utils - lerp, inverse_lerp and remap") {
        CHECK(lerp(10.0f, 20.0f, 0.0f) == 10.0f);
        CHECK(lerp(10.0f, 20.0f, 1.0f) == 20.0f);
        CHECK(lerp(10.0f, 20.0f, 0.25f) == 12.5f);
        CHECK(lerp(10.0f, 20.0f, 2.0f) == 30.0f);   // not clamped

        CHECK(inverse_lerp(10.0f, 20.0f, 15.0f) == 0.5f);
        CHECK(inverse_lerp(10.0f, 20.0f, 10.0f) == 0.0f);
        CHECK(inverse_lerp(10.0f, 20.0f, 25.0f) == 1.5f);
        CHECK(inverse_lerp(5.0f, 5.0f, 100.0f) == 0.0f);   // degenerate range

        // Only an exactly empty range collapses to 0; ranges narrower than EPSILON still divide.
        CHECK(nearly_equal(inverse_lerp(0.0f, 5.0e-7f, 2.5e-7f), 0.5f));
        CHECK(nearly_equal(inverse_lerp(1.0f, 1.0f + 2.0e-7f, 1.0f + 2.0e-7f), 1.0f));
        CHECK(nearly_equal(remap(2.5e-7f, 0.0f, 5.0e-7f, 0.0f, 100.0f), 50.0f, 1.0e-3f));
        CHECK(nearly_equal(smoothstep(0.0f, 5.0e-7f, 2.5e-7f), 0.5f));

        CHECK(nearly_equal(remap(5.0f, 0.0f, 10.0f, 100.0f, 200.0f), 150.0f));
        CHECK(nearly_equal(remap(-1.0f, 0.0f, 1.0f, 0.0f, 10.0f), -10.0f));
    }

    TEST_CASE("math_utils - smoothstep is clamped and smooth") {
        CHECK(smoothstep(0.0f, 1.0f, -1.0f) == 0.0f);
        CHECK(smoothstep(0.0f, 1.0f, 0.0f) == 0.0f);
        CHECK(smoothstep(0.0f, 1.0f, 0.5f) == 0.5f);
        CHECK(smoothstep(0.0f, 1.0f, 1.0f) == 1.0f);
        CHECK(smoothstep(0.0f, 1.0f, 2.0f) == 1.0f);
        CHECK(nearly_equal(smoothstep(2.0f, 4.0f, 2.5f), 0.15625f));
        CHECK(smoothstep(0.0f, 1.0f, 0.25f) < 0.25f);
        CHECK(smoothstep(0.0f, 1.0f, 0.75f) > 0.75f);
    }

    TEST_CASE("math_utils - is_power_of_two") {
        CHECK_FALSE(is_power_of_two(0));
        CHECK(is_power_of_two(1));
        CHECK(is_power_of_two(2));
        CHECK_FALSE(is_power_of_two(3));
        CHECK(is_power_of_two(4096));
        CHECK_FALSE(is_power_of_two(4097));
        CHECK(is_power_of_two(u64{1} << 63));
        CHECK_FALSE(is_power_of_two(std::numeric_limits<u64>::max()));
    }

    TEST_CASE("math_utils - align_up rounds to the next multiple for every unsigned width") {
        CHECK(align_up(u32{0}, 16) == 0);
        CHECK(align_up(u32{1}, 16) == 16);
        CHECK(align_up(u32{16}, 16) == 16);
        CHECK(align_up(u32{17}, 16) == 32);
        CHECK(align_up(u32{17}, 1) == 17);
        CHECK(align_up(u64{0xFFFFFFFFull} + 1, 4096) == 0x100000000ull);
        CHECK(align_up(u64{0x100000001ull}, 4096) == 0x100001000ull);
        CHECK(align_up(usize{100}, 64) == 128);
        CHECK(align_up(u8{3}, 4) == 4);
        CHECK(align_up(u16{300}, 256) == 512);

        // The last representable multiple is reached without an overflow check firing.
        CHECK(align_up(u32{0xFFFFFFF0u}, 16) == 0xFFFFFFF0u);
        CHECK(align_up(u32{0xFFFFFFE1u}, 16) == 0xFFFFFFF0u);
        CHECK(align_up(u8{240}, 16) == 240);
        CHECK(align_up(std::numeric_limits<u64>::max(), 1) == std::numeric_limits<u64>::max());
    }

    TEST_CASE("math_utils - align_up asserts instead of wrapping to 0 when the rounded value does not fit") {
        testing::expect_assert asserts = {};
        CHECK(align_up(u32{0xFFFFFFF1u}, 16) == 0xFFFFFFF1u);
        CHECK(asserts.count() == 1);
        CHECK(align_up(u8{250}, 16) == 250);
        CHECK(asserts.count() == 2);
        CHECK(align_up(std::numeric_limits<u64>::max(), 2) == std::numeric_limits<u64>::max());
        CHECK(asserts.count() == 3);
    }

    TEST_CASE("math_utils - integer helpers accept every unsigned width but not bool") {
        static_assert(unsigned_integer<u8> && unsigned_integer<u16> && unsigned_integer<u32>);
        static_assert(unsigned_integer<u64> && unsigned_integer<usize>);
        static_assert(!unsigned_integer<bool>);
        static_assert(!unsigned_integer<i32> && !unsigned_integer<f32>);
        static_assert(!CAN_ALIGN_UP<bool>);
        static_assert(!CAN_ALIGN_UP<i32>);
        static_assert(CAN_ALIGN_UP<u32> && CAN_ALIGN_UP<usize>);
        CHECK(align_up(static_cast<u32>(i32{13}), 8) == 16);   // signed indices are cast by the caller
    }

    TEST_CASE("math_utils - align_up and align_down with a non power-of-two alignment assert and pass through") {
        testing::expect_assert asserts = {};
        CHECK(align_up(u32{13}, 12) == 13);
        CHECK(asserts.count() == 1);
        CHECK(align_down(u32{13}, 0) == 13);
        CHECK(asserts.count() == 2);
    }

    TEST_CASE("math_utils - align_down rounds to the previous multiple") {
        CHECK(align_down(u32{0}, 16) == 0);
        CHECK(align_down(u32{15}, 16) == 0);
        CHECK(align_down(u32{16}, 16) == 16);
        CHECK(align_down(u32{31}, 16) == 16);
        CHECK(align_down(u64{0x100000FFFull}, 4096) == 0x100000000ull);
        CHECK(align_down(usize{129}, 64) == 128);
    }

    TEST_CASE("math_utils - is_aligned for integers and pointers") {
        CHECK(is_aligned(u32{0}, 16));
        CHECK(is_aligned(u32{32}, 16));
        CHECK_FALSE(is_aligned(u32{33}, 16));
        CHECK(is_aligned(usize{1}, 1));

        alignas(64) const u8 buffer[128] = {};
        CHECK(is_aligned(static_cast<const void *>(buffer), 64));
        CHECK(is_aligned(static_cast<const void *>(buffer + 16), 16));
        CHECK_FALSE(is_aligned(static_cast<const void *>(buffer + 1), 2));

        testing::expect_assert asserts = {};
        CHECK_FALSE(is_aligned(u32{12}, 3));
        CHECK(asserts.count() == 1);
    }

    TEST_CASE("math_utils - next_power_of_two") {
        CHECK(next_power_of_two(u32{0}) == 1);
        CHECK(next_power_of_two(u32{1}) == 1);
        CHECK(next_power_of_two(u32{2}) == 2);
        CHECK(next_power_of_two(u32{3}) == 4);
        CHECK(next_power_of_two(u32{1000}) == 1024);
        CHECK(next_power_of_two(u32{1} << 31) == (u32{1} << 31));
        CHECK(next_power_of_two((u32{1} << 31) + 1) == 0);   // does not fit
        CHECK(next_power_of_two(std::numeric_limits<u32>::max()) == 0);
        CHECK(next_power_of_two(u64{1} << 40) == (u64{1} << 40));
        CHECK(next_power_of_two((u64{1} << 40) + 1) == (u64{1} << 41));
        CHECK(next_power_of_two(std::numeric_limits<u64>::max()) == 0);
        CHECK(next_power_of_two(u8{129}) == 0);
        CHECK(next_power_of_two(u8{100}) == 128);
    }

    TEST_CASE("math_utils - previous_power_of_two") {
        CHECK(previous_power_of_two(u32{0}) == 0);
        CHECK(previous_power_of_two(u32{1}) == 1);
        CHECK(previous_power_of_two(u32{3}) == 2);
        CHECK(previous_power_of_two(u32{1023}) == 512);
        CHECK(previous_power_of_two(u32{1024}) == 1024);
        CHECK(previous_power_of_two(std::numeric_limits<u64>::max()) == (u64{1} << 63));
    }

    TEST_CASE("math_utils - log2_floor") {
        CHECK(log2_floor(u32{1}) == 0);
        CHECK(log2_floor(u32{2}) == 1);
        CHECK(log2_floor(u32{3}) == 1);
        CHECK(log2_floor(u32{1024}) == 10);
        CHECK(log2_floor(u32{1025}) == 10);
        CHECK(log2_floor(std::numeric_limits<u64>::max()) == 63);

        testing::expect_assert asserts = {};
        CHECK(log2_floor(u32{0}) == 0);
        CHECK(asserts.count() == 1);
    }

    TEST_CASE("math_utils - divide_round_up") {
        CHECK(divide_round_up(u32{0}, 4) == 0);
        CHECK(divide_round_up(u32{1}, 4) == 1);
        CHECK(divide_round_up(u32{4}, 4) == 1);
        CHECK(divide_round_up(u32{5}, 4) == 2);
        CHECK(divide_round_up(std::numeric_limits<u32>::max(), 2) == 0x80000000u);   // no overflow
        CHECK(divide_round_up(usize{1000}, 333) == 4);

        testing::expect_assert asserts = {};
        CHECK(divide_round_up(u32{5}, 0) == 0);
        CHECK(asserts.count() == 1);
    }
}
