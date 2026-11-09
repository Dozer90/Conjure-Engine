#include "conjure/platform/platform_error.h"

using namespace conjure;
using namespace conjure::platform;

std::string_view conjure::platform::platform_error_name(platform_error code) noexcept {
    switch (code) {
        case platform_error::BACKEND_NOT_COMPILED:
            return "BACKEND_NOT_COMPILED";
        case platform_error::UNKNOWN_BACKEND:
            return "UNKNOWN_BACKEND";
        case platform_error::INIT_FAILED:
            return "INIT_FAILED";
        case platform_error::INVALID_DESC:
            return "INVALID_DESC";
        case platform_error::WINDOW_CREATE_FAILED:
            return "WINDOW_CREATE_FAILED";
        case platform_error::MONITOR_NOT_FOUND:
            return "MONITOR_NOT_FOUND";
        case platform_error::VULKAN_UNSUPPORTED:
            return "VULKAN_UNSUPPORTED";
        case platform_error::SURFACE_CREATE_FAILED:
            return "SURFACE_CREATE_FAILED";
        case platform_error::CLIPBOARD_UNAVAILABLE:
            return "CLIPBOARD_UNAVAILABLE";
        case platform_error::FEATURE_UNAVAILABLE:
            return "FEATURE_UNAVAILABLE";
        case platform_error::COUNT:
            break;
    }
    return "";
}
