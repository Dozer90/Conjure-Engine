#pragma once

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/file_watcher.h"

#include "conjure/core/api.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/types.h"

#include <vector>

// Hot reload (docs/design/05_assets.md 3.7): watcher changes -> dirty sources -> re-import ->
// reload dependents through the normal pipeline in dependency-depth order.

namespace conjure::assets {

    /// <summary>
    /// What the last update() did.
    /// </summary>
    struct hot_reload_report {
        u32 changesSeen = 0;
        u32 sourcesReimported = 0;
        u32 reloads = 0;
        std::vector<asset_id> reloaded = {};   // in the order reload() was called
        std::vector<asset_id> deferred = {};   // changed while in flight; reloaded once LOADED
    };

    /// <summary>
    /// Drives re-import and reload from file changes. Main thread, after asset_manager::update().
    /// </summary>
    class CONJURE_API hot_reload_controller {
        private:
            asset_database *mDatabase = nullptr;
            asset_manager *mManager = nullptr;
            file_watcher *mWatcher = nullptr;
            core::jobs::job_system *mJobs = nullptr;
            std::vector<file_change> mChanges = {};
            std::vector<asset_id> mDeferred = {};   // slots that changed while QUEUED/LOADING/FINALISING/RELOADING
            hot_reload_report mLastReport = {};

        public:
            hot_reload_controller(
              asset_database &database,
              asset_manager &manager,
              file_watcher &watcher,
              core::jobs::job_system &jobs
            );
            ~hot_reload_controller();

            hot_reload_controller(const hot_reload_controller &) = delete;
            hot_reload_controller &operator=(const hot_reload_controller &) = delete;

        public:
            /// <summary>
            /// Polls the watcher, classifies changes (a removed source marks its record
            /// MISSING_SOURCE, a re-added one restores it), re-imports dirty sources (plus slots
            /// FAILED with CACHE_MISS) and reloads changed assets and flagged dependents. A changed
            /// asset whose slot is still in flight is reloaded by a later update(), once it is LOADED.
            /// </summary>
            void update();

            /// <summary>
            /// reload() calls made by the last update().
            /// </summary>
            [[nodiscard]] u32 reloads_this_frame() const noexcept {
                return mLastReport.reloads;
            }

            [[nodiscard]] const hot_reload_report &last_report() const noexcept {
                return mLastReport;
            }

            /// <summary>
            /// Changed assets waiting for their in-flight load to finish before they reload.
            /// </summary>
            [[nodiscard]] usize deferred_count() const noexcept {
                return mDeferred.size();
            }

        private:
            void collect_dirty(std::vector<asset_id> &outDirty);
            void reload_or_defer(const asset_id &id);
            void reload_deferred();
    };
}
