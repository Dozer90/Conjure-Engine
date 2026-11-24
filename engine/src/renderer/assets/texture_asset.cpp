#include "conjure/renderer/texture_asset.h"
#include "conjure/renderer/renderer_error.h"
#include "renderer/assets/blob_io.h"
#include "renderer/import/file_bytes.h"
#include "renderer/import/image_loader.h"
#include "renderer/import/mip_generator.h"

#include "conjure/assets/assets_error.h"
#include "conjure/core/assert.h"
#include "conjure/core/error.h"
#include "conjure/core/vfs/path_utils.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <utility>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    constexpr std::array<char, 4> TEXTURE_MAGIC = {'C', 'J', 'T', 'X'};
    constexpr std::string_view KEY_COLOUR_SPACE = "colourSpace";
    constexpr std::string_view KEY_FILTER = "filter";
    constexpr std::string_view KEY_WRAP = "wrap";
    constexpr std::string_view KEY_GENERATE_MIPS = "generateMips";

    [[nodiscard]] rhi::texture_format format_for(texture_colour_space colourSpace) noexcept {
        return colourSpace == texture_colour_space::SRGB ? rhi::texture_format::RGBA8_SRGB
          : rhi::texture_format::RGBA8_UNORM;
    }

    [[nodiscard]] bool is_supported_format(rhi::texture_format format) noexcept {
        return format == rhi::texture_format::RGBA8_UNORM || format == rhi::texture_format::RGBA8_SRGB ||
          format == rhi::texture_format::R8_UNORM || format == rhi::texture_format::RG8_UNORM ||
          format == rhi::texture_format::RGBA16_FLOAT;
    }

    [[nodiscard]] bool parse_colour_space(std::string_view text, texture_colour_space &out) noexcept {
        if (text == "srgb") {
            out = texture_colour_space::SRGB;
            return true;
        }
        if (text == "linear") {
            out = texture_colour_space::LINEAR;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool parse_filter(std::string_view text, sampler_filter &out) noexcept {
        if (text == "linear") {
            out = sampler_filter::LINEAR;
            return true;
        }
        if (text == "point") {
            out = sampler_filter::POINT;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool parse_wrap(std::string_view text, texture_wrap &out) noexcept {
        if (text == "repeat") {
            out = texture_wrap::REPEAT;
            return true;
        }
        if (text == "clamp") {
            out = texture_wrap::CLAMP;
            return true;
        }
        if (text == "mirror") {
            out = texture_wrap::MIRROR;
            return true;
        }
        return false;
    }

    [[nodiscard]] core::error invalid_setting(std::string_view detail) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "texture import settings: {}", detail);
    }
}

u32 conjure::renderer::mip_count_for(u32 width, u32 height) noexcept {
    const u32 largest = std::max(std::max(width, height), 1u);
    return static_cast<u32>(std::bit_width(largest));
}

/*===========================================================================*/
/*===========================================================================*/
// texture_asset

texture_asset::texture_asset() {}

texture_asset::~texture_asset() {}

core::result<texture_asset> texture_asset::from_rgba8(
  u32 width,
  u32 height,
  std::span<const std::byte> pixels,
  texture_colour_space colourSpace,
  bool generateMips
) {
    if (width == 0 || height == 0) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "texture_asset::from_rgba8: zero size");
    }
    if (width > MAX_DIMENSION || height > MAX_DIMENSION) {
        return core::make_error(
          core::error_code::INVALID_ARGUMENT,
          "texture_asset::from_rgba8: {}x{} exceeds the {} texel limit",
          width,
          height,
          MAX_DIMENSION
        );
    }
    const usize expected = static_cast<usize>(width) * height * 4;
    if (pixels.size() != expected) {
        return core::make_error(
          core::error_code::INVALID_ARGUMENT,
          "texture_asset::from_rgba8: {} pixel bytes given, {} expected",
          pixels.size(),
          expected
        );
    }

    texture_asset result = {};
    result.mFormat = format_for(colourSpace);
    result.mColourSpace = colourSpace;
    if (generateMips) {
        result.mMips = detail::generate_rgba8_mips(width, height, pixels, colourSpace == texture_colour_space::SRGB);
    } else {
        texture_mip base = {};
        base.width = width;
        base.height = height;
        base.pixels.assign(pixels.begin(), pixels.end());
        result.mMips.push_back(std::move(base));
    }
    return result;
}

