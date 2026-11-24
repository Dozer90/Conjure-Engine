#pragma once

#include "conjure/platform/native_handle.h"

#include "conjure/core/result.h"

struct GLFWwindow;

// The two places the glfw backend touches OS or Vulkan types: glfw_native.cpp is the only file that
// includes glfw3native.h or calls the OS directly (one CONJURE_PLATFORM_* block per OS, CONVENTIONS 3),
// glfw_vulkan.cpp the only one that sees Vulkan handle types.

namespace conjure::platform::glfw {

    /// <summary>
    /// The OS handles of a GLFW window: HWND + HINSTANCE, NSWindow, X11 Window + Display or
    /// wl_surface + wl_display depending on the platform GLFW chose at init.
    /// </summary>
    [[nodiscard]] native_window_handle query_native_handle(GLFWwindow *window) noexcept;

    /// <summary>
    /// glfwCreateWindowSurface for an opaque VkInstance, storing the VkSurfaceKHR in *surface.
    /// VULKAN_UNSUPPORTED when GLFW found no Vulkan loader, SURFACE_CREATE_FAILED with the VkResult
    /// otherwise.
    /// </summary>
    [[nodiscard]] core::result<void> create_surface(GLFWwindow *window, void *instance, void **surface);

    /// <summary>
    /// Turns on or off what the OS needs besides the window itself while a borderless fullscreen window is shown and
    /// not minimised (P12). The effects belong to the process, so only glfw_context's claim count calls it, when the
    /// first window claims them and when the last one releases them (claim_fullscreen_extras).
    /// Windows: the display stays awake (no screen saver, no display sleep), which GLFW asked for only while a window
    /// owned a monitor and was not minimised. macOS: the menu bar and the Dock are hidden while the application is
    /// active, and come back for any other application. X11: nothing; an X11 screen saver may still start. Wayland:
    /// nothing; GLFW's compositor fullscreen inhibits idle through the compositor when it offers the idle-inhibit
    /// protocol. Main thread only; calling it twice with the same value changes nothing.
    /// </summary>
    void set_fullscreen_extras(bool active) noexcept;
}
