#pragma once

#include "conjure/platform/monitor.h"
#include "conjure/platform/window.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <cmath>
#include <span>

// The game's output surface (docs/design/06_engine_loop.md D4): the window framebuffer in pixels and the scales
// that relate it to the window's logical coordinates and to the OS UI scale. One helper defines it, so a later
// editor game-view panel overrides the screen in one place (and must remap the input together with it). The
// scripting subsystem copies it into the per-frame block every frame, after the platform pump.

namespace conjure::engine {

    /// <summary>
    /// The screen a game draws to, as game_screen_of reports it.
    /// </summary>
    struct game_screen {
        math::uvec2 pixels = math::uvec2(0);        // framebuffer size; 0 x 0 while minimised or with no window
        math::vec2 contentScale = math::vec2(1.0f); // framebuffer / logical per axis, window::content_scale()
        f32 uiScale = 1.0f;                         // OS UI scale of the window's monitor (DPI / 96 on Windows)
        bool focused = false;
        bool minimised = false;
        bool fullscreen = false;

        [[nodiscard]] bool operator==(const game_screen &) const = default;
    };

    /// <summary>
    /// The UI scale (contentScale.y) of the first monitor whose bounds contain 'point': the rectangle from
    /// monitor_info::position spanning currentMode's width and height, in the virtual desktop coordinates that
    /// window::position() uses (screen coordinates: pixels, points on macOS). The bounds are used, not the work
    /// area, because a backend clamps a negative work area origin to 0 (a monitor left of or above the primary
    /// would then cover the wrong place) and a point over a task bar still belongs to its monitor. A monitor whose
    /// current mode is 0 x 0 contains nothing. When no monitor contains the point, the primary monitor's scale
    /// (the first monitor's when none is marked primary); 1 when there are no monitors or the scale found is not
    /// a positive finite number.
    /// </summary>
    [[nodiscard]] inline f32 monitor_ui_scale(
      std::span<const platform::monitor_info> monitors,
      math::ivec2 point
    ) noexcept {
        const platform::monitor_info *chosen = nullptr;
        for (const platform::monitor_info &monitor : monitors) {
            const i64 left = static_cast<i64>(monitor.position.x);
            const i64 top = static_cast<i64>(monitor.position.y);
            const i64 right = left + static_cast<i64>(monitor.currentMode.width);
            const i64 bottom = top + static_cast<i64>(monitor.currentMode.height);
            const bool inside = point.x >= left && point.x < right && point.y >= top && point.y < bottom;
            if (inside) {
                chosen = &monitor;
                break;
            }
        }
        if (chosen == nullptr) {
            for (const platform::monitor_info &monitor : monitors) {
                if (monitor.isPrimary) {
                    chosen = &monitor;
                    break;
                }
            }
        }
        if (chosen == nullptr && !monitors.empty()) {
            chosen = &monitors.front();
        }
        if (chosen == nullptr) {
            return 1.0f;
        }
        const f32 scale = chosen->contentScale.y;
        if (!std::isfinite(scale) || !(scale > 0.0f)) {
            return 1.0f;
        }
        return scale;
    }

    /// <summary>
    /// The screen of 'window': its framebuffer in pixels (0 x 0 while minimised), content_scale(), the UI scale
    /// of the monitor under the window centre (position() + window_size() / 2, see monitor_ui_scale) and the
    /// focus, minimised and fullscreen states. A null window gives a default game_screen (0 x 0, scales 1,
    /// every state false). On a backend that cannot learn the window position (glfw on Wayland), position() is
    /// the last one requested, so the UI scale can be another monitor's than the one showing the window. Main
    /// thread only, like every window call.
    /// </summary>
    [[nodiscard]] inline game_screen game_screen_of(const platform::window *window) noexcept {
        game_screen screen = {};
        if (window == nullptr) {
            return screen;
        }
        screen.minimised = window->is_minimised();
        screen.focused = window->is_focused();
        screen.fullscreen = window->is_fullscreen();
        screen.pixels = screen.minimised ? math::uvec2(0) : window->framebuffer_size();
        screen.contentScale = window->content_scale();

        const math::uvec2 logicalSize = window->window_size();
        const math::ivec2 centre = window->position() + math::ivec2(logicalSize / 2u);
        screen.uiScale = monitor_ui_scale(window->monitors(), centre);
        return screen;
    }
}
