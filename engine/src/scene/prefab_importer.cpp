#include "scene/prefab_importer.h"

#include "scene/archive_tools.h"
#include "scene/scene_json.h"

#include "conjure/scene/prefab.h"
#include "conjure/scene/register.h"
#include "conjure/scene/scene_serializer.h"

#include "conjure/assets/assets_error.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/vfs/path_utils.h"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <utility>

namespace conjure::scene::detail {

namespace {

    constexpr std::array<std::string_view, 1> SCENE_EXTENSIONS = {SCENE_FILE_EXTENSION};
    constexpr std::array<std::string_view, 1> PREFAB_EXTENSIONS = {PREFAB_FILE_EXTENSION};

    [[nodiscard]] std::string_view text_of(std::span<const std::byte> bytes) noexcept {
        return std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    }

    [[nodiscard]] core::error import_failure(const assets::import_context &context, const core::error &inner) {
        return assets::make_asset_error(
          assets::asset_error_code::IMPORT_FAILED,
          std::format("{}: {}", context.source_path(), inner.to_string())
        );
    }

    [[nodiscard]] std::vector<std::byte> bytes_of(const std::string &text) {
        const std::byte *begin = reinterpret_cast<const std::byte *>(text.data());
        return std::vector<std::byte>(begin, begin + text.size());
    }

    [[nodiscard]] assets::import_output finish_import(
      assets::import_context &context,
      const ecs::entity_archive &archive,
      std::string payload,
      assets::asset_type_id type
    ) {
        for (const std::string &unresolved : unresolved_names(archive)) {
            context.add_warning(std::format("unknown component '{}' preserved verbatim", unresolved));
        }
        std::vector<assets::asset_id> dependencies = collect_asset_refs(archive);
        const auto self = std::remove(dependencies.begin(), dependencies.end(), context.primary_id());
        dependencies.erase(self, dependencies.end());

        assets::imported_asset primary = {};
        primary.displayName = std::string(core::vfs::path_utils::stem(context.source_path()));
        primary.type = type;
        primary.dependencies = std::move(dependencies);
        primary.payload = bytes_of(payload);

        assets::import_output output = {};
        output.assets.push_back(std::move(primary));
        return output;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// scene_importer

scene_importer::scene_importer(const ecs::component_registry &registry) :
    mRegistry(&registry) {}

scene_importer::~scene_importer() {}

std::string_view scene_importer::id() const noexcept {
    return SCENE_IMPORTER_ID;
}

u32 scene_importer::version() const noexcept {
    return VERSION;
}

std::span<const std::string_view> scene_importer::extensions() const noexcept {
    return SCENE_EXTENSIONS;
}

core::result<assets::import_output> scene_importer::import(assets::import_context &context) const {
    const core::result<scene_document> parsed = parse_scene_text(text_of(context.source_bytes()), *mRegistry);
    if (!parsed.has_value()) {
        return import_failure(context, parsed.get_error());
    }
    core::json::writer out = {};
    const core::result<void> written = scene_serializer::write_document(parsed.value(), out);
    if (!written.has_value()) {
        return import_failure(context, written.get_error());
    }
    return finish_import(context, *parsed.value().archive, out.finish(), assets::asset_type_traits<scene_asset>::id());
}


/*===========================================================================*/
/*===========================================================================*/
// prefab_importer

prefab_importer::prefab_importer(const ecs::component_registry &registry) :
    mRegistry(&registry) {}

prefab_importer::~prefab_importer() {}

std::string_view prefab_importer::id() const noexcept {
    return PREFAB_IMPORTER_ID;
}

u32 prefab_importer::version() const noexcept {
    return VERSION;
}

std::span<const std::string_view> prefab_importer::extensions() const noexcept {
    return PREFAB_EXTENSIONS;
}

core::result<assets::import_output> prefab_importer::import(assets::import_context &context) const {
    const core::result<prefab_document> parsed = parse_prefab_text(text_of(context.source_bytes()), *mRegistry);
    if (!parsed.has_value()) {
        return import_failure(context, parsed.get_error());
    }
    core::json::writer out = {};
    const core::result<void> written = scene_serializer::write_prefab(parsed.value(), out);
    if (!written.has_value()) {
        return import_failure(context, written.get_error());
    }
    return finish_import(context, *parsed.value().archive, out.finish(), assets::asset_type_traits<prefab_asset>::id());
}
}
