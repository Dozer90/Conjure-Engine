#pragma once

#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"

#include <array>
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
    /// Longest name a prepared_thread_name keeps on Windows (in UTF-16 units). Debuggers show far
    /// fewer characters, so the bound costs nothing in practice.
    /// </summary>
    inline constexpr usize MAX_WINDOWS_THREAD_NAME_LENGTH = 63;

    /// <summary>
    /// A thread name already converted to what the OS call takes (UTF-16 on Windows, a bounded
    /// byte buffer elsewhere). Preparing it on one thread and applying it on another lets the
    /// named thread stay allocation-free from its first instruction: the job system workers are
    /// named this way so that nothing of their start-up is charged to an allocation gate.
    /// </summary>
    struct prepared_thread_name {
        #if CONJURE_PLATFORM_WINDOWS
            std::array<wchar_t, MAX_WINDOWS_THREAD_NAME_LENGTH + 1> characters = {};   // terminated
        #else
            std::array<char, MAX_POSIX_THREAD_NAME_LENGTH + 1> characters = {};        // terminated
        #endif
    };

    /// <summary>
    /// Converts a UTF-8 name for set_current_thread_name(const prepared_thread_name &). Names longer
    /// than the platform allows are cut on a code point boundary: to MAX_WINDOWS_THREAD_NAME_LENGTH
    /// UTF-16 units on Windows, where invalid UTF-8 sequences become U+FFFD, and to
    /// MAX_POSIX_THREAD_NAME_LENGTH bytes on Linux and macOS, where the kept bytes are copied unchanged
    /// (invalid UTF-8 included).
    /// </summary>
    [[nodiscard]] prepared_thread_name prepare_thread_name(std::string_view name) noexcept;

    /// <summary>
    /// Names the calling thread from a prepared name without allocating. Failures are ignored
    /// because the name is diagnostic only.
    /// </summary>
    void set_current_thread_name(const prepared_thread_name &name) noexcept;

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
