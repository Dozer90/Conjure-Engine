#include "platform/glfw/glfw_context.h"
#include "platform/glfw/glfw_native.h"
#include "platform/glfw/glfw_window.h"
#include "conjure/platform/log_category.h"
#include "conjure/platform/platform_error.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"
#include "conjure/core/platform_detect.h"

#include <GLFW/glfw3.h>

#include <format>
#include <string>

using namespace conjure;
using namespace conjure::platform;
using namespace conjure::platform::glfw;

namespace {
    u32 sReferenceCount = 0;
    int sLastErrorCode = GLFW_NO_ERROR;
    std::string sLastErrorText = {};
    std::string sLastErrorSummary = {};
    glfw_window *sActiveWindow = nullptr;
    u32 sFullscreenExtrasClaims = 0;

    void glfw_error_callback(int code, const char *description) {
        const char *text = description != nullptr ? description : "";
        sLastErrorCode = code;
        sLastErrorText = text;
        sLastErrorSummary = std::format("{} ({:#x}): {}", error_name(code), static_cast<u32>(code), text);
        CONJURE_LOG_ERROR(LOG_PLATFORM, "GLFW {} ({:#x}): {}", error_name(code), static_cast<u32>(code), text);
    }

    void glfw_joystick_callback(int joystickId, int connectionEvent) {
        if (sActiveWindow == nullptr) {
            return;
        }
        sActiveWindow->on_joystick(joystickId, connectionEvent == GLFW_CONNECTED);
    }

    void glfw_monitor_callback(GLFWmonitor *monitor, int connectionEvent) {
        CONJURE_UNUSED(monitor);
        CONJURE_UNUSED(connectionEvent);
        if (sActiveWindow == nullptr) {
            return;
        }
        sActiveWindow->on_monitors_changed();
    }

    [[nodiscard]] int platform_hint_value(display_server_hint displayServer) noexcept {
        // The display server choice only exists on Linux; elsewhere GLFW picks the one platform it has.
#if CONJURE_PLATFORM_LINUX
        switch (displayServer) {
            case display_server_hint::X11:
                return GLFW_PLATFORM_X11;
            case display_server_hint::WAYLAND:
                return GLFW_PLATFORM_WAYLAND;
            case display_server_hint::ANY:
                break;
        }
#else
        CONJURE_UNUSED(displayServer);
#endif
        return GLFW_ANY_PLATFORM;
    }

    [[nodiscard]] std::string_view platform_name_of(int platform) noexcept {
        switch (platform) {
            case GLFW_PLATFORM_WIN32:
                return "Win32";
            case GLFW_PLATFORM_COCOA:
                return "Cocoa";
            case GLFW_PLATFORM_WAYLAND:
                return "Wayland";
            case GLFW_PLATFORM_X11:
                return "X11";
            case GLFW_PLATFORM_NULL:
                return "Null";
            default:
                return "unknown";
        }
    }
}

core::result<void> conjure::platform::glfw::acquire(display_server_hint displayServer) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "GLFW must be initialised on the main thread");

    if (sReferenceCount > 0) {
        ++sReferenceCount;
        return core::result<void>::success();
    }

    glfwSetErrorCallback(&glfw_error_callback);
    clear_last_error();

    glfwInitHint(GLFW_PLATFORM, platform_hint_value(displayServer));
    // GLFW would otherwise change the working directory into the bundle's Resources folder on macOS
    // and break every project-relative path.
    glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);

    if (glfwInit() != GLFW_TRUE) {
        return core::make_error(platform_error::INIT_FAILED, "glfwInit failed: {}", last_error_summary());
    }

    glfwSetJoystickCallback(&glfw_joystick_callback);
    glfwSetMonitorCallback(&glfw_monitor_callback);
    CONJURE_LOG_INFO(
      LOG_PLATFORM,
      "GLFW {} initialised on platform {}",
      glfwGetVersionString(),
      platform_name_of(glfwGetPlatform())
    );

    sReferenceCount = 1;
    return core::result<void>::success();
}