core::result<texture_asset> texture_asset::from_image(
  const core::image &source,
  texture_colour_space colourSpace,
  bool generateMips
) {
    return from_rgba8(source.width(), source.height(), source.pixels(), colourSpace, generateMips);
}

core::result<texture_asset> texture_asset::decode(
  std::span<const std::byte> encoded,
  texture_colour_space colourSpace,
  bool generateMips,
  core::jobs::job_system *jobs
) {
    detail::decoded_image image = {};
    CONJURE_TRY_ASSIGN(image, detail::decode_rgba8(encoded, colourSpace, jobs));
    return from_rgba8(image.width, image.height, image.rgba, colourSpace, generateMips);
}

core::result<texture_asset> texture_asset::load_from_file(
  const std::filesystem::path &path,
  texture_colour_space colourSpace,
  bool generateMips,
  core::jobs::job_system *jobs
) {
    std::vector<std::byte> bytes = {};
    CONJURE_TRY_ASSIGN(bytes, detail::read_file_bytes(path));
    core::result<texture_asset> decoded = decode(bytes, colourSpace, generateMips, jobs);
    if (decoded.has_value()) {
        decoded.value().mName = detail::path_utf8(path.filename());
    }
    return decoded;
}

texture_asset texture_asset::solid_colour(core::rgba8 colour, texture_colour_space colourSpace, u32 width, u32 height) {
    const u32 clampedWidth = std::clamp<u32>(width, 1, MAX_DIMENSION);
    const u32 clampedHeight = std::clamp<u32>(height, 1, MAX_DIMENSION);
    std::vector<std::byte> pixels(static_cast<usize>(clampedWidth) * clampedHeight * 4);
    for (usize index = 0; index < pixels.size(); index += 4) {
        pixels[index] = static_cast<std::byte>(colour.r);
        pixels[index + 1] = static_cast<std::byte>(colour.g);
        pixels[index + 2] = static_cast<std::byte>(colour.b);
        pixels[index + 3] = static_cast<std::byte>(colour.a);
    }
    core::result<texture_asset> built = from_rgba8(clampedWidth, clampedHeight, pixels, colourSpace, true);
    CONJURE_ASSERT(built.has_value(), "solid_colour input is always valid");
    return built.has_value() ? std::move(built).value() : texture_asset();
}

texture_asset texture_asset::checker(
  core::rgba8 first,
  core::rgba8 second,
  u32 size,
  u32 cellSize,
  texture_colour_space colourSpace
) {
    const u32 edge = std::clamp<u32>(size, 1, MAX_DIMENSION);
    const u32 cell = std::max<u32>(cellSize, 1);
    std::vector<std::byte> pixels(static_cast<usize>(edge) * edge * 4);
    for (u32 y = 0; y < edge; ++y) {
        for (u32 x = 0; x < edge; ++x) {
            const bool odd = (((x / cell) + (y / cell)) & 1u) != 0;
            const core::rgba8 colour = odd ? second : first;
            std::byte *texel = pixels.data() + (static_cast<usize>(y) * edge + x) * 4;
            texel[0] = static_cast<std::byte>(colour.r);
            texel[1] = static_cast<std::byte>(colour.g);
            texel[2] = static_cast<std::byte>(colour.b);
            texel[3] = static_cast<std::byte>(colour.a);
        }
    }
    core::result<texture_asset> built = from_rgba8(edge, edge, pixels, colourSpace, true);
    CONJURE_ASSERT(built.has_value(), "checker input is always valid");
    return built.has_value() ? std::move(built).value() : texture_asset();
}

