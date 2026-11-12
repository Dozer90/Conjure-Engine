#include "conjure/scripting/hot_reload.h"

#include "scripting/scripting_internal.h"

#include "conjure/scripting/log_category.h"
#include "conjure/scripting/scripting_error.h"

#include "conjure/core/log.h"

#include <system_error>
#include <utility>

using namespace conjure;
using namespace conjure::scripting;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

hot_reload_controller::hot_reload_controller(script_runtime &runtime, std::filesystem::path assemblyPath) :
    mRuntime(&runtime),
    mAssemblyPath(std::move(assemblyPath)) {}

hot_reload_controller::~hot_reload_controller() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void hot_reload_controller::request(bool immediate) {
    // Only the watch's own requests are retried (poll_assembly_file re-arms the flag after this call).
    mWatchTriggered = false;
    ++mRequestCount;
    mImmediate = mImmediate || immediate;
    if (mState == reload_state::RELOADING) {
        // A request from inside on_reloaded is honoured by the next tick.
        mState = reload_state::PENDING;
        return;
    }
    if (mState == reload_state::PENDING) {
        return;     // coalesced: the window started with the first request
    }
    mState = reload_state::PENDING;
    mRequestTime = -1.0;    // stamped by the next tick so request() needs no clock
}

void hot_reload_controller::tick(f64 nowSeconds) {
    if (mWatchIntervalSeconds > 0.0) {
        poll_assembly_file(nowSeconds);
    }
    if (mState != reload_state::PENDING) {
        return;
    }
    if (mRequestTime < 0.0) {
        mRequestTime = nowSeconds;
        if (!mImmediate && mDebounceSeconds > 0.0) {
            return;
        }
    }
    if (!mImmediate && nowSeconds - mRequestTime < mDebounceSeconds) {
        return;
    }
    perform_reload();
}

void hot_reload_controller::set_debounce_seconds(f64 seconds) noexcept {
    mDebounceSeconds = seconds < 0.0 ? 0.0 : seconds;
}

void hot_reload_controller::set_watch_interval_seconds(f64 seconds) noexcept {
    mWatchIntervalSeconds = seconds < 0.0 ? 0.0 : seconds;
    mHasWatchStamp = false;
}

void hot_reload_controller::set_assembly_path(std::filesystem::path assemblyPath) {
    mAssemblyPath = std::move(assemblyPath);
    mHasWatchStamp = false;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void hot_reload_controller::perform_reload() {
    mState = reload_state::RELOADING;
    mImmediate = false;
    mLastResult = mRuntime->reload_game_assembly(mAssemblyPath);
    ++mReloadCount;
    if (!mLastResult.has_value()) {
        // A watched build may still be writing the .dll or the .pdb, or hold them locked, when the change is
        // seen: the load is tried again one debounce window later, a few times, before the failure stands.
        const bool retry = mWatchTriggered && mWatchRetriesLeft > 0 &&
          mLastResult.get_error().is(script_error::ASSEMBLY_LOAD_FAILED);
        if (retry) {
            --mWatchRetriesLeft;
            CONJURE_LOG_WARN(
              LOG_SCRIPTING,
              "hot reload of '{}' failed: {}; retrying ({} left)",
              detail::path_to_utf8(mAssemblyPath),
              mLastResult.get_error(),
              mWatchRetriesLeft
            );
            mState = reload_state::PENDING;
            mRequestTime = -1.0;        // restamped by the next tick, which then waits one debounce window
            return;
        }
        CONJURE_LOG_WARN(
          LOG_SCRIPTING,
          "hot reload of '{}' failed: {}",
          detail::path_to_utf8(mAssemblyPath),
          mLastResult.get_error()
        );
    }
    mWatchTriggered = false;
    if (mState == reload_state::RELOADING) {
        mState = reload_state::IDLE;
    }
}

void hot_reload_controller::poll_assembly_file(f64 nowSeconds) {
    if (mHasWatchStamp && nowSeconds - mLastWatchTime < mWatchIntervalSeconds) {
        return;
    }
    mLastWatchTime = nowSeconds;
    std::error_code probe = {};
    std::filesystem::file_time_type stamp = std::filesystem::last_write_time(mAssemblyPath, probe);
    if (probe) {
        return;     // not built yet or being written; try again next interval
    }

    // The load context reads the symbols next to the assembly too, so a build that rewrote only the .pdb, or
    // finished it after the .dll, counts as a change: the newer of the two stamps is watched.
    std::filesystem::path symbolsPath = mAssemblyPath;
    symbolsPath.replace_extension(".pdb");
    std::error_code symbolsProbe = {};
    const std::filesystem::file_time_type symbolsStamp = std::filesystem::last_write_time(symbolsPath, symbolsProbe);
    if (!symbolsProbe && symbolsStamp > stamp) {
        stamp = symbolsStamp;
    }

    if (!mHasWatchStamp) {
        mHasWatchStamp = true;
        mWatchStamp = stamp;
        return;
    }
    if (stamp != mWatchStamp) {
        mWatchStamp = stamp;
        request(false);
        mWatchTriggered = true;
        mWatchRetriesLeft = WATCH_RETRY_LIMIT;
    }
}

std::string_view conjure::scripting::to_string(reload_state state) noexcept {
    switch (state) {
        case reload_state::IDLE:
            return "IDLE";
        case reload_state::PENDING:
            return "PENDING";
        case reload_state::RELOADING:
            return "RELOADING";
    }
    return "IDLE";
}
