#pragma once

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <string_view>

// Which implementation sits behind a platform::window (docs/design/11_platform.md P1, 4.1): the
// real GLFW 3.4 window or the virtual null window used by every headless test and server build.

namespace conjure::platform {

    enum class backend_kind : u8 {
        NULL_BACKEND = 0,   // virtual window: no OS calls, events queued by tests
        GLFW = 1,           // real OS window through GLFW 3.4 (CONJURE_PLATFORM_GLFW)
        COUNT
    };

    /// <summary>
    /// The configuration name of a backend: "null" or "glfw"; "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(backend_kind kind) noexcept;

    /// <summary>
    /// Parses a backend name case-insensitively ("null", "glfw"). Any other name fails with
    /// platform_error::UNKNOWN_BACKEND.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<backend_kind> parse_backend_kind(std::string_view name) noexcept;

    /// <summary>
    /// True when the backend was compiled into this library. The null backend always is; the glfw
    /// backend only when CONJURE_PLATFORM_GLFW was ON.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_backend_compiled(backend_kind kind) noexcept;

    /// <summary>
    /// GLFW when compiled, otherwise NULL_BACKEND.
    /// </summary>
    [[nodiscard]] CONJURE_API backend_kind default_backend_kind() noexcept;
}
