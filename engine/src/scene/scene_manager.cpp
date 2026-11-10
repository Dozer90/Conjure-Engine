#include "conjure/scene/scene_manager.h"

#include "scene/archive_tools.h"
#include "scene/scene_json.h"

#include "conjure/scene/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#include <algorithm>
#include <utility>

namespace conjure::scene {

/*===========================================================================*/
/*===========================================================================*/
// layer_table

layer_table::layer_table() {}

layer_table::~layer_table() {}

void layer_table::set_name(u8 index, core::string_id value) noexcept {
    if (index >= LAYER_COUNT) {
        CONJURE_ASSERT(false, "layer_table::set_name: layer {} is outside 0..31", index);
        return;
    }
    mNames[index] = value;
}

core::string_id layer_table::name(u8 index) const noexcept {
    if (index >= LAYER_COUNT) {
        return core::string_id();
    }
    return mNames[index];
}

std::optional<u8> layer_table::find(core::string_id value) const noexcept {
    if (value.is_null()) {
        return std::nullopt;
    }
    for (u32 index = 0; index < LAYER_COUNT; ++index) {
        if (mNames[index] == value) {
            return static_cast<u8>(index);
        }
    }
    return std::nullopt;
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

scene_manager::scene_manager(ecs::world &world, assets::asset_manager &assets) :
    mWorld(&world),
    mAssets(&assets),
    mScene(world, &assets),
    mTransforms(mScene) {}

scene_manager::~scene_manager() {
    mPending.clear();
    mRecords.clear();
}


/*===========================================================================*/
/*===========================================================================*/
// Loading

core::result<scene_id> scene_manager::load(const assets::asset_id &id, load_mode mode) {
    core::result<assets::asset_handle> loaded = mAssets->load_blocking(id);
    if (!loaded.has_value()) {
        return core::make_error(scene_error::LOAD_FAILED, "scene asset {} failed to load: {}", id,
          loaded.get_error().to_string());
    }
    assets::asset_handle handle = std::move(loaded).value();
    const scene_asset *document = handle.get<scene_asset>();
    if (document == nullptr) {
        return core::make_error(scene_error::LOAD_FAILED, "asset {} is not a loaded scene", id);
    }
    std::vector<assets::asset_handle> handles = {};
    handles.push_back(handle);
    return apply_document(*document, mode, assets::asset_load_batch(std::move(handles)), id, std::string());
}

core::result<scene_id> scene_manager::load_file(core::vfs::file_system &fs, std::string_view path, load_mode mode) {
    CONJURE_TRY_ASSIGN(const std::string text, fs.read_text_file(path));
    CONJURE_TRY_ASSIGN(const scene_document document, detail::parse_scene_text(text, mWorld->registry()));
    return apply_document(document, mode, assets::asset_load_batch(), assets::asset_id(), std::string(path));
}

load_request_id scene_manager::request_load(const assets::asset_id &id, load_mode mode) {
    pending_load request = {};
    request.id = load_request_id{mNextRequest};
    ++mNextRequest;
    if (mNextRequest == 0) {
        mNextRequest = 1;
    }
    request.asset = id;
    request.mode = mode;
    request.handle = mAssets->load(id);
    mPending.push_back(std::move(request));
    return mPending.back().id;
}

void scene_manager::request_unload(scene_id id) {
    if (!is_loaded(id)) {
        CONJURE_LOG_DEBUG(LOG_SCENE, "request_unload ignored: scene {}:{} is not loaded", id.index, id.generation);
        return;
    }
    if (std::find(mPendingUnloads.begin(), mPendingUnloads.end(), id) == mPendingUnloads.end()) {
        mPendingUnloads.push_back(id);
    }
}

core::result<void> scene_manager::unload(scene_id id) {
    loaded_record *record = find_record(id);
    if (record == nullptr) {
        return core::make_error(scene_error::SCENE_NOT_LOADED, "scene {}:{} is not loaded", id.index, id.generation);
    }
    std::vector<ecs::entity> roots = {};
    collect_owned_roots(id, roots);
    mOnBeforeUnload.emit(id, std::span<const ecs::entity>(roots));
    for (usize index = roots.size(); index > 0; --index) {
        const ecs::entity root = roots[index - 1];
        if (mScene.contains(root)) {
            const core::result<void> destroyed = mScene.destroy_now(root);
            CONJURE_UNUSED(destroyed);
        }
    }
    record->alive = false;
    record->batch = assets::asset_load_batch();
    mLoadOrder.erase(std::remove(mLoadOrder.begin(), mLoadOrder.end(), id.index), mLoadOrder.end());
    const u32 collected = mAssets->collect();
    CONJURE_UNUSED(collected);
    return core::result<void>::success();
}

void scene_manager::unload_all() {
    const std::vector<u32> order = mLoadOrder;
    for (usize index = order.size(); index > 0; --index) {
        const loaded_record &record = mRecords[order[index - 1]];
        if (record.alive) {
            const core::result<void> unloaded = unload(record.info.id);
            CONJURE_UNUSED(unloaded);
        }
    }
    mScene.clear();
}

void scene_manager::apply_pending_loads() {
    if (!mPendingUnloads.empty()) {
        const std::vector<scene_id> unloads = std::move(mPendingUnloads);
        mPendingUnloads.clear();
        for (const scene_id id : unloads) {
            const core::result<void> unloaded = unload(id);
            CONJURE_UNUSED(unloaded);
        }
    }
    while (!mPending.empty()) {
        pending_load &request = mPending.front();
        const assets::asset_state state = request.handle.state();
        const bool failed = !request.handle.is_bound() || state == assets::asset_state::FAILED;
        if (failed) {
            const core::error *inner = mAssets->last_error(request.asset);
            const core::error failure = core::make_error(scene_error::LOAD_FAILED, "scene asset {} failed to load: {}",
              request.asset, inner == nullptr ? std::string("asset not found") : inner->to_string());
            CONJURE_LOG_ERROR(LOG_SCENE, "{}", failure.message());
            const load_request_id id = request.id;
            mPending.erase(mPending.begin());
            mOnLoadFailed.emit(id, failure);
            continue;
        }
        if (state != assets::asset_state::LOADED) {
            return;
        }
        const scene_asset *document = request.handle.get<scene_asset>();
        if (document == nullptr) {
            const core::error failure =
              core::make_error(scene_error::LOAD_FAILED, "asset {} is not a scene", request.asset);
            CONJURE_LOG_ERROR(LOG_SCENE, "{}", failure.message());
            const load_request_id id = request.id;
            mPending.erase(mPending.begin());
            mOnLoadFailed.emit(id, failure);
            continue;
        }
        std::vector<assets::asset_handle> handles = {};
        handles.push_back(request.handle);
        const assets::asset_id asset = request.asset;
        const load_mode mode = request.mode;
        const load_request_id id = request.id;
        mPending.erase(mPending.begin());
        const core::result<scene_id> applied =
          apply_document(*document, mode, assets::asset_load_batch(std::move(handles)), asset, std::string());
        if (!applied.has_value()) {
            CONJURE_LOG_ERROR(LOG_SCENE, "scene asset {} could not be applied: {}", asset,
              applied.get_error().to_string());
            mOnLoadFailed.emit(id, applied.get_error());
        }
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Queries

bool scene_manager::is_loaded(scene_id id) const noexcept {
    return find_record(id) != nullptr;
}

std::optional<scene_id> scene_manager::find_loaded(const assets::asset_id &asset) const noexcept {
    if (asset.is_null()) {
        return std::nullopt;
    }
    for (const u32 slot : mLoadOrder) {
        const loaded_record &record = mRecords[slot];
        if (record.alive && record.info.asset == asset) {
            return record.info.id;
        }
    }
    return std::nullopt;
}

std::vector<loaded_scene_info> scene_manager::loaded_scenes() const {
    std::vector<loaded_scene_info> result = {};
    for (const u32 slot : mLoadOrder) {
        const loaded_record &record = mRecords[slot];
        if (record.alive) {
            result.push_back(record.info);
            collect_owned_roots(record.info.id, result.back().roots);
        }
    }
    return result;
}

std::optional<loaded_scene_info> scene_manager::info(scene_id id) const {
    const loaded_record *record = find_record(id);
    if (record == nullptr) {
        return std::nullopt;
    }
    // The roots are read from the scene on every call: restore() and reparenting change them.
    loaded_scene_info result = record->info;
    collect_owned_roots(id, result.roots);
    return result;
}

bool scene_manager::is_load_pending(const assets::asset_id &id) const noexcept {
    for (const pending_load &request : mPending) {
        if (request.asset == id) {
            return true;
        }
    }
    return false;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

core::result<scene_id> scene_manager::apply_document(
  const scene_document &document,
  load_mode mode,
  assets::asset_load_batch batch,
  const assets::asset_id &asset,
  std::string path
) {
    if (document.archive == nullptr) {
        return core::error(core::error_code::INVALID_ARGUMENT, "scene document without an archive");
    }
    if (mode == load_mode::REPLACE) {
        // Everything instantiate_archive would reject is checked first, so a bad file leaves the
        // current content in place instead of an empty scene plus an error.
        CONJURE_TRY(detail::check_loadable(*document.archive, mScene.ids()));
        unload_all();
        mScene.set_settings(document.settings);
        mScene.set_name(document.name);
    } else if (!(detail::canonical_settings(document.settings) == detail::canonical_settings(mScene.settings()))) {
        CONJURE_LOG_DEBUG(LOG_SCENE, "additive scene '{}' carries different settings; the active settings are kept",
          document.name);
    }

    const u32 slot = allocate_record();
    loaded_record &record = mRecords[slot];
    record.alive = true;
    record.info = {};
    record.info.id = scene_id{slot, mNextGeneration};
    ++mNextGeneration;
    if (mNextGeneration == 0) {
        mNextGeneration = 2;
    }
    record.info.asset = asset;
    record.info.name = document.name;
    record.info.path = std::move(path);
    record.batch = std::move(batch);
    mLoadOrder.push_back(slot);

    const guid_policy policy = mode == load_mode::REPLACE ? guid_policy::KEEP : guid_policy::REGENERATE_ON_CONFLICT;
    const core::result<instantiate_report> report =
      scene_serializer::load_scene(mScene, document, record.info.id, policy);
    if (!report.has_value()) {
        record.alive = false;
        record.batch = assets::asset_load_batch();
        mLoadOrder.erase(std::remove(mLoadOrder.begin(), mLoadOrder.end(), slot), mLoadOrder.end());
        return report.get_error();
    }
    if (asset.is_null() && !report.value().assetIds.empty()) {
        // A file load owns no scene asset: the referenced assets are kept resident through a batch.
        record.batch = mAssets->load_batch(report.value().assetIds);
    }
    const scene_id id = record.info.id;
    mOnLoaded.emit(id);
    return id;
}

scene_manager::loaded_record *scene_manager::find_record(scene_id id) noexcept {
    if (!id.is_valid() || id.index >= mRecords.size()) {
        return nullptr;
    }
    loaded_record &record = mRecords[id.index];
    if (!record.alive || !(record.info.id == id)) {
        return nullptr;
    }
    return &record;
}

const scene_manager::loaded_record *scene_manager::find_record(scene_id id) const noexcept {
    if (!id.is_valid() || id.index >= mRecords.size()) {
        return nullptr;
    }
    const loaded_record &record = mRecords[id.index];
    if (!record.alive || !(record.info.id == id)) {
        return nullptr;
    }
    return &record;
}

u32 scene_manager::allocate_record() {
    for (u32 slot = 0; slot < mRecords.size(); ++slot) {
        if (!mRecords[slot].alive) {
            return slot;
        }
    }
    mRecords.push_back(loaded_record{});
    return static_cast<u32>(mRecords.size() - 1);
}

void scene_manager::collect_owned_roots(scene_id id, std::vector<ecs::entity> &out) const {
    out.clear();
    for (const root_entry &entry : mScene.roots()) {
        if (entry.owner == id) {
            out.push_back(entry.entity);
        }
    }
}
}
