#include "conjure/testing/doctest.h"

#include "conjure/testing/headless_engine.h"
#include "integration_engine_support.h"

#include "conjure/core/types.h"
#include "conjure/engine/application.h"

// docs/design/08_testing.md 6.1 test_headless_frames: an empty project runs 600 frames on the null window and
// the null rhi device: 600 frames, 600 fixed updates (one per frame at 1/60), 600 command-list submissions by
// the renderer and not one ERROR (the fixture's log_capture fails the case on one).

using namespace conjure;
using namespace conjure::testing;

TEST_SUITE("integration/headless_frames") {

    TEST_CASE("headless_frames - 600 empty frames give 600 fixed updates and 600 rhi submissions") {
        headless_engine engine(integration_test::renderer_options());
        REQUIRE_MESSAGE(engine.renderer_enabled(), "the renderer needs the shader index conjure_shaders writes");
        CHECK(engine.frame_index() == 0);
        // Renderer creation submits its own set-up work (the BRDF look-up table); the frames are counted from there.
        const u32 submittedAtStart = engine.rhi_stats().commandListsSubmitted;

        engine.step_frames(600);

        CHECK(engine.frame_index() == 600);
        CHECK(engine.fixed_update_count() == 600);
        CHECK(engine.application().time().frameIndex == 600);
        CHECK(engine.rhi_stats().commandListsSubmitted - submittedAtStart == 600);
        CHECK(engine.rhi_stats().commandListsOpened == engine.rhi_stats().commandListsClosed);
        CHECK(engine.world().entity_count() == 0);
        CHECK_FALSE(engine.application().is_quit_requested());
        CHECK(engine.logs().count(core::log_level::ERR) == 0);
    }
}
