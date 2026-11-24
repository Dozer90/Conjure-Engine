#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "conjure/testing/test_environment.h"
#include "renderer_test_support.h"

#include "renderer/import/ibl_baker.h"
#include "renderer/import/jpeg_xl_codec.h"

#include "conjure/renderer/environment_asset.h"
#include "conjure/renderer/limits.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/renderer_error.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_payload.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/assets_error.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/json/document.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/memory_file_system.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// environment_asset.h (docs/design/02_renderer.md R13, 3.6, R34): cubemap storage, equirect
// decoding and sampling, the deterministic CPU bake (SH9 + GGX prefilter), the .cenv blob, the
// importer and the loader. The .hdr files are written in-test (flat RGBE, no binary assets), the JPEG XL
// equirects through the renderer's encoder.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

// The renderer's private JPEG XL codec writes the equirect fixtures, and its private baker shows the bake's float
// sums; conjure::testing has a detail namespace of its own.
namespace codec = conjure::renderer::detail;
namespace ibl = conjure::renderer::detail;

namespace {

    /// <summary>
    /// A Radiance .hdr with flat (non run-length) RGBE scanlines; width below 8 keeps stb on the
    /// flat path. Row 0 is the top (zenith).
    /// </summary>
    [[nodiscard]] std::vector<std::byte> encode_hdr(u32 width, u32 height, std::span<const math::vec3> rgb) {
        std::string header = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n";
        header += "-Y " + std::to_string(height) + " +X " + std::to_string(width) + "\n";
        std::vector<std::byte> bytes = {};
        for (const char character : header) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        for (const math::vec3 &pixel : rgb) {
            const f32 largest = std::max(pixel.x, std::max(pixel.y, pixel.z));
            if (largest < 1.0e-32f) {
                bytes.insert(bytes.end(), 4, std::byte{0});
                continue;
            }
            int exponent = 0;
            static_cast<void>(std::frexp(largest, &exponent));
            const f32 scale = 256.0f / std::ldexp(1.0f, exponent);
            bytes.push_back(static_cast<std::byte>(static_cast<u8>(pixel.x * scale)));
            bytes.push_back(static_cast<std::byte>(static_cast<u8>(pixel.y * scale)));
            bytes.push_back(static_cast<std::byte>(static_cast<u8>(pixel.z * scale)));
            bytes.push_back(static_cast<std::byte>(static_cast<u8>(exponent + 128)));
        }
        return bytes;
    }

    [[nodiscard]] std::vector<std::byte> two_row_hdr() {
        std::vector<math::vec3> rgb = {};
        rgb.insert(rgb.end(), 4, math::vec3(0.0f, 0.0f, 2.0f));   // top row: blue
        rgb.insert(rgb.end(), 4, math::vec3(2.0f, 0.0f, 0.0f));   // bottom row: red
        return encode_hdr(4, 2, rgb);
    }

    /// <summary>
    /// The pixels of two_row_hdr() as a float JPEG XL tagged linear sRGB: 2.0 and 0.0 are exact in RGBE, so
    /// both files decode to the same floats.
    /// </summary>
    [[nodiscard]] std::vector<std::byte> two_row_jpeg_xl() {
        std::vector<f32> rgb = {};
        for (u32 index = 0; index < 4; ++index) {
            rgb.insert(rgb.end(), {0.0f, 0.0f, 2.0f});
        }
        for (u32 index = 0; index < 4; ++index) {
            rgb.insert(rgb.end(), {2.0f, 0.0f, 0.0f});
        }
        std::vector<std::byte> samples(rgb.size() * sizeof(f32));
        std::memcpy(samples.data(), rgb.data(), samples.size());
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = 4;
        desc.height = 2;
        desc.channelCount = 3;
        desc.sampleType = codec::jpeg_xl_sample_type::FLOAT32;
        desc.colourEncoding = codec::jpeg_xl_colour_encoding::LINEAR_SRGB;
        desc.frames.push_back(samples);
        core::result<std::vector<std::byte>> encoded = codec::encode_jpeg_xl(desc, nullptr);
        CONJURE_REQUIRE_OK(encoded);
        return std::move(encoded).value();
    }

    [[nodiscard]] environment_bake_options small_options() {
        environment_bake_options options = {};
        options.skySize = 8;
        options.prefilteredSize = 8;
        options.sampleCount = 8;
        return options;
    }

    [[nodiscard]] std::vector<std::byte> bytes_of(std::string_view text) {
        std::vector<std::byte> bytes = {};
        for (const char character : text) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        return bytes;
    }

    // A .cenv cubemap header: size (u32), mip count (u32), texel count (u64).
    constexpr usize CUBEMAP_HEADER_SIZE = 16;

    /// <summary>
    /// Offset of the sky cubemap header in a .cenv blob: magic, version, the name, then nine SH vec3s.
    /// </summary>
    [[nodiscard]] usize sky_header_offset(const environment_asset &environment) noexcept {
        return 4 + 4 + 4 + environment.name().size() + 9 * sizeof(math::vec3);
    }
}

