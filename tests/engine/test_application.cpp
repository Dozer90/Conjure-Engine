#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "engine_test_support.h"

#include "conjure/core/clock.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/exit_code.h"
#include "engine/executable_path.h"

#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

// application.h (docs/design/06_engine_loop.md 4.2, R13): the state machine, the simulation modes, the
// step variants, quit codes, the main-thread queue and the injected clock.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    /// <summary>
    /// A layer that quits with a code on its first update.
    /// </summary>
    class quitting_layer final : public layer {
        private:
            exit_code mCode = exit_code::OK;

        public:
            explicit quitting_layer(exit_code code) :
                layer("quitter"),
                mCode(code) {}
            ~quitting_layer() override = default;

        public:
            void on_update(engine_context &context) override {
                context.app->request_quit(mCode);
            }
    };
}

TEST_SUITE("engine/application") {

    TEST_CASE("application - state names") {
        CHECK(to_string(application_state::CREATED) == "CREATED");
        CHECK(to_string(application_state::STARTING) == "STARTING");
        CHECK(to_string(application_state::RUNNING) == "RUNNING");
        CHECK(to_string(application_state::STOPPING) == "STOPPING");
        CHECK(to_string(application_state::STOPPED) == "STOPPED");
        CHECK(to_string(simulation_mode::RUNNING) == "RUNNING");
        CHECK(to_string(simulation_mode::PAUSED) == "PAUSED");
        CHECK(to_string(simulation_mode::STEP_ONCE) == "STEP_ONCE");
    }

    TEST_CASE("application - the state machine runs CREATED, RUNNING, STOPPED and starts again") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CHECK(app.state() == application_state::CREATED);
        CHECK(app.subsystems().empty());
        CONJURE_REQUIRE_OK(app.initialise());
        CHECK(app.state() == application_state::RUNNING);
        CHECK_FALSE(app.subsystems().empty());
        CONJURE_CHECK_ERROR(app.initialise(), engine_error::ALREADY_INITIALISED);
        CONJURE_REQUIRE_OK(app.step_frame());
        app.shutdown();
        CHECK(app.state() == application_state::STOPPED);
        CHECK(app.subsystems().empty());
        app.shutdown();
        CHECK(app.state() == application_state::STOPPED);
        CONJURE_REQUIRE_OK(app.initialise());
        CHECK(app.state() == application_state::RUNNING);
        CHECK(app.time().frameIndex == 0);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.time().frameIndex == 1);
        app.shutdown();
    }

    TEST_CASE("application - an invalid configuration is refused before anything starts") {
        engine_config config = engine_test::inline_config();
        config.time.fixedDeltaSeconds = -1.0f;
        application app(engine_test::make_desc(config));
        CONJURE_CHECK_ERROR(app.initialise(), engine_error::INVALID_CONFIG);
        CHECK(app.state() == application_state::CREATED);
        CHECK(app.context().world == nullptr);
    }

    TEST_CASE("application - step_frame before initialise asserts and reports NOT_INITIALISED") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        core::result<void> stepped = core::result<void>::success();
        CONJURE_CHECK_ASSERTS(stepped = app.step_frame());
        CONJURE_CHECK_ERROR(stepped, engine_error::NOT_INITIALISED);
    }

    TEST_CASE("application - run without initialise returns INIT_FAILURE") {
        log_capture logs = {};
        logs.allow_errors();
        application app(engine_test::make_desc(engine_test::inline_config()));
        CHECK(app.run() == static_cast<int>(exit_code::INIT_FAILURE));
    }

    TEST_CASE("application - request_quit keeps the worst code and run returns it") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        app.request_quit(exit_code::OK);
        CHECK(app.is_quit_requested());
        app.request_quit(exit_code::RUNTIME_FAILURE);
        app.request_quit(exit_code::OK);
        CHECK(app.current_exit_code() == exit_code::RUNTIME_FAILURE);
        CHECK(app.run() == static_cast<int>(exit_code::RUNTIME_FAILURE));
        CHECK(app.state() == application_state::STOPPED);
    }

    TEST_CASE("application - a layer may quit with a code from inside a frame") {
        engine_config config = engine_test::inline_config();
        application app(engine_test::make_desc(config));
        app.layers().push_layer(std::make_unique<quitting_layer>(exit_code::DEVICE_LOST));
        CONJURE_REQUIRE_OK(app.initialise());
        CHECK(app.run() == static_cast<int>(exit_code::DEVICE_LOST));
        CHECK(app.time().frameIndex == 1);
    }

    TEST_CASE("application - set_time_scale clamps at zero and reaches engine_time next frame") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        app.set_time_scale(0.5f);
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.time().timeScale == 0.5f);
        CHECK(std::abs(app.time().deltaTime - 0.5f / 60.0f) < 1.0e-6f);
        app.set_time_scale(-3.0f);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.time().timeScale == 0.0f);
        CHECK(app.time().deltaTime == 0.0f);
        CHECK(app.time().fixedStepsThisFrame == 0);
        app.shutdown();
    }

    TEST_CASE("application - set_time_scale treats NaN and infinity as zero") {
        for (const f32 scale : {std::numeric_limits<f32>::quiet_NaN(), std::numeric_limits<f32>::infinity()}) {
            CAPTURE(scale);
            application app(engine_test::make_desc(engine_test::inline_config()));
            CONJURE_REQUIRE_OK(app.initialise());
            CONJURE_REQUIRE_OK(app.step_frame());
            app.set_time_scale(scale);
            CHECK(app.time().timeScale == 0.0f);
            CONJURE_REQUIRE_OK(app.step_frame());
            CHECK(app.time().timeScale == 0.0f);
            CHECK(app.time().deltaTime == 0.0f);
            CHECK(std::isfinite(app.time().time));
            CHECK(std::isfinite(app.time().fixedTime));
            app.shutdown();
        }
    }

    TEST_CASE("application - PAUSED skips the fixed loop, STEP_ONCE runs one frame then pauses") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        CONJURE_REQUIRE_OK(app.step_frame());
        const u64 steps = app.time().fixedStepIndex;
        app.set_simulation_mode(simulation_mode::PAUSED);
        CHECK(app.current_simulation_mode() == simulation_mode::PAUSED);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.time().fixedStepIndex == steps);
        CHECK(app.time().fixedStepsThisFrame == 0);
        app.set_simulation_mode(simulation_mode::STEP_ONCE);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.time().fixedStepIndex == steps + 1);
        CHECK(app.current_simulation_mode() == simulation_mode::PAUSED);
        app.shutdown();
    }

    TEST_CASE("application - step_fixed runs exactly n fixed steps and leaves the mode paused") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        CONJURE_REQUIRE_OK(app.step_frame());
        const u64 steps = app.time().fixedStepIndex;
        const f64 fixedTime = app.time().fixedTime;
        app.step_fixed(3);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.time().fixedStepsThisFrame == 3);
        CHECK(app.time().fixedStepIndex == steps + 3);
        CHECK(std::abs(app.time().deltaTime - 3.0f / 60.0f) < 1.0e-6f);
        CHECK(std::abs(app.time().fixedTime - (fixedTime + 3.0 / 60.0)) < 1.0e-6);
        CHECK(app.time().interpolationAlpha == 0.0f);
        CHECK(app.current_simulation_mode() == simulation_mode::PAUSED);
        // The request is consumed: the next frame stays paused and runs no fixed step.
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.time().fixedStepsThisFrame == 0);
        app.shutdown();
    }

    TEST_CASE("regression - N1 cancel_fixed_steps drops a step_fixed request before the next frame") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        CONJURE_REQUIRE_OK(app.step_frame());
        app.set_simulation_mode(simulation_mode::PAUSED);
        const u64 steps = app.time().fixedStepIndex;
        app.step_fixed(3);
        app.cancel_fixed_steps();
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.time().fixedStepsThisFrame == 0);
        CHECK(app.time().fixedStepIndex == steps);
        CHECK(app.current_simulation_mode() == simulation_mode::PAUSED);

        // Without a request it changes nothing, and a later request still runs in full.
        app.cancel_fixed_steps();
        app.step_fixed(2);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.time().fixedStepsThisFrame == 2);
        CHECK(app.time().fixedStepIndex == steps + 2);
        app.shutdown();
    }

    TEST_CASE("regression - review finding 5: step_fixed at the step cap keeps time and fixedTime together") {
        engine_config config = engine_test::inline_config();
        REQUIRE(config.time.maxFixedStepsPerFrame == 8);
        REQUIRE(config.time.maxFrameDeltaSeconds < 8.0f * config.time.fixedDeltaSeconds);
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        CONJURE_REQUIRE_OK(app.step_frame());
        const f64 time = app.time().time;
        const f64 fixedTime = app.time().fixedTime;

        // Eight steps at 60 Hz are 0.133 s, above the 0.1 s frame clamp: the forced frame keeps the whole delta.
        app.step_fixed(8);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.time().fixedStepsThisFrame == 8);
        CHECK(std::abs(app.time().deltaTime - 8.0f / 60.0f) < 1.0e-6f);
        CHECK(std::abs(app.time().unscaledDeltaTime - 8.0f / 60.0f) < 1.0e-6f);
        CHECK(std::abs((app.time().time - time) - 8.0 / 60.0) < 1.0e-6);
        CHECK(std::abs((app.time().fixedTime - fixedTime) - 8.0 / 60.0) < 1.0e-6);
        CHECK(std::abs(app.time().time - app.time().fixedTime) < 1.0e-6);
        CHECK(app.time().interpolationAlpha == 0.0f);
        app.shutdown();
    }

    TEST_CASE("regression - review finding 15: step_frame with a negative or NaN delta is a zero-length frame") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame(-1.0));
        CHECK(app.time().frameIndex == 2);
        CHECK(app.time().unscaledDeltaTime == 0.0f);
        CHECK(app.time().deltaTime == 0.0f);
        CHECK(app.time().fixedStepsThisFrame == 0);
        CONJURE_REQUIRE_OK(app.step_frame(std::numeric_limits<f64>::quiet_NaN()));
        CHECK(app.time().frameIndex == 3);
        CHECK(app.time().unscaledDeltaTime == 0.0f);
        CHECK(app.time().fixedStepsThisFrame == 0);
        app.shutdown();
    }

    TEST_CASE("application - step_frame with a delta replaces the forced delta and is clamped") {
        engine_config config = engine_test::inline_config();
        config.time.maxFrameDeltaSeconds = 0.1f;
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame(0.055));
        CHECK(std::abs(app.time().unscaledDeltaTime - 0.055f) < 1.0e-6f);
        CHECK(app.time().fixedStepsThisFrame == 3);
        CONJURE_REQUIRE_OK(app.step_frame(5.0));
        CHECK(app.time().unscaledDeltaTime == 0.1f);
        CHECK(app.time().fixedStepsThisFrame >= 5);
        CHECK(app.time().fixedStepsThisFrame <= 6);
        app.shutdown();
    }

    TEST_CASE("application - a manual clock drives the frame delta when no delta is forced") {
        engine_config config = engine_test::inline_config();
        config.time.forcedFrameDeltaSeconds = 0.0f;
        config.pacing.targetHz = 0;
        core::manual_clock clock(100.0);
        application_desc desc = engine_test::make_desc(config);
        desc.clockOverride = &clock;
        application app(std::move(desc));
        CHECK(&app.clock() == &clock);
        CONJURE_REQUIRE_OK(app.initialise());
        CONJURE_REQUIRE_OK(app.step_frame());
        // Headless pacing sleeps the manual clock to the fixed rate at the end of a frame (06 3.3); the next
        // frame measures that sleep plus whatever the test advanced.
        const f64 slept = static_cast<f64>(app.stats().latest().pacerSleepMilliseconds) / 1000.0;
        clock.advance(0.025);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(std::abs(static_cast<f64>(app.time().unscaledDeltaTime) - (0.025 + slept)) < 1.0e-5);
        clock.advance(0.2);
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(app.time().unscaledDeltaTime == config.time.maxFrameDeltaSeconds);
        app.shutdown();
    }

    TEST_CASE("application - tasks posted to the main thread run before FRAME_BEGIN of the next frame") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        std::vector<std::string> log = {};
        ecs::system_desc probe = {};
        probe.name = core::string_id::intern("probe.main_thread_queue");
        probe.phase = ecs::system_phase::FRAME_BEGIN;
        probe.flags = ecs::system_flags::EXCLUSIVE;
        probe.run = [&log](ecs::system_context &) {
            log.emplace_back("frame_begin");
        };
        CONJURE_REQUIRE_OK(app.context().scheduler->add_system(std::move(probe)));
        CONJURE_REQUIRE_OK(app.context().scheduler->finalise());
        app.post_to_main_thread([&log, &app]() {
            log.emplace_back("task");
            CHECK(app.is_inside_frame());
        });
        CHECK(log.empty());
        CONJURE_REQUIRE_OK(app.step_frame());
        const std::vector<std::string> expected = {"task", "frame_begin"};
        CHECK(log == expected);
        CHECK_FALSE(app.is_inside_frame());
        app.shutdown();
    }

    TEST_CASE("application - engine_directory is the executable's directory before, during and after a run") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        const std::filesystem::path expected = executable_directory();
        CHECK(expected.is_absolute());
        CHECK(app.engine_directory() == expected);
        CONJURE_REQUIRE_OK(app.initialise());
        CHECK(app.engine_directory() == expected);
        app.shutdown();
        CHECK(app.engine_directory() == expected);
    }

    TEST_CASE("regression - code sweep F084 managed_directory is absolute and is what the script host is given") {
        // Unset: the SDK that ships next to the executable.
        engine_config config = engine_test::inline_config();
        config.scripting.managedDirectory.clear();
        application defaults(engine_test::make_desc(config));
        CHECK(defaults.managed_directory() == executable_directory() / "managed");
        CHECK(defaults.managed_directory().is_absolute());

        // An absolute setting is used as given.
        const std::filesystem::path elsewhere = std::filesystem::temp_directory_path() / "conjure_sdk";
        config.scripting.managedDirectory = elsewhere;
        application configured(engine_test::make_desc(config));
        CHECK(configured.managed_directory() == elsewhere);

        // A relative one is made absolute once, against the current directory, so the script host and a build
        // tool that resolves paths against its own project directory still mean the same place.
        config.scripting.managedDirectory = std::filesystem::path("sdk") / "managed";
        application relative(engine_test::make_desc(config));
        CHECK(relative.managed_directory().is_absolute());
        CHECK(relative.managed_directory() == std::filesystem::current_path() / "sdk" / "managed");
    }

    TEST_CASE("application - add_subsystem rejects null and late additions") {
        std::vector<std::string> log = {};
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_CHECK_ASSERTS(app.add_subsystem(nullptr));
        CONJURE_REQUIRE_OK(app.initialise());
        CONJURE_CHECK_ASSERTS(app.add_subsystem(std::make_unique<engine_test::probe_subsystem>("late", log)));
        CHECK(engine_test::filter_log(log, "late.startup").empty());
        app.shutdown();
    }

    TEST_CASE("application - the destructor shuts down a running application") {
        std::vector<std::string> log = {};
        {
            application app(engine_test::make_desc(engine_test::inline_config()));
            app.add_subsystem(std::make_unique<engine_test::probe_subsystem>("probe", log));
            CONJURE_REQUIRE_OK(app.initialise());
            CONJURE_REQUIRE_OK(app.step_frame());
        }
        const std::vector<std::string> expected = {
            "probe.startup", "probe.post_startup", "probe.register_systems", "probe.begin_frame", "probe.end_frame",
            "probe.shutdown", "probe.destroyed"
        };
        CHECK(log == expected);
    }
}
