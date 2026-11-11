#include "conjure/testing/doctest.h"

#include "conjure/testing/headless_engine.h"
#include "integration_engine_support.h"

#include "conjure/core/error.h"
#include "conjure/core/types.h"
#include "conjure/engine/application.h"
#include "conjure/engine/exit_code.h"
#include "conjure/renderer/renderer.h"
#include "conjure/rhi/device.h"

// docs/design/06_engine_loop.md 10 test_engine_render_failure: through the headless fixture, three consecutive
// render_frame failures quit the application with RUNTIME_FAILURE and a lost device quits with DEVICE_LOST (06 6).

using namespace conjure;
using namespace conjure::testing;

namespace {

    [[nodiscard]] headless_engine_options failing_renderer_options() {
        headless_engine_options options = integration_test::renderer_options();
        options.allowErrors = true;   // the failures are logged at ERROR by design
        return options;
    }
}

TEST_SUITE("integration/engine_render_failure") {

    TEST_CASE("engine_render_failure - three consecutive render failures quit with RUNTIME_FAILURE") {
        headless_engine engine(failing_renderer_options());
        REQUIRE_MESSAGE(engine.renderer_enabled(), "the renderer needs the shader index conjure_shaders writes");
        engine.step_frames(2);

        engine.application().inject_render_failure(core::error(core::error_code::IO_FAILURE, "injected"), 3);
        engine.step_frames(2);
        CHECK_FALSE(engine.application().is_quit_requested());
        CHECK(engine.application().current_exit_code() == engine::exit_code::OK);
        engine.step_frames(1);
        CHECK(engine.application().is_quit_requested());
        CHECK(engine.application().current_exit_code() == engine::exit_code::RUNTIME_FAILURE);
        CHECK(engine.logs().contains(core::log_level::ERR, "injected"));
        CHECK(engine.logs().contains(core::log_level::ERR, "consecutive render failures"));
        CHECK(engine.frame_index() == 5);
    }

    TEST_CASE("engine_render_failure - a lost device quits with DEVICE_LOST") {
        headless_engine engine(failing_renderer_options());
        REQUIRE_MESSAGE(engine.renderer_enabled(), "the renderer needs the shader index conjure_shaders writes");
        REQUIRE(engine.context().device != nullptr);
        engine.step_frames(1);
        CHECK_FALSE(engine.context().renderer->is_lost());

        engine.context().device->simulate_device_loss();
        CHECK(engine.context().device->is_lost());
        engine.step_frames(1);
        CHECK(engine.application().is_quit_requested());
        CHECK(engine.application().current_exit_code() == engine::exit_code::DEVICE_LOST);
        CHECK(engine.context().renderer->is_lost());
        CHECK(engine.logs().count(core::log_level::ERR) >= 1);
    }
}
