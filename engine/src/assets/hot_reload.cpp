#include "conjure/assets/hot_reload.h"
#include "conjure/assets/asset_meta.h"
#include "conjure/assets/log_category.h"
#include "assets/path_filters.h"

#include "conjure/core/assert.h"
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/path_utils.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;

namespace {
    constexpr std::string_view META_SUFFIX = ".meta";

    bool is_in_flight(asset_state state) noexcept {
        return state == asset_state::QUEUED || state == asset_state::LOADING || state == asset_state::FINALISING ||
          state == asset_state::RELOADING;
    }

    bool has_ok_record(const asset_database &database, const asset_id &id) {
        return database.status_of(id) == std::optional<asset_record_status>(asset_record_status::OK);
    }

    /// <summary>
    /// Design 3.7: a .meta change marks its owner dirty only when the import it describes differs
    /// from the last one (importer or settings), the record is not OK or its blobs are stale. The
    /// metas written by add_file and by an import (sub-asset uuids appended) therefore cause no
    /// second import; an inspector write_meta() that changes the importer or the settings leaves
    /// the owner stale (asset_database tracks an importer switch itself) and re-imports it.
    /// </summary>
    bool meta_needs_import(const asset_database &database, const asset_id &owner) {
        const core::result<asset_meta> meta = database.read_meta(owner);
        if (!meta.has_value()) {
            return true;
        }
        const std::optional<asset_record> record = database.find(owner);
        if (!record.has_value() || record->status != asset_record_status::OK) {
            return true;
        }
        if (meta.value().importerId != record->importerId) {
            return true;
        }
        if (canonical_settings_hash(meta.value().settings.root()) != record->settingsHash) {
            return true;
        }
        const std::vector<asset_id> stale = database.stale_ids();
        return std::find(stale.begin(), stale.end(), owner) != stale.end();
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

hot_reload_controller::hot_reload_controller(
  asset_database &database,
  asset_manager &manager,
  file_watcher &watcher,
  core::jobs::job_system &jobs
) :
    mDatabase(&database),
    mManager(&manager),
    mWatcher(&watcher),
    mJobs(&jobs) {}

hot_reload_controller::~hot_reload_controller() {}

/*===========================================================================*/
/*===========================================================================*/
// Public methods

void hot_reload_controller::update() {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "hot_reload_controller::update is main-thread only");
    mLastReport = hot_reload_report();

    // Assets that changed while a load was in flight reload now that (or if) they are LOADED.
    reload_deferred();

    mChanges.clear();
    mWatcher->poll(mChanges);
    mLastReport.changesSeen = static_cast<u32>(mChanges.size());

    std::vector<asset_id> dirty = {};
    collect_dirty(dirty);

    // IMPORT_FAILED means the import already ran for the current source state; a change to the
    // source, its meta or a side file puts the owner back into dirty through collect_dirty.
    const std::vector<asset_id> cacheMisses = mManager->failed_ids(assets_error::CACHE_MISS);
    for (const asset_id &failed : cacheMisses) {
        if (has_ok_record(*mDatabase, failed)) {
            dirty.push_back(failed);
        }
    }
    std::sort(dirty.begin(), dirty.end());
    dirty.erase(std::unique(dirty.begin(), dirty.end()), dirty.end());
    if (dirty.empty()) {
        return;
    }

    core::result<import_report> imported = mDatabase->import(dirty, *mJobs);
    if (!imported.has_value()) {
        CONJURE_LOG_WARN(LOG_ASSETS, "hot reload import failed: {}", imported.get_error().to_string());
        return;
    }
    mLastReport.sourcesReimported = imported.value().sourcesImported;

    // Changed blobs plus, for the previously missing ones, the failed slots themselves, so the
    // CACHE_MISS repair re-queues them even when the payload hash is unchanged. Only a record that
    // is OK after the import has a blob now; a dropped sub-asset has no record any more.
    std::vector<asset_id> changed = imported.value().changed;
    for (const asset_id &failed : cacheMisses) {
        if (has_ok_record(*mDatabase, failed)) {
            changed.push_back(failed);
        }
    }
    std::sort(changed.begin(), changed.end());
    changed.erase(std::unique(changed.begin(), changed.end()), changed.end());
    if (changed.empty()) {
        return;
    }

    std::vector<asset_id> ordered = {};
    mDatabase->collect_dependents(changed, ordered);
    const asset_type_registry &types = mDatabase->types();
    for (const asset_id &id : ordered) {
        const bool isChanged = std::binary_search(changed.begin(), changed.end(), id);
        const asset_state state = mManager->state_of(id);
        if (state == asset_state::FAILED && isChanged) {
            mManager->retry_failed(id);
            mLastReport.reloaded.push_back(id);
            ++mLastReport.reloads;
            continue;
        }
        if (state != asset_state::LOADED && !is_in_flight(state)) {
            continue;   // UNLOADED or FAILED for another reason: nothing resident to refresh
        }
        bool reload = isChanged;
        if (!reload) {
            const std::optional<asset_type_id> type = mDatabase->type_of(id);
            const asset_type_desc *desc = type.has_value() ? types.find(*type) : nullptr;
            reload = desc != nullptr && has_flag(desc->flags, asset_type_flags::RELOAD_WITH_DEPENDENCIES);
        }
        if (!reload) {
            continue;
        }
        reload_or_defer(id);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Private methods

void hot_reload_controller::collect_dirty(std::vector<asset_id> &outDirty) {
    for (const file_change &change : mChanges) {
        std::string_view path = change.path;
        if (detail::is_temporary_path(path)) {
            continue;   // an atomic write in progress, never a source
        }
        const bool isMeta = core::str::ends_with(path, META_SUFFIX);
        if (isMeta) {
            path.remove_suffix(META_SUFFIX.size());
        }

        const std::optional<asset_id> owner = mDatabase->find_by_file_path(path);
        if (owner.has_value()) {
            if (isMeta && change.kind != file_change_kind::REMOVED && !meta_needs_import(*mDatabase, *owner)) {
                continue;
            }
            if (change.kind == file_change_kind::REMOVED && !isMeta) {
                // Design 3.7: REMOVED -> MISSING_SOURCE. Loaded payloads stay loaded; a fresh load
                // fails NOT_FOUND until the file comes back or the record is deleted.
                core::result<void> marked = mDatabase->mark_source_missing(*owner);
                if (!marked.has_value()) {
                    CONJURE_LOG_WARN(
                      LOG_ASSETS,
                      "hot reload cannot mark '{}' missing: {}",
                      path,
                      marked.get_error().message()
                    );
                }
                continue;
            }
            if (change.kind == file_change_kind::ADDED && !isMeta) {
                // The file is back (moved back or re-created): the record returns to service and
                // is re-imported like any other dirty source.
                core::result<void> restored = mDatabase->mark_source_present(*owner);
                if (!restored.has_value()) {
                    CONJURE_LOG_WARN(
                      LOG_ASSETS,
                      "hot reload cannot restore '{}': {}",
                      path,
                      restored.get_error().message()
                    );
                }
            }
            outDirty.push_back(*owner);
        } else if (change.kind == file_change_kind::ADDED && !isMeta) {
            std::string assetPath = {};
            for (u32 mountIndex = 0; mountIndex < mDatabase->mounts().size(); ++mountIndex) {
                const asset_mount &mount = mDatabase->mounts()[mountIndex];
                const std::string prefix = mount.rootPath.empty() ? std::string() : mount.rootPath + "/";
                if (core::str::starts_with(path, prefix)) {
                    assetPath = make_asset_path(mount.name, path.substr(prefix.size()));
                    break;
                }
            }
            if (!assetPath.empty()) {
                core::result<asset_id> added = mDatabase->add_file(assetPath);
                if (added.has_value()) {
                    outDirty.push_back(added.value());
                } else if (!added.get_error().is_category<assets_error>() ||
                           !added.get_error().is(assets_error::NOT_FOUND)) {
                    CONJURE_LOG_WARN(
                      LOG_ASSETS,
                      "hot reload cannot add '{}': {}",
                      assetPath,
                      added.get_error().message()
                    );
                }
            }
        }

        // A side file belongs to every source that read it.
        for (u32 mountIndex = 0; mountIndex < mDatabase->mounts().size(); ++mountIndex) {
            const asset_mount &mount = mDatabase->mounts()[mountIndex];
            const std::string prefix = mount.rootPath.empty() ? std::string() : mount.rootPath + "/";
            if (!core::str::starts_with(path, prefix)) {
                continue;
            }
            const std::string sideAssetPath = make_asset_path(mount.name, path.substr(prefix.size()));
            for (const asset_id &sideOwner : mDatabase->owners_of_side_file(sideAssetPath)) {
                outDirty.push_back(sideOwner);
            }
        }
    }
}

void hot_reload_controller::reload_or_defer(const asset_id &id) {
    const asset_state state = mManager->state_of(id);
    if (state == asset_state::LOADED) {
        mManager->reload(id);
        mLastReport.reloaded.push_back(id);
        ++mLastReport.reloads;
        return;
    }
    if (!is_in_flight(state)) {
        return;
    }
    // The load in flight may have read the blob before the re-import wrote the new one, so the
    // slot is reloaded once more as soon as it is LOADED (recorded once).
    if (std::find(mDeferred.begin(), mDeferred.end(), id) == mDeferred.end()) {
        mDeferred.push_back(id);
    }
    mLastReport.deferred.push_back(id);
}

void hot_reload_controller::reload_deferred() {
    if (mDeferred.empty()) {
        return;
    }
    std::vector<asset_id> stillPending = {};
    for (const asset_id &id : mDeferred) {
        const asset_state state = mManager->state_of(id);
        if (state == asset_state::LOADED) {
            mManager->reload(id);
            mLastReport.reloaded.push_back(id);
            ++mLastReport.reloads;
        } else if (is_in_flight(state)) {
            stillPending.push_back(id);
        }
        // UNLOADED or FAILED: nothing resident is out of date; the next load reads the new blob.
    }
    mDeferred.swap(stillPending);
}
