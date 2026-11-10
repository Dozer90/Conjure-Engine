#include "scene/scene_json.h"

#include "conjure/scene/components.h"
#include "conjure/scene/scene_error.h"

#include "conjure/core/json/document.h"

#include <utility>

using namespace conjure;
using namespace conjure::scene;

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
