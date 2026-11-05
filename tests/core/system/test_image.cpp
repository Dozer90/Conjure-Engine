#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/temp_directory.h"

#include "conjure/core/image.h"

#include <stb_image_write.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;

namespace {
    rgba8 random_color(std::mt19937_64 &engine) {
        std::uniform_int_distribution<int> channel(0, 255);
        rgba8 color = {};
        color.r = static_cast<u8>(channel(engine));
        color.g = static_cast<u8>(channel(engine));
        color.b = static_cast<u8>(channel(engine));
        color.a = static_cast<u8>(channel(engine));
        return color;
    }

    image random_image(u32 width, u32 height, u64 seed = 0x5EED) {
        std::mt19937_64 engine(seed);
        image result(width, height);
        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                result.set_pixel(x, y, random_color(engine));
            }
        }
        return result;
    }

    image solid(u32 width, u32 height, rgba8 color) {
        image result(width, height);
        result.fill(color);
        return result;
    }

    // Encodes raw pixels with 'channels' components per pixel through stb directly, to produce
    // the grey / grey+alpha / RGB files image::load_png has to expand.
    std::vector<std::byte> encode_raw_png(int width, int height, int channels, const std::vector<u8> &pixels) {
        std::vector<std::byte> encoded = {};
        const auto collect = [](void *context, void *bytes, int size) {
            std::vector<std::byte> *target = static_cast<std::vector<std::byte> *>(context);
            const std::byte *begin = static_cast<const std::byte *>(bytes);
            target->insert(target->end(), begin, begin + size);
        };
        const int ok = stbi_write_png_to_func(
          collect,
          &encoded,
          width,
          height,
          channels,
          pixels.data(),
          width * channels
        );
        REQUIRE(ok != 0);
        return encoded;
    }

    std::vector<std::byte> bytes_of(std::string_view text) {
        std::vector<std::byte> bytes = {};
        for (const char character : text) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        return bytes;
    }

    constexpr std::array<u8, 8> PNG_SIGNATURE = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    // Hand-made files stb_image_write cannot produce. A 2x2 RGB image with 16 bits per channel:
    // row 0 = (FF00 0000 8000) (1234 ABCD FFFF), row 1 = (0000 4080 0000) (7F00 8100 0080).
    constexpr std::array<u8, 89> RGB16_PNG = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
        0x10, 0x02, 0x00, 0x00, 0x00, 0xAD, 0x44, 0x46, 0x30, 0x00, 0x00, 0x00,
        0x20, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63, 0xF8, 0xCF, 0xC0, 0xC0,
        0xD0, 0xC0, 0x20, 0x64, 0xB2, 0xFA, 0xEC, 0x7F, 0x10, 0xD3, 0xA1, 0x81,
        0x81, 0xA1, 0x9E, 0xA1, 0x91, 0x81, 0xA1, 0x01, 0x00, 0x6A, 0x0B, 0x07,
        0x7C, 0x09, 0xA9, 0xCF, 0x09, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E,
        0x44, 0xAE, 0x42, 0x60, 0x82,
    };

    // A 2x2 palette image: entries red, green, blue, (10, 20, 30); tRNS makes the last entry
    // alpha 128. Indices row 0 = 0 1, row 1 = 2 3.
    constexpr std::array<u8, 111> PALETTE_PNG = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x03, 0x00, 0x00, 0x00, 0x45, 0x68, 0xFD, 0x16, 0x00, 0x00, 0x00,
        0x0C, 0x50, 0x4C, 0x54, 0x45, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,
        0x00, 0xFF, 0x0A, 0x14, 0x1E, 0x22, 0x88, 0x29, 0x04, 0x00, 0x00, 0x00,
        0x04, 0x74, 0x52, 0x4E, 0x53, 0xFF, 0xFF, 0xFF, 0x80, 0xAD, 0x92, 0x2A,
        0xD4, 0x00, 0x00, 0x00, 0x0E, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63,
        0x60, 0x60, 0x64, 0x60, 0x62, 0x06, 0x00, 0x00, 0x11, 0x00, 0x07, 0x83,
        0xCA, 0x64, 0x64, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE,
        0x42, 0x60, 0x82,
    };

    template <usize N>
    std::vector<std::byte> bytes_of(const std::array<u8, N> &values) {
        std::vector<std::byte> bytes = {};
        for (const u8 value : values) {
            bytes.push_back(std::byte{value});
        }
        return bytes;
    }

    bool starts_with_png_signature(std::span<const std::byte> data) {
        if (data.size() < PNG_SIGNATURE.size()) {
            return false;
        }
        for (usize index = 0; index < PNG_SIGNATURE.size(); ++index) {
            if (static_cast<u8>(data[index]) != PNG_SIGNATURE[index]) {
                return false;
            }
        }
        return true;
    }
}

