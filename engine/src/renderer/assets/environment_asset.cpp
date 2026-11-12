#include "conjure/renderer/environment_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/renderer_error.h"
#include "renderer/assets/blob_io.h"
#include "renderer/import/ibl_baker.h"
#include "renderer/import/image_loader.h"

#include "conjure/assets/assets_error.h"
#include "conjure/core/assert.h"
#include "conjure/core/error.h"
#include "conjure/core/vfs/path_utils.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <utility>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    constexpr std::array<char, 4> ENVIRONMENT_MAGIC = {'C', 'J', 'E', 'N'};
    constexpr u32 MIN_BAKE_SIZE = 8;
    constexpr u32 MAX_BAKE_SIZE = 4096;
    constexpr u32 MAX_BAKE_SAMPLES = 4096;
    constexpr std::string_view KEY_SKY_SIZE = "skySize";
    constexpr std::string_view KEY_PREFILTERED_SIZE = "prefilteredSize";
    constexpr std::string_view KEY_SAMPLE_COUNT = "sampleCount";

    constexpr f32 SH_A0 = math::PI;
    constexpr f32 SH_A1 = 2.0f * math::PI / 3.0f;
    constexpr f32 SH_A2 = math::PI / 4.0f;

    [[nodiscard]] core::error invalid_options(std::string_view detail) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "environment bake options: {}", detail);
    }

    [[nodiscard]] core::result<u32> read_u32_setting(
      const core::json::value &object,
      std::string_view key,
      u32 fallback
    ) {
        const core::json::value member = object[key];
        if (!member.is_valid()) {
            return fallback;
        }
        if (!member.is_integer() || member.as_i64(-1) < 0) {
            return invalid_options(std::string(key) + " must be a non-negative integer");
        }
        return static_cast<u32>(std::min<i64>(member.as_i64(0), 0xFFFFFFFFll));
    }

    [[nodiscard]] core::result<cubemap_data> read_cubemap(detail::blob_reader &reader) {
        cubemap_data data = {};
        u64 halfCount = 0;
        CONJURE_TRY_ASSIGN(data.size, reader.read_u32());
        CONJURE_TRY_ASSIGN(data.mipCount, reader.read_u32());
        CONJURE_TRY_ASSIGN(halfCount, reader.read_u64());
        if (data.size == 0 || data.size > MAX_BAKE_SIZE || data.mipCount == 0 || data.mipCount > 16) {
            return reader.invalid("cubemap size or mip count out of range");
        }
        // mip_size clamps at 1, so a longer chain would repeat 1x1 levels nothing produces.
        if (data.mipCount > static_cast<u32>(std::bit_width(data.size))) {
            return reader.invalid("cubemap mip count exceeds the chain of its size");
        }
        usize expected = 0;
        for (u32 mip = 0; mip < data.mipCount; ++mip) {
            expected += data.face_length(mip) * CUBEMAP_FACE_COUNT;
        }
        if (halfCount != expected) {
            return reader.invalid("cubemap texel count does not match its size");
        }
        CONJURE_TRY_ASSIGN(data.texels, reader.read_array<u16>(static_cast<usize>(halfCount), "cubemap texels"));
        return data;
    }

    void write_cubemap(const cubemap_data &data, detail::blob_writer &writer) {
        writer.write_u32(data.size);
        writer.write_u32(data.mipCount);
        writer.write_u64(data.texels.size());
        writer.write_array<u16>(data.texels);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// cubemap_data

u32 cubemap_data::mip_size(u32 mip) const noexcept {
    return std::max<u32>(1, size >> mip);
}

usize cubemap_data::face_length(u32 mip) const noexcept {
    const usize edge = mip_size(mip);
    return edge * edge * 4;
}

usize cubemap_data::face_offset(u32 mip, u32 face) const noexcept {
    usize offset = 0;
    for (u32 level = 0; level < mip; ++level) {
        offset += face_length(level) * CUBEMAP_FACE_COUNT;
    }
    return offset + face_length(mip) * face;
}

math::vec3 cubemap_data::texel(u32 mip, u32 face, u32 x, u32 y) const noexcept {
    const usize index = face_offset(mip, face) + (static_cast<usize>(y) * mip_size(mip) + x) * 4;
    if (index + 2 >= texels.size()) {
        return math::vec3(0.0f);
    }
    return math::vec3(unpack_half(texels[index]), unpack_half(texels[index + 1]), unpack_half(texels[index + 2]));
}

bool cubemap_data::is_consistent() const noexcept {
    if (size == 0 || mipCount == 0) {
        return texels.empty();
    }
    usize expected = 0;
    for (u32 mip = 0; mip < mipCount; ++mip) {
        expected += face_length(mip) * CUBEMAP_FACE_COUNT;
    }
    return texels.size() == expected;
}

/*===========================================================================*/
/*===========================================================================*/
// equirect_image

core::result<equirect_image> equirect_image::decode_hdr(std::span<const std::byte> encoded) {
    detail::decoded_hdr_image decoded = {};
    CONJURE_TRY_ASSIGN(decoded, detail::decode_hdr_rgb(encoded));
    equirect_image image = {};
    image.width = decoded.width;
    image.height = decoded.height;
    image.rgb = std::move(decoded.rgb);
    return image;
}

equirect_image equirect_image::constant(u32 width, u32 height, const math::vec3 &colour) {
    equirect_image image = {};
    image.width = std::max<u32>(width, 1);
    image.height = std::max<u32>(height, 1);
    image.rgb.resize(static_cast<usize>(image.width) * image.height * 3);
    for (usize index = 0; index < image.rgb.size(); index += 3) {
        image.rgb[index] = colour.x;
        image.rgb[index + 1] = colour.y;
        image.rgb[index + 2] = colour.z;
    }
    return image;
}

math::vec3 equirect_image::sample(const math::vec3 &direction) const noexcept {
    if (!is_valid()) {
        return math::vec3(0.0f);
    }
    const math::vec3 unit = math::safe_normalize(direction, math::vec3(0.0f, 0.0f, 1.0f));
    // +Z maps to the image centre, -Z to the seam; v runs from the zenith (0) to the nadir (1).
    const f32 u = 0.5f + std::atan2(unit.x, unit.z) / math::TWO_PI;
    const f32 v = std::acos(math::clamp(unit.y, -1.0f, 1.0f)) / math::PI;

    const f32 fx = u * static_cast<f32>(width) - 0.5f;
    const f32 fy = math::clamp(v * static_cast<f32>(height) - 0.5f, 0.0f, static_cast<f32>(height - 1));
    const f32 floorX = std::floor(fx);
    const i32 x0 = static_cast<i32>(floorX);
    const u32 y0 = static_cast<u32>(fy);
    const u32 y1 = std::min(y0 + 1, height - 1);
    const f32 wx = fx - floorX;
    const f32 wy = fy - static_cast<f32>(y0);
    const auto wrapX = [this](i32 x) -> u32 {
        const i32 w = static_cast<i32>(width);
        return static_cast<u32>(((x % w) + w) % w);
    };
    const auto pixel = [this](u32 x, u32 y) -> math::vec3 {
        const usize index = (static_cast<usize>(y) * width + x) * 3;
        return math::vec3(rgb[index], rgb[index + 1], rgb[index + 2]);
    };
    const u32 xa = wrapX(x0);
    const u32 xb = wrapX(x0 + 1);
    const math::vec3 top = pixel(xa, y0) * (1.0f - wx) + pixel(xb, y0) * wx;
    const math::vec3 bottom = pixel(xa, y1) * (1.0f - wx) + pixel(xb, y1) * wx;
    return top * (1.0f - wy) + bottom * wy;
}

/*===========================================================================*/
/*===========================================================================*/
// environment_bake_options

core::result<environment_bake_options> environment_bake_options::from_json(const core::json::value &source) {
    environment_bake_options options = {};
    if (!source.is_valid() || source.is_null()) {
        return options;
    }
    if (!source.is_object()) {
        return invalid_options("must be a JSON object");
    }
    CONJURE_TRY_ASSIGN(options.skySize, read_u32_setting(source, KEY_SKY_SIZE, options.skySize));
    CONJURE_TRY_ASSIGN(
      options.prefilteredSize,
      read_u32_setting(source, KEY_PREFILTERED_SIZE, options.prefilteredSize)
    );
    CONJURE_TRY_ASSIGN(options.sampleCount, read_u32_setting(source, KEY_SAMPLE_COUNT, options.sampleCount));
    return options;
}

void environment_bake_options::to_json(core::json::writer &out) const {
    out.begin_object();
    out.key(KEY_PREFILTERED_SIZE);
    out.value(prefilteredSize);
    out.key(KEY_SAMPLE_COUNT);
    out.value(sampleCount);
    out.key(KEY_SKY_SIZE);
    out.value(skySize);
    out.end_object();
}

core::json::document environment_bake_options::to_document() const {
    core::json::writer out = {};
    to_json(out);
    core::result<core::json::document> parsed = core::json::document::parse(out.finish());
    CONJURE_ASSERT(parsed.has_value(), "environment_bake_options writes valid JSON");
    return parsed.has_value() ? std::move(parsed).value() : core::json::document::object();
}

/*===========================================================================*/
/*===========================================================================*/
// environment_asset

environment_asset::environment_asset() {}

environment_asset::environment_asset(
  cubemap_data sky,
  cubemap_data prefiltered,
  const sh9 &irradiance,
  std::string name
) :
    mSky(std::move(sky)),
    mPrefiltered(std::move(prefiltered)),
    mIrradiance(irradiance),
    mName(std::move(name)) {}

environment_asset::~environment_asset() {}

core::result<environment_asset> environment_asset::parse_blob(std::span<const std::byte> bytes) {
    detail::blob_reader reader(bytes, "cenv");
    CONJURE_TRY(reader.expect_magic(ENVIRONMENT_MAGIC));
    u32 version = 0;
    CONJURE_TRY_ASSIGN(version, reader.read_u32());
    if (version != ENVIRONMENT_BLOB_VERSION) {
        return reader.invalid("unsupported version");
    }
    environment_asset result = {};
    CONJURE_TRY_ASSIGN(result.mName, reader.read_string(detail::MAX_BLOB_NAME_LENGTH));
    for (math::vec3 &coefficient : result.mIrradiance) {
        CONJURE_TRY_ASSIGN(coefficient, reader.read_vec3());
    }
    CONJURE_TRY_ASSIGN(result.mSky, read_cubemap(reader));
    CONJURE_TRY_ASSIGN(result.mPrefiltered, read_cubemap(reader));
    if (reader.remaining() != 0) {
        return reader.invalid("trailing bytes");
    }
    return result;
}

std::vector<std::byte> environment_asset::write_blob() const {
    detail::blob_writer writer = {};
    writer.write_magic(ENVIRONMENT_MAGIC);
    writer.write_u32(ENVIRONMENT_BLOB_VERSION);
    writer.write_string(mName);
    for (const math::vec3 &coefficient : mIrradiance) {
        writer.write_vec3(coefficient);
    }
    write_cubemap(mSky, writer);
    write_cubemap(mPrefiltered, writer);
    return writer.finish();
}

environment_asset environment_asset::uniform(const math::vec3 &radiance, u32 size) {
    const u32 edge = std::clamp<u32>(size, 1, MAX_BAKE_SIZE);
    const u32 mips = detail::environment_mip_count(edge);
    detail::cubemap_f32 chain = detail::cubemap_f32::allocate(edge, mips);
    std::fill(chain.texels.begin(), chain.texels.end(), radiance);
    sh9 coefficients = {};
    coefficients.fill(math::vec3(0.0f));
    // The projection of a constant: 4 pi * Y00 * L on band 0, nothing on the others.
    coefficients[0] = radiance * (4.0f * math::PI * 0.282095f);
    return environment_asset(detail::pack_cubemap(chain), detail::pack_cubemap(chain), coefficients, "uniform");
}

u64 environment_asset::gpu_byte_size() const noexcept {
    return (static_cast<u64>(mSky.texels.size()) + mPrefiltered.texels.size()) * sizeof(u16);
}

bool environment_asset::operator==(const environment_asset &other) const noexcept {
    return mName == other.mName && mIrradiance == other.mIrradiance && mSky == other.mSky
      && mPrefiltered == other.mPrefiltered;
}

/*===========================================================================*/
/*===========================================================================*/
// Free functions

core::result<environment_asset> conjure::renderer::bake_environment(
  const equirect_image &source,
  const environment_bake_options &options,
  core::jobs::job_system *jobs
) {
    if (!source.is_valid()) {
        return invalid_options("the equirect image is empty or inconsistent");
    }
    const auto validSize = [](u32 size) {
        return size >= MIN_BAKE_SIZE && size <= MAX_BAKE_SIZE && math::is_power_of_two(size);
    };
    if (!validSize(options.skySize) || !validSize(options.prefilteredSize)) {
        return invalid_options("skySize and prefilteredSize must be powers of two in [8, 4096]");
    }
    if (options.prefilteredSize > options.skySize) {
        return invalid_options("prefilteredSize must not exceed skySize");
    }
    if (options.sampleCount == 0 || options.sampleCount > MAX_BAKE_SAMPLES) {
        return invalid_options("sampleCount must be in [1, 4096]");
    }

    detail::cubemap_f32 sky = detail::equirect_to_cubemap(
      source,
      options.skySize,
      detail::environment_mip_count(options.skySize),
      jobs
    );
    detail::build_mip_chain(sky, jobs);

    // Integrate the mip closest to 32^2 (mip 4 of a 512^2 sky).
    const u32 shMip = static_cast<u32>(std::max<i32>(0, static_cast<i32>(math::log2_floor(options.skySize)) - 5));
    const sh9 irradiance = detail::project_sh9(sky, std::min(shMip, sky.mipCount - 1));

    const detail::cubemap_f32 prefiltered = detail::prefilter_ggx(
      sky,
      options.prefilteredSize,
      detail::environment_mip_count(options.prefilteredSize),
      options.sampleCount,
      jobs
    );
    return environment_asset(detail::pack_cubemap(sky), detail::pack_cubemap(prefiltered), irradiance, {});
}

math::vec3 conjure::renderer::evaluate_sh9_irradiance(const sh9 &coefficients, const math::vec3 &normal) noexcept {
    const math::vec3 n = math::safe_normalize(normal, math::vec3(0.0f, 1.0f, 0.0f));
    math::vec3 irradiance = coefficients[0] * (SH_A0 * 0.282095f);
    irradiance += coefficients[1] * (SH_A1 * 0.488603f * n.y);
    irradiance += coefficients[2] * (SH_A1 * 0.488603f * n.z);
    irradiance += coefficients[3] * (SH_A1 * 0.488603f * n.x);
    irradiance += coefficients[4] * (SH_A2 * 1.092548f * n.x * n.y);
    irradiance += coefficients[5] * (SH_A2 * 1.092548f * n.y * n.z);
    irradiance += coefficients[6] * (SH_A2 * 0.315392f * (3.0f * n.z * n.z - 1.0f));
    irradiance += coefficients[7] * (SH_A2 * 1.092548f * n.x * n.z);
    irradiance += coefficients[8] * (SH_A2 * 0.546274f * (n.x * n.x - n.y * n.y));
    return glm::max(irradiance, math::vec3(0.0f));
}

math::vec3 conjure::renderer::cubemap_texel_direction(u32 face, u32 x, u32 y, u32 size) noexcept {
    const f32 inverse = 1.0f / static_cast<f32>(std::max<u32>(size, 1));
    const f32 u = (static_cast<f32>(x) + 0.5f) * inverse * 2.0f - 1.0f;
    const f32 v = (static_cast<f32>(y) + 0.5f) * inverse * 2.0f - 1.0f;
    math::vec3 direction(0.0f, 0.0f, 1.0f);
    switch (face) {
        case 0: {
            direction = math::vec3(1.0f, -v, -u);
            break;
        }
        case 1: {
            direction = math::vec3(-1.0f, -v, u);
            break;
        }
        case 2: {
            direction = math::vec3(u, 1.0f, v);
            break;
        }
        case 3: {
            direction = math::vec3(u, -1.0f, -v);
            break;
        }
        case 4: {
            direction = math::vec3(u, -v, 1.0f);
            break;
        }
        case 5: {
            direction = math::vec3(-u, -v, -1.0f);
            break;
        }
        default: {
            break;
        }
    }
    return glm::normalize(direction);
}

/*===========================================================================*/
/*===========================================================================*/
// environment_importer

environment_importer::environment_importer(core::jobs::job_system *jobs) :
    mJobs(jobs) {}

environment_importer::~environment_importer() {}

std::string_view environment_importer::id() const noexcept {
    return ENVIRONMENT_IMPORTER_ID;
}

u32 environment_importer::version() const noexcept {
    return ENVIRONMENT_IMPORTER_VERSION;
}

std::span<const std::string_view> environment_importer::extensions() const noexcept {
    return ENVIRONMENT_EXTENSIONS;
}

core::json::document environment_importer::default_settings() const {
    return environment_bake_options().to_document();
}

core::result<assets::import_output> environment_importer::import(assets::import_context &context) const {
    core::result<environment_bake_options> options = environment_bake_options::from_json(context.settings());
    if (!options.has_value()) {
        return assets::make_asset_error(assets::asset_error_code::IMPORT_FAILED, options.get_error().message());
    }
    core::result<equirect_image> image = equirect_image::decode_hdr(context.source_bytes());
    if (!image.has_value()) {
        return assets::make_asset_error(assets::asset_error_code::IMPORT_FAILED, image.get_error().message());
    }
    core::result<environment_asset> baked = bake_environment(image.value(), options.value(), mJobs);
    if (!baked.has_value()) {
        return assets::make_asset_error(assets::asset_error_code::IMPORT_FAILED, baked.get_error().message());
    }
    baked.value().set_name(std::string(core::vfs::path_utils::stem(context.source_path())));

    assets::imported_asset primary = {};
    primary.displayName = std::string(baked.value().name());
    primary.type = assets::asset_type_traits<environment_asset>::id();
    primary.payload = baked.value().write_blob();
    assets::import_output output = {};
    output.assets.push_back(std::move(primary));
    return output;
}

/*===========================================================================*/
/*===========================================================================*/
// environment_loader

environment_loader::environment_loader() {}

environment_loader::~environment_loader() {}

core::result<environment_asset> environment_loader::load_cpu_typed(
  const assets::load_context &,
  std::span<const std::byte> payloadBytes
) const {
    return environment_asset::parse_blob(payloadBytes);
}

core::result<environment_asset> environment_loader::finalise_typed(
  const assets::finalise_context &,
  environment_asset &&intermediate
) const {
    return std::move(intermediate);
}

std::optional<environment_asset> environment_loader::create_fallback_typed() const {
    return environment_asset::uniform(math::vec3(FALLBACK_ENVIRONMENT_RADIANCE));
}
