#include "conjure/renderer/material_asset.h"

#include "conjure/assets/assets_error.h"
#include "conjure/core/assert.h"
#include "conjure/core/error.h"
#include "conjure/core/reflect/json_conversion.h"
#include "conjure/core/uuid.h"
#include "conjure/core/vfs/path_utils.h"

#include <algorithm>
#include <utility>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    constexpr std::array<std::string_view, MATERIAL_TEXTURE_SLOTS> TEXTURE_KEYS = {
      "baseColour", "metallicRoughness", "normal", "occlusion", "emissive"
    };

    [[nodiscard]] core::error invalid_material(std::string_view detail) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "material_asset: {}", detail);
    }

    [[nodiscard]] core::result<f32> read_number(const core::json::value &object, std::string_view key, f32 fallback) {
        const core::json::value member = object[key];
        if (!member.is_valid()) {
            return fallback;
        }
        if (!member.is_number()) {
            return invalid_material(std::string(key) + " must be a number");
        }
        return member.as_f32(fallback);
    }

    [[nodiscard]] core::result<bool> read_bool(const core::json::value &object, std::string_view key, bool fallback) {
        const core::json::value member = object[key];
        if (!member.is_valid()) {
            return fallback;
        }
        if (!member.is_bool()) {
            return invalid_material(std::string(key) + " must be a boolean");
        }
        return member.as_bool(fallback);
    }

    [[nodiscard]] bool parse_alpha_mode(std::string_view text, alpha_mode &out) noexcept {
        if (text == "opaque") {
            out = alpha_mode::SOLID;
            return true;
        }
        if (text == "mask") {
            out = alpha_mode::MASK;
            return true;
        }
        if (text == "blend") {
            out = alpha_mode::BLEND;
            return true;
        }
        return false;
    }

    [[nodiscard]] std::string_view alpha_mode_json(alpha_mode mode) noexcept {
        switch (mode) {
            case alpha_mode::SOLID: {
                return "opaque";
            }
            case alpha_mode::MASK: {
                return "mask";
            }
            case alpha_mode::BLEND: {
                return "blend";
            }
        }
        return "opaque";
    }
}

/*===========================================================================*/
/*===========================================================================*/
// material_desc

bool material_desc::operator==(const material_desc &other) const noexcept {
    for (usize slot = 0; slot < MATERIAL_TEXTURE_SLOTS; ++slot) {
        if (textures[slot].id() != other.textures[slot].id()) {
            return false;
        }
    }
    return baseColourFactor == other.baseColourFactor && emissiveFactor == other.emissiveFactor &&
      emissiveStrength == other.emissiveStrength && metallicFactor == other.metallicFactor &&
      roughnessFactor == other.roughnessFactor && normalScale == other.normalScale &&
      occlusionStrength == other.occlusionStrength && alphaCutoff == other.alphaCutoff
      && alphaMode == other.alphaMode &&
      doubleSided == other.doubleSided && unlit == other.unlit && textureUvSets == other.textureUvSets;
}

/*===========================================================================*/
/*===========================================================================*/
// material_asset

material_asset::material_asset() {}

material_asset::material_asset(material_desc desc, std::string name) :
    mDesc(std::move(desc)),
    mName(std::move(name)) {}

material_asset::~material_asset() {}