core::result<texture_asset> texture_asset::parse_blob(std::span<const std::byte> bytes) {
    detail::blob_reader reader(bytes, "ctex");
    CONJURE_TRY(reader.expect_magic(TEXTURE_MAGIC));
    u32 version = 0;
    CONJURE_TRY_ASSIGN(version, reader.read_u32());
    if (version != TEXTURE_BLOB_VERSION) {
        return reader.invalid("unsupported version");
    }
    u8 formatValue = 0;
    u8 colourSpaceValue = 0;
    u8 filterValue = 0;
    u8 wrapValue = 0;
    u32 mipCount = 0;
    CONJURE_TRY_ASSIGN(formatValue, reader.read_u8());
    CONJURE_TRY_ASSIGN(colourSpaceValue, reader.read_u8());
    CONJURE_TRY_ASSIGN(filterValue, reader.read_u8());
    CONJURE_TRY_ASSIGN(wrapValue, reader.read_u8());
    CONJURE_TRY_ASSIGN(mipCount, reader.read_u32());

    if (formatValue >= static_cast<u8>(rhi::texture_format::COUNT) ||
      !is_supported_format(static_cast<rhi::texture_format>(formatValue))) {
        return reader.invalid("unsupported texture format");
    }
    const bool colourSpaceOk = colourSpaceValue <= static_cast<u8>(texture_colour_space::LINEAR);
    const bool filterOk = filterValue <= static_cast<u8>(sampler_filter::POINT);
    const bool wrapOk = wrapValue <= static_cast<u8>(texture_wrap::MIRROR);
    if (!colourSpaceOk || !filterOk || !wrapOk) {
        return reader.invalid("enumerator out of range");
    }
    // RGBA8 carries its colour space in the format (from_rgba8 picks it); the two must agree.
    const rhi::texture_format format = static_cast<rhi::texture_format>(formatValue);
    const bool isRgba8 = format == rhi::texture_format::RGBA8_UNORM || format == rhi::texture_format::RGBA8_SRGB;
    if (isRgba8 && format != format_for(static_cast<texture_colour_space>(colourSpaceValue))) {
        return reader.invalid("format does not match the colour space");
    }
    if (mipCount == 0 || mipCount > 15) {
        return reader.invalid("mip count out of range");
    }

    texture_asset result = {};
    result.mFormat = format;
    result.mColourSpace = static_cast<texture_colour_space>(colourSpaceValue);
    result.mFilter = static_cast<sampler_filter>(filterValue);
    result.mWrap = static_cast<texture_wrap>(wrapValue);
    CONJURE_TRY_ASSIGN(result.mName, reader.read_string(detail::MAX_BLOB_NAME_LENGTH));

    for (u32 mip = 0; mip < mipCount; ++mip) {
        texture_mip level = {};
        u64 byteCount = 0;
        CONJURE_TRY_ASSIGN(level.width, reader.read_u32());
        CONJURE_TRY_ASSIGN(level.height, reader.read_u32());
        CONJURE_TRY_ASSIGN(byteCount, reader.read_u64());
        if (level.width == 0 || level.height == 0 || level.width > MAX_DIMENSION || level.height > MAX_DIMENSION) {
            return reader.invalid("mip size out of range");
        }
        // The halving rule below clamps at 1x1, so without this a chain could repeat 1x1 mips.
        if (mip == 0 && mipCount > mip_count_for(level.width, level.height)) {
            return reader.invalid("mip count exceeds the chain of the base size");
        }
        const u64 expected = rhi::row_pitch(result.mFormat, level.width) * level.height;
        if (byteCount != expected) {
            return reader.invalid("mip byte count does not match its size");
        }
        if (mip > 0) {
            const texture_mip &previous = result.mMips.back();
            const bool halvedWidth = level.width == std::max<u32>(1, previous.width / 2);
            const bool halvedHeight = level.height == std::max<u32>(1, previous.height / 2);
            if (!halvedWidth || !halvedHeight) {
                return reader.invalid("mip chain is not halving");
            }
        }
        std::span<const std::byte> pixelBytes = {};
        CONJURE_TRY_ASSIGN(pixelBytes, reader.read_bytes(static_cast<usize>(byteCount), "mip pixels"));
        level.pixels.assign(pixelBytes.begin(), pixelBytes.end());
        result.mMips.push_back(std::move(level));
    }
    if (reader.remaining() != 0) {
        return reader.invalid("trailing bytes");
    }
    return result;
}

