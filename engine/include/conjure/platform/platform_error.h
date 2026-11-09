#pragma once

#include "conjure/core/api.h"
#include "conjure/core/error.h"
#include "conjure/core/types.h"

#include <string_view>

// Error codes of the platform module (docs/design/11_platform.md 4.1). Errors are built with
// core::make_error(platform_error::X, "...") and tested with error::is(platform_error::X). Generic
// conditions (a bad gamepad index, an unknown key name) use core::error_code instead.

namespace conjure::platform {

    enum class platform_error : u16 {
        BACKEND_NOT_COMPILED = 1,   // backend_kind requested but CONJURE_PLATFORM_GLFW is OFF
        UNKNOWN_BACKEND,            // parse_backend_kind: name is neither "null" nor "glfw"
        INIT_FAILED,                // glfwInit failed (no display server, missing libraries); message has GLFW text
        INVALID_DESC,               // zero size when not fullscreen, title longer than 255 bytes, second live window
        WINDOW_CREATE_FAILED,       // glfwCreateWindow returned null
        MONITOR_NOT_FOUND,          // monitorIndex >= monitors().size()
        VULKAN_UNSUPPORTED,         // glfwVulkanSupported() false, or any Vulkan call on the null backend
        SURFACE_CREATE_FAILED,      // glfwCreateWindowSurface returned a VkResult != VK_SUCCESS (code in message)
        CLIPBOARD_UNAVAILABLE,      // glfwGetClipboardString returned null (no text / platform error)
        FEATURE_UNAVAILABLE,        // raw mouse motion, cursor_mode::CAPTURED or window positioning unsupported here
        COUNT
    };

    /// <summary>
    /// The enumerator name of a platform error code ("INIT_FAILED"), or "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view platform_error_name(platform_error code) noexcept;
}

CONJURE_DECLARE_ERROR_CATEGORY(conjure::platform::platform_error, "platform", conjure::platform::platform_error_name);