void conjure::platform::glfw::release() noexcept {
    CONJURE_ASSERT(sReferenceCount > 0, "glfw::release without a matching acquire");
    if (sReferenceCount == 0) {
        return;
    }
    --sReferenceCount;
    if (sReferenceCount > 0) {
        return;
    }

    glfwSetJoystickCallback(nullptr);
    glfwSetMonitorCallback(nullptr);
    glfwTerminate();
    CONJURE_LOG_INFO(LOG_PLATFORM, "GLFW terminated");
}

u32 conjure::platform::glfw::reference_count() noexcept {
    return sReferenceCount;
}

void conjure::platform::glfw::clear_last_error() noexcept {
    sLastErrorCode = GLFW_NO_ERROR;
    sLastErrorText.clear();
    sLastErrorSummary.clear();
}

int conjure::platform::glfw::last_error_code() noexcept {
    return sLastErrorCode;
}

std::string_view conjure::platform::glfw::last_error_text() noexcept {
    return sLastErrorText;
}

std::string_view conjure::platform::glfw::last_error_summary() noexcept {
    if (sLastErrorCode == GLFW_NO_ERROR) {
        return "no GLFW error recorded";
    }
    return sLastErrorSummary;
}

std::string_view conjure::platform::glfw::error_name(int code) noexcept {
    switch (code) {
        case GLFW_NO_ERROR:
            return "GLFW_NO_ERROR";
        case GLFW_NOT_INITIALIZED:
            return "GLFW_NOT_INITIALIZED";
        case GLFW_NO_CURRENT_CONTEXT:
            return "GLFW_NO_CURRENT_CONTEXT";
        case GLFW_INVALID_ENUM:
            return "GLFW_INVALID_ENUM";
        case GLFW_INVALID_VALUE:
            return "GLFW_INVALID_VALUE";
        case GLFW_OUT_OF_MEMORY:
            return "GLFW_OUT_OF_MEMORY";
        case GLFW_API_UNAVAILABLE:
            return "GLFW_API_UNAVAILABLE";
        case GLFW_VERSION_UNAVAILABLE:
            return "GLFW_VERSION_UNAVAILABLE";
        case GLFW_PLATFORM_ERROR:
            return "GLFW_PLATFORM_ERROR";
        case GLFW_FORMAT_UNAVAILABLE:
            return "GLFW_FORMAT_UNAVAILABLE";
        case GLFW_NO_WINDOW_CONTEXT:
            return "GLFW_NO_WINDOW_CONTEXT";
        case GLFW_CURSOR_UNAVAILABLE:
            return "GLFW_CURSOR_UNAVAILABLE";
        case GLFW_FEATURE_UNAVAILABLE:
            return "GLFW_FEATURE_UNAVAILABLE";
        case GLFW_FEATURE_UNIMPLEMENTED:
            return "GLFW_FEATURE_UNIMPLEMENTED";
        case GLFW_PLATFORM_UNAVAILABLE:
            return "GLFW_PLATFORM_UNAVAILABLE";
        default:
            return "GLFW_UNKNOWN_ERROR";
    }
}

void conjure::platform::glfw::set_active_window(glfw_window *window) noexcept {
    sActiveWindow = window;
}

glfw_window *conjure::platform::glfw::active_window() noexcept {
    return sActiveWindow;
}

void conjure::platform::glfw::claim_fullscreen_extras() noexcept {
    ++sFullscreenExtrasClaims;
    if (sFullscreenExtrasClaims == 1) {
        set_fullscreen_extras(true);
    }
}

void conjure::platform::glfw::release_fullscreen_extras() noexcept {
    CONJURE_ASSERT(sFullscreenExtrasClaims > 0, "glfw::release_fullscreen_extras without a matching claim");
    if (sFullscreenExtrasClaims == 0) {
        return;
    }
    --sFullscreenExtrasClaims;
    if (sFullscreenExtrasClaims == 0) {
        set_fullscreen_extras(false);
    }
}

u32 conjure::platform::glfw::fullscreen_extras_claims() noexcept {
    return sFullscreenExtrasClaims;
}

std::string_view conjure::platform::glfw::version_string() noexcept {
    return glfwGetVersionString();
}

std::string_view conjure::platform::glfw::platform_name() noexcept {
    return platform_name_of(glfwGetPlatform());
}