std::vector<std::byte> texture_asset::write_blob() const {
    detail::blob_writer writer = {};
    writer.write_magic(TEXTURE_MAGIC);
    writer.write_u32(TEXTURE_BLOB_VERSION);
    writer.write_u8(static_cast<u8>(mFormat));
    writer.write_u8(static_cast<u8>(mColourSpace));
    writer.write_u8(static_cast<u8>(mFilter));
    writer.write_u8(static_cast<u8>(mWrap));
    writer.write_u32(mip_count());
    writer.write_string(mName);
    for (const texture_mip &level : mMips) {
        writer.write_u32(level.width);
        writer.write_u32(level.height);
        writer.write_u64(level.pixels.size());
        writer.write_bytes(level.pixels);
    }
    return writer.finish();
}

u32 texture_asset::width() const noexcept {
    return mMips.empty() ? 0 : mMips.front().width;
}

u32 texture_asset::height() const noexcept {
    return mMips.empty() ? 0 : mMips.front().height;
}

u64 texture_asset::gpu_byte_size() const noexcept {
    u64 total = 0;
    for (const texture_mip &level : mMips) {
        total += level.pixels.size();
    }
    return total;
}

core::rgba8 texture_asset::pixel(u32 x, u32 y) const noexcept {
    const core::rgba8 black = {0, 0, 0, 255};
    if (mMips.empty()) {
        return black;
    }
    const texture_mip &base = mMips.front();
    CONJURE_ASSERT(x < base.width && y < base.height, "texture_asset::pixel: ({}, {}) out of range", x, y);
    if (x >= base.width || y >= base.height) {
        return black;
    }
    if (mFormat != rhi::texture_format::RGBA8_UNORM && mFormat != rhi::texture_format::RGBA8_SRGB) {
        return black;
    }
    const std::byte *texel = base.pixels.data() + (static_cast<usize>(y) * base.width + x) * 4;
    core::rgba8 colour = {};
    colour.r = static_cast<u8>(texel[0]);
    colour.g = static_cast<u8>(texel[1]);
    colour.b = static_cast<u8>(texel[2]);
    colour.a = static_cast<u8>(texel[3]);
    return colour;
}

bool texture_asset::operator==(const texture_asset &other) const noexcept {
    return mFormat == other.mFormat && mColourSpace == other.mColourSpace && mFilter == other.mFilter &&
      mWrap == other.mWrap && mName == other.mName && mMips == other.mMips;
}

/*===========================================================================*/
/*===========================================================================*/
// texture_import_settings

core::result<texture_import_settings> texture_import_settings::from_json(const core::json::value &source) {
    texture_import_settings settings = {};
    if (!source.is_valid() || source.is_null()) {
        return settings;
    }
    if (!source.is_object()) {
        return invalid_setting("must be a JSON object");
    }
    if (const core::json::value colourSpace = source[KEY_COLOUR_SPACE]; colourSpace.is_valid()) {
        if (!colourSpace.is_string() || !parse_colour_space(colourSpace.as_string(), settings.colourSpace)) {
            return invalid_setting("colourSpace must be \"srgb\" or \"linear\"");
        }
    }
    if (const core::json::value filter = source[KEY_FILTER]; filter.is_valid()) {
        if (!filter.is_string() || !parse_filter(filter.as_string(), settings.filter)) {
            return invalid_setting("filter must be \"linear\" or \"point\"");
        }
    }
    if (const core::json::value wrap = source[KEY_WRAP]; wrap.is_valid()) {
        if (!wrap.is_string() || !parse_wrap(wrap.as_string(), settings.wrap)) {
            return invalid_setting("wrap must be \"repeat\", \"clamp\" or \"mirror\"");
        }
    }
    if (const core::json::value mips = source[KEY_GENERATE_MIPS]; mips.is_valid()) {
        if (!mips.is_bool()) {
            return invalid_setting("generateMips must be a boolean");
        }
        settings.generateMips = mips.as_bool();
    }
    return settings;
}

