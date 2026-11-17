#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/temp_directory.h"
#include "renderer_test_support.h"

#include "conjure/renderer/jpeg_xl.h"
#include "conjure/renderer/renderer_error.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_payload.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/assets_error.h"
#include "conjure/core/image.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/json/document.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/memory_file_system.h"
#include "conjure/rhi/texture_format.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// texture_asset.h (docs/design/02_renderer.md 2.6, 4.2): RGBA8 textures with CPU mips, decoding
// through stb_image and libjxl, the .ctex blob, the import settings, the importer and the loader. The JPEG XL
// decoder's own behaviour (colour, layouts, orientation, animation, failures) is covered in test_jpeg_xl.cpp.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    [[nodiscard]] std::vector<std::byte> solid_pixels(u32 width, u32 height, core::rgba8 colour) {
        std::vector<std::byte> pixels = {};
        pixels.reserve(static_cast<usize>(width) * height * 4);
        for (u32 index = 0; index < width * height; ++index) {
            pixels.push_back(static_cast<std::byte>(colour.r));
            pixels.push_back(static_cast<std::byte>(colour.g));
            pixels.push_back(static_cast<std::byte>(colour.b));
            pixels.push_back(static_cast<std::byte>(colour.a));
        }
        return pixels;
    }

    [[nodiscard]] std::vector<std::byte> png_of(u32 width, u32 height, core::rgba8 colour) {
        core::image image(width, height);
        image.fill(colour);
        core::result<std::vector<std::byte>> encoded = image.encode_png();
        CONJURE_REQUIRE_OK(encoded);
        return std::move(encoded).value();
    }

    [[nodiscard]] std::vector<std::byte> bytes_of(std::string_view text) {
        std::vector<std::byte> bytes = {};
        for (const char character : text) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        return bytes;
    }

    void append_u32_big_endian(std::vector<std::byte> &bytes, u32 value) {
        for (const u32 shift : {24u, 16u, 8u, 0u}) {
            bytes.push_back(static_cast<std::byte>((value >> shift) & 0xFFu));
        }
    }

    void append_u32(std::vector<std::byte> &bytes, u32 value) {
        for (const u32 shift : {0u, 8u, 16u, 24u}) {
            bytes.push_back(static_cast<std::byte>((value >> shift) & 0xFFu));
        }
    }

    void write_u32(std::vector<std::byte> &bytes, usize offset, u32 value) {
        for (usize index = 0; index < 4; ++index) {
            bytes[offset + index] = static_cast<std::byte>((value >> (index * 8)) & 0xFFu);
        }
    }

    /// <summary>
    /// The start of an RGBA PNG: the signature, an IHDR of the given size (stb does not check its CRC)
    /// and the header of an empty IDAT. stb's header scan stops at the first IDAT, so the size is
    /// readable while a decode fails for want of image data.
    /// </summary>
    [[nodiscard]] std::vector<std::byte> png_header_only(u32 width, u32 height) {
        constexpr std::array<u8, 8> SIGNATURE = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        constexpr std::array<u8, 5> FORMAT = {8, 6, 0, 0, 0};   // 8 bits per channel, RGBA, deflate, no interlace
        std::vector<std::byte> bytes = {};
        for (const u8 value : SIGNATURE) {
            bytes.push_back(static_cast<std::byte>(value));
        }
        append_u32_big_endian(bytes, 13);
        for (const char character : std::string_view("IHDR")) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        append_u32_big_endian(bytes, width);
        append_u32_big_endian(bytes, height);
        for (const u8 value : FORMAT) {
            bytes.push_back(static_cast<std::byte>(value));
        }
        append_u32_big_endian(bytes, 0);              // CRC
        append_u32_big_endian(bytes, 0);
        for (const char character : std::string_view("IDAT")) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        append_u32_big_endian(bytes, 0);              // CRC
        return bytes;
    }

    /// <summary>
    /// Offset of the first mip's pixels in a .ctex blob: magic, version, four enumerator bytes, the mip
    /// count, the name, then the mip's width, height and byte count.
    /// </summary>
    [[nodiscard]] usize first_pixel_offset(const texture_asset &texture) noexcept {
        return 4 + 4 + 4 + 4 + 4 + texture.name().size() + 4 + 4 + 8;
    }

    constexpr usize CTEX_COLOUR_SPACE_OFFSET = 9;
    constexpr usize CTEX_MIP_COUNT_OFFSET = 12;
}