TEST_SUITE("renderer/environment_asset") {

    TEST_CASE("cubemap_data - offsets, lengths and texel reads follow the mip-major face-major layout") {
        cubemap_data cubemap = {};
        cubemap.size = 4;
        cubemap.mipCount = 3;
        CHECK(cubemap.mip_size(0) == 4);
        CHECK(cubemap.mip_size(1) == 2);
        CHECK(cubemap.mip_size(2) == 1);
        CHECK(cubemap.mip_size(9) == 1);
        CHECK(cubemap.face_length(0) == 64);
        CHECK(cubemap.face_length(1) == 16);
        CHECK(cubemap.face_offset(0, 0) == 0);
        CHECK(cubemap.face_offset(0, 1) == 64);
        CHECK(cubemap.face_offset(1, 0) == 64 * 6);
        CHECK(cubemap.face_offset(1, 3) == 64 * 6 + 16 * 3);
        CHECK(cubemap.face_offset(2, 0) == 64 * 6 + 16 * 6);
        CHECK_FALSE(cubemap.is_consistent());
        cubemap.texels.assign((64 + 16 + 4) * 6, pack_half(0.0f));
        CHECK(cubemap.is_consistent());
        const usize index = cubemap.face_offset(1, 2) + (1 * 2 + 1) * 4;
        cubemap.texels[index] = pack_half(0.25f);
        cubemap.texels[index + 1] = pack_half(0.5f);
        cubemap.texels[index + 2] = pack_half(1.0f);
        CONJURE_CHECK_APPROX(cubemap.texel(1, 2, 1, 1), math::vec3(0.25f, 0.5f, 1.0f));
        CONJURE_CHECK_APPROX(cubemap.texel(0, 0, 0, 0), math::vec3(0.0f));
        CONJURE_CHECK_APPROX(cubemap.texel(5, 5, 5, 5), math::vec3(0.0f));   // out of range reads black
        cubemap_data empty = {};
        CHECK(empty.is_consistent());
        empty.texels.push_back(0);
        CHECK_FALSE(empty.is_consistent());
    }

    TEST_CASE("cubemap_texel_direction - face centres map to the axis directions") {
        CONJURE_CHECK_APPROX(cubemap_texel_direction(0, 2, 2, 5), math::vec3(1.0f, 0.0f, 0.0f));
        CONJURE_CHECK_APPROX(cubemap_texel_direction(1, 2, 2, 5), math::vec3(-1.0f, 0.0f, 0.0f));
        CONJURE_CHECK_APPROX(cubemap_texel_direction(2, 2, 2, 5), math::vec3(0.0f, 1.0f, 0.0f));
        CONJURE_CHECK_APPROX(cubemap_texel_direction(3, 2, 2, 5), math::vec3(0.0f, -1.0f, 0.0f));
        CONJURE_CHECK_APPROX(cubemap_texel_direction(4, 2, 2, 5), math::vec3(0.0f, 0.0f, 1.0f));
        CONJURE_CHECK_APPROX(cubemap_texel_direction(5, 2, 2, 5), math::vec3(0.0f, 0.0f, -1.0f));
        // Every texel direction is unit length and lies on its face.
        for (u32 face = 0; face < CUBEMAP_FACE_COUNT; ++face) {
            for (u32 y = 0; y < 4; ++y) {
                for (u32 x = 0; x < 4; ++x) {
                    const math::vec3 direction = cubemap_texel_direction(face, x, y, 4);
                    CHECK(glm::length(direction) == doctest::Approx(1.0f));
                    const f32 major = face < 2 ? direction.x : (face < 4 ? direction.y : direction.z);
                    CHECK((face % 2 == 0 ? major : -major) > 0.0f);
                }
            }
        }
    }

    TEST_CASE("equirect_image - decode_hdr reads flat RGBE scanlines and sample looks up by direction") {
        const std::vector<std::byte> encoded = two_row_hdr();
        core::result<equirect_image> decoded = equirect_image::decode_hdr(encoded);
        CONJURE_REQUIRE_OK(decoded);
        const equirect_image &image = decoded.value();
        CHECK(image.width == 4);
        CHECK(image.height == 2);
        CHECK(image.is_valid());
        CHECK(image.rgb.size() == 24);
        CHECK(image.rgb[2] == doctest::Approx(2.0f).epsilon(0.01));
        CHECK(image.rgb[12] == doctest::Approx(2.0f).epsilon(0.01));
        // Up samples the top row, down the bottom row, the horizon a blend of both.
        CONJURE_CHECK_APPROX(image.sample(math::vec3(0.0f, 1.0f, 0.0f)), math::vec3(0.0f, 0.0f, 2.0f));
        CONJURE_CHECK_APPROX(image.sample(math::vec3(0.0f, -1.0f, 0.0f)), math::vec3(2.0f, 0.0f, 0.0f));
        const math::vec3 horizon = image.sample(math::vec3(1.0f, 0.0f, 0.0f));
        CHECK(horizon.x == doctest::Approx(1.0f).epsilon(0.05));
        CHECK(horizon.z == doctest::Approx(1.0f).epsilon(0.05));
        CHECK(horizon.y == doctest::Approx(0.0f));

        CONJURE_CHECK_ERROR(equirect_image::decode_hdr({}), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(equirect_image::decode_hdr(bytes_of("#?RADIANCE garbage")), renderer_error::DECODE_FAILED);
        CONJURE_CHECK_ERROR(equirect_image::decode_hdr(bytes_of("not an hdr at all")), renderer_error::DECODE_FAILED);

        const equirect_image constant = equirect_image::constant(3, 2, math::vec3(0.5f, 0.25f, 0.125f));
        CHECK(constant.is_valid());
        CONJURE_CHECK_APPROX(constant.sample(math::vec3(0.3f, -0.7f, 0.2f)), math::vec3(0.5f, 0.25f, 0.125f));
        equirect_image invalid = {};
        CHECK_FALSE(invalid.is_valid());
        invalid.width = 2;
        invalid.height = 1;
        CHECK_FALSE(invalid.is_valid());
    }

    TEST_CASE("bake_environment - a constant image bakes to constant cubemaps and pi times the radiance") {
        const equirect_image source = equirect_image::constant(16, 8, math::vec3(0.5f, 0.25f, 1.0f));
        core::result<environment_asset> baked = bake_environment(source, small_options(), nullptr);
        CONJURE_REQUIRE_OK(baked);
        const environment_asset &environment = baked.value();
        CHECK(environment.sky().size == 8);
        CHECK(environment.sky().mipCount == 4);
        CHECK(environment.sky().is_consistent());
        CHECK(environment.prefiltered().size == 8);
        CHECK(environment.prefiltered().mipCount == 4);
        CHECK(environment.prefiltered().is_consistent());
        CHECK(
          environment.gpu_byte_size() == (environment.sky().texels.size() + environment.prefiltered().texels.size()) * 2
        );
        for (u32 mip = 0; mip < 4; ++mip) {
            for (u32 face = 0; face < CUBEMAP_FACE_COUNT; ++face) {
                CAPTURE(mip);
                CAPTURE(face);
                const math::vec3 sky = environment.sky().texel(mip, face, 0, 0);
                CHECK(sky.x == doctest::Approx(0.5f).epsilon(0.01));
                CHECK(sky.y == doctest::Approx(0.25f).epsilon(0.01));
                CHECK(sky.z == doctest::Approx(1.0f).epsilon(0.01));
                const math::vec3 specular = environment.prefiltered().texel(mip, face, 0, 0);
                CHECK(specular.x == doctest::Approx(0.5f).epsilon(0.03));
                CHECK(specular.z == doctest::Approx(1.0f).epsilon(0.03));
            }
        }
        // The SH irradiance of a uniform environment is pi * L in every direction.
        const std::array<math::vec3, 4> directions = {
          math::vec3(0.0f, 1.0f, 0.0f),
          math::vec3(1.0f, 0.0f, 0.0f),
          math::vec3(0.0f, 0.0f, -1.0f),
          glm::normalize(math::vec3(1.0f, 1.0f, 1.0f)),
        };
        for (const math::vec3 &direction : directions) {
            const math::vec3 irradiance = evaluate_sh9_irradiance(environment.irradiance(), direction);
            CHECK(irradiance.x == doctest::Approx(math::PI * 0.5f).epsilon(0.05));
            CHECK(irradiance.y == doctest::Approx(math::PI * 0.25f).epsilon(0.05));
            CHECK(irradiance.z == doctest::Approx(math::PI * 1.0f).epsilon(0.05));
        }
        CHECK(environment.name().empty());
    }

    TEST_CASE("bake_environment - a two-tone sky keeps blue above and red below in every product") {
        core::result<equirect_image> decoded = equirect_image::decode_hdr(two_row_hdr());
        CONJURE_REQUIRE_OK(decoded);
        core::result<environment_asset> baked = bake_environment(decoded.value(), small_options(), nullptr);
        CONJURE_REQUIRE_OK(baked);
        const environment_asset &environment = baked.value();
        // +Y face is blue, -Y face is red, at every mip.
        for (u32 mip = 0; mip < environment.sky().mipCount; ++mip) {
            const math::vec3 up = environment.sky().texel(mip, 2, 0, 0);
            const math::vec3 down = environment.sky().texel(mip, 3, 0, 0);
            CHECK(up.z > up.x);
            CHECK(down.x > down.z);
        }
        const math::vec3 upSpecular = environment.prefiltered().texel(0, 2, 2, 2);
        const math::vec3 downSpecular = environment.prefiltered().texel(0, 3, 2, 2);
        CHECK(upSpecular.z > upSpecular.x);
        CHECK(downSpecular.x > downSpecular.z);
        const math::vec3 upIrradiance = evaluate_sh9_irradiance(environment.irradiance(), math::vec3(0.0f, 1.0f, 0.0f));
        const math::vec3 downIrradiance = evaluate_sh9_irradiance(
          environment.irradiance(),
          math::vec3(0.0f, -1.0f, 0.0f)
        );
        CHECK(upIrradiance.z > upIrradiance.x);
        CHECK(downIrradiance.x > downIrradiance.z);
        CHECK(upIrradiance.y == doctest::Approx(0.0f).epsilon(1.0e-3));
        // Irradiance is never negative.
        for (f32 angle = 0.0f; angle < math::TWO_PI; angle += 0.3f) {
            const math::vec3 irradiance = evaluate_sh9_irradiance(
              environment.irradiance(),
              math::vec3(std::cos(angle), std::sin(angle), 0.0f)
            );
            CHECK(irradiance.x >= 0.0f);
            CHECK(irradiance.z >= 0.0f);
        }
    }

    TEST_CASE("bake_environment - the bake is deterministic and bit-identical with and without jobs") {
        core::result<equirect_image> decoded = equirect_image::decode_hdr(two_row_hdr());
        CONJURE_REQUIRE_OK(decoded);
        core::result<environment_asset> first = bake_environment(decoded.value(), small_options(), nullptr);
        core::result<environment_asset> second = bake_environment(decoded.value(), small_options(), nullptr);
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(second);
        CHECK(first.value() == second.value());
        CHECK(first.value().write_blob() == second.value().write_blob());

        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = environment().jobThreadCount;
        CONJURE_REQUIRE_OK(jobs.start(desc));
        core::result<environment_asset> parallel = bake_environment(decoded.value(), small_options(), &jobs);
        jobs.stop();
        CONJURE_REQUIRE_OK(parallel);
        CHECK(parallel.value() == first.value());
        CHECK(parallel.value().write_blob() == first.value().write_blob());
    }

    TEST_CASE("bake_environment - invalid images and options fail with INVALID_ARGUMENT") {
        const equirect_image source = equirect_image::constant(8, 4, math::vec3(1.0f));
        CONJURE_CHECK_ERROR(
          bake_environment(equirect_image(), small_options(), nullptr),
          core::error_code::INVALID_ARGUMENT
        );
        environment_bake_options options = small_options();
        options.skySize = 12;
        CONJURE_CHECK_ERROR(bake_environment(source, options, nullptr), core::error_code::INVALID_ARGUMENT);
        options = small_options();
        options.skySize = 4;
        CONJURE_CHECK_ERROR(bake_environment(source, options, nullptr), core::error_code::INVALID_ARGUMENT);
        options = small_options();
        options.prefilteredSize = 16;
        CONJURE_CHECK_ERROR(bake_environment(source, options, nullptr), core::error_code::INVALID_ARGUMENT);
        options = small_options();
        options.sampleCount = 0;
        CONJURE_CHECK_ERROR(bake_environment(source, options, nullptr), core::error_code::INVALID_ARGUMENT);
        options = small_options();
        options.prefilteredSize = 6;
        CONJURE_CHECK_ERROR(bake_environment(source, options, nullptr), core::error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("environment_bake_options - JSON round-trips, defaults apply and wrong types fail") {
        const environment_bake_options defaults = {};
        CHECK(defaults.skySize == 512);
        CHECK(defaults.prefilteredSize == 128);
        CHECK(defaults.sampleCount == 128);
        environment_bake_options options = small_options();
        const core::json::document document = options.to_document();
        CHECK(document.root()["skySize"].as_i64() == 8);
        CHECK(document.root()["prefilteredSize"].as_i64() == 8);
        CHECK(document.root()["sampleCount"].as_i64() == 8);
        core::result<environment_bake_options> restored = environment_bake_options::from_json(document.root());
        CONJURE_REQUIRE_OK(restored);
        CHECK(restored.value() == options);
        core::result<environment_bake_options> partial = environment_bake_options::from_json(
          core::json::object({{"sampleCount", 16}}).root()
        );
        CONJURE_REQUIRE_OK(partial);
        CHECK(partial.value().sampleCount == 16);
        CHECK(partial.value().skySize == 512);
        core::result<environment_bake_options> absent = environment_bake_options::from_json(core::json::value());
        CONJURE_REQUIRE_OK(absent);
        CHECK(absent.value() == defaults);
        CONJURE_CHECK_ERROR(
          environment_bake_options::from_json(core::json::object({{"skySize", "big"}}).root()),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          environment_bake_options::from_json(core::json::object({{"sampleCount", -4}}).root()),
          core::error_code::INVALID_ARGUMENT
        );
        const core::result<core::json::document> array = core::json::document::parse("[8]");
        CONJURE_REQUIRE_OK(array);
        CONJURE_CHECK_ERROR(
          environment_bake_options::from_json(array.value().root()),
          core::error_code::INVALID_ARGUMENT
        );
    }

    TEST_CASE("environment_asset - uniform builds the grey fallback with matching SH band 0") {
        const environment_asset grey = environment_asset::uniform(math::vec3(0.5f), 4);
        CHECK(grey.sky().size == 4);
        CHECK(grey.sky().mipCount >= 1);
        CHECK(grey.sky().is_consistent());
        CHECK(grey.prefiltered().is_consistent());
        CONJURE_CHECK_APPROX(grey.sky().texel(0, 3, 1, 1), math::vec3(0.5f));
        CONJURE_CHECK_APPROX(grey.prefiltered().texel(0, 0, 0, 0), math::vec3(0.5f));
        const math::vec3 irradiance = evaluate_sh9_irradiance(grey.irradiance(), math::vec3(0.0f, 0.0f, 1.0f));
        CHECK(irradiance.x == doctest::Approx(math::PI * 0.5f).epsilon(1.0e-3));
        for (usize coefficient = 1; coefficient < 9; ++coefficient) {
            CONJURE_CHECK_APPROX(grey.irradiance()[coefficient], math::vec3(0.0f));
        }
        CHECK(environment_asset::uniform(math::vec3(0.25f), 1).sky().size == 1);
    }

    TEST_CASE("environment_asset - write_blob and parse_blob round-trip and corrupt blobs fail with INVALID_ASSET") {
        core::result<equirect_image> decoded = equirect_image::decode_hdr(two_row_hdr());
        CONJURE_REQUIRE_OK(decoded);
        core::result<environment_asset> baked = bake_environment(decoded.value(), small_options(), nullptr);
        CONJURE_REQUIRE_OK(baked);
        baked.value().set_name("sky");
        const std::vector<std::byte> blob = baked.value().write_blob();
        CHECK(blob == baked.value().write_blob());
        core::result<environment_asset> parsed = environment_asset::parse_blob(blob);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value() == baked.value());
        CHECK(parsed.value().name() == "sky");
        CHECK(parsed.value().sky().mipCount == 4);
        CHECK_FALSE(parsed.value() == environment_asset());

        CONJURE_CHECK_ERROR(environment_asset::parse_blob({}), renderer_error::INVALID_ASSET);
        std::vector<std::byte> badMagic = blob;
        badMagic[0] = std::byte{'x'};
        CONJURE_CHECK_ERROR(environment_asset::parse_blob(badMagic), renderer_error::INVALID_ASSET);
        std::vector<std::byte> badVersion = blob;
        badVersion[4] = std::byte{2};
        CONJURE_CHECK_ERROR(environment_asset::parse_blob(badVersion), renderer_error::INVALID_ASSET);
        for (const usize length : {usize{4}, usize{10}, usize{60}, blob.size() / 2, blob.size() - 1}) {
            CAPTURE(length);
            CONJURE_CHECK_ERROR(
              environment_asset::parse_blob(std::span<const std::byte>(blob).subspan(0, length)),
              renderer_error::INVALID_ASSET
            );
        }
        std::vector<std::byte> trailing = blob;
        trailing.push_back(std::byte{0});
        CONJURE_CHECK_ERROR(environment_asset::parse_blob(trailing), renderer_error::INVALID_ASSET);
        // A corrupted name length or sky cubemap header (size, mip count, texel count) must be refused,
        // not allocated; corrupted name, SH or texel bytes may parse but never change the layout.
        const usize skyHeader = sky_header_offset(baked.value());
        for (usize offset = 8; offset < skyHeader + CUBEMAP_HEADER_SIZE + 1 && offset < blob.size(); ++offset) {
            CAPTURE(offset);
            std::vector<std::byte> huge = blob;
            huge[offset] = std::byte{0xFF};
            const core::result<environment_asset> result = environment_asset::parse_blob(huge);
            const bool nameLength = offset < 12;
            const bool skyHeaderField = offset >= skyHeader && offset < skyHeader + CUBEMAP_HEADER_SIZE;
            if (nameLength || skyHeaderField) {
                CHECK_FALSE(result.has_value());
            }
            if (!result.has_value()) {
                CHECK(result.get_error().is(renderer_error::INVALID_ASSET));
                continue;
            }
            CHECK(result.value().gpu_byte_size() == baked.value().gpu_byte_size());
            CHECK(result.value().sky().is_consistent());
            CHECK(result.value().prefiltered().is_consistent());
        }
    }

    TEST_CASE("environment_asset - parse_blob rejects a cubemap mip count above the size's chain") {
        const environment_asset grey = environment_asset::uniform(math::vec3(0.5f), 4);
        REQUIRE(grey.sky().mipCount == 3);
        const std::vector<std::byte> blob = grey.write_blob();
        CONJURE_REQUIRE_OK(environment_asset::parse_blob(blob));

        // A size-4 cubemap has the chain 4, 2, 1; a fourth (repeated 1x1) level used to be accepted
        // because the mip size clamps at 1.
        cubemap_data overlong = grey.sky();
        overlong.mipCount = 4;
        overlong.texels.resize(overlong.texels.size() + overlong.face_length(3) * CUBEMAP_FACE_COUNT, pack_half(0.5f));
        REQUIRE(overlong.is_consistent());
        const environment_asset tampered(overlong, grey.prefiltered(), grey.irradiance(), "uniform");
        CONJURE_CHECK_ERROR(environment_asset::parse_blob(tampered.write_blob()), renderer_error::INVALID_ASSET);
        // Non-power-of-two sizes keep their full chain.
        const environment_asset odd = environment_asset::uniform(math::vec3(0.5f), 5);
        core::result<environment_asset> parsed = environment_asset::parse_blob(odd.write_blob());
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value() == odd);
    }

    TEST_CASE("regression - an HDRI texel above the half range bakes to finite halves") {
        // A constant sky with a small sun of 1e6 at the image centre (+Z): RGBA16F tops out at 65504,
        // and the sun used to pack to +inf, which bloom's Karis weight turns into NaN.
        equirect_image source = equirect_image::constant(32, 16, math::vec3(1.0f));
        for (u32 y = 6; y < 10; ++y) {
            for (u32 x = 14; x < 18; ++x) {
                const usize index = (static_cast<usize>(y) * source.width + x) * 3;
                source.rgb[index] = 1.0e6f;
                source.rgb[index + 1] = 1.0e6f;
                source.rgb[index + 2] = 1.0e6f;
            }
        }
        core::result<environment_asset> baked = bake_environment(source, small_options(), nullptr);
        CONJURE_REQUIRE_OK(baked);
        const auto finite = [](const cubemap_data &cubemap) {
            return std::ranges::none_of(cubemap.texels, [](u16 half) { return (half & 0x7C00u) == 0x7C00u; });
        };
        CHECK(finite(baked.value().sky()));
        CHECK(finite(baked.value().prefiltered()));
        f32 brightest = 0.0f;
        for (const u16 half : baked.value().sky().texels) {
            brightest = std::max(brightest, unpack_half(half));
        }
        CHECK(brightest == 65504.0f);
        // Ordinary radiance is untouched.
        CHECK(baked.value().sky().texel(0, 2, 0, 0).x == doctest::Approx(1.0f).epsilon(0.01));
    }

    TEST_CASE("bake_environment - NaN, infinite, negative and overflowing radiance filled in by hand bakes finite") {
        // An infinite texel times a bilinear weight of 0 is NaN, which the SH9 projection would spread into every
        // coefficient, and the largest float overflows the mip chain's 4-texel sums. sample() reads NaN and
        // negative values as 0 and anything above MAX_ENVIRONMENT_RADIANCE as that; a bright 1e9 stays 1e9.
        const f32 infinity = std::numeric_limits<f32>::infinity();
        equirect_image dirty = equirect_image::constant(32, 16, math::vec3(1.0f));
        equirect_image clean = dirty;
        const std::array<std::array<f32, 2>, 6> texels = {{
          {infinity, MAX_ENVIRONMENT_RADIANCE}, {std::numeric_limits<f32>::quiet_NaN(), 0.0f}, {-3.0f, 0.0f},
          {-infinity, 0.0f}, {std::numeric_limits<f32>::max(), MAX_ENVIRONMENT_RADIANCE}, {1.0e9f, 1.0e9f}
        }};
        for (usize index = 0; index < texels.size(); ++index) {
            const usize offset = (static_cast<usize>(7) * dirty.width + 13 + index) * 3;
            dirty.rgb[offset] = texels[index][0];
            clean.rgb[offset] = texels[index][1];
        }
        core::result<environment_asset> dirtyBaked = bake_environment(dirty, small_options(), nullptr);
        core::result<environment_asset> cleanBaked = bake_environment(clean, small_options(), nullptr);
        CONJURE_REQUIRE_OK(dirtyBaked);
        CONJURE_REQUIRE_OK(cleanBaked);
        for (const math::vec3 &coefficient : dirtyBaked.value().irradiance()) {
            CHECK(std::isfinite(coefficient.x));
            CHECK(std::isfinite(coefficient.y));
            CHECK(std::isfinite(coefficient.z));
        }
        const math::vec3 up = evaluate_sh9_irradiance(dirtyBaked.value().irradiance(), math::vec3(0.0f, 1.0f, 0.0f));
        CHECK(std::isfinite(up.x));
        CHECK(dirtyBaked.value() == cleanBaked.value());
        const math::vec3 towardTexels = dirty.sample(math::vec3(0.0f, 0.0f, 1.0f));
        CHECK(std::isfinite(towardTexels.x));
        CHECK(towardTexels.x <= MAX_ENVIRONMENT_RADIANCE);
    }

    TEST_CASE("regression - bake_environment integrates a sun brighter than the half range unclipped") {
        // A sun of 1e5 on a sky of 1, and the same sky a thousand times darker (a sun of 100, inside the half
        // range). The bake is linear, so the bright sky's SH9 and rough prefiltered mip are the dark one's times
        // 1000: clipping the sun at 65504 before integrating would take a third of its light. Only the stored
        // cubemap texels are clamped.
        constexpr f32 SCALE = 1000.0f;
        equirect_image bright = equirect_image::constant(64, 32, math::vec3(1.0f));
        for (u32 y = 6; y < 10; ++y) {
            for (u32 x = 30; x < 34; ++x) {
                const usize index = (static_cast<usize>(y) * bright.width + x) * 3;
                bright.rgb[index] = 1.0e5f;
                bright.rgb[index + 1] = 1.0e5f;
                bright.rgb[index + 2] = 1.0e5f;
            }
        }
        equirect_image dark = bright;
        for (f32 &value : dark.rgb) {
            value /= SCALE;
        }
        environment_bake_options options = {};
        options.skySize = 32;
        options.prefilteredSize = 16;
        options.sampleCount = 32;
        core::result<environment_asset> brightBaked = bake_environment(bright, options, nullptr);
        core::result<environment_asset> darkBaked = bake_environment(dark, options, nullptr);
        CONJURE_REQUIRE_OK(brightBaked);
        CONJURE_REQUIRE_OK(darkBaked);

        for (usize coefficient = 0; coefficient < brightBaked.value().irradiance().size(); ++coefficient) {
            CAPTURE(coefficient);
            const math::vec3 &actual = brightBaked.value().irradiance()[coefficient];
            const math::vec3 expected = darkBaked.value().irradiance()[coefficient] * SCALE;
            CHECK(actual.x == doctest::Approx(expected.x).epsilon(0.001));
            CHECK(actual.y == doctest::Approx(expected.y).epsilon(0.001));
            CHECK(actual.z == doctest::Approx(expected.z).epsilon(0.001));
        }
        // The sun dominates: band 0 is far above the sky's own share (4 pi x 0.282 = 3.5).
        CHECK(brightBaked.value().irradiance()[0].x > 1000.0f);

        const cubemap_data &brightPrefiltered = brightBaked.value().prefiltered();
        const cubemap_data &darkPrefiltered = darkBaked.value().prefiltered();
        const u32 roughest = brightPrefiltered.mipCount - 1;
        const u32 edge = brightPrefiltered.mip_size(roughest);
        for (u32 face = 0; face < CUBEMAP_FACE_COUNT; ++face) {
            for (u32 y = 0; y < edge; ++y) {
                for (u32 x = 0; x < edge; ++x) {
                    CAPTURE(face);
                    const f32 actual = brightPrefiltered.texel(roughest, face, x, y).x;
                    const f32 expected = darkPrefiltered.texel(roughest, face, x, y).x * SCALE;
                    REQUIRE(actual < 65504.0f);
                    CHECK(actual == doctest::Approx(expected).epsilon(0.003));
                }
            }
        }
    }

    TEST_CASE("bake_environment - radiance at MAX_ENVIRONMENT_RADIANCE keeps every sum of the bake finite") {
        // The bound is what the largest sums of the bake can take: the mip chain's four texels, the SH9
        // projection and the prefilter's 4096 samples (the most environment_bake_options allows).
        CHECK(MAX_ENVIRONMENT_RADIANCE > 1.0e33f);
        const equirect_image brightest = equirect_image::constant(16, 8, math::vec3(MAX_ENVIRONMENT_RADIANCE));
        ibl::cubemap_f32 sky = ibl::equirect_to_cubemap(brightest, 8, ibl::environment_mip_count(8), nullptr);
        ibl::build_mip_chain(sky, nullptr);
        const ibl::cubemap_f32 prefiltered = ibl::prefilter_ggx(sky, 2, 2, 4096, nullptr);
        const auto all_finite = [](const ibl::cubemap_f32 &cubemap) {
            return std::ranges::all_of(cubemap.texels, [](const math::vec3 &texel) {
                return std::isfinite(texel.x) && std::isfinite(texel.y) && std::isfinite(texel.z);
            });
        };
        CHECK(all_finite(sky));
        CHECK(all_finite(prefiltered));
        CHECK(prefiltered.at(1, 0, 0, 0).x == doctest::Approx(MAX_ENVIRONMENT_RADIANCE).epsilon(0.001));

        core::result<environment_asset> baked = bake_environment(brightest, small_options(), nullptr);
        CONJURE_REQUIRE_OK(baked);
        CHECK(baked.value().irradiance()[0].x ==
          doctest::Approx(MAX_ENVIRONMENT_RADIANCE * 4.0f * math::PI * 0.282095f).epsilon(0.001));
        const math::vec3 up = evaluate_sh9_irradiance(baked.value().irradiance(), math::vec3(0.0f, 1.0f, 0.0f));
        CHECK(std::isfinite(up.x));

        // A sky at the largest float would overflow four texels' sum; it bakes as the bound does.
        const equirect_image beyond =
          equirect_image::constant(16, 8, math::vec3(std::numeric_limits<f32>::max()));
        core::result<environment_asset> beyondBaked = bake_environment(beyond, small_options(), nullptr);
        CONJURE_REQUIRE_OK(beyondBaked);
        CHECK(std::isfinite(beyondBaked.value().irradiance()[0].x));
        CHECK(beyondBaked.value() == baked.value());
        CHECK(ENVIRONMENT_IMPORTER_VERSION == 3);       // the sanitised samples change such a source's bake
    }

    TEST_CASE("environment_importer - decodes and bakes the .hdr with the settings, inline or on jobs") {
        const environment_importer importer(nullptr);
        CHECK(importer.id() == ENVIRONMENT_IMPORTER_ID);
        CHECK(importer.version() == ENVIRONMENT_IMPORTER_VERSION);
        REQUIRE(importer.extensions().size() == 1);
        CHECK(importer.extensions()[0] == ".hdr");
        CHECK(importer.default_settings().root()["skySize"].as_i64() == 512);

        core::vfs::memory_file_system fileSystem = {};
        assets::sub_asset_table subAssets = {};
        const std::vector<std::byte> hdr = two_row_hdr();
        const core::json::document settings = small_options().to_document();
        assets::import_context context(
          fileSystem,
          "assets://sky/dawn.hdr",
          "assets/sky/dawn.hdr",
          hdr,
          settings.root(),
          "windows",
          renderer_support::test_asset_id(12),
          subAssets,
          {}
        );
        core::result<assets::import_output> imported = importer.import(context);
        CONJURE_REQUIRE_OK(imported);
        REQUIRE(imported.value().assets.size() == 1);
        const assets::imported_asset &primary = imported.value().assets[0];
        CHECK(primary.subPath.empty());
        CHECK(primary.type == assets::asset_type_traits<environment_asset>::id());
        CHECK(primary.displayName == "dawn");
        CHECK(primary.dependencies.empty());
        core::result<environment_asset> cooked = environment_asset::parse_blob(primary.payload);
        CONJURE_REQUIRE_OK(cooked);
        CHECK(cooked.value().sky().size == 8);
        CHECK(cooked.value().name() == "dawn");

        // The same import on a job system is bit-identical (the importer output is a cache key).
        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = environment().jobThreadCount;
        CONJURE_REQUIRE_OK(jobs.start(desc));
        const environment_importer parallelImporter(&jobs);
        assets::import_context parallelContext(
          fileSystem,
          "assets://sky/dawn.hdr",
          "assets/sky/dawn.hdr",
          hdr,
          settings.root(),
          "windows",
          renderer_support::test_asset_id(12),
          subAssets,
          {}
        );
        core::result<assets::import_output> parallel = parallelImporter.import(parallelContext);
        jobs.stop();
        CONJURE_REQUIRE_OK(parallel);
        REQUIRE(parallel.value().assets.size() == 1);
        CHECK(parallel.value().assets[0].payload == primary.payload);

        const std::vector<std::byte> garbage = bytes_of("not radiance");
        assets::import_context badContext(
          fileSystem,
          "assets://a.hdr",
          "assets/a.hdr",
          garbage,
          settings.root(),
          "windows",
          renderer_support::test_asset_id(13),
          subAssets,
          {}
        );
        CONJURE_CHECK_ERROR(importer.import(badContext), assets::assets_error::IMPORT_FAILED);
        const core::json::document badSettings = core::json::object({{"skySize", 12}});
        assets::import_context badSettingsContext(
          fileSystem,
          "assets://a.hdr",
          "assets/a.hdr",
          hdr,
          badSettings.root(),
          "windows",
          renderer_support::test_asset_id(14),
          subAssets,
          {}
        );
        CONJURE_CHECK_ERROR(importer.import(badSettingsContext), assets::assets_error::IMPORT_FAILED);
    }

    TEST_CASE("environment_importer - bakes a float JPEG XL equirect exactly like the same .hdr") {
        // .jxl belongs to the texture importer; an HDR JPEG XL reaches this one through its .meta, and the
        // importer reads it by its signature.
        const std::vector<std::byte> hdr = two_row_hdr();
        const std::vector<std::byte> jxl = two_row_jpeg_xl();
        core::result<equirect_image> fromHdr = equirect_image::decode_hdr(hdr);
        core::result<equirect_image> fromJxl = equirect_image::decode_hdr(jxl);
        CONJURE_REQUIRE_OK(fromHdr);
        CONJURE_REQUIRE_OK(fromJxl);
        CHECK(fromJxl.value().width == fromHdr.value().width);
        CHECK(fromJxl.value().height == fromHdr.value().height);
        CHECK(fromJxl.value().rgb == fromHdr.value().rgb);

        core::vfs::memory_file_system fileSystem = {};
        assets::sub_asset_table subAssets = {};
        const core::json::document settings = small_options().to_document();
        const auto cook = [&](
          const environment_importer &which,
          std::string_view name,
          std::span<const std::byte> bytes
        ) {
            assets::import_context context(
              fileSystem,
              std::string("assets://sky/") + std::string(name),
              std::string("assets/sky/") + std::string(name),
              bytes,
              settings.root(),
              "windows",
              renderer_support::test_asset_id(15),
              subAssets,
              {}
            );
            return which.import(context);
        };
        const environment_importer importer(nullptr);
        core::result<assets::import_output> cookedHdr = cook(importer, "dawn.hdr", hdr);
        core::result<assets::import_output> cookedJxl = cook(importer, "dawn.jxl", jxl);
        CONJURE_REQUIRE_OK(cookedHdr);
        CONJURE_REQUIRE_OK(cookedJxl);
        REQUIRE(cookedJxl.value().assets.size() == 1);
        CHECK(cookedJxl.value().assets[0].displayName == "dawn");
        CHECK(cookedJxl.value().assets[0].payload == cookedHdr.value().assets[0].payload);

        // Decoded and baked on jobs, the bytes are the same.
        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = environment().jobThreadCount;
        CONJURE_REQUIRE_OK(jobs.start(desc));
        const environment_importer parallelImporter(&jobs);
        core::result<assets::import_output> parallel = cook(parallelImporter, "dawn.jxl", jxl);
        jobs.stop();
        CONJURE_REQUIRE_OK(parallel);
        CHECK(parallel.value().assets[0].payload == cookedHdr.value().assets[0].payload);

        // A truncated JPEG XL fails the import.
        const std::span<const std::byte> truncated = std::span<const std::byte>(jxl).first(jxl.size() / 2);
        CONJURE_CHECK_ERROR(cook(importer, "cut.jxl", truncated), assets::assets_error::IMPORT_FAILED);
    }

    TEST_CASE("environment_loader - loads a blob and falls back to the uniform grey environment") {
        const environment_loader loader = {};
        CHECK(loader.type() == assets::asset_type_traits<environment_asset>::id());
        const environment_asset source = environment_asset::uniform(math::vec3(0.75f, 0.5f, 0.25f), 4);
        const std::vector<std::byte> blob = source.write_blob();
        assets::load_context context = {};
        context.id = renderer_support::test_asset_id(15);
        context.type = loader.type();
        core::result<assets::asset_payload> loaded = loader.load_cpu(context, blob);
        CONJURE_REQUIRE_OK(loaded);
        REQUIRE(assets::payload_as<environment_asset>(loaded.value()) != nullptr);
        CHECK(*assets::payload_as<environment_asset>(loaded.value()) == source);
        core::result<assets::asset_payload> finalised = loader.finalise(
          assets::finalise_context{},
          std::move(loaded).value()
        );
        CONJURE_REQUIRE_OK(finalised);
        CHECK(*assets::payload_as<environment_asset>(finalised.value()) == source);
        CONJURE_CHECK_ERROR(
          loader.load_cpu(context, std::span<const std::byte>(blob).subspan(0, 7)),
          renderer_error::INVALID_ASSET
        );
        core::result<assets::asset_payload> fallback = loader.create_fallback();
        CONJURE_REQUIRE_OK(fallback);
        const environment_asset *grey = assets::payload_as<environment_asset>(fallback.value());
        REQUIRE(grey != nullptr);
        CHECK(grey->sky().is_consistent());
        const math::vec3 texel = grey->sky().texel(0, 0, 0, 0);
        CHECK(texel.x == doctest::Approx(FALLBACK_ENVIRONMENT_RADIANCE).epsilon(0.01));
        CHECK(*grey == environment_asset::uniform(math::vec3(FALLBACK_ENVIRONMENT_RADIANCE)));
        CHECK(texel.x == doctest::Approx(texel.y));
        CHECK(texel.x == doctest::Approx(texel.z));
        CHECK(ENVIRONMENT_ASSET_TYPE_NAME == "conjure.renderer.environment");
    }
}