void texture_import_settings::to_json(core::json::writer &out) const {
    out.begin_object();
    out.key(KEY_COLOUR_SPACE);
    out.value(colourSpace == texture_colour_space::SRGB ? "srgb" : "linear");
    out.key(KEY_FILTER);
    out.value(filter == sampler_filter::LINEAR ? "linear" : "point");
    out.key(KEY_GENERATE_MIPS);
    out.value(generateMips);
    out.key(KEY_WRAP);
    out.value(wrap == texture_wrap::REPEAT ? "repeat" : (wrap == texture_wrap::CLAMP ? "clamp" : "mirror"));
    out.end_object();
}

core::json::document texture_import_settings::to_document() const {
    core::json::writer out = {};
    to_json(out);
    core::result<core::json::document> parsed = core::json::document::parse(out.finish());
    CONJURE_ASSERT(parsed.has_value(), "texture_import_settings writes valid JSON");
    return parsed.has_value() ? std::move(parsed).value() : core::json::document::object();
}

/*===========================================================================*/
/*===========================================================================*/
// texture_importer

texture_importer::texture_importer() {}

texture_importer::texture_importer(core::jobs::job_system *jobs) :
    mJobs(jobs) {}

texture_importer::~texture_importer() {}

std::string_view texture_importer::id() const noexcept {
    return TEXTURE_IMPORTER_ID;
}

u32 texture_importer::version() const noexcept {
    return TEXTURE_IMPORTER_VERSION;
}

std::span<const std::string_view> texture_importer::extensions() const noexcept {
    return TEXTURE_EXTENSIONS;
}

core::json::document texture_importer::default_settings() const {
    return texture_import_settings().to_document();
}

core::result<assets::import_output> texture_importer::import(assets::import_context &context) const {
    core::result<texture_import_settings> settings = texture_import_settings::from_json(context.settings());
    if (!settings.has_value()) {
        return core::make_error(assets::assets_error::IMPORT_FAILED, settings.get_error().message());
    }
    core::result<texture_asset> decoded = texture_asset::decode(
      context.source_bytes(),
      settings.value().colourSpace,
      settings.value().generateMips,
      mJobs
    );
    if (!decoded.has_value()) {
        return core::make_error(assets::assets_error::IMPORT_FAILED, decoded.get_error().message());
    }
    texture_asset &texture = decoded.value();
    texture.set_filter(settings.value().filter);
    texture.set_wrap(settings.value().wrap);
    texture.set_name(std::string(core::vfs::path_utils::filename(context.source_path())));

    assets::imported_asset primary = {};
    primary.displayName = std::string(texture.name());
    primary.type = assets::asset_type_traits<texture_asset>::id();
    primary.payload = texture.write_blob();
    assets::import_output output = {};
    output.assets.push_back(std::move(primary));
    return output;
}

/*===========================================================================*/
/*===========================================================================*/
// texture_loader

texture_loader::texture_loader() {}

texture_loader::~texture_loader() {}

core::result<texture_asset> texture_loader::load_cpu_typed(
  const assets::load_context &,
  std::span<const std::byte> payloadBytes
) const {
    return texture_asset::parse_blob(payloadBytes);
}

core::result<texture_asset> texture_loader::finalise_typed(
  const assets::finalise_context &,
  texture_asset &&intermediate
) const {
    return std::move(intermediate);
}

std::optional<texture_asset> texture_loader::create_fallback_typed() const {
    return texture_asset::solid_colour(core::rgba8{255, 255, 255, 255}, texture_colour_space::SRGB);
}

/*===========================================================================*/
/*===========================================================================*/
// Names

std::string_view conjure::renderer::to_string(texture_colour_space value) noexcept {
    return value == texture_colour_space::SRGB ? "SRGB" : "LINEAR";
}

std::string_view conjure::renderer::to_string(sampler_filter value) noexcept {
    return value == sampler_filter::LINEAR ? "LINEAR" : "POINT";
}

std::string_view conjure::renderer::to_string(texture_wrap value) noexcept {
    switch (value) {
        case texture_wrap::REPEAT: {
            return "REPEAT";
        }
        case texture_wrap::CLAMP: {
            return "CLAMP";
        }
        case texture_wrap::MIRROR: {
            return "MIRROR";
        }
    }
    return "REPEAT";
}
