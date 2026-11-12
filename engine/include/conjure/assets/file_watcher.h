#pragma once

#include "conjure/assets/asset_types.h"

#include "conjure/core/api.h"
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/file_system.h"

#include <chrono>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// File change detection for hot reload (docs/design/05_assets.md 3.7, 4.7). The polling watcher
// stats every file under its roots on a job and reports a change once it has stayed stable for
// one whole interval, which debounces files written in several steps.

namespace conjure::assets {

    enum class file_change_kind : u8 {
        ADDED = 0,
        MODIFIED,
        REMOVED
    };

    /// <summary>
    /// One debounced change. The path is the file-system path of the file.
    /// </summary>
    struct file_change {
        std::string path = {};
        file_change_kind kind = file_change_kind::MODIFIED;

        [[nodiscard]] bool operator==(const file_change &) const = default;
    };

    /// <summary>
    /// Produces debounced file changes. Main-thread API.
    /// </summary>
    class CONJURE_API file_watcher {
        public:
            file_watcher();
            virtual ~file_watcher();

            file_watcher(const file_watcher &) = delete;
            file_watcher &operator=(const file_watcher &) = delete;

        public:
            /// <summary>
            /// Main thread. Watches another directory tree; may be called while watching.
            /// </summary>
            virtual void add_root(std::string_view rootPath) = 0;

            /// <summary>
            /// Records the current state as the baseline; emits nothing.
            /// </summary>
            [[nodiscard]] virtual core::result<void> snapshot() = 0;

            /// <summary>
            /// Main thread. Moves the debounced changes since the last call into outChanges.
            /// </summary>
            virtual void poll(std::vector<file_change> &outChanges) = 0;
    };

    /// <summary>
    /// Stat-based watcher. poll() schedules one stat pass per interval on the job system and
    /// collects the pass's results; scan_now() runs a pass synchronously (tests, tools).
    /// Atomic-write temporaries ("*.tmp", "*.tmp-<uuid>") are never tracked or reported.
    /// </summary>
    class CONJURE_API polling_file_watcher final : public file_watcher {
        private:
            struct file_state {
                u64 size = 0;
                u64 writeTime = 0;

                [[nodiscard]] bool operator==(const file_state &) const = default;
            };

            core::vfs::file_system *mFileSystem = nullptr;
            core::jobs::job_system *mJobs = nullptr;
            std::chrono::milliseconds mInterval = DEFAULT_WATCH_INTERVAL;
            std::vector<std::string> mRoots = {};

            core::flat_hash_map<std::string, file_state> mSnapshot = {};   // last reported state
            core::flat_hash_map<std::string, file_state> mPending = {};    // seen once, awaiting confirmation
            core::flat_hash_map<std::string, bool> mPendingRemoval = {};   // missing once, awaiting confirmation

            std::mutex mMutex = {};
            std::vector<file_change> mReady = {};                           // debounced changes, guarded by mMutex
            core::flat_hash_map<std::string, file_state> mPassResult = {};  // the last pass's table, guarded by mMutex
            bool mPassResultReady = false;                                  // guarded by mMutex
            std::string mLastPassFailure = {};                              // last logged failure; guarded by mMutex
            core::jobs::job_handle mPass = {};
            std::chrono::steady_clock::time_point mLastPassStart = {};
            bool mHasPassed = false;                                        // a pass was started at least once
            bool mHasBaseline = false;                                      // mSnapshot holds a baseline

        public:
            polling_file_watcher(
              core::vfs::file_system &fileSystem,
              core::jobs::job_system &jobs,
              std::chrono::milliseconds interval = DEFAULT_WATCH_INTERVAL
            );
            ~polling_file_watcher() override;

        public:
            void add_root(std::string_view rootPath) override;
            [[nodiscard]] core::result<void> snapshot() override;
            void poll(std::vector<file_change> &outChanges) override;

            /// <summary>
            /// One synchronous stat pass on the caller; changes become visible to the next poll().
            /// Fails when a root cannot be enumerated.
            /// </summary>
            [[nodiscard]] core::result<void> scan_now();

            [[nodiscard]] std::chrono::milliseconds interval() const noexcept {
                return mInterval;
            }

            [[nodiscard]] usize tracked_file_count() const noexcept {
                return mSnapshot.size();
            }

        private:
            [[nodiscard]] core::result<core::flat_hash_map<std::string, file_state>> stat_roots() const;
            void apply_pass(core::flat_hash_map<std::string, file_state> &&current);
    };
}
