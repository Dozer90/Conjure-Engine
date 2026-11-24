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
    // The typed objc_msgSend prototype (void (void)), which every call casts to the signature it sends.
    #define OBJC_OLD_DISPATCH_PROTOTYPES 0
#elif CONJURE_PLATFORM_LINUX
    #define GLFW_EXPOSE_NATIVE_X11
    #define GLFW_EXPOSE_NATIVE_WAYLAND
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#if CONJURE_PLATFORM_MACOS
    #include <objc/message.h>
    #include <objc/runtime.h>
#endif

using namespace conjure;
using namespace conjure::platform;

#if CONJURE_PLATFORM_MACOS
namespace {
    // NSApplicationPresentationOptions from AppKit's NSApplication.h. AppKit is an Objective-C API and this file is
    // C++, so NSApplication is reached through the Objective-C runtime. Hiding the menu bar requires hiding the Dock
    // too (AppKit raises an exception for the menu bar alone); both apply only while this application is active.
    constexpr unsigned long PRESENTATION_DEFAULT = 0;
    constexpr unsigned long PRESENTATION_HIDE_DOCK = 1UL << 1;
    constexpr unsigned long PRESENTATION_HIDE_MENU_BAR = 1UL << 3;

    void set_presentation_options(unsigned long options) noexcept {
        // objc_getClass returns Class in current SDKs and id in old ones; the send signature follows whichever it is.
        const auto applicationClass = objc_getClass("NSApplication");
        if (applicationClass == nullptr) {
            return;
        }
        using shared_application_send = id (*)(decltype(applicationClass), SEL);
        using set_options_send = void (*)(id, SEL, unsigned long);

        const shared_application_send sendSharedApplication = reinterpret_cast<shared_application_send>(&objc_msgSend);
        const id application = sendSharedApplication(applicationClass, sel_registerName("sharedApplication"));
        if (application == nullptr) {
            return;
        }
        const set_options_send sendSetOptions = reinterpret_cast<set_options_send>(&objc_msgSend);
        sendSetOptions(application, sel_registerName("setPresentationOptions:"), options);
    }
}
#endif

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

void conjure::platform::glfw::set_fullscreen_extras([[maybe_unused]] bool active) noexcept {
#if CONJURE_PLATFORM_WINDOWS
    // GLFW sets this only while a window owns a monitor, and a borderless window owns none. A game played with a
    // gamepad sends no input that resets the idle timer, so without it the display would sleep mid-game.
    const EXECUTION_STATE state = active ? (ES_CONTINUOUS | ES_DISPLAY_REQUIRED) : ES_CONTINUOUS;
    static_cast<void>(SetThreadExecutionState(state));
#elif CONJURE_PLATFORM_MACOS
    // A borderless window sits at the normal window level, below the menu bar and the Dock.
    set_presentation_options(active ? (PRESENTATION_HIDE_MENU_BAR | PRESENTATION_HIDE_DOCK) : PRESENTATION_DEFAULT);
#endif
}
