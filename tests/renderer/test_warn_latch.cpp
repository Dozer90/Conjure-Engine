#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "renderer/warn_latch.h"

#include "conjure/renderer/limits.h"

#include "conjure/core/types.h"

#include <array>

// The latches behind the render path's warnings (docs/design/02_renderer.md R19; code sweep F612): a
// warning is due when its condition starts, not again while it lasts, and again after it has been
// seen to end. The renderer's use of them is in test_renderer_failures.cpp (views, lights, unknown
// render targets, sprites, a refused acquire or present, and the camera case of F052),
// test_renderer_null_frame.cpp (an overflow that lasts is not logged again) and, for an acquire a real
// swapchain refuses, test_renderer_gpu.cpp; sprite_batcher's own flag is in test_sprite_batch.cpp.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

TEST_SUITE("renderer/warn_latch") {

    TEST_CASE("warn_latch - a warning is due when the condition starts and again only after it has ended") {
        warn_latch latch = {};
        CHECK_FALSE(latch.active);

        // A condition that never held asks for nothing.
        CHECK_FALSE(latch.update(false));
        CHECK_FALSE(latch.active);

        // It starts: due once, then silent for as long as it lasts.
        CHECK(latch.update(true));
        CHECK(latch.active);
        for (u32 frame = 0; frame < 4; ++frame) {
            CAPTURE(frame);
            CHECK_FALSE(latch.update(true));
            CHECK(latch.active);
        }

        // It ends, which is never a warning, and the next start is due again.
        CHECK_FALSE(latch.update(false));
        CHECK_FALSE(latch.active);
        CHECK_FALSE(latch.update(false));
        CHECK(latch.update(true));
        CHECK_FALSE(latch.update(true));
    }

    TEST_CASE("warn_latch - a condition that alternates is due every time it starts") {
        warn_latch latch = {};
        for (u32 round = 0; round < 3; ++round) {
            CAPTURE(round);
            CHECK(latch.update(true));
            CHECK_FALSE(latch.update(false));
        }
    }

    TEST_CASE("warn_latch - update is usable in a constant expression") {
        constexpr bool dueOnceUntilItEnds = []() {
            warn_latch latch = {};
            const bool first = latch.update(true);
            const bool second = latch.update(true);
            const bool ended = latch.update(false);
            const bool again = latch.update(true);
            return first && !second && !ended && again;
        }();
        static_assert(dueOnceUntilItEnds, "warn_latch reports the start of a condition only");
        CHECK(dueOnceUntilItEnds);
    }

    TEST_CASE("view_warn_latch - every view index has a latch of its own") {
        view_warn_latch latch = {};
        CHECK(latch.activeViews == 0);

        // The first and the last index start; the ones between are untouched.
        CHECK(latch.update(0, true));
        CHECK(latch.update(MAX_VIEWS - 1, true));
        CHECK(latch.activeViews == (1u | (1u << (MAX_VIEWS - 1))));
        CHECK_FALSE(latch.update(0, true));
        CHECK_FALSE(latch.update(MAX_VIEWS - 1, true));
        CHECK_FALSE(latch.update(1, false));
        CHECK(latch.activeViews == (1u | (1u << (MAX_VIEWS - 1))));

        // One index ending re-arms that index only.
        CHECK_FALSE(latch.update(0, false));
        CHECK(latch.activeViews == (1u << (MAX_VIEWS - 1)));
        CHECK(latch.update(0, true));
        CHECK_FALSE(latch.update(MAX_VIEWS - 1, true));

        // An index nobody updates keeps its state, however many others move.
        for (u32 viewIndex = 0; viewIndex + 1 < MAX_VIEWS; ++viewIndex) {
            CAPTURE(viewIndex);
            CHECK_FALSE(latch.update(viewIndex, false));
        }
        CHECK(latch.activeViews == (1u << (MAX_VIEWS - 1)));
    }

    TEST_CASE("view_warn_latch - every index in range is due once when all of them start") {
        view_warn_latch latch = {};
        for (u32 viewIndex = 0; viewIndex < MAX_VIEWS; ++viewIndex) {
            CAPTURE(viewIndex);
            CHECK(latch.update(viewIndex, true));
            CHECK_FALSE(latch.update(viewIndex, true));
        }
        CHECK(latch.activeViews == 0xFFFFFFFFu >> (32u - MAX_VIEWS));
    }

    TEST_CASE("view_warn_latch - an index of MAX_VIEWS or more asserts, changes nothing and is never due") {
        view_warn_latch latch = {};
        REQUIRE(latch.update(2, true));
        const std::array<u32, 3> outOfRange = {MAX_VIEWS, 32u, 0xFFFFFFFFu};
        for (const u32 viewIndex : outOfRange) {
            CAPTURE(viewIndex);
            // The calls run in every configuration: without asserts (Distribution) the early-out is all
            // that stands between such an index and a shift by more than the width of the mask.
            expect_assert asserts = {};
            CHECK_FALSE(latch.update(viewIndex, true));
            CHECK_FALSE(latch.update(viewIndex, false));
            CHECK(asserts.count() == (CONJURE_ASSERTS_ENABLED ? 2u : 0u));
            CHECK(latch.activeViews == (1u << 2));
        }
    }
}
