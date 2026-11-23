#pragma once

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <string>

// Monitors as window::monitors() reports them (docs/design/11_platform.md 2.4, P12). Fullscreen is
// windowed fullscreen on a monitor's current video mode; video_modes() exists so a settings UI can
// list the alternatives even though v1 never switches modes.

namespace conjure::platform {

    struct video_mode {
        u32 width = 0;              // pixels
        u32 height = 0;
        u32 refreshRateHz = 0;

        [[nodiscard]] bool operator==(const video_mode &) const noexcept = default;
    };

    struct monitor_info {
        std::string name = {};
        bool isPrimary = false;
        math::ivec2 position = math::ivec2(0);      // virtual desktop, logical coordinates
        video_mode currentMode = {};                // pixels
        // The OS UI scale (DPI / 96 on Windows, 2 on a Retina display). window::content_scale() is the
        // framebuffer / window ratio instead; the null backend makes the two equal. A change made while the
        // window is on another monitor can show late (window::monitors()).
        math::vec2 contentScale = math::vec2(1.0f);
        math::urect workArea = {};                  // logical coordinates, excludes task bars

        [[nodiscard]] bool operator==(const monitor_info &) const noexcept = default;
    };
}
