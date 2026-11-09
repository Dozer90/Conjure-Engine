#include "platform/glfw/glfw_native.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"

// glfw3native.h exposes only the platforms named before it is included; the whole file is one
// CONJURE_PLATFORM_* switch (CONVENTIONS 3). Linux builds carry both X11 and Wayland and ask GLFW
// which one it picked at run time.

#if CONJURE_PLATFORM_WINDOWS
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif CONJURE_PLATFORM_MACOS
    #define GLFW_EXPOSE_NATIVE_COCOA
#elif CONJURE_PLATFORM_LINUX
    #define GLFW_EXPOSE_NATIVE_X11
    #define GLFW_EXPOSE_NATIVE_WAYLAND
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

using namespace conjure;
using namespace conjure::platform;

native_window_handle conjure::platform::glfw::query_native_handle(GLFWwindow *window) noexcept {
    native_window_handle handle = {};
    if (window == nullptr) {
        return handle;
    }

#if CONJURE_PLATFORM_WINDOWS
    handle.kind = native_handle_kind::WINDOWS;
    handle.window = static_cast<void *>(glfwGetWin32Window(window));
    handle.display = static_cast<void *>(GetModuleHandleW(nullptr));
#elif CONJURE_PLATFORM_MACOS
    handle.kind = native_handle_kind::COCOA;
    handle.window = static_cast<void *>(glfwGetCocoaWindow(window));
    handle.display = nullptr;
#elif CONJURE_PLATFORM_LINUX
    const int platform = glfwGetPlatform();
    if (platform == GLFW_PLATFORM_WAYLAND) {
        handle.kind = native_handle_kind::WAYLAND;
        handle.window = static_cast<void *>(glfwGetWaylandWindow(window));
        handle.display = static_cast<void *>(glfwGetWaylandDisplay());
    } else if (platform == GLFW_PLATFORM_X11) {
        handle.kind = native_handle_kind::X11;
        handle.x11Window = static_cast<u64>(glfwGetX11Window(window));
        handle.display = static_cast<void *>(glfwGetX11Display());
    }
#endif

    return handle;
}
