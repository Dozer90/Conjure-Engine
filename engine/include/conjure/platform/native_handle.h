#pragma once

#include "conjure/core/types.h"

// Opaque per-OS window handles for the rhi (docs/design/11_platform.md 2.4): the D3D12 swapchain
// needs the HWND, MoltenVK the NSWindow. No OS header is involved; the pointers are what the backend
// got from GLFW's native access functions. kind == NONE on the null backend.

namespace conjure::platform {

    enum class native_handle_kind : u8 {
        NONE = 0,
        WINDOWS,    // not WIN32: CMake defines that macro on every Windows build
        COCOA,
        X11,
        WAYLAND
    };

    struct native_window_handle {
        native_handle_kind kind = native_handle_kind::NONE;
        void *window = nullptr;     // HWND | NSWindow * | wl_surface *   (X11 uses x11Window)
        void *display = nullptr;    // HINSTANCE | nullptr | Display * | wl_display *
        u64 x11Window = 0;          // X11 Window id (an unsigned long, not a pointer)

        [[nodiscard]] bool operator==(const native_window_handle &) const noexcept = default;
    };
}
