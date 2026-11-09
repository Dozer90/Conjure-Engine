#include "conjure/assets/asset_extension_desc.h"

#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

using namespace conjure;
using namespace conjure::assets;

core::result<void> conjure::assets::register_importer_extension(
  importer_registry &importers,
  const importer_extension_desc &desc
) {
    if (desc.create == nullptr) {
        return core::error(core::error_code::INVALID_ARGUMENT, "importer extension has no factory");
    }
    std::unique_ptr<asset_importer> importer = desc.create();
    if (importer == nullptr) {
        return core::error(core::error_code::INVALID_ARGUMENT, "importer extension factory returned nothing");
    }
    const std::string_view declared = desc.id == nullptr ? std::string_view() : std::string_view(desc.id);
    if (importer->id() != declared) {
        return core::error(
          core::error_code::INVALID_ARGUMENT,
          std::format("importer extension declares id '{}' but creates '{}'", declared, importer->id())
        );
    }
    return importers.register_importer(std::move(importer));
}

core::result<void> conjure::assets::register_asset_type_extension(
  asset_type_registry &types,
  const asset_type_extension_desc &desc
) {
    if (desc.createLoader == nullptr) {
        return core::error(core::error_code::INVALID_ARGUMENT, "asset type extension has no loader factory");
    }
    std::unique_ptr<asset_loader> loader = desc.createLoader();
    if (loader == nullptr) {
        return core::error(core::error_code::INVALID_ARGUMENT, "asset type extension factory returned nothing");
    }
    const std::string_view name = desc.name == nullptr ? std::string_view() : std::string_view(desc.name);
    if (loader->type() != make_asset_type_id(name)) {
        return core::error(
          core::error_code::INVALID_ARGUMENT,
          std::format("asset type extension '{}' creates a loader of another type", name)
        );
    }

    asset_type_desc typeDesc = {};
    typeDesc.id = make_asset_type_id(name);
    typeDesc.name = std::string(name);
    typeDesc.version = desc.version;
    typeDesc.flags = desc.flags;
    typeDesc.loader = std::move(loader);
    return types.register_type(std::move(typeDesc));
}