TEST_SUITE("renderer/texture_asset") {

    TEST_CASE("mip_count_for - counts the chain down to 1x1") {
        CHECK(mip_count_for(1, 1) == 1);
        CHECK(mip_count_for(2, 1) == 2);
        CHECK(mip_count_for(4, 4) == 3);
        CHECK(mip_count_for(256, 256) == 9);
        CHECK(mip_count_for(256, 16) == 9);
        CHECK(mip_count_for(5, 3) == 3);
        CHECK(mip_count_for(0, 0) == 1);
    }

    TEST_CASE("texture_asset - from_rgba8 builds the mip chain in the requested colour space") {
        const std::vector<std::byte> pixels = solid_pixels(4, 2, core::rgba8{200, 100, 50, 255});
        core::result<texture_asset> srgb = texture_asset::from_rgba8(4, 2, pixels, texture_colour_space::SRGB);
        CONJURE_REQUIRE_OK(srgb);
        const texture_asset &texture = srgb.value();
        CHECK(texture.format() == rhi::texture_format::RGBA8_SRGB);
        CHECK(texture.colour_space() == texture_colour_space::SRGB);
        CHECK(texture.width() == 4);
        CHECK(texture.height() == 2);
        CHECK(texture.mip_count() == 3);
        CHECK(texture.mips()[1].width == 2);
        CHECK(texture.mips()[1].height == 1);
        CHECK(texture.mips()[2].width == 1);
        CHECK(texture.mips()[2].height == 1);
        CHECK(texture.mips()[1].pixels.size() == 8);
        CHECK(texture.gpu_byte_size() == 32 + 8 + 4);
        CHECK(texture.pixel(0, 0) == core::rgba8{200, 100, 50, 255});
        CHECK(texture.pixel(3, 1) == core::rgba8{200, 100, 50, 255});
        // A solid texture stays solid down the chain in both colour spaces.
        CHECK(static_cast<u8>(texture.mips()[2].pixels[0]) == 200);
        CHECK(static_cast<u8>(texture.mips()[2].pixels[1]) == 100);
        CHECK(texture.filter() == sampler_filter::LINEAR);
        CHECK(texture.wrap() == texture_wrap::REPEAT);

        core::result<texture_asset> linear = texture_asset::from_rgba8(
          4,
          2,
          pixels,
          texture_colour_space::LINEAR,
          false
        );
        CONJURE_REQUIRE_OK(linear);
        CHECK(linear.value().format() == rhi::texture_format::RGBA8_UNORM);
        CHECK(linear.value().mip_count() == 1);
        CHECK(linear.value().gpu_byte_size() == 32);
    }

    TEST_CASE("texture_asset - the box filter averages in linear light for sRGB and in value for linear") {
        // Half black, half white: the sRGB average is brighter than 127 because black and white
        // average in linear space (0.5 linear = 188 sRGB).
        std::vector<std::byte> pixels = solid_pixels(2, 1, core::rgba8{0, 0, 0, 255});
        const std::vector<std::byte> white = solid_pixels(1, 1, core::rgba8{255, 255, 255, 255});
        std::copy(white.begin(), white.end(), pixels.begin() + 4);
        core::result<texture_asset> srgb = texture_asset::from_rgba8(2, 1, pixels, texture_colour_space::SRGB);
        CONJURE_REQUIRE_OK(srgb);
        REQUIRE(srgb.value().mip_count() == 2);
        const u8 srgbAverage = static_cast<u8>(srgb.value().mips()[1].pixels[0]);
        CHECK(srgbAverage >= 185);
        CHECK(srgbAverage <= 190);
        core::result<texture_asset> linear = texture_asset::from_rgba8(2, 1, pixels, texture_colour_space::LINEAR);
        CONJURE_REQUIRE_OK(linear);
        const u8 linearAverage = static_cast<u8>(linear.value().mips()[1].pixels[0]);
        CHECK(linearAverage >= 127);
        CHECK(linearAverage <= 128);
        // Alpha is averaged plainly in both cases.
        CHECK(static_cast<u8>(linear.value().mips()[1].pixels[3]) == 255);
    }

    TEST_CASE("texture_asset - from_rgba8 and from_image reject bad sizes and pixel arrays") {
        const std::vector<std::byte> pixels = solid_pixels(2, 2, core::rgba8{});
        CONJURE_CHECK_ERROR(
          texture_asset::from_rgba8(0, 2, pixels, texture_colour_space::SRGB),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          texture_asset::from_rgba8(2, 0, pixels, texture_colour_space::SRGB),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          texture_asset::from_rgba8(2, 3, pixels, texture_colour_space::SRGB),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          texture_asset::from_rgba8(1, 1, pixels, texture_colour_space::SRGB),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          texture_asset::from_rgba8(texture_asset::MAX_DIMENSION + 1, 1, pixels, texture_colour_space::SRGB),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          texture_asset::from_image(core::image(), texture_colour_space::SRGB),
          core::error_code::INVALID_ARGUMENT
        );
        core::image image(3, 2);
        image.fill(core::rgba8{10, 20, 30, 40});
        core::result<texture_asset> fromImage = texture_asset::from_image(image, texture_colour_space::LINEAR);
        CONJURE_REQUIRE_OK(fromImage);
        CHECK(fromImage.value().width() == 3);
        CHECK(fromImage.value().height() == 2);
        CHECK(fromImage.value().pixel(2, 1) == core::rgba8{10, 20, 30, 40});
        CHECK(fromImage.value().mip_count() == 2);
    }

    TEST_CASE("texture_asset - decode reads PNG bytes and fails cleanly on garbage or empty input") {
        const std::vector<std::byte> png = png_of(8, 4, core::rgba8{12, 34, 56, 78});
        core::result<texture_asset> decoded = texture_asset::decode(png, texture_colour_space::SRGB);
        CONJURE_REQUIRE_OK(decoded);
        CHECK(decoded.value().width() == 8);
        CHECK(decoded.value().height() == 4);
        CHECK(decoded.value().mip_count() == 4);
        CHECK(decoded.value().pixel(7, 3) == core::rgba8{12, 34, 56, 78});
        CHECK(decoded.value().format() == rhi::texture_format::RGBA8_SRGB);
        core::result<texture_asset> noMips = texture_asset::decode(png, texture_colour_space::LINEAR, false);
        CONJURE_REQUIRE_OK(noMips);
        CHECK(noMips.value().mip_count() == 1);
        CHECK(noMips.value().format() == rhi::texture_format::RGBA8_UNORM);

        CONJURE_CHECK_ERROR(texture_asset::decode({}, texture_colour_space::SRGB), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(
          texture_asset::decode(bytes_of("this is not an image"), texture_colour_space::SRGB),
          renderer_error::DECODE_FAILED
        );
        std::vector<std::byte> truncated(png.begin(), png.begin() + static_cast<std::ptrdiff_t>(png.size() / 2));
        CONJURE_CHECK_ERROR(
          texture_asset::decode(truncated, texture_colour_space::SRGB),
          renderer_error::DECODE_FAILED
        );
    }

    TEST_CASE("texture_asset - load_from_file reads a file and reports a missing one as NOT_FOUND") {
        temp_directory directory("texture");
        const std::vector<std::byte> png = png_of(2, 2, core::rgba8{1, 2, 3, 4});
        const std::string text(reinterpret_cast<const char *>(png.data()), png.size());
        const std::filesystem::path path = directory.write_file("tiny.png", text);
        core::result<texture_asset> loaded = texture_asset::load_from_file(path, texture_colour_space::LINEAR);
        CONJURE_REQUIRE_OK(loaded);
        CHECK(loaded.value().width() == 2);
        CHECK(loaded.value().pixel(1, 1) == core::rgba8{1, 2, 3, 4});
        CHECK(loaded.value().name() == "tiny.png");
        CONJURE_CHECK_ERROR(
          texture_asset::load_from_file(directory.file("missing.png"), texture_colour_space::SRGB),
          core::error_code::NOT_FOUND
        );
        static_cast<void>(directory.write_file("bad.png", "not a png"));
        CONJURE_CHECK_ERROR(
          texture_asset::load_from_file(directory.file("bad.png"), texture_colour_space::SRGB),
          renderer_error::DECODE_FAILED
        );
    }

    TEST_CASE("texture_asset - solid_colour and checker produce the documented patterns") {
        const texture_asset white = texture_asset::solid_colour(
          core::rgba8{255, 255, 255, 255},
          texture_colour_space::SRGB
        );
        CHECK(white.width() == 1);
        CHECK(white.height() == 1);
        CHECK(white.mip_count() == 1);
        CHECK(white.pixel(0, 0) == core::rgba8{255, 255, 255, 255});
        const texture_asset flat = texture_asset::solid_colour(
          core::rgba8{128, 128, 255, 255},
          texture_colour_space::LINEAR,
          4,
          2
        );
        CHECK(flat.width() == 4);
        CHECK(flat.height() == 2);
        CHECK(flat.format() == rhi::texture_format::RGBA8_UNORM);
        CHECK(flat.pixel(3, 1) == core::rgba8{128, 128, 255, 255});

        const core::rgba8 magenta{255, 0, 255, 255};
        const core::rgba8 black{0, 0, 0, 255};
        const texture_asset checker = texture_asset::checker(magenta, black, 8, 2);
        CHECK(checker.width() == 8);
        CHECK(checker.height() == 8);
        CHECK(checker.pixel(0, 0) == magenta);
        CHECK(checker.pixel(1, 1) == magenta);
        CHECK(checker.pixel(2, 0) == black);
        CHECK(checker.pixel(0, 2) == black);
        CHECK(checker.pixel(2, 2) == magenta);
        CHECK(checker.mip_count() == 4);
        // Out of range reads assert and yield opaque black.
        CONJURE_CHECK_ASSERTS(CHECK(checker.pixel(8, 0) == black));
        CHECK(texture_asset().pixel(0, 0) == black);
    }

    TEST_CASE("texture_asset - write_blob and parse_blob round-trip every field") {
        texture_asset texture = texture_asset::checker(
          core::rgba8{1, 2, 3, 4},
          core::rgba8{5, 6, 7, 8},
          4,
          1,
          texture_colour_space::LINEAR
        );
        texture.set_filter(sampler_filter::POINT);
        texture.set_wrap(texture_wrap::MIRROR);
        texture.set_name("checker");
        const std::vector<std::byte> blob = texture.write_blob();
        CHECK(blob == texture.write_blob());
        core::result<texture_asset> parsed = texture_asset::parse_blob(blob);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value() == texture);
        CHECK(parsed.value().name() == "checker");
        CHECK(parsed.value().filter() == sampler_filter::POINT);
        CHECK(parsed.value().wrap() == texture_wrap::MIRROR);
        CHECK(parsed.value().format() == rhi::texture_format::RGBA8_UNORM);
        CHECK(parsed.value().mip_count() == 3);
        CHECK(parsed.value().pixel(1, 0) == core::rgba8{5, 6, 7, 8});
        CHECK_FALSE(parsed.value() == texture_asset());
    }

    TEST_CASE("texture_asset - corrupt blobs fail with INVALID_ASSET") {
        const texture_asset source = texture_asset::checker(core::rgba8{}, core::rgba8{255, 255, 255, 255}, 4, 2);
        const std::vector<std::byte> blob = source.write_blob();
        CONJURE_CHECK_ERROR(texture_asset::parse_blob({}), renderer_error::INVALID_ASSET);
        std::vector<std::byte> badMagic = blob;
        badMagic[1] = std::byte{'?'};
        CONJURE_CHECK_ERROR(texture_asset::parse_blob(badMagic), renderer_error::INVALID_ASSET);
        std::vector<std::byte> badVersion = blob;
        badVersion[4] = std::byte{9};
        CONJURE_CHECK_ERROR(texture_asset::parse_blob(badVersion), renderer_error::INVALID_ASSET);
        for (const usize length : {usize{4}, usize{12}, usize{20}, blob.size() / 2, blob.size() - 1}) {
            CAPTURE(length);
            CONJURE_CHECK_ERROR(
              texture_asset::parse_blob(std::span<const std::byte>(blob).subspan(0, length)),
              renderer_error::INVALID_ASSET
            );
        }
        std::vector<std::byte> trailing = blob;
        trailing.push_back(std::byte{1});
        CONJURE_CHECK_ERROR(texture_asset::parse_blob(trailing), renderer_error::INVALID_ASSET);
        // A corrupted header field (enumerators, mip count, name length, the first mip's size and byte
        // count) must fail; a corrupted pixel byte may parse but never changes the layout.
        const usize pixels = first_pixel_offset(source);
        for (usize offset = 8; offset < pixels + 4 && offset < blob.size(); ++offset) {
            CAPTURE(offset);
            std::vector<std::byte> huge = blob;
            huge[offset] = std::byte{0xFF};
            const core::result<texture_asset> parsed = texture_asset::parse_blob(huge);
            if (offset < pixels) {
                CHECK_FALSE(parsed.has_value());
            }
            if (!parsed.has_value()) {
                CHECK(parsed.get_error().is(renderer_error::INVALID_ASSET));
                continue;
            }
            CHECK(parsed.value().width() == source.width());
            CHECK(parsed.value().height() == source.height());
            CHECK(parsed.value().mip_count() == source.mip_count());
            CHECK(parsed.value().format() == source.format());
            CHECK(parsed.value().gpu_byte_size() == source.gpu_byte_size());
        }
    }

    TEST_CASE(
      "texture_asset - parse_blob rejects a mip chain longer than the base size allows and an RGBA8 format "
      "that disagrees with the colour space"
    ) {
        // A 2x2 texture has two mips; a third 1x1 one used to pass the halving rule.
        const core::rgba8 colour = {1, 2, 3, 4};
        const texture_asset small = texture_asset::solid_colour(colour, texture_colour_space::SRGB, 2, 2);
        REQUIRE(small.mip_count() == 2);
        std::vector<std::byte> longChain = small.write_blob();
        CONJURE_REQUIRE_OK(texture_asset::parse_blob(longChain));
        write_u32(longChain, CTEX_MIP_COUNT_OFFSET, 3);
        append_u32(longChain, 1);
        append_u32(longChain, 1);
        append_u32(longChain, 4);
        append_u32(longChain, 0);
        longChain.insert(longChain.end(), 4, std::byte{7});
        CONJURE_CHECK_ERROR(texture_asset::parse_blob(longChain), renderer_error::INVALID_ASSET);

        // RGBA8_UNORM is the linear format; claiming sRGB for it is a corrupt blob.
        const texture_asset linear = texture_asset::solid_colour(colour, texture_colour_space::LINEAR);
        REQUIRE(linear.format() == rhi::texture_format::RGBA8_UNORM);
        std::vector<std::byte> mismatched = linear.write_blob();
        CONJURE_REQUIRE_OK(texture_asset::parse_blob(mismatched));
        mismatched[CTEX_COLOUR_SPACE_OFFSET] = static_cast<std::byte>(texture_colour_space::SRGB);
        CONJURE_CHECK_ERROR(texture_asset::parse_blob(mismatched), renderer_error::INVALID_ASSET);
    }

    TEST_CASE("texture_asset - decode refuses an image whose header exceeds MAX_DIMENSION before decoding it") {
        // 20000 x 16 RGBA fits stb's own size budget, so only the engine's limit refuses it; without the
        // header check the decode ran first and failed on the missing image data instead.
        const std::vector<std::byte> wide = png_header_only(20000, 16);
        const core::result<texture_asset> decoded = texture_asset::decode(wide, texture_colour_space::SRGB, false);
        CONJURE_CHECK_ERROR(decoded, core::error_code::INVALID_ARGUMENT);
        if (!decoded.has_value()) {
            CHECK(decoded.get_error().message().find("texel limit") != std::string::npos);
        }
        // Within the limit the same truncated file is an ordinary decode failure.
        CONJURE_CHECK_ERROR(
          texture_asset::decode(png_header_only(64, 16), texture_colour_space::SRGB, false),
          renderer_error::DECODE_FAILED
        );
    }

    TEST_CASE("texture_asset - load_from_file handles non-ASCII paths without throwing") {
        // Japanese names no Western ANSI code page can encode: path::string() throws for them on Windows.
        temp_directory directory("texture_utf8");
        const std::u8string_view missingName = u8"存在しない.png";
        const core::result<texture_asset> missing = texture_asset::load_from_file(
          directory.path() / std::filesystem::path(missingName),
          texture_colour_space::SRGB
        );
        CONJURE_CHECK_ERROR(missing, core::error_code::NOT_FOUND);
        const std::string missingUtf8(reinterpret_cast<const char *>(missingName.data()), missingName.size());
        if (!missing.has_value()) {
            CHECK(missing.get_error().message().find(missingUtf8) != std::string::npos);
        }

        const std::vector<std::byte> png = png_of(2, 2, core::rgba8{5, 6, 7, 8});
        const std::filesystem::path ascii = directory.write_file(
          "ascii.png",
          std::string_view(reinterpret_cast<const char *>(png.data()), png.size())
        );
        const std::u8string_view name = u8"画像.png";
        const std::filesystem::path named = directory.path() / std::filesystem::path(name);
        std::error_code renameError = {};
        std::filesystem::rename(ascii, named, renameError);
        REQUIRE_FALSE(renameError);
        core::result<texture_asset> loaded = texture_asset::load_from_file(named, texture_colour_space::LINEAR);
        CONJURE_REQUIRE_OK(loaded);
        CHECK(loaded.value().pixel(1, 1) == core::rgba8{5, 6, 7, 8});
        CHECK(loaded.value().name() == std::string_view(reinterpret_cast<const char *>(name.data()), name.size()));
    }

    TEST_CASE("texture_import_settings - JSON round-trips, missing keys keep defaults and bad values fail") {
        texture_import_settings settings = {};
        settings.colourSpace = texture_colour_space::LINEAR;
        settings.filter = sampler_filter::POINT;
        settings.wrap = texture_wrap::CLAMP;
        settings.generateMips = false;
        const core::json::document document = settings.to_document();
        const core::json::value root = document.root();
        CHECK(root["colourSpace"].as_string() == "linear");
        CHECK(root["filter"].as_string() == "point");
        CHECK(root["wrap"].as_string() == "clamp");
        CHECK_FALSE(root["generateMips"].as_bool(true));
        core::result<texture_import_settings> restored = texture_import_settings::from_json(root);
        CONJURE_REQUIRE_OK(restored);
        CHECK(restored.value() == settings);

        core::result<texture_import_settings> defaults = texture_import_settings::from_json(
          core::json::document::object().root()
        );
        CONJURE_REQUIRE_OK(defaults);
        CHECK(defaults.value() == texture_import_settings());
        core::result<texture_import_settings> partial = texture_import_settings::from_json(
          core::json::object({{"wrap", "mirror"}}).root()
        );
        CONJURE_REQUIRE_OK(partial);
        CHECK(partial.value().wrap == texture_wrap::MIRROR);
        CHECK(partial.value().colourSpace == texture_colour_space::SRGB);
        // An invalid (absent) settings value yields the defaults.
        core::result<texture_import_settings> absent = texture_import_settings::from_json(core::json::value());
        CONJURE_REQUIRE_OK(absent);
        CHECK(absent.value() == texture_import_settings());

        CONJURE_CHECK_ERROR(
          texture_import_settings::from_json(core::json::object({{"colourSpace", "gamma"}}).root()),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          texture_import_settings::from_json(core::json::object({{"filter", "cubic"}}).root()),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          texture_import_settings::from_json(core::json::object({{"wrap", 3}}).root()),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          texture_import_settings::from_json(core::json::object({{"generateMips", "yes"}}).root()),
          core::error_code::INVALID_ARGUMENT
        );
        const core::result<core::json::document> array = core::json::document::parse("[1, 2]");
        CONJURE_REQUIRE_OK(array);
        CONJURE_CHECK_ERROR(
          texture_import_settings::from_json(array.value().root()),
          core::error_code::INVALID_ARGUMENT
        );
        CHECK(to_string(texture_colour_space::SRGB) == "SRGB");
        CHECK(to_string(texture_colour_space::LINEAR) == "LINEAR");
        CHECK(to_string(sampler_filter::POINT) == "POINT");
        CHECK(to_string(texture_wrap::MIRROR) == "MIRROR");
    }

    TEST_CASE("texture_importer - cooks a PNG into one texture asset with the settings applied") {
        const texture_importer importer = {};
        CHECK(importer.id() == TEXTURE_IMPORTER_ID);
        CHECK(importer.version() == TEXTURE_IMPORTER_VERSION);
        CHECK(importer.extensions().size() == TEXTURE_EXTENSIONS.size());
        CHECK(importer.extensions()[0] == ".png");
        CHECK(importer.default_settings().root()["colourSpace"].as_string() == "srgb");

        core::vfs::memory_file_system fileSystem = {};
        const std::vector<std::byte> png = png_of(4, 4, core::rgba8{9, 8, 7, 255});
        const core::json::document settings = core::json::object(
          {{"colourSpace", "linear"}, {"filter", "point"}, {"generateMips", false}}
        );
        assets::sub_asset_table subAssets = {};
        assets::import_context context(
          fileSystem,
          "assets://textures/tile.png",
          "assets/textures/tile.png",
          png,
          settings.root(),
          "windows",
          renderer_support::test_asset_id(3),
          subAssets,
          {}
        );
        core::result<assets::import_output> imported = importer.import(context);
        CONJURE_REQUIRE_OK(imported);
        REQUIRE(imported.value().assets.size() == 1);
        const assets::imported_asset &primary = imported.value().assets[0];
        CHECK(primary.subPath.empty());
        CHECK(primary.type == assets::asset_type_traits<texture_asset>::id());
        CHECK(primary.dependencies.empty());
        CHECK(primary.displayName == "tile.png");
        core::result<texture_asset> cooked = texture_asset::parse_blob(primary.payload);
        CONJURE_REQUIRE_OK(cooked);
        CHECK(cooked.value().colour_space() == texture_colour_space::LINEAR);
        CHECK(cooked.value().filter() == sampler_filter::POINT);
        CHECK(cooked.value().mip_count() == 1);
        CHECK(cooked.value().pixel(3, 3) == core::rgba8{9, 8, 7, 255});
        CHECK(context.warnings().empty());

        // Garbage input and bad settings both fail as IMPORT_FAILED (the importer contract).
        const std::vector<std::byte> garbage = bytes_of("nope");
        assets::import_context badContext(
          fileSystem,
          "assets://a.png",
          "assets/a.png",
          garbage,
          settings.root(),
          "windows",
          renderer_support::test_asset_id(4),
          subAssets,
          {}
        );
        CONJURE_CHECK_ERROR(importer.import(badContext), assets::asset_error_code::IMPORT_FAILED);
        const core::json::document badSettings = core::json::object({{"wrap", "spherical"}});
        assets::import_context badSettingsContext(
          fileSystem,
          "assets://a.png",
          "assets/a.png",
          png,
          badSettings.root(),
          "windows",
          renderer_support::test_asset_id(5),
          subAssets,
          {}
        );
        CONJURE_CHECK_ERROR(importer.import(badSettingsContext), assets::asset_error_code::IMPORT_FAILED);
    }

    TEST_CASE("texture_importer - version 2 claims .jxl and cooks JPEG XL by signature, inline or on jobs") {
        // Version 2 is the JPEG XL importer: a cache entry cooked by version 1 is stale and the source re-imports.
        const texture_importer importer = {};
        CHECK(TEXTURE_IMPORTER_VERSION == 2);
        CHECK(importer.version() == 2);
        REQUIRE(importer.extensions().size() == 6);
        CHECK(importer.extensions()[5] == ".jxl");

        core::image image(5, 3);
        for (u32 y = 0; y < image.height(); ++y) {
            for (u32 x = 0; x < image.width(); ++x) {
                image.set_pixel(x, y, core::rgba8{static_cast<u8>(x * 50), static_cast<u8>(y * 80), 33, 200});
            }
        }
        core::result<std::vector<std::byte>> jxl = encode_jpeg_xl(image);
        core::result<std::vector<std::byte>> png = image.encode_png();
        CONJURE_REQUIRE_OK(jxl);
        CONJURE_REQUIRE_OK(png);
        core::vfs::memory_file_system fileSystem = {};
        assets::sub_asset_table subAssets = {};
        const core::json::document settings = core::json::object({{"generateMips", false}});
        const auto cook = [&](const texture_importer &which, std::string_view name, std::span<const std::byte> bytes) {
            const std::string logical = std::string("assets://textures/") + std::string(name);
            const std::string physical = std::string("assets/textures/") + std::string(name);
            assets::import_context context(
              fileSystem,
              logical,
              physical,
              bytes,
              settings.root(),
              "windows",
              renderer_support::test_asset_id(21),
              subAssets,
              {}
            );
            return which.import(context);
        };

        core::result<assets::import_output> cooked = cook(importer, "tile.jxl", jxl.value());
        CONJURE_REQUIRE_OK(cooked);
        REQUIRE(cooked.value().assets.size() == 1);
        const std::vector<std::byte> payload = cooked.value().assets[0].payload;
        core::result<texture_asset> texture = texture_asset::parse_blob(payload);
        CONJURE_REQUIRE_OK(texture);
        CHECK(texture.value().name() == "tile.jxl");
        CHECK(texture.value().pixel(4, 2) == image.pixel(4, 2));
        CHECK(texture.value().pixel(0, 1) == image.pixel(0, 1));

        // The extension only picks the importer; the signature picks the decoder.
        const std::span<const std::byte> jxlBytes = jxl.value();
        const std::span<const std::byte> pngBytes = png.value();
        for (const std::string_view name : {"misnamed.png", "misnamed.jxl"}) {
            CAPTURE(std::string(name));
            const std::span<const std::byte> bytes = name.ends_with(".png") ? jxlBytes : pngBytes;
            core::result<assets::import_output> misnamed = cook(importer, name, bytes);
            CONJURE_REQUIRE_OK(misnamed);
            core::result<texture_asset> decoded = texture_asset::parse_blob(misnamed.value().assets[0].payload);
            CONJURE_REQUIRE_OK(decoded);
            CHECK(decoded.value().pixel(4, 2) == image.pixel(4, 2));
        }

        // On a job system the cooked bytes are the same (the importer output is a cache key).
        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = 4;
        CONJURE_REQUIRE_OK(jobs.start(desc));
        const texture_importer parallelImporter(&jobs);
        core::result<assets::import_output> parallel = cook(parallelImporter, "tile.jxl", jxl.value());
        jobs.stop();
        CONJURE_REQUIRE_OK(parallel);
        CHECK(parallel.value().assets[0].payload == payload);

        // A truncated file is an import failure like any undecodable source.
        CONJURE_CHECK_ERROR(
          cook(importer, "cut.jxl", jxlBytes.first(jxlBytes.size() / 2)),
          assets::asset_error_code::IMPORT_FAILED
        );
    }

    TEST_CASE("texture_loader - loads a blob and falls back to a 1x1 white texture") {
        const texture_loader loader = {};
        CHECK(loader.type() == assets::asset_type_traits<texture_asset>::id());
        const texture_asset source = texture_asset::solid_colour(
          core::rgba8{10, 20, 30, 255},
          texture_colour_space::SRGB,
          2,
          2
        );
        const std::vector<std::byte> blob = source.write_blob();
        assets::load_context context = {};
        context.id = renderer_support::test_asset_id(6);
        context.type = loader.type();
        core::result<assets::asset_payload> loaded = loader.load_cpu(context, blob);
        CONJURE_REQUIRE_OK(loaded);
        REQUIRE(assets::payload_as<texture_asset>(loaded.value()) != nullptr);
        CHECK(*assets::payload_as<texture_asset>(loaded.value()) == source);
        core::result<assets::asset_payload> finalised = loader.finalise(
          assets::finalise_context{},
          std::move(loaded).value()
        );
        CONJURE_REQUIRE_OK(finalised);
        CHECK(*assets::payload_as<texture_asset>(finalised.value()) == source);
        CONJURE_CHECK_ERROR(
          loader.load_cpu(context, std::span<const std::byte>(blob).subspan(0, 6)),
          renderer_error::INVALID_ASSET
        );

        core::result<assets::asset_payload> fallback = loader.create_fallback();
        CONJURE_REQUIRE_OK(fallback);
        const texture_asset *white = assets::payload_as<texture_asset>(fallback.value());
        REQUIRE(white != nullptr);
        CHECK(white->width() == 1);
        CHECK(white->height() == 1);
        CHECK(white->pixel(0, 0) == core::rgba8{255, 255, 255, 255});
        CHECK(TEXTURE_ASSET_TYPE_NAME == "conjure.renderer.texture");
    }
}
