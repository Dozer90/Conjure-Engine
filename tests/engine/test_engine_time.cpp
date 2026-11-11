#include "conjure/testing/doctest.h"

#include "engine_test_support.h"

#include "conjure/core/types.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_phase.h"
#include "conjure/engine/engine_time.h"

#include <cmath>
#include <vector>

// engine_time.h (docs/design/06_engine_loop.md 2.1): the per-frame timing block, its clamp and scale rules
// and the frame_info it hands the scheduler. The last cases drive a headless application for the sequences
// only the loop produces.

using namespace conjure;
using namespace conjure::engine;

namespace {

    [[nodiscard]] time_settings sixty_hz() {
        time_settings settings = {};
        settings.fixedDeltaSeconds = 1.0f / 60.0f;
        settings.maxFrameDeltaSeconds = 0.1f;
        settings.maxFixedStepsPerFrame = 8;
        return settings;
    }
}

TEST_SUITE("engine/engine_time") {

    TEST_CASE("engine_time - the first frame delta equals the fixed delta whatever the clock says") {
        engine_time time = {};
        begin_frame(time, sixty_hz(), 5.0);
        CHECK(time.frameIndex == 1);
        CHECK(time.unscaledDeltaTime == 1.0f / 60.0f);
        CHECK(time.deltaTime == 1.0f / 60.0f);
    }

    TEST_CASE("engine_time - the frame delta is clamped to the maximum") {
        engine_time time = {};
        begin_frame(time, sixty_hz(), 0.0);
        begin_frame(time, sixty_hz(), 2.0);
        CHECK(time.unscaledDeltaTime == 0.1f);
        CHECK(time.frameIndex == 2);
        begin_frame(time, sixty_hz(), -1.0);
        CHECK(time.unscaledDeltaTime == 0.0f);
    }

    TEST_CASE("engine_time - scaled and unscaled time accumulate under half time scale") {
        engine_time time = {};
        time_settings settings = sixty_hz();
        settings.timeScale = 0.5f;
        begin_frame(time, settings, 0.0);
        for (u32 frame = 0; frame < 10; ++frame) {
            begin_frame(time, settings, 0.02);
        }
        CHECK(std::abs(time.unscaledTime - (1.0 / 60.0 + 0.2)) < 1.0e-6);
        CHECK(std::abs(time.time - (1.0 / 120.0 + 0.1)) < 1.0e-6);
        CHECK(time.deltaTime == 0.01f);
        CHECK(time.timeScale == 0.5f);
    }

    TEST_CASE("engine_time - a zero time scale yields a zero delta but the frame still advances") {
        engine_time time = {};
        time_settings settings = sixty_hz();
        settings.timeScale = 0.0f;
        begin_frame(time, settings, 0.0);
        begin_frame(time, settings, 0.016);
        CHECK(time.deltaTime == 0.0f);
        CHECK(time.unscaledDeltaTime > 0.0f);
        CHECK(time.frameIndex == 2);
        CHECK(time.time == 0.0);
    }

    TEST_CASE("engine_time - fixed time advances per step and counts steps") {
        engine_time time = {};
        begin_frame(time, sixty_hz(), 0.0);
        advance_fixed_step(time);
        advance_fixed_step(time);
        CHECK(time.fixedStepIndex == 2);
        CHECK(std::abs(time.fixedTime - 2.0 / 60.0) < 1.0e-6);
    }

    TEST_CASE("engine_time - the frame index is monotonic") {
        engine_time time = {};
        u64 previous = 0;
        for (u32 frame = 0; frame < 100; ++frame) {
            begin_frame(time, sixty_hz(), 0.001 * frame);
            CHECK(time.frameIndex == previous + 1);
            previous = time.frameIndex;
        }
    }

    TEST_CASE("engine_time - make_frame_info mirrors the block and switches the delta inside fixed steps") {
        engine_time time = {};
        begin_frame(time, sixty_hz(), 0.0);
        begin_frame(time, sixty_hz(), 0.05);
        time.interpolationAlpha = 0.25f;
        ecs::frame_info info = make_frame_info(time);
        CHECK(info.deltaTime == time.deltaTime);
        CHECK(info.fixedDeltaTime == time.fixedDeltaTime);
        CHECK(info.interpolationAlpha == 0.25f);
        CHECK(info.frameIndex == 2);
        CHECK_FALSE(info.inFixedStep);

        time.inFixedStep = true;
        info = make_frame_info(time);
        CHECK(info.inFixedStep);
        CHECK(info.deltaTime == time.fixedDeltaTime);
    }

    TEST_CASE("engine_time - forced delta sequences are identical across two applications") {
        std::vector<engine_time> first = {};
        std::vector<engine_time> second = {};
        for (std::vector<engine_time> *target : {&first, &second}) {
            engine_config config = engine_test::inline_config();
            config.time.forcedFrameDeltaSeconds = 1.0f / 30.0f;
            application app(engine_test::make_desc(config));
            CONJURE_REQUIRE_OK(app.initialise());
            for (u32 frame = 0; frame < 12; ++frame) {
                CONJURE_REQUIRE_OK(app.step_frame());
                target->push_back(app.time());
            }
            app.shutdown();
        }
        REQUIRE(first.size() == second.size());
        for (usize index = 0; index < first.size(); ++index) {
            CHECK(first[index] == second[index]);
        }
        CHECK(first.back().frameIndex == 12);
        CHECK(first.back().fixedStepIndex == 23);   // frame 1 runs one step, the eleven others two each
    }

    TEST_CASE("engine_time - inFixedStep is true only inside the fixed phases") {
        engine_config config = engine_test::inline_config();
        config.time.forcedFrameDeltaSeconds = 1.0f / 30.0f;
        application app(engine_test::make_desc(config));
        CONJURE_REQUIRE_OK(app.initialise());

        std::vector<bool> fixedFlags = {};
        std::vector<bool> otherFlags = {};
        const engine_time *time = app.context().time;
        for (u32 index = 0; index < ecs::SYSTEM_PHASE_COUNT; ++index) {
            const ecs::system_phase phase = static_cast<ecs::system_phase>(index);
            ecs::system_desc desc = {};
            desc.name = core::string_id::intern(std::string("probe.fixed_flag.") + std::string(ecs::to_string(phase)));
            desc.phase = phase;
            desc.flags = ecs::system_flags::EXCLUSIVE;
            const bool fixed = is_fixed_phase(phase);
            desc.run = [time, fixed, &fixedFlags, &otherFlags](ecs::system_context &context) {
                (fixed ? fixedFlags : otherFlags).push_back(time->inFixedStep);
                CHECK(context.frame().inFixedStep == time->inFixedStep);
            };
            CONJURE_REQUIRE_OK(app.context().scheduler->add_system(std::move(desc)));
        }
        CONJURE_REQUIRE_OK(app.context().scheduler->finalise());
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());

        CHECK_FALSE(fixedFlags.empty());
        for (const bool flag : fixedFlags) {
            CHECK(flag);
        }
        for (const bool flag : otherFlags) {
            CHECK_FALSE(flag);
        }
        app.shutdown();
    }
}
