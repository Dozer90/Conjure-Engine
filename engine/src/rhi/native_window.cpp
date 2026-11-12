#include "conjure/rhi/native_window.h"

using namespace conjure;
using namespace conjure::rhi;

std::string_view conjure::rhi::to_string(native_window_kind kind) noexcept {
    switch (kind) {
        case native_window_kind::NONE: {
            return "NONE";
        }
        case native_window_kind::WINDOWS: {
            return "WINDOWS";
        }
        case native_window_kind::COCOA: {
            return "COCOA";
        }
        case native_window_kind::X11: {
            return "X11";
        }
        case native_window_kind::WAYLAND: {
            return "WAYLAND";
        }
    }
    // An out-of-range value must not print as a valid kind.
    return "";
}
