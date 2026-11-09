#pragma once

#include "conjure/platform/native_handle.h"

#include "conjure/core/result.h"

struct GLFWwindow;

// The two places the glfw backend touches OS or Vulkan types: glfw_native.cpp is the only file that
// includes glfw3native.h (one CONJURE_PLATFORM_* block per OS, CONVENTIONS 3), glfw_vulkan.cpp the
// only one that sees Vulkan handle types.

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
}
