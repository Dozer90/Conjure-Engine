#include "conjure/scripting/hot_reload.h"

#include "conjure/scripting/log_category.h"

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
        CONJURE_LOG_WARN(
          LOG_SCRIPTING,
          "hot reload of '{}' failed: {}",
          mAssemblyPath.string(),
          mLastResult.get_error()
        );
    }
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
    const std::filesystem::file_time_type stamp = std::filesystem::last_write_time(mAssemblyPath, probe);
    if (probe) {
        return;     // not built yet or being written; try again next interval
    }
    if (!mHasWatchStamp) {
        mHasWatchStamp = true;
        mWatchStamp = stamp;
        return;
    }
    if (stamp != mWatchStamp) {
        mWatchStamp = stamp;
        request(false);
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
