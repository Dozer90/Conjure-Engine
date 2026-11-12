#pragma once

#include <ctime>

// Local broken-down time for the logger. The C library spells the thread-safe conversion
// differently per operating system, so each spelling lives in its own translation unit
// (local_time_windows.cpp, local_time_posix.cpp) and CMake picks one.

namespace conjure::core::detail {

    /// <summary>
    /// Local broken-down time of 'seconds'; the per-OS backends wrap localtime_s / localtime_r.
    /// Thread-safe.
    /// </summary>
    [[nodiscard]] std::tm local_time_of(std::time_t seconds) noexcept;
}
