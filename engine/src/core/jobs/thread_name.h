#pragma once

#include "conjure/core/types.h"

#include <string>
#include <string_view>

// OS thread naming, used by the worker pool and dedicated_thread so threads are identifiable in
// debuggers and profilers. Implemented per platform in thread_name.cpp.

namespace conjure::core::jobs::detail {

    /// <summary>
    /// Longest name Linux keeps for a thread (16 bytes including the terminator).
    /// </summary>
    inline constexpr usize MAX_POSIX_THREAD_NAME_LENGTH = 15;

    /// <summary>
    /// Names the calling thread. Names longer than the platform allows are truncated; failures are
    /// ignored because the name is diagnostic only.
    /// </summary>
    void set_current_thread_name(std::string_view name) noexcept;

    /// <summary>
    /// The calling thread's name as the OS reports it, or an empty string when unavailable.
    /// </summary>
    [[nodiscard]] std::string current_thread_name();
}