TEST_SUITE("core/image") {

    TEST_CASE("image - a default image is empty") {
        const image empty = {};
        CHECK(empty.width() == 0);
        CHECK(empty.height() == 0);
        CHECK(empty.is_empty());
        CHECK(empty.pixels().empty());
    }

    TEST_CASE("image - a sized image is zero-filled and reports its dimensions") {
        const image sized(3, 2);
        CHECK(sized.width() == 3);
        CHECK(sized.height() == 2);
        CHECK_FALSE(sized.is_empty());
        REQUIRE(sized.pixels().size() == 3 * 2 * image::BYTES_PER_PIXEL);
        for (const std::byte value : sized.pixels()) {
            CHECK(value == std::byte{0});
        }
        CHECK(sized.pixel(2, 1) == rgba8{0, 0, 0, 0});
    }

    TEST_CASE("image - zero or oversized dimensions yield the empty image") {
        const image noWidth(0, 5);
        CHECK(noWidth.is_empty());
        CHECK(noWidth.width() == 0);
        CHECK(noWidth.height() == 0);

        const image noHeight(5, 0);
        CHECK(noHeight.is_empty());

        testing::expect_assert asserts = {};
        const image tooWide(image::MAX_DIMENSION + 1, 1);
        CHECK(asserts.count() == 1);
        CHECK(tooWide.is_empty());
        CHECK(tooWide.width() == 0);
    }

    TEST_CASE("image - pixels are stored row-major RGBA") {
        image picture(3, 2);
        picture.set_pixel(1, 0, rgba8{10, 20, 30, 40});
        picture.set_pixel(2, 1, rgba8{50, 60, 70, 80});

        CHECK(picture.pixel(1, 0) == rgba8{10, 20, 30, 40});
        CHECK(picture.pixel(2, 1) == rgba8{50, 60, 70, 80});
        CHECK(picture.pixel(0, 0) == rgba8{0, 0, 0, 0});

        const std::span<const std::byte> bytes = picture.pixels();
        CHECK(static_cast<u8>(bytes[4]) == 10);
        CHECK(static_cast<u8>(bytes[5]) == 20);
        CHECK(static_cast<u8>(bytes[6]) == 30);
        CHECK(static_cast<u8>(bytes[7]) == 40);

        const usize offset = (1 * 3 + 2) * image::BYTES_PER_PIXEL;
        CHECK(static_cast<u8>(bytes[offset + 0]) == 50);
        CHECK(static_cast<u8>(bytes[offset + 3]) == 80);

        // Writing through the span is the same as set_pixel.
        picture.pixels()[0] = std::byte{255};
        CHECK(picture.pixel(0, 0).r == 255);
    }

    TEST_CASE("image - the default rgba8 is opaque black") {
        constexpr rgba8 color = {};
        CHECK(color.r == 0);
        CHECK(color.g == 0);
        CHECK(color.b == 0);
        CHECK(color.a == 255);
    }

    TEST_CASE("image - out-of-range pixel access asserts and is harmless") {
        testing::expect_assert asserts = {};
        image picture(2, 2);

        CHECK(picture.pixel(2, 0) == rgba8{});
        CHECK(asserts.count() == 1);
        CHECK(picture.pixel(0, 2) == rgba8{});
        CHECK(asserts.count() == 2);

        picture.set_pixel(5, 5, rgba8{1, 2, 3, 4});
        CHECK(asserts.count() == 3);
        for (const std::byte value : picture.pixels()) {
            CHECK(value == std::byte{0});
        }

        const image empty = {};
        CHECK(empty.pixel(0, 0) == rgba8{});
        CHECK(asserts.count() == 4);
    }

    TEST_CASE("image - fill sets every pixel") {
        image picture(4, 3);
        picture.fill(rgba8{1, 2, 3, 4});
        for (u32 y = 0; y < 3; ++y) {
            for (u32 x = 0; x < 4; ++x) {
                CHECK(picture.pixel(x, y) == rgba8{1, 2, 3, 4});
            }
        }

        image empty = {};
        empty.fill(rgba8{9, 9, 9, 9});
        CHECK(empty.is_empty());
    }

    TEST_CASE("image - PNG encode and decode round trip") {
        const image original = random_image(7, 5);
        const result<std::vector<std::byte>> encoded = original.encode_png();
        REQUIRE(encoded.has_value());
        CHECK(starts_with_png_signature(*encoded));

        const result<image> decoded = image::load_png(*encoded);
        REQUIRE(decoded.has_value());
        CHECK(decoded->width() == 7);
        CHECK(decoded->height() == 5);
        CHECK(*decoded == original);
        CHECK(decoded->compare(original) == 0.0);
    }

    TEST_CASE("image - a one-pixel image round trips") {
        const image dot = solid(1, 1, rgba8{200, 100, 50, 25});
        const result<std::vector<std::byte>> encoded = dot.encode_png();
        REQUIRE(encoded.has_value());
        const result<image> decoded = image::load_png(*encoded);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == dot);
    }

    TEST_CASE("image - grey, grey-alpha and RGB PNGs are expanded to RGBA") {
        SUBCASE("grey") {
            const std::vector<u8> grey = {0, 128, 255, 7};
            const result<image> decoded = image::load_png(encode_raw_png(2, 2, 1, grey));
            REQUIRE(decoded.has_value());
            CHECK(decoded->pixel(0, 0) == rgba8{0, 0, 0, 255});
            CHECK(decoded->pixel(1, 0) == rgba8{128, 128, 128, 255});
            CHECK(decoded->pixel(0, 1) == rgba8{255, 255, 255, 255});
            CHECK(decoded->pixel(1, 1) == rgba8{7, 7, 7, 255});
        }
        SUBCASE("grey with alpha") {
            const std::vector<u8> greyAlpha = {10, 20, 30, 40};
            const result<image> decoded = image::load_png(encode_raw_png(2, 1, 2, greyAlpha));
            REQUIRE(decoded.has_value());
            CHECK(decoded->pixel(0, 0) == rgba8{10, 10, 10, 20});
            CHECK(decoded->pixel(1, 0) == rgba8{30, 30, 30, 40});
        }
        SUBCASE("rgb") {
            const std::vector<u8> rgb = {1, 2, 3, 4, 5, 6};
            const result<image> decoded = image::load_png(encode_raw_png(1, 2, 3, rgb));
            REQUIRE(decoded.has_value());
            CHECK(decoded->pixel(0, 0) == rgba8{1, 2, 3, 255});
            CHECK(decoded->pixel(0, 1) == rgba8{4, 5, 6, 255});
        }
    }

    TEST_CASE("image - 16-bit and palette PNGs are converted to RGBA8") {
        SUBCASE("16 bits per channel keep the high byte") {
            const result<image> decoded = image::load_png(bytes_of(RGB16_PNG));
            REQUIRE(decoded.has_value());
            CHECK(decoded->width() == 2);
            CHECK(decoded->height() == 2);
            CHECK(decoded->pixel(0, 0) == rgba8{0xFF, 0x00, 0x80, 255});
            CHECK(decoded->pixel(1, 0) == rgba8{0x12, 0xAB, 0xFF, 255});
            CHECK(decoded->pixel(0, 1) == rgba8{0x00, 0x40, 0x00, 255});
            CHECK(decoded->pixel(1, 1) == rgba8{0x7F, 0x81, 0x00, 255});
        }
        SUBCASE("palette entries are expanded and tRNS supplies alpha") {
            const result<image> decoded = image::load_png(bytes_of(PALETTE_PNG));
            REQUIRE(decoded.has_value());
            CHECK(decoded->width() == 2);
            CHECK(decoded->height() == 2);
            CHECK(decoded->pixel(0, 0) == rgba8{255, 0, 0, 255});
            CHECK(decoded->pixel(1, 0) == rgba8{0, 255, 0, 255});
            CHECK(decoded->pixel(0, 1) == rgba8{0, 0, 255, 255});
            CHECK(decoded->pixel(1, 1) == rgba8{10, 20, 30, 128});
        }
    }

    TEST_CASE("image - load_png rejects empty and malformed data") {
        const result<image> empty = image::load_png(std::span<const std::byte>());
        REQUIRE_FALSE(empty.has_value());
        CHECK(empty.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        const std::vector<std::byte> text = bytes_of("definitely not a png file");
        const result<image> notPng = image::load_png(text);
        REQUIRE_FALSE(notPng.has_value());
        CHECK(notPng.get_error().generic_code() == error_code::PARSE_FAILURE);
        CHECK_FALSE(notPng.get_error().message().empty());

        std::vector<std::byte> garbage = {};
        for (const u8 value : PNG_SIGNATURE) {
            garbage.push_back(std::byte{value});
        }
        const std::vector<std::byte> junk = bytes_of("garbage after a valid signature....");
        garbage.insert(garbage.end(), junk.begin(), junk.end());
        const result<image> signatureOnly = image::load_png(garbage);
        REQUIRE_FALSE(signatureOnly.has_value());
        CHECK(signatureOnly.get_error().generic_code() == error_code::PARSE_FAILURE);
        CHECK_FALSE(signatureOnly.get_error().message().empty());

        const result<std::vector<std::byte>> valid = random_image(4, 4).encode_png();
        REQUIRE(valid.has_value());
        const std::span<const std::byte> truncated(valid->data(), valid->size() / 2);
        const result<image> cut = image::load_png(truncated);
        REQUIRE_FALSE(cut.has_value());
        CHECK(cut.get_error().generic_code() == error_code::PARSE_FAILURE);
    }

    TEST_CASE("image - save_png and load_file round trip through a file") {
        const testing::temp_directory directory("core_image");
        const std::filesystem::path path = directory.path() / "picture.png";
        const image original = random_image(9, 6, 0xC0FFEE);

        REQUIRE(original.save_png(path).has_value());
        CHECK(std::filesystem::exists(path));

        const result<image> loaded = image::load_file(path);
        REQUIRE(loaded.has_value());
        CHECK(*loaded == original);

        // Saving again replaces the file rather than appending to it.
        const image replacement = solid(2, 2, rgba8{1, 1, 1, 1});
        REQUIRE(replacement.save_png(path).has_value());
        const result<image> reloaded = image::load_file(path);
        REQUIRE(reloaded.has_value());
        CHECK(*reloaded == replacement);
    }

    TEST_CASE("image - load_file reports NOT_FOUND for a missing file and PARSE_FAILURE for a non-PNG file") {
        const testing::temp_directory directory("core_image_bad");

        const result<image> missing = image::load_file(directory.path() / "missing.png");
        REQUIRE_FALSE(missing.has_value());
        CHECK(missing.get_error().generic_code() == error_code::NOT_FOUND);
        CHECK(missing.get_error().message().find("missing.png") != std::string::npos);

        const std::filesystem::path textFile = directory.write_file("text.png", "hello");
        const result<image> text = image::load_file(textFile);
        REQUIRE_FALSE(text.has_value());
        CHECK(text.get_error().generic_code() == error_code::PARSE_FAILURE);

        const result<image> isDirectory = image::load_file(directory.path());
        REQUIRE_FALSE(isDirectory.has_value());
        CHECK(isDirectory.get_error().generic_code() == error_code::NOT_FOUND);
    }

    TEST_CASE("image - save_png fails with IO_FAILURE when the directory does not exist") {
        const testing::temp_directory directory("core_image_nodir");
        const image picture = solid(2, 2, rgba8{});
        const result<void> saved = picture.save_png(directory.path() / "no_such_dir" / "picture.png");

        REQUIRE_FALSE(saved.has_value());
        CHECK(saved.get_error().generic_code() == error_code::IO_FAILURE);
        CHECK(saved.get_error().message().find("picture.png") != std::string::npos);
    }

    TEST_CASE("image - encode_png and save_png refuse an empty image") {
        const testing::temp_directory directory("core_image_empty");
        const image empty = {};

        const result<std::vector<std::byte>> encoded = empty.encode_png();
        REQUIRE_FALSE(encoded.has_value());
        CHECK(encoded.get_error().generic_code() == error_code::INVALID_STATE);

        const result<void> saved = empty.save_png(directory.path() / "empty.png");
        REQUIRE_FALSE(saved.has_value());
        CHECK(saved.get_error().generic_code() == error_code::INVALID_STATE);
        CHECK_FALSE(std::filesystem::exists(directory.path() / "empty.png"));
    }

    TEST_CASE("image - flip_vertical reverses rows") {
        SUBCASE("odd height") {
            image picture(2, 3);
            picture.set_pixel(0, 0, rgba8{1, 0, 0, 0});
            picture.set_pixel(1, 0, rgba8{2, 0, 0, 0});
            picture.set_pixel(0, 1, rgba8{3, 0, 0, 0});
            picture.set_pixel(1, 1, rgba8{4, 0, 0, 0});
            picture.set_pixel(0, 2, rgba8{5, 0, 0, 0});
            picture.set_pixel(1, 2, rgba8{6, 0, 0, 0});
            const image original = picture;

            picture.flip_vertical();
            CHECK(picture.pixel(0, 0).r == 5);
            CHECK(picture.pixel(1, 0).r == 6);
            CHECK(picture.pixel(0, 1).r == 3);
            CHECK(picture.pixel(1, 1).r == 4);
            CHECK(picture.pixel(0, 2).r == 1);
            CHECK(picture.pixel(1, 2).r == 2);

            picture.flip_vertical();
            CHECK(picture == original);
        }
        SUBCASE("even height") {
            image picture = random_image(5, 4);
            const image original = picture;
            picture.flip_vertical();
            CHECK(picture != original);
            for (u32 y = 0; y < 4; ++y) {
                for (u32 x = 0; x < 5; ++x) {
                    CHECK(picture.pixel(x, y) == original.pixel(x, 3 - y));
                }
            }
            picture.flip_vertical();
            CHECK(picture == original);
        }
        SUBCASE("single row and empty image are unchanged") {
            image row = random_image(6, 1);
            const image original = row;
            row.flip_vertical();
            CHECK(row == original);

            image empty = {};
            empty.flip_vertical();
            CHECK(empty.is_empty());
        }
    }

    TEST_CASE("image - compare measures the mean absolute difference") {
        const image black = solid(4, 4, rgba8{0, 0, 0, 0});
        const image white = solid(4, 4, rgba8{255, 255, 255, 255});
        const image half = solid(4, 4, rgba8{255, 255, 0, 0});

        CHECK(black.compare(black) == 0.0);
        CHECK(black.compare(white) == 1.0);
        CHECK(white.compare(black) == 1.0);
        CHECK(black.compare(half) == doctest::Approx(0.5));
        CHECK(half.compare(black) == doctest::Approx(0.5));

        // One byte off by one in a 4x4 image: 1 / (64 * 255).
        image almost = black;
        almost.set_pixel(0, 0, rgba8{1, 0, 0, 0});
        CHECK(black.compare(almost) == doctest::Approx(1.0 / (64.0 * 255.0)));

        CHECK(black.compare(solid(4, 5, rgba8{0, 0, 0, 0})) == 1.0);
        CHECK(black.compare(image()) == 1.0);
        CHECK(image().compare(image()) == 0.0);
    }

    TEST_CASE("image - equality compares dimensions and pixels") {
        const image a = random_image(3, 3);
        const image b = random_image(3, 3);
        const image c = random_image(3, 3, 99);
        CHECK(a == b);
        CHECK_FALSE(a != b);
        CHECK(a != c);
        CHECK(a != random_image(3, 4));
        CHECK(a != random_image(4, 3));
        CHECK(image() == image());
        CHECK(image() != a);
    }

    TEST_CASE("image - copies and moves are deep and leave the source as expected") {
        const image original = random_image(3, 2);
        image copy = original;
        CHECK(copy == original);
        copy.set_pixel(0, 0, rgba8{9, 9, 9, 9});
        CHECK(copy != original);

        image moved = std::move(copy);
        CHECK(moved.pixel(0, 0) == rgba8{9, 9, 9, 9});
        CHECK(moved.width() == 3);
    }

    TEST_CASE("image - a larger image round trips through PNG") {
        image gradient(256, 256);
        for (u32 y = 0; y < 256; ++y) {
            for (u32 x = 0; x < 256; ++x) {
                gradient.set_pixel(x, y, rgba8{static_cast<u8>(x), static_cast<u8>(y), static_cast<u8>(x ^ y), 255});
            }
        }
        const result<std::vector<std::byte>> encoded = gradient.encode_png();
        REQUIRE(encoded.has_value());
        CHECK(encoded->size() < gradient.pixels().size());   // the gradient compresses

        const result<image> decoded = image::load_png(*encoded);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == gradient);
    }
}
