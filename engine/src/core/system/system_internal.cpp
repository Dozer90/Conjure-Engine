#include "core/system/system_internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
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

    // The operating system takes these as NUL-terminated strings: an embedded NUL would silently cut the
    // text short (a different program, a truncated argument) rather than fail.
    if (desc.executable.find('\0') != std::string::npos) {
        return error(error_code::INVALID_ARGUMENT, "process_desc::executable contains a NUL character");
    }
    for (conjure::usize index = 0; index < desc.arguments.size(); ++index) {
        if (desc.arguments[index].find('\0') != std::string::npos) {
            return make_error(
              error_code::INVALID_ARGUMENT,
              "process_desc::arguments[{}] contains a NUL character",
              index
            );
        }
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

    // An empty name, one containing '=' or a NUL anywhere cannot be represented in an environment block;
    // it would silently corrupt the block or match an unrelated entry. A NUL is reported by index because
    // the text up to it would be misleading.
    for (conjure::usize index = 0; index < desc.environment.size(); ++index) {
        const auto &[name, value] = desc.environment[index];
        if (name.empty()) {
            return error(error_code::INVALID_ARGUMENT, "process_desc::environment contains an empty variable name");
        }
        if (name.find('\0') != std::string::npos || value.find('\0') != std::string::npos) {
            return make_error(
              error_code::INVALID_ARGUMENT,
              "process_desc::environment entry {} contains a NUL character",
              index
            );
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
// Library loading

error_code conjure::core::detail::classify_bare_name_load_failure(
  std::string_view requested,
  std::string_view loaderMessage
) {
    std::string lowered(loaderMessage);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](char character) {
        return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
    });

    // dyld's "Symbol not found" and "Library not loaded" (its reason may still say "no such file"), and
    // glibc's "undefined symbol", are about an export or a dependency: the requested file was found. dyld's
    // "tried:" list gives a reason per search location, so a file found with the wrong architecture ("mach-o
    // file, but is an incompatible architecture", "not a mach-o file") or an invalid "code signature" still
    // exists even when the other locations say "no such file".
    constexpr std::array<std::string_view, 5> FOUND_BUT_BROKEN = {
        "symbol not found",
        "undefined symbol",
        "library not loaded",
        "mach-o file",
        "code signature",
    };
    for (const std::string_view text : FOUND_BUT_BROKEN) {
        if (lowered.find(text) != std::string::npos) {
            return error_code::BACKEND_FAILURE;
        }
    }

    const bool missingText = lowered.find("no such file") != std::string::npos
      || lowered.find("image not found") != std::string::npos;
    if (!missingText) {
        return error_code::BACKEND_FAILURE;
    }

    // glibc names the object it could not open first ("<object>: cannot open shared object file: ..."):
    // anything other than the requested name is a dependency that is missing.
    constexpr std::string_view GLIBC_MARKER = ": cannot open shared object file";
    const conjure::usize marker = loaderMessage.find(GLIBC_MARKER);
    if (marker != std::string_view::npos && loaderMessage.substr(0, marker) != requested) {
        return error_code::BACKEND_FAILURE;
    }
    return error_code::NOT_FOUND;
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