core::result<material_asset> material_asset::parse_json(std::string_view text) {
    core::result<core::json::document> parsed = core::json::document::parse(text);
    if (!parsed.has_value()) {
        return core::make_error(core::error_code::PARSE_FAILURE, "material_asset: {}", parsed.get_error().message());
    }
    const core::json::value root = parsed.value().root();
    if (!root.is_object()) {
        return invalid_material("the root must be an object");
    }

    material_asset result = {};
    material_desc &desc = result.mDesc;
    if (const core::json::value name = root["name"]; name.is_valid()) {
        if (!name.is_string()) {
            return invalid_material("name must be a string");
        }
        result.mName = std::string(name.as_string());
    }
    if (const core::json::value factor = root["baseColourFactor"]; factor.is_valid()) {
        core::result<math::vec4> value = factor.to<math::vec4>();
        if (!value.has_value()) {
            return invalid_material("baseColourFactor must be four numbers");
        }
        desc.baseColourFactor = value.value();
    }
    if (const core::json::value factor = root["emissiveFactor"]; factor.is_valid()) {
        core::result<math::vec3> value = factor.to<math::vec3>();
        if (!value.has_value()) {
            return invalid_material("emissiveFactor must be three numbers");
        }
        desc.emissiveFactor = value.value();
    }
    CONJURE_TRY_ASSIGN(desc.emissiveStrength, read_number(root, "emissiveStrength", desc.emissiveStrength));
    CONJURE_TRY_ASSIGN(desc.metallicFactor, read_number(root, "metallicFactor", desc.metallicFactor));
    CONJURE_TRY_ASSIGN(desc.roughnessFactor, read_number(root, "roughnessFactor", desc.roughnessFactor));
    CONJURE_TRY_ASSIGN(desc.normalScale, read_number(root, "normalScale", desc.normalScale));
    CONJURE_TRY_ASSIGN(desc.occlusionStrength, read_number(root, "occlusionStrength", desc.occlusionStrength));
    CONJURE_TRY_ASSIGN(desc.alphaCutoff, read_number(root, "alphaCutoff", desc.alphaCutoff));
    CONJURE_TRY_ASSIGN(desc.doubleSided, read_bool(root, "doubleSided", desc.doubleSided));
    CONJURE_TRY_ASSIGN(desc.unlit, read_bool(root, "unlit", desc.unlit));
    if (const core::json::value mode = root["alphaMode"]; mode.is_valid()) {
        if (!mode.is_string() || !parse_alpha_mode(mode.as_string(), desc.alphaMode)) {
            return invalid_material("alphaMode must be \"opaque\", \"mask\" or \"blend\"");
        }
    }
    if (const core::json::value textures = root["textures"]; textures.is_valid()) {
        if (!textures.is_object()) {
            return invalid_material("textures must be an object");
        }
        for (usize slot = 0; slot < MATERIAL_TEXTURE_SLOTS; ++slot) {
            const core::json::value entry = textures[TEXTURE_KEYS[slot]];
            if (!entry.is_valid() || entry.is_null()) {
                continue;
            }
            if (!entry.is_string()) {
                return invalid_material("texture references must be uuid strings");
            }
            const std::optional<core::uuid> id = core::uuid::parse(entry.as_string());
            if (!id.has_value()) {
                return invalid_material("texture reference is not a uuid");
            }
            desc.textures[slot] = assets::asset_ref<texture_asset>(*id);
        }
    }
    if (const core::json::value uvSets = root["textureUvSets"]; uvSets.is_valid()) {
        if (!uvSets.is_array() || uvSets.size() != MATERIAL_TEXTURE_SLOTS) {
            return invalid_material("textureUvSets must hold one entry per texture slot");
        }
        usize slot = 0;
        for (const core::json::value entry : uvSets) {
            if (!entry.is_integer()) {
                return invalid_material("textureUvSets entries must be integers");
            }
            const i64 set = entry.as_i64(0);
            if (set < 0 || set > 1) {
                return invalid_material("textureUvSets entries must be 0 or 1");
            }
            desc.textureUvSets[slot] = static_cast<u8>(set);
            ++slot;
        }
    }
    return result;
}

