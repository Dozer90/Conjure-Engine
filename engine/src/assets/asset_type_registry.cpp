#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"

#include <format>
#include <memory>
#include <utility>

using namespace conjure;
using namespace conjure::assets;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

asset_type_registry::asset_type_registry() {
    mTypes.reserve(MAX_ASSET_TYPES);
    mView.reserve(MAX_ASSET_TYPES);
    mIndexById.reserve(MAX_ASSET_TYPES);
}

asset_type_registry::~asset_type_registry() {}

/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> asset_type_registry::register_type(asset_type_desc desc) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_type_registry::register_type is main-thread only");

    if (desc.id == asset_type_id::INVALID) {
        return core::error(core::error_code::INVALID_ARGUMENT, "asset type id is INVALID");
    }
    if (desc.name.empty() || make_asset_type_id(desc.name) != desc.id) {
        return core::error(
          core::error_code::INVALID_ARGUMENT,
          std::format("asset type name '{}' does not hash to the given id", desc.name)
        );
    }
    if (desc.loader != nullptr && desc.loader->type() != desc.id) {
        return core::error(
          core::error_code::INVALID_ARGUMENT,
          std::format("loader of asset type '{}' reports another type id", desc.name)
        );
    }
    if (mIndexById.contains(desc.id)) {
        return make_asset_error(
          asset_error_code::TYPE_DUPLICATE,
          std::format("asset type '{}' is already registered", desc.name)
        );
    }
    if (mTypes.size() >= MAX_ASSET_TYPES) {
        return core::error(
          core::error_code::OUT_OF_RANGE,
          std::format("cannot register '{}': MAX_ASSET_TYPES ({}) reached", desc.name, MAX_ASSET_TYPES)
        );
    }

    // Each description owns its heap block, so pointers handed out by find() survive every later
    // registration and the removal of any other type.
    mTypes.push_back(std::make_unique<asset_type_desc>(std::move(desc)));
    rebuild_index();

    const asset_type_desc &registered = *mTypes.back();
    CONJURE_LOG_DEBUG(LOG_ASSETS, "registered asset type '{}' (version {})", registered.name, registered.version);
    if (mHooks.onRegistered) {
        mHooks.onRegistered(registered);
    }
    return core::status::success();
}

core::result<void> asset_type_registry::unregister_type(asset_type_id id) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "asset_type_registry::unregister_type is main-thread only");

    const auto found = mIndexById.find(id);
    if (found == mIndexById.end()) {
        return make_asset_error(asset_error_code::TYPE_UNKNOWN, "asset type is not registered");
    }
    if (mHooks.isInUse && mHooks.isInUse(id)) {
        return core::error(
          core::error_code::INVALID_STATE,
          std::format("asset type '{}' has loaded or in-flight assets", mTypes[found->second]->name)
        );
    }

    const usize index = found->second;
    mTypes.erase(mTypes.begin() + static_cast<isize>(index));
    rebuild_index();
    if (mHooks.onUnregistered) {
        mHooks.onUnregistered(id);
    }
    return core::status::success();
}

const asset_type_desc *asset_type_registry::find(asset_type_id id) const noexcept {
    const auto found = mIndexById.find(id);
    if (found == mIndexById.end()) {
        return nullptr;
    }
    return mTypes[found->second].get();
}

const asset_type_desc *asset_type_registry::find(std::string_view name) const noexcept {
    return find(make_asset_type_id(name));
}

const asset_loader *asset_type_registry::find_loader(asset_type_id id) const noexcept {
    const asset_type_desc *desc = find(id);
    if (desc == nullptr) {
        return nullptr;
    }
    return desc->loader.get();
}

std::span<const asset_type_desc *const> asset_type_registry::types() const noexcept {
    return mView;
}

void asset_type_registry::set_hooks(asset_type_registry_hooks hooks) {
    mHooks = std::move(hooks);
}

/*===========================================================================*/
/*===========================================================================*/
// Private methods

void asset_type_registry::rebuild_index() {
    mIndexById.clear();
    mView.clear();
    for (usize position = 0; position < mTypes.size(); ++position) {
        mIndexById.try_emplace(mTypes[position]->id, position);
        mView.push_back(mTypes[position].get());
    }
}
