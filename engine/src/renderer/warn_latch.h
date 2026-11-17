#pragma once

#include "conjure/renderer/limits.h"

#include "conjure/core/assert.h"
#include "conjure/core/types.h"

// When the render path warns (docs/design/02_renderer.md R19; code sweep F612): a warning reports a
// condition when it starts, never again while it lasts, and is armed again once the condition has been
// seen to end. A frame therefore logs nothing about a problem it only inherits, however the size of
// the problem moves from frame to frame; the live figures are in render_stats. The latches below hold
// that rule for the renderer's frame path; sprite_batcher, a public type, keeps the same rule in a
// flag of its own.
//
// A latch only learns what update() tells it: a frame that never looks at the condition (no view was
// prepared, the back buffer was not acquired) leaves the latch as it was.

namespace conjure::renderer {

    /// <summary>
    /// The state of one warning that is logged when its condition starts.
    /// </summary>
    struct warn_latch {
        bool active = false;                // the condition held at the last update()

        /// <summary>
        /// Records whether the condition holds now. True exactly when it holds and did not hold at the
        /// last call (a latch never updated counts as not holding): the caller logs the warning then.
        /// </summary>
        [[nodiscard]] constexpr bool update(bool holds) noexcept {
            const bool started = holds && !active;
            active = holds;
            return started;
        }
    };

    /// <summary>
    /// One warn_latch per view index, for a condition each view of a frame has on its own (an invalid
    /// camera, an unknown render target). The state belongs to the index, not to the view that sat
    /// there: an index the frame does not look at keeps its state.
    /// </summary>
    struct view_warn_latch {
        u32 activeViews = 0;                // bit i: the condition held at the last update() of view index i
        static_assert(MAX_VIEWS <= 32, "view_warn_latch holds one bit per view index");

        /// <summary>
        /// Records whether the condition holds for the view at 'viewIndex'. True exactly when it holds
        /// and did not hold at the last call for that index. An index of MAX_VIEWS or more asserts,
        /// changes nothing and answers false.
        /// </summary>
        [[nodiscard]] bool update(u32 viewIndex, bool holds) noexcept {
            CONJURE_ASSERT(viewIndex < MAX_VIEWS, "view_warn_latch: view index {} is out of range", viewIndex);
            if (viewIndex >= MAX_VIEWS) {
                return false;
            }
            const u32 viewBit = 1u << viewIndex;
            const bool started = holds && (activeViews & viewBit) == 0;
            activeViews = holds ? (activeViews | viewBit) : (activeViews & ~viewBit);
            return started;
        }
    };
}