core::result<material_asset> material_asset::parse_blob(std::span<const std::byte> bytes) {
    return parse_json(std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
}

std::string material_asset::to_json() const {
    core::json::writer out = {};
    out.begin_object();
    out.key("alphaCutoff");
    out.value(mDesc.alphaCutoff);
    out.key("alphaMode");
    out.value(alpha_mode_json(mDesc.alphaMode));
    out.key("baseColourFactor");
    core::reflect::write_json(mDesc.baseColourFactor, out);
    out.key("doubleSided");
    out.value(mDesc.doubleSided);
    out.key("emissiveFactor");
    core::reflect::write_json(mDesc.emissiveFactor, out);
    out.key("emissiveStrength");
    out.value(mDesc.emissiveStrength);
    out.key("metallicFactor");
    out.value(mDesc.metallicFactor);
    out.key("name");
    out.value(mName);
    out.key("normalScale");
    out.value(mDesc.normalScale);
    out.key("occlusionStrength");
    out.value(mDesc.occlusionStrength);
    out.key("roughnessFactor");
    out.value(mDesc.roughnessFactor);
    out.key("textureUvSets");
    out.begin_array();
    for (const u8 set : mDesc.textureUvSets) {
        out.value(static_cast<u32>(set));
    }
    out.end_array();
    out.key("textures");
    out.begin_object();
    for (usize slot = 0; slot < MATERIAL_TEXTURE_SLOTS; ++slot) {
        out.key(TEXTURE_KEYS[slot]);
        if (mDesc.textures[slot].id().is_null()) {
            out.value(nullptr);
        } else {
            out.value(mDesc.textures[slot].id().to_string());
        }
    }
    out.end_object();
    out.key("unlit");
    out.value(mDesc.unlit);
    out.end_object();
    return out.finish();
}

std::vector<std::byte> material_asset::write_blob() const {
    const std::string text = to_json();
    std::vector<std::byte> bytes(text.size());
    std::transform(text.begin(), text.end(), bytes.begin(), [](char character) {
        return static_cast<std::byte>(character);
    });
    return bytes;
}

material_asset material_asset::fallback() {
    material_desc desc = {};
    desc.baseColourFactor = math::vec4(1.0f, 0.0f, 1.0f, 1.0f);
    desc.metallicFactor = 0.0f;
    desc.roughnessFactor = 1.0f;
    desc.unlit = true;
    return material_asset(desc, "fallback_magenta");
}

u32 material_asset::texture_flags() const noexcept {
    u32 flags = 0;
    for (usize slot = 0; slot < MATERIAL_TEXTURE_SLOTS; ++slot) {
        if (!mDesc.textures[slot].id().is_null()) {
            flags |= 1u << slot;
        }
    }
    return flags;
}

bool material_asset::operator==(const material_asset &other) const noexcept {
    return mName == other.mName && mDesc == other.mDesc;
}

/*===========================================================================*/
/*===========================================================================*/
// material_importer

material_importer::material_importer() {}

material_importer::~material_importer() {}

std::string_view material_importer::id() const noexcept {
    return MATERIAL_IMPORTER_ID;
}

u32 material_importer::version() const noexcept {
    return MATERIAL_IMPORTER_VERSION;
}

std::span<const std::string_view> material_importer::extensions() const noexcept {
    return MATERIAL_EXTENSIONS;
}

core::result<assets::import_output> material_importer::import(assets::import_context &context) const {
    core::result<material_asset> parsed = material_asset::parse_blob(context.source_bytes());
    if (!parsed.has_value()) {
        return assets::make_asset_error(assets::asset_error_code::IMPORT_FAILED, parsed.get_error().message());
    }
    material_asset &material = parsed.value();
    if (material.name().empty()) {
        material.set_name(std::string(core::vfs::path_utils::stem(context.source_path())));
    }

    assets::imported_asset primary = {};
    primary.displayName = std::string(material.name());
    primary.type = assets::asset_type_traits<material_asset>::id();
    primary.payload = material.write_blob();
    for (const assets::asset_ref<texture_asset> &texture : material.desc().textures) {
        if (!texture.id().is_null()) {
            primary.dependencies.push_back(texture.id());
        }
    }
    std::sort(primary.dependencies.begin(), primary.dependencies.end());
    primary.dependencies.erase(
      std::unique(primary.dependencies.begin(), primary.dependencies.end()),
      primary.dependencies.end()
    );
    assets::import_output output = {};
    output.assets.push_back(std::move(primary));
    return output;
}

/*===========================================================================*/
/*===========================================================================*/
// material_loader

material_loader::material_loader() {}

material_loader::~material_loader() {}

core::result<material_asset> material_loader::load_cpu_typed(
  const assets::load_context &,
  std::span<const std::byte> payloadBytes
) const {
    return material_asset::parse_blob(payloadBytes);
}

core::result<material_asset> material_loader::finalise_typed(
  const assets::finalise_context &,
  material_asset &&intermediate
) const {
    return std::move(intermediate);
}

std::optional<material_asset> material_loader::create_fallback_typed() const {
    return material_asset::fallback();
}

/*===========================================================================*/
/*===========================================================================*/
// Names

std::string_view conjure::renderer::to_string(alpha_mode value) noexcept {
    switch (value) {
        case alpha_mode::SOLID: {
            return "SOLID";
        }
        case alpha_mode::MASK: {
            return "MASK";
        }
        case alpha_mode::BLEND: {
            return "BLEND";
        }
    }
    return "SOLID";
}

std::string_view conjure::renderer::to_string(material_texture slot) noexcept {
    switch (slot) {
        case material_texture::BASE_COLOUR: {
            return "BASE_COLOUR";
        }
        case material_texture::METALLIC_ROUGHNESS: {
            return "METALLIC_ROUGHNESS";
        }
        case material_texture::NORMAL: {
            return "NORMAL";
        }
        case material_texture::OCCLUSION: {
            return "OCCLUSION";
        }
        case material_texture::EMISSIVE: {
            return "EMISSIVE";
        }
    }
    return "BASE_COLOUR";
}
