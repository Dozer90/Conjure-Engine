#include "conjure/assets/file_watcher.h"
#include "conjure/assets/log_category.h"
#include "assets/durations.h"
#include "assets/path_filters.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"
#include "conjure/core/vfs/path_utils.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;

/*===========================================================================*/
/*===========================================================================*/
// file_watcher

file_watcher::file_watcher() {}

file_watcher::~file_watcher() {}

/*===========================================================================*/
/*===========================================================================*/
// polling_file_watcher

polling_file_watcher::polling_file_watcher(
  core::vfs::file_system &fileSystem,
  core::jobs::job_system &jobs,
  f64 intervalSeconds
) :
    mFileSystem(&fileSystem),
    mJobs(&jobs),
    mIntervalSeconds(detail::clamp_duration_seconds(intervalSeconds)),
    mInterval(detail::to_steady_duration(mIntervalSeconds)) {}

polling_file_watcher::~polling_file_watcher() {
    if (mPass.is_valid()) {
        mJobs->wait(mPass);
    }
}

void polling_file_watcher::add_root(std::string_view rootPath) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "polling_file_watcher::add_root is main-thread only");
    const std::string normalized = core::vfs::path_utils::normalize(rootPath);
    if (std::find(mRoots.begin(), mRoots.end(), normalized) != mRoots.end()) {
        return;
    }
    // A pass scheduled by poll() reads mRoots on a worker; it must finish before the list changes.
    if (mPass.is_valid()) {
        mJobs->wait(mPass);
        mPass.reset();
    }
    mRoots.push_back(normalized);
}

core::result<void> polling_file_watcher::snapshot() {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "polling_file_watcher::snapshot is main-thread only");
    if (mPass.is_valid()) {
        mJobs->wait(mPass);
        mPass.reset();
    }
    core::result<core::flat_hash_map<std::string, file_state>> current = stat_roots();
    if (!current.has_value()) {
        return std::move(current).get_error();
    }
    mSnapshot = std::move(current).value();
    mPending.clear();
    mPendingRemoval.clear();
    {
        const std::scoped_lock lock(mMutex);
        mReady.clear();
        mPassResult = {};
        mPassResultReady = false;
    }
    mHasPassed = true;
    mHasBaseline = true;
    mLastPassStart = std::chrono::steady_clock::now();
    return core::status::success();
}

void polling_file_watcher::poll(std::vector<file_change> &outChanges) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "polling_file_watcher::poll is main-thread only");

    // A pass runs on a job; its stat table is applied here, on the main thread, where the
    // snapshot and the debounce tables live.
    if (mPass.is_valid() && mPass.is_complete()) {
        mPass.reset();
    }
    const auto now = std::chrono::steady_clock::now();
    const bool due = !mHasPassed || now - mLastPassStart >= mInterval;
    if (!mPass.is_valid() && due) {
        mLastPassStart = now;
        mHasPassed = true;
        mPass = mJobs->schedule([this]() {
            core::result<core::flat_hash_map<std::string, file_state>> current = stat_roots();

            // A pass runs every interval: a lasting failure (a deleted root) is reported once,
            // again only when its message changes, and its end is reported too.
            if (!current.has_value()) {
                const std::string message = current.get_error().message();
                bool report = false;
                {
                    const std::scoped_lock lock(mMutex);
                    report = message != mLastPassFailure;
                    mLastPassFailure = message;
                }
                if (report) {
                    CONJURE_LOG_WARN(LOG_ASSETS, "file watcher pass failed: {} (repeats are not logged)", message);
                }
                return;
            }
            bool recovered = false;
            {
                const std::scoped_lock lock(mMutex);
                mPassResult = std::move(current).value();
                mPassResultReady = true;
                recovered = !mLastPassFailure.empty();
                mLastPassFailure.clear();
            }
            if (recovered) {
                CONJURE_LOG_INFO(LOG_ASSETS, "file watcher pass succeeded again");
            }
        });
        if (mPass.is_complete()) {
            mPass.reset();   // inline job systems finish before schedule() returns
        }
    }

    core::flat_hash_map<std::string, file_state> result = {};
    bool haveResult = false;
    {
        const std::scoped_lock lock(mMutex);
        if (mPassResultReady) {
            result = std::move(mPassResult);
            mPassResult = {};
            mPassResultReady = false;
            haveResult = true;
        }
    }
    if (haveResult && !mHasBaseline) {
        mSnapshot = std::move(result);   // the first pass is the baseline and emits nothing
        mHasBaseline = true;
    } else if (haveResult) {
        apply_pass(std::move(result));
    }

    const std::scoped_lock lock(mMutex);
    for (file_change &change : mReady) {
        outChanges.push_back(std::move(change));
    }
    mReady.clear();
}

