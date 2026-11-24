#include "engine/module_consumers/assets_module_consumer.h"

#include "conjure/engine/log_category.h"

#include "conjure/assets/asset_extension_desc.h"
#include "conjure/core/log.h"
#include "conjure/core/vfs/native_file_system.h"
#include "conjure/modules/module_context.h"
#include "conjure/modules/module_error.h"
#include "conjure/modules/module_manifest.h"

#include <filesystem>
#include <format>
#include <string_view>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

namespace {

    [[nodiscard]] std::string_view module_name(const modules::module_registry &registry, modules::module_id id) {
        if (!registry.is_valid(id)) {
            return "<unknown>";
        }
        return registry.manifest(id).name;
    }

    [[nodiscard]] core::error conflict(std::string_view module, std::string_view detail) {
        return core::make_error(
          modules::module_error::REGISTRATION_CONFLICT,
          "assets: module '{}': {}",
          module,
          detail
        );
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

assets_module_consumer::assets_module_consumer(
  assets::asset_type_registry &types,
  assets::importer_registry &importers,
  std::shared_ptr<module_content_file_system> content
) :
    mTypes(&types),
    mImporters(&importers),
    mContent(std::move(content)),
    mName(core::string_id::intern("assets")) {
    if (mContent == nullptr) {
        mContent = std::make_shared<module_content_file_system>();
    }
}

assets_module_consumer::~assets_module_consumer() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::string_id assets_module_consumer::name() const noexcept {
    return mName;
}

core::result<void> assets_module_consumer::validate(
  modules::module_id id,
  const modules::module_registration_set &set,
  const modules::module_registry &registry
) {
    const std::string_view module = module_name(registry, id);
    for (const assets::importer_extension_desc *record : modules::extensions_of<assets::importer_extension_desc>(set)) {
        if (record->id == nullptr || std::string_view(record->id).empty() || record->create == nullptr) {
            return conflict(module, "an importer record lacks its id or factory");
        }
        if (mImporters->find(record->id) != nullptr) {
            return conflict(module, std::format("importer '{}' is already registered", record->id));
        }
    }
    using type_desc = assets::asset_type_extension_desc;
    for (const type_desc *record : modules::extensions_of<type_desc>(set)) {
        if (record->name == nullptr || std::string_view(record->name).empty() || record->createLoader == nullptr) {
            return conflict(module, "an asset type record lacks its name or loader factory");
        }
        if (mTypes->find(std::string_view(record->name)) != nullptr) {
            return conflict(module, std::format("asset type '{}' is already registered", record->name));
        }
    }
    return core::result<void>::success();
}

void assets_module_consumer::attach(
  modules::module_id id,
  modules::module_context &context,
  const modules::module_registration_set &set
) {
    module_record record = {};
    record.id = id;
    record.name = context.manifest().name;

    for (const assets::importer_extension_desc *desc : modules::extensions_of<assets::importer_extension_desc>(set)) {
        const core::result<void> registered = assets::register_importer_extension(*mImporters, *desc);
        if (!registered.has_value()) {
            CONJURE_LOG_ERROR(
              LOG_ENGINE,
              "assets module consumer: importer '{}' could not be registered: {}",
              desc->id,
              registered.get_error().to_string()
            );
            continue;
        }
        record.importerIds.emplace_back(desc->id);
    }

    using type_desc = assets::asset_type_extension_desc;
    for (const type_desc *desc : modules::extensions_of<type_desc>(set)) {
        const core::result<void> registered = assets::register_asset_type_extension(*mTypes, *desc);
        if (!registered.has_value()) {
            CONJURE_LOG_ERROR(
              LOG_ENGINE,
              "assets module consumer: asset type '{}' could not be registered: {}",
              desc->name,
              registered.get_error().to_string()
            );
            continue;
        }
        record.types.push_back(assets::make_asset_type_id(std::string_view(desc->name)));
    }

    const modules::module_manifest &manifest = context.manifest();
    if (!manifest.contentDirectories.empty() && !context.root_directory().empty()) {
        if (manifest.contentDirectories.size() > 1) {
            CONJURE_LOG_WARN(
              LOG_ENGINE,
              "module '{}' lists {} content directories; only the first is mounted as modules://{}/",
              record.name,
              manifest.contentDirectories.size(),
              record.name
            );
        }
        const std::filesystem::path root = context.root_directory() / manifest.contentDirectories.front();
        const core::result<void> mounted =
          mContent->add_root(record.name, std::make_shared<core::vfs::native_file_system>(root, true));
        if (mounted.has_value()) {
            record.contentMounted = true;
        } else {
            CONJURE_LOG_ERROR(
              LOG_ENGINE,
              "assets module consumer: content of module '{}' could not be mounted: {}",
              record.name,
              mounted.get_error().to_string()
            );
        }
    }
    mRecords.push_back(std::move(record));
}

core::result<void> assets_module_consumer::can_detach(modules::module_id id, modules::unload_policy policy) {
    CONJURE_UNUSED(id);
    CONJURE_UNUSED(policy);
    return core::result<void>::success();
}

void assets_module_consumer::detach(modules::module_id id, modules::reload_context *reload) {
    CONJURE_UNUSED(reload);
    module_record *record = find_record(id);
    if (record == nullptr) {
        return;
    }
    for (const std::string &importerId : record->importerIds) {
        const core::result<void> removed = mImporters->unregister_importer(importerId);
        if (!removed.has_value()) {
            CONJURE_LOG_ERROR(
              LOG_ENGINE,
              "assets module consumer: importer '{}' could not be unregistered: {}",
              importerId,
              removed.get_error().to_string()
            );
        }
    }
    for (const assets::asset_type_id type : record->types) {
        const core::result<void> removed = mTypes->unregister_type(type);
        if (!removed.has_value()) {
            CONJURE_LOG_ERROR(
              LOG_ENGINE,
              "assets module consumer: an asset type of module '{}' could not be unregistered: {}",
              record->name,
              removed.get_error().to_string()
            );
        }
    }
    if (record->contentMounted) {
        const core::result<void> unmounted = mContent->remove_root(record->name);
        CONJURE_UNUSED(unmounted);
    }
    const usize index = static_cast<usize>(record - mRecords.data());
    mRecords.erase(mRecords.begin() + static_cast<isize>(index));
}

void assets_module_consumer::restore(modules::module_id id, const modules::reload_context &reload) {
    CONJURE_UNUSED(id);
    CONJURE_UNUSED(reload);
}

u32 assets_module_consumer::live_use_count(modules::module_id id) const {
    CONJURE_UNUSED(id);
    return 0;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

assets_module_consumer::module_record *assets_module_consumer::find_record(modules::module_id id) noexcept {
    for (module_record &record : mRecords) {
        if (record.id == id) {
            return &record;
        }
    }
    return nullptr;
}
