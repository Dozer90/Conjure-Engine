#include "conjure/testing/doctest.h"

#include "conjure/rhi/texture_format.h"

#include "conjure/core/error.h"

#include <array>
#include <string_view>

using namespace conjure;
using namespace conjure::rhi;

namespace {
    struct format_expectation {
        texture_format format = texture_format::UNKNOWN;
        std::string_view name = "";
        u32 bytesPerPixel = 0;
        u32 bytesPerBlock = 0;
        u32 blockSize = 0;
        bool depth = false;
        bool srgb = false;
        bool compressed = false;
    };

    // One row per enumerator, in declaration order, so a new format must be added here too.
    constexpr std::array<format_expectation, static_cast<usize>(texture_format::COUNT)> EXPECTATIONS = {{
        {texture_format::UNKNOWN, "UNKNOWN", 0, 0, 0, false, false, false},
        {texture_format::R8_UNORM, "R8_UNORM", 1, 1, 1, false, false, false},
        {texture_format::RG8_UNORM, "RG8_UNORM", 2, 2, 1, false, false, false},
        {texture_format::RGBA8_UNORM, "RGBA8_UNORM", 4, 4, 1, false, false, false},
        {texture_format::RGBA8_SRGB, "RGBA8_SRGB", 4, 4, 1, false, true, false},
        {texture_format::BGRA8_SRGB, "BGRA8_SRGB", 4, 4, 1, false, true, false},
        {texture_format::RG16_SNORM, "RG16_SNORM", 4, 4, 1, false, false, false},
        {texture_format::RGBA16_SNORM, "RGBA16_SNORM", 8, 8, 1, false, false, false},
        {texture_format::R16_FLOAT, "R16_FLOAT", 2, 2, 1, false, false, false},
        {texture_format::RG16_FLOAT, "RG16_FLOAT", 4, 4, 1, false, false, false},
        {texture_format::RGBA16_FLOAT, "RGBA16_FLOAT", 8, 8, 1, false, false, false},
        {texture_format::R32_FLOAT, "R32_FLOAT", 4, 4, 1, false, false, false},
        {texture_format::RG32_FLOAT, "RG32_FLOAT", 8, 8, 1, false, false, false},
        {texture_format::RGB32_FLOAT, "RGB32_FLOAT", 12, 12, 1, false, false, false},
        {texture_format::RGBA32_FLOAT, "RGBA32_FLOAT", 16, 16, 1, false, false, false},
        {texture_format::R32_UINT, "R32_UINT", 4, 4, 1, false, false, false},
        {texture_format::R11G11B10_FLOAT, "R11G11B10_FLOAT", 4, 4, 1, false, false, false},
        {texture_format::D32_FLOAT, "D32_FLOAT", 4, 4, 1, true, false, false},
        {texture_format::BC1_SRGB, "BC1_SRGB", 0, 8, 4, false, true, true},
        {texture_format::BC5_UNORM, "BC5_UNORM", 0, 16, 4, false, false, true},
        {texture_format::BC7_SRGB, "BC7_SRGB", 0, 16, 4, false, true, true},
    }};
}

TEST_SUITE("rhi/texture_format") {

    TEST_CASE("texture_format - the expectation table covers every enumerator in order") {
        for (usize index = 0; index < EXPECTATIONS.size(); ++index) {
            CAPTURE(index);
            CHECK(static_cast<usize>(EXPECTATIONS[index].format) == index);
        }
    }

    TEST_CASE("texture_format - bytes_per_pixel, bytes_per_block and block_size match the table") {
        for (const format_expectation &expected : EXPECTATIONS) {
            CAPTURE(expected.name);
            CHECK(bytes_per_pixel(expected.format) == expected.bytesPerPixel);
            CHECK(bytes_per_block(expected.format) == expected.bytesPerBlock);
            CHECK(block_size(expected.format) == expected.blockSize);
        }
        CHECK(bytes_per_pixel(texture_format::COUNT) == 0);
        CHECK(block_size(texture_format::COUNT) == 0);
    }

    TEST_CASE("texture_format - is_depth, is_srgb and is_compressed match the table") {
        for (const format_expectation &expected : EXPECTATIONS) {
            CAPTURE(expected.name);
            CHECK(is_depth(expected.format) == expected.depth);
            CHECK(is_srgb(expected.format) == expected.srgb);
            CHECK(is_compressed(expected.format) == expected.compressed);
        }
    }

    TEST_CASE("texture_format - the helpers are usable in constant expressions") {
        static_assert(bytes_per_pixel(texture_format::RGBA16_FLOAT) == 8);
        static_assert(is_depth(texture_format::D32_FLOAT));
        static_assert(is_srgb(texture_format::BC7_SRGB));
        static_assert(is_compressed(texture_format::BC5_UNORM));
        static_assert(row_pitch(texture_format::RGBA8_UNORM, 3) == 12);
        CHECK(true);
    }

    TEST_CASE("texture_format - row_pitch packs rows tightly and rounds compressed rows up to blocks") {
        CHECK(row_pitch(texture_format::R8_UNORM, 7) == 7);
        CHECK(row_pitch(texture_format::RGBA32_FLOAT, 3) == 48);
        CHECK(row_pitch(texture_format::BC1_SRGB, 4) == 8);
        CHECK(row_pitch(texture_format::BC1_SRGB, 5) == 16);
        CHECK(row_pitch(texture_format::BC7_SRGB, 9) == 48);
        CHECK(row_pitch(texture_format::RGBA8_UNORM, 0) == 0);
        CHECK(row_pitch(texture_format::UNKNOWN, 16) == 0);
        CHECK(row_pitch(texture_format::RGBA8_UNORM, 0xFFFFFFFFu) == 0xFFFFFFFFull * 4);
    }

    TEST_CASE("texture_format - to_string names every enumerator and UNKNOWN for out-of-range values") {
        for (const format_expectation &expected : EXPECTATIONS) {
            CAPTURE(expected.name);
            CHECK(to_string(expected.format) == expected.name);
        }
        CHECK(to_string(texture_format::COUNT) == "UNKNOWN");
        CHECK(to_string(static_cast<texture_format>(200)) == "UNKNOWN");
    }

    TEST_CASE("texture_format - parse_texture_format round-trips every name, ignoring case and whitespace") {
        for (const format_expectation &expected : EXPECTATIONS) {
            CAPTURE(expected.name);
            const core::result<texture_format> parsed = parse_texture_format(expected.name);
            CONJURE_REQUIRE_OK(parsed);
            CHECK(parsed.value() == expected.format);
        }
        CHECK(parse_texture_format("rgba8_srgb").value() == texture_format::RGBA8_SRGB);
        CHECK(parse_texture_format("  d32_Float ").value() == texture_format::D32_FLOAT);
    }

    TEST_CASE("texture_format - parse_texture_format rejects names that are not formats") {
        const std::array<std::string_view, 5> rejected = {"", "RGBA8", "RGBA8_SRGB_", "COUNT", "R8G8B8A8_UNORM"};
        for (const std::string_view name : rejected) {
            CAPTURE(name);
            CONJURE_CHECK_ERROR(parse_texture_format(name), core::error_code::INVALID_ARGUMENT);
        }
    }
}
