#include "scene/scene_json.h"

#include "conjure/scene/components.h"
#include "conjure/scene/scene_error.h"

#include "conjure/core/json/document.h"

#include <array>
#include <format>
#include <numbers>
#include <utility>

using namespace conjure;
using namespace conjure::scene;

namespace {

    /// <summary>
    /// A component field renamed since files were written: an old file's value under oldName is multiplied
    /// by scale and stored under newName. The names hold no '/' or '~', so they need no JSON Pointer escaping.
    /// </summary>
    struct renamed_field {
        std::string_view component;
        std::string_view oldName;
        std::string_view newName;
        f64 scale = 1.0;
    };

    // Code sweep F628: the slope limit is stored in radians like every other authored angle.
    constexpr std::array<renamed_field, 1> RENAMED_FIELDS = {
        renamed_field{
          "conjure.physics.character_controller",
          "maxSlopeAngleDegrees",
          "maxSlopeAngleRadians",
          std::numbers::pi_v<f64> / 180.0
        },
    };

    [[nodiscard]] bool needs_migration(const core::json::value &component, const renamed_field &field) noexcept {
        return component.is_object() && component.contains(field.oldName) && !component.contains(field.newName);
    }
}

core::result<detail::file_header> conjure::scene::detail::read_header(
  const core::json::value &in,
  std::string_view expectedFormat
) {
    if (!in.is_object()) {
        return core::make_error(scene_error::BAD_FORMAT, "the file root must be an object");
    }
    const core::json::value format = in["format"];
    if (!format.is_string()) {
        return core::make_error(scene_error::BAD_FORMAT, "the header has no \"format\" string");
    }
    if (format.as_string() != expectedFormat) {
        return core::make_error(
          scene_error::BAD_FORMAT,
          "the header format is \"{}\", expected \"{}\"",
          format.as_string(),
          expectedFormat
        );
    }
    const core::json::value version = in["version"];
    if (!version.is_number() || !version.is_integer()) {
        return core::make_error(scene_error::BAD_FORMAT, "the header has no integer \"version\"");
    }
    const u64 declared = version.as_u64(0xFFFFFFFFFFFFFFFFull);
    if (declared > SCENE_FORMAT_VERSION) {
        return core::make_error(
          scene_error::UNSUPPORTED_VERSION,
          "the file is version {}, this build reads up to {}",
          declared,
          SCENE_FORMAT_VERSION
        );
    }
    if (declared == 0) {
        return core::make_error(scene_error::BAD_FORMAT, "the header version must be at least 1");
    }

    file_header header = {};
    header.version = static_cast<u32>(declared);
    const core::json::value name = in["name"];
    if (name.is_valid() && !name.is_string() && !name.is_null()) {
        return core::make_error(scene_error::BAD_FORMAT, "the header \"name\" must be a string");
    }
    header.name = std::string(name.as_string());
    return header;
}

void conjure::scene::detail::write_header(core::json::writer &out, std::string_view format, std::string_view name) {
    out.key("format");
    out.value(format);
    out.key("version");
    out.value(SCENE_FORMAT_VERSION);
    out.key("name");
    out.value(name);
}

std::optional<core::json::document> conjure::scene::detail::migrate_renamed_fields(
  const core::json::value &archive
) {
    std::optional<core::json::document> migrated = std::nullopt;
    usize entityIndex = 0;
    for (const core::json::value entity : archive["entities"]) {
        const core::json::value components = entity["components"];
        for (const renamed_field &field : RENAMED_FIELDS) {
            const core::json::value component = components[field.component];
            if (!needs_migration(component, field)) {
                continue;
            }
            core::json::document updated = core::json::document::from_value(component);
            const core::json::value old = component[field.oldName];
            if (old.is_number()) {
                updated.set(field.newName, old.as_f64() * field.scale);
            } else {
                updated.set(field.newName, old);
            }
            updated.erase(field.oldName);
            if (!migrated.has_value()) {
                migrated = core::json::document::from_value(archive);
            }
            migrated->set_pointer(std::format("/entities/{}/components/{}", entityIndex, field.component), updated);
        }
        ++entityIndex;
    }
    return migrated;
}

std::string conjure::scene::detail::canonical_settings(const core::json::document &settings) {
    if (!settings.root().is_object()) {
        return "{}";
    }
    return settings.to_canonical_string();
}

core::result<scene_document> conjure::scene::detail::parse_scene_text(
  std::string_view text,
  const ecs::component_registry &registry
) {
    CONJURE_TRY_ASSIGN(const core::json::document document, core::json::document::parse(text));
    return scene_serializer::parse_document(document.root(), registry);
}

core::result<prefab_document> conjure::scene::detail::parse_prefab_text(
  std::string_view text,
  const ecs::component_registry &registry
) {
    CONJURE_TRY_ASSIGN(const core::json::document document, core::json::document::parse(text));
    return scene_serializer::parse_prefab(document.root(), registry);
}
