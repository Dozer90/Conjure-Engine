#pragma once

#include "conjure/platform/window.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <string_view>

// Process-wide GLFW state (docs/design/11_platform.md P7, 3.3 step 1): GLFW is initialised once by
// the first acquire(), a reference count tracks live glfw windows and glfwTerminate() runs when the
// last one releases. The error callback installed before glfwInit() routes every GLFW error to
// LOG_PLATFORM and keeps the last error so make_error messages can quote it. Main thread only.

namespace conjure::platform::glfw {

    class glfw_window;

    /// <summary>
    /// Takes one reference. The first reference installs the error callback, applies the init hints
    /// (GLFW_PLATFORM from the display server hint on Linux, GLFW_COCOA_CHDIR_RESOURCES off) and
    /// calls glfwInit(); failure is INIT_FAILED with GLFW's error text and takes no reference.
    /// </summary>
    [[nodiscard]] core::result<void> acquire(display_server_hint displayServer);

    /// <summary>
    /// Drops one reference; the last one terminates GLFW. Asserts when nothing was acquired.
    /// </summary>
    void release() noexcept;

    /// <summary>
    /// Live references (glfw windows plus windows being created).
    /// </summary>
    [[nodiscard]] u32 reference_count() noexcept;

    /// <summary>
    /// Forgets the last recorded error so a following call can be checked with last_error_code().
    /// </summary>
    void clear_last_error() noexcept;

    /// <summary>
    /// GLFW error code of the most recent error since clear_last_error(), GLFW_NO_ERROR when none.
    /// </summary>
    [[nodiscard]] int last_error_code() noexcept;

    /// <summary>
    /// Description of the most recent error since clear_last_error(); empty when none.
    /// </summary>
    [[nodiscard]] std::string_view last_error_text() noexcept;

    /// <summary>
    /// "name (0x....): description" of the last error, or "no GLFW error recorded".
    /// </summary>
    [[nodiscard]] std::string_view last_error_summary() noexcept;

    /// <summary>
    /// The enumerator name of a GLFW error code ("GLFW_PLATFORM_ERROR"), "GLFW_UNKNOWN_ERROR" otherwise.
    /// </summary>
    [[nodiscard]] std::string_view error_name(int code) noexcept;

    /// <summary>
    /// The window that receives the process-wide joystick and monitor callbacks (the single live
    /// window in v1); nullptr when none is alive.
    /// </summary>
    void set_active_window(glfw_window *window) noexcept;
    [[nodiscard]] glfw_window *active_window() noexcept;

    /// <summary>
    /// glfwGetVersionString(): compile-time version and platform list; callable before glfwInit().
    /// </summary>
    [[nodiscard]] std::string_view version_string() noexcept;

    /// <summary>
    /// The name of the platform GLFW chose at init ("Win32", "Cocoa", "Wayland", "X11", "Null").
    /// Valid only while a reference is held; glfwGetPlatform() errors before glfwInit().
    /// </summary>
    [[nodiscard]] std::string_view platform_name() noexcept;
}
