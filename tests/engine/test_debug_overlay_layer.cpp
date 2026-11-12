#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "engine_test_support.h"

#include "conjure/core/log.h"
#include "conjure/core/types.h"
#include "conjure/engine/application.h"
#include "conjure/engine/debug_overlay_layer.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/frame_stats.h"

#include <memory>
#include <string>

// debug_overlay_layer.h (docs/design/06_engine_loop.md 2.8): the frame statistics text and its periodic
// INFO log; compiled outside Distribution only.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

#if !CONJURE_DISTRIBUTION

TEST_SUITE("engine/debug_overlay_layer") {

    TEST_CASE("debug_overlay_layer - formats the latest sample after a frame") {
        log_capture logs = {};
        application app(engine_test::make_desc(engine_test::inline_config()));
        layer *pushed = app.layers().push_overlay(std::make_unique<debug_overlay_layer>());
        debug_overlay_layer *overlay = static_cast<debug_overlay_layer *>(pushed);
        REQUIRE(overlay != nullptr);
        overlay->set_log_interval_frames(0);
        CHECK(overlay->text().empty());
        CONJURE_REQUIRE_OK(app.initialise());
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(overlay->frames_seen() == 2);
        const std::string text(overlay->text());
        CHECK(text.find("frame 2") != std::string::npos);
        CHECK(text.find("fixed") != std::string::npos);
        CHECK(text.find("cpu") != std::string::npos);
        CHECK(text.find("frame memory") != std::string::npos);
        CHECK(logs.count(core::log_level::INFO) == 0);
        app.shutdown();
    }

    TEST_CASE("debug_overlay_layer - logs at the configured interval") {
        log_capture logs = {};
        engine_config config = engine_test::inline_config();
        config.logLevel = core::log_level::INFO;
        application app(engine_test::make_desc(config));
        layer *pushed = app.layers().push_overlay(std::make_unique<debug_overlay_layer>());
        debug_overlay_layer *overlay = static_cast<debug_overlay_layer *>(pushed);
        overlay->set_log_interval_frames(2);
        CHECK(overlay->log_interval_frames() == 2);
        CONJURE_REQUIRE_OK(app.initialise());
        for (u32 frame = 0; frame < 4; ++frame) {
            CONJURE_REQUIRE_OK(app.step_frame());
        }
        CHECK(logs.count(core::log_level::INFO) >= 2);
        CHECK(logs.contains(core::log_level::INFO, "frame 4"));
        app.shutdown();
    }

    TEST_CASE("debug_overlay_layer - cpu time excludes the pacer sleep") {
        frame_stats stats = {};
        frame_sample sample = {};
        sample.frameMilliseconds = 33.3f;
        sample.pacerSleepMilliseconds = 30.0f;
        stats.commit(sample);
        engine_context context = {};
        context.stats = &stats;
        const std::string text = debug_overlay_layer::format_sample(context);
        CHECK(text.find("cpu 3.30 ms") != std::string::npos);
        CHECK(text.find("frame 33.30 ms") != std::string::npos);
        CHECK(text.find("sleep 30.00 ms") != std::string::npos);
    }

    TEST_CASE("debug_overlay_layer - format_sample tolerates an empty context and visibility toggles") {
        const engine_context empty = {};
        CHECK(debug_overlay_layer::format_sample(empty).empty());
        debug_overlay_layer overlay = {};
        CHECK(overlay.is_visible());
        overlay.set_visible(false);
        CHECK_FALSE(overlay.is_visible());
        engine_context context = {};
        overlay.on_imgui(context);
        CHECK(overlay.name() == "debug_overlay");
    }
}

#endif
