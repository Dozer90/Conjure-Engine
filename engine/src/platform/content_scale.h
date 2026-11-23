#pragma once

#include "conjure/core/math.h"

#include <optional>

// The rules behind the glfw backend's window::content_scale() and content_scale_changed_event, kept apart
// from GLFW so that tests/platform/test_content_scale.cpp checks them headless on every OS.

namespace conjure::platform::detail {

    /// <summary>
    /// A framebuffer size and the window size it belongs to, both as the backend reports them.
    /// </summary>
    struct scaled_sizes {
        math::uvec2 framebuffer = math::uvec2(0);   // pixels
        math::uvec2 window = math::uvec2(0);        // logical units
    };

    /// <summary>
    /// The content scale of a window with these sizes, as window::content_scale() reports it:
    /// framebuffer / window per axis. When either size has a zero axis there is no ratio (a minimised
    /// window is 0 x 0), so 'previous', the last scale the window had, is returned unchanged.
    /// </summary>
    [[nodiscard]] math::vec2 framebuffer_content_scale(const scaled_sizes &sizes, math::vec2 previous) noexcept;

    /// <summary>
    /// True when the scale last announced, from the sizes 'announced', can no longer have produced 'current'.
    /// GLFW truncates window * scale to whole framebuffer pixels (Cocoa's backing rectangle, the Wayland
    /// fractional scale), so a framebuffer F of a window W stands for every scale in [F / W, (F + 1) / W); the
    /// scale changed when, on some axis, the two pairs' ranges do not overlap. A resize that only truncates a
    /// fractional scale's framebuffer differently is no change at any window size, while a change between 1
    /// and 2 shows even on a 1 pixel window. A scale change too small to move a window's framebuffer by a
    /// pixel is not seen until a size where it does. 'current' with a zero axis is never outdated (a minimised
    /// window keeps its scale); 'announced' with a zero axis stands for the starting scale 1, so then
    /// 'current' is outdated when its framebuffer differs from its window.
    /// </summary>
    [[nodiscard]] bool content_scale_outdated(const scaled_sizes &announced, const scaled_sizes &current) noexcept;

    /// <summary>
    /// The glfw backend's content scale between pumps. Every size callback records the sizes after it, and the
    /// pump announces from the recorded sizes, so a pump makes no size query of its own (on X11 each is a round
    /// trip to the X server). GLFW raises a size callback for every change of either size, a Cocoa backing
    /// scale or a Wayland output scale change included, so the recorded sizes are the live ones at every pump.
    /// </summary>
    class content_scale_tracker {
        public:
            /// <summary>
            /// Starts over from the sizes the window was created with. Their ratio is the starting scale, which
            /// no announcement reports; a window with no size then starts at 1 (see content_scale_outdated).
            /// </summary>
            void start(const scaled_sizes &sizes) noexcept;

            /// <summary>
            /// Records the sizes a size callback left. scale() follows them at once, except that sizes with a
            /// zero axis keep the scale before them (a window minimised before the next pump).
            /// </summary>
            void record(const scaled_sizes &sizes) noexcept;

            /// <summary>
            /// The scale a pump announces, or nothing while content_scale_outdated says the recorded sizes can
            /// still come from the scale last announced (the starting scale before any). An announced scale
            /// becomes the one later calls compare with, so each change is announced once.
            /// </summary>
            [[nodiscard]] std::optional<math::vec2> announce() noexcept;

            /// <summary>
            /// The ratio of the last recorded sizes that had one; the starting scale before any.
            /// </summary>
            [[nodiscard]] math::vec2 scale() const noexcept;

        private:
            math::vec2 mScale = math::vec2(1.0f);
            scaled_sizes mRecorded = {};
            scaled_sizes mAnnounced = {};
    };
}