core::result<void> polling_file_watcher::scan_now() {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "polling_file_watcher::scan_now is main-thread only");
    if (mPass.is_valid()) {
        mJobs->wait(mPass);
        mPass.reset();
    }
    core::result<core::flat_hash_map<std::string, file_state>> current = stat_roots();
    if (!current.has_value()) {
        return std::move(current).get_error();
    }
    mHasPassed = true;
    if (!mHasBaseline) {
        mSnapshot = std::move(current).value();
        mHasBaseline = true;
        return core::status::success();
    }
    apply_pass(std::move(current).value());
    return core::status::success();
}

/*===========================================================================*/
/*===========================================================================*/
// Private methods

core::result<core::flat_hash_map<std::string, polling_file_watcher::file_state>>
polling_file_watcher::stat_roots() const {
    core::flat_hash_map<std::string, file_state> current = {};
    for (const std::string &root : mRoots) {
        core::result<void> enumerated = mFileSystem->enumerate(
          root,
          core::vfs::enumerate_flags::RECURSIVE | core::vfs::enumerate_flags::FILES,
          [&current](std::string_view path, const core::vfs::file_stat &stat) {
              // Atomic-write temporaries come and go within a pass; reporting them would turn
              // every .meta write into ADDED + REMOVED noise for the hot reload controller.
              if (!stat.isRegular || detail::is_temporary_path(path)) {
                  return;
              }
              current.insert_or_assign(std::string(path), file_state{stat.size, stat.writeTimeNanoseconds});
          }
        );
        if (!enumerated.has_value()) {
            return make_asset_error(
              asset_error_code::IO_ERROR,
              std::format("cannot enumerate watched root '{}': {}", root, enumerated.get_error().message())
            );
        }
    }
    return current;
}

void polling_file_watcher::apply_pass(core::flat_hash_map<std::string, file_state> &&current) {
    std::vector<file_change> emitted = {};

    for (const auto &[path, state] : current) {
        const auto known = mSnapshot.find(path);
        const bool isNew = known == mSnapshot.end();
        if (!isNew && known->second == state) {
            mPending.erase(path);   // back to the reported state: nothing to say
            continue;
        }
        // Changed against the snapshot: report it once the state has survived one whole pass.
        const auto pending = mPending.find(path);
        if (pending != mPending.end() && pending->second == state) {
            emitted.push_back(file_change{path, isNew ? file_change_kind::ADDED : file_change_kind::MODIFIED});
            mSnapshot.insert_or_assign(path, state);
            mPending.erase(path);
            continue;
        }
        mPending.insert_or_assign(path, state);
    }

    std::vector<std::string> removed = {};
    for (const auto &[path, state] : mSnapshot) {
        if (current.contains(path)) {
            mPendingRemoval.erase(path);
            continue;
        }
        if (mPendingRemoval.contains(path)) {
            emitted.push_back(file_change{path, file_change_kind::REMOVED});
            removed.push_back(path);
            continue;
        }
        mPendingRemoval.insert_or_assign(path, true);
    }
    for (const std::string &path : removed) {
        mSnapshot.erase(path);
        mPendingRemoval.erase(path);
        mPending.erase(path);
    }

    // Files that vanished while pending and never were in the snapshot simply drop out.
    std::vector<std::string> stalePending = {};
    for (const auto &[path, state] : mPending) {
        if (!current.contains(path)) {
            stalePending.push_back(path);
        }
    }
    for (const std::string &path : stalePending) {
        mPending.erase(path);
    }

    std::sort(emitted.begin(), emitted.end(), [](const file_change &a, const file_change &b) {
        return a.path < b.path;
    });
    const std::scoped_lock lock(mMutex);
    for (file_change &change : emitted) {
        mReady.push_back(std::move(change));
    }
}
