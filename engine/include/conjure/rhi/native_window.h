#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <string_view>

// The operating-system window a swapchain presents to, as opaque handles. The fields mirror
// platform::native_window_handle (docs/design/11_platform.md 2.4) one to one so the engine copies
// window.native_handle() into this struct; the rhi module itself never includes a platform header.
// A handle of kind NONE means headless: the null device ignores it and the D3D12 backend creates a
// composition swapchain that is never attached to a visual, which lets GPU tests exercise the real
// present path without a window.

namespace conjure::rhi {

    enum class native_window_kind : u8 {
        NONE = 0,
        WINDOWS,
        COCOA,
        X11,
        WAYLAND
    };

    struct native_window {
        native_window_kind kind = native_window_kind::NONE;
        void *window = nullptr;         // HWND | NSWindow * | wl_surface *  (X11 uses x11Window)
        void *display = nullptr;        // HINSTANCE | nullptr | Display * | wl_display *
        u64 x11Window = 0;              // X11 Window id (an unsigned long, not a pointer)

        /// <summary>
        /// True when no window was given (kind NONE).
        /// </summary>
        [[nodiscard]] constexpr bool is_headless() const noexcept {
            return kind == native_window_kind::NONE;
        }
    };

    /// <summary>
    /// The enumerator name of a window kind ("WINDOWS", "NONE"); "" for an out-of-range value.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(native_window_kind kind) noexcept;
}
