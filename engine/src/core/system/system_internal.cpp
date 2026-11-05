#include "core/system/system_internal.h"

#include <algorithm>
#include <cmath>
#include <system_error>

using namespace conjure::core;
using namespace conjure::core::detail;

/*===========================================================================*/
/*===========================================================================*/
// Validation

status conjure::core::detail::validate_process_desc(const process_desc &desc) {
    if (desc.executable.empty()) {
        return error(error_code::INVALID_ARGUMENT, "process_desc::executable is empty");
    }
    if (!std::isfinite(desc.timeoutSeconds)) {
        return error(error_code::INVALID_ARGUMENT, "process_desc::timeoutSeconds must be a finite number");
    }
    if (desc.timeoutSeconds < 0.0) {
        return error(error_code::INVALID_ARGUMENT, "process_desc::timeoutSeconds must not be negative");
    }
    if (desc.mergeStderr && !desc.captureStdout) {
        return error(error_code::INVALID_ARGUMENT, "process_desc::mergeStderr requires captureStdout");
    }

    // An empty name or one containing '=' cannot be represented in an environment block; it
    // would silently corrupt the block or match an unrelated entry.
    for (const auto &[name, value] : desc.environment) {
        if (name.empty()) {
            return error(error_code::INVALID_ARGUMENT, "process_desc::environment contains an empty variable name");
        }
        if (name.find('=') != std::string::npos) {
            return make_error(
              error_code::INVALID_ARGUMENT,
              "process_desc::environment variable name '{}' contains '='",
              name
            );
        }
    }

    if (!desc.workingDirectory.empty()) {
        std::error_code ignored = {};
        if (!std::filesystem::is_directory(desc.workingDirectory, ignored)) {
            return make_error(
              error_code::NOT_FOUND,
              "working directory '{}' does not exist",
              path_to_utf8(desc.workingDirectory)
            );
        }
    }
    return {};
}


/*===========================================================================*/
/*===========================================================================*/
// Timeouts

conjure::f64 conjure::core::detail::clamp_timeout(conjure::f64 timeoutSeconds) noexcept {
    return std::min(timeoutSeconds, process::MAX_TIMEOUT_SECONDS);
}

std::chrono::steady_clock::time_point conjure::core::detail::timeout_deadline(
  std::chrono::steady_clock::time_point start,
  conjure::f64 timeoutSeconds
) noexcept {
    // The clamp keeps the nanosecond count far below the i64 range of steady_clock::duration.
    const std::chrono::duration<f64> seconds(clamp_timeout(timeoutSeconds));
    return start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(seconds);
}


/*===========================================================================*/
/*===========================================================================*/
// process_output

void process_output::append_output(const char *data, conjure::usize size) {
    const std::scoped_lock lock(mMutex);
    mStandardOutput.append(data, size);
}

void process_output::append_error(const char *data, conjure::usize size) {
    const std::scoped_lock lock(mMutex);
    mStandardError.append(data, size);
}

conjure::usize process_output::take_output(std::string &out) {
    const std::scoped_lock lock(mMutex);
    const conjure::usize count = mStandardOutput.size();
    out.append(mStandardOutput);
    mStandardOutput.clear();
    return count;
}

conjure::usize process_output::take_error(std::string &out) {
    const std::scoped_lock lock(mMutex);
    const conjure::usize count = mStandardError.size();
    out.append(mStandardError);
    mStandardError.clear();
    return count;
}


/*===========================================================================*/
/*===========================================================================*/
// completion_latch

void completion_latch::expect_one() {
    const std::scoped_lock lock(mMutex);
    ++mPending;
}

void completion_latch::arrive() {
    {
        const std::scoped_lock lock(mMutex);
        if (mPending > 0) {
            --mPending;
        }
    }
    mCondition.notify_all();
}

bool completion_latch::is_complete() const {
    const std::scoped_lock lock(mMutex);
    return mPending == 0;
}

void completion_latch::wait() {
    std::unique_lock lock(mMutex);
    mCondition.wait(lock, [this] {
        return mPending == 0;
    });
}

bool completion_latch::wait_until(std::chrono::steady_clock::time_point deadline) {
    std::unique_lock lock(mMutex);
    return mCondition.wait_until(lock, deadline, [this] {
        return mPending == 0;
    });
}
