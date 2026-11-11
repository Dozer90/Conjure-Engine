#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "engine_test_support.h"

#include "conjure/core/error.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/exit_code.h"
#include "conjure/platform/null_backend.h"
#include "conjure/platform/window.h"
#include "conjure/rhi/device.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/renderer.h"
#endif

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// The failure rules of docs/design/06_engine_loop.md 6 and the render-failure cases of 06 10, run headless on
// the null device: a render_frame failure is logged once per distinct message and three consecutive ones quit
// with RUNTIME_FAILURE, a successful frame resets the count, device loss maps to DEVICE_LOST, and a resize
// without a swapchain is harmless.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    [[nodiscard]] usize count_containing(const std::vector<std::string> &messages, std::string_view text) {
        return static_cast<usize>(std::count_if(messages.begin(), messages.end(), [text](const std::string &message) {
            return message.find(text) != std::string::npos;
        }));
    }

    [[nodiscard]] core::error disk_full() {
        return core::error(core::error_code::IO_FAILURE, "disk full");
    }
}

TEST_SUITE("engine/render_failure") {

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    TEST_CASE("render_failure - three consecutive render failures quit with RUNTIME_FAILURE and log once") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        log_capture logs = {};
        logs.allow_errors();
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().renderer != nullptr);

        app.inject_render_failure(disk_full(), 3);
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK_FALSE(app.is_quit_requested());
        CHECK(app.current_exit_code() == exit_code::OK);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.is_quit_requested());
        CHECK(app.current_exit_code() == exit_code::RUNTIME_FAILURE);

        // The renderer kept running (its verdict was overruled after the fact) and the identical message was
        // logged once, followed by the escalation.
        CHECK(app.context().renderer->frame_count() == 3);
        const std::vector<std::string> errors = logs.messages(core::log_level::ERR);
        CHECK(count_containing(errors, "render_frame failed") == 1);
        CHECK(count_containing(errors, "disk full") == 1);
        CHECK(count_containing(errors, "consecutive render failures") == 1);
        app.shutdown();
    }

    TEST_CASE("render_failure - a successful frame resets the consecutive failure count") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        log_capture logs = {};
        logs.allow_errors();
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());

        app.inject_render_failure(disk_full(), 2);
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());    // the injection is spent: this frame renders
        app.inject_render_failure(disk_full(), 2);
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK_FALSE(app.is_quit_requested());
        CHECK(app.current_exit_code() == exit_code::OK);

        // A different message is logged again; the same one is not.
        CONJURE_REQUIRE_OK(app.step_frame());
        app.inject_render_failure(core::error(core::error_code::IO_FAILURE, "other reason"), 1);
        CONJURE_REQUIRE_OK(app.step_frame());
        const std::vector<std::string> errors = logs.messages(core::log_level::ERR);
        CHECK(count_containing(errors, "disk full") == 2);
        CHECK(count_containing(errors, "other reason") == 1);
        CHECK_FALSE(app.is_quit_requested());
        app.shutdown();
    }

    TEST_CASE("render_failure - a lost device quits with DEVICE_LOST after on_device_lost") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        log_capture logs = {};
        logs.allow_errors();
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().device != nullptr);
        REQUIRE(app.context().renderer != nullptr);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK_FALSE(app.context().renderer->is_lost());

        app.context().device->simulate_device_loss();
        CHECK(app.context().device->is_lost());
        CONJURE_REQUIRE_OK(app.step_frame());    // the frame completes; the loop asks run() to stop
        CHECK(app.is_quit_requested());
        CHECK(app.current_exit_code() == exit_code::DEVICE_LOST);
        CHECK(app.context().renderer->is_lost());
        CHECK(logs.contains(core::log_level::ERR, "graphics device lost"));
        CHECK(app.run() == static_cast<int>(exit_code::DEVICE_LOST));
        CHECK(app.state() == application_state::STOPPED);
    }

    TEST_CASE("render_failure - injecting a failure with zero frames disarms the hook") {
        REQUIRE_FALSE(engine_test::shader_directory().empty());
        log_capture logs = {};
        engine_config config = engine_test::inline_config();
        engine_test::enable_null_rendering(config);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        app.inject_render_failure(disk_full(), 5);
        app.inject_render_failure(disk_full(), 0);
        for (u32 frame = 0; frame < 5; ++frame) {
            CONJURE_REQUIRE_OK(app.step_frame());
        }
        CHECK_FALSE(app.is_quit_requested());
        CHECK(logs.count(core::log_level::ERR) == 0);
        app.shutdown();
    }
#endif

    TEST_CASE("render_failure - a framebuffer resize without a swapchain reaches the layers and nothing else") {
        log_capture logs = {};
        std::vector<std::string> log = {};
        engine_config config = engine_test::inline_config();
        application app(engine_test::make_desc(config));
        app.layers().push_layer(std::make_unique<engine_test::probe_layer>("L", log));
        CONJURE_REQUIRE_OK(app.initialise());
        platform::null_backend *window = app.context().window->as_null();
        REQUIRE(window != nullptr);
        CONJURE_REQUIRE_OK(app.step_frame());
        log.clear();

        // Headless: no swapchain, so apply_resize has nothing to resize and must neither fail nor retry.
        window->simulate_resize(800, 600);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.context().window->framebuffer_size() == math::uvec2(800, 600));
        CHECK(engine_test::filter_log(log, "L.event").size() == 1);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK_FALSE(app.is_quit_requested());
        CHECK(logs.count(core::log_level::WARN) == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
        app.shutdown();
    }
}
