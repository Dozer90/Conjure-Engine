#include "conjure/testing/doctest.h"

#include "conjure/core/clock.h"
#include "conjure/core/types.h"
#include "conjure/engine/engine_config.h"
#include "engine/frame_pacer.h"

#include <cmath>
#include <limits>

// frame_pacer.h (docs/design/06_engine_loop.md 3.3): the sleep-to-target pacer on a manual clock.

using namespace conjure;
using namespace conjure::engine;

namespace {

    [[nodiscard]] engine_config paced_config(u32 targetHz, bool headless) {
        engine_config config = headless ? engine_config::headless_defaults() : engine_config::defaults();
        config.time.forcedFrameDeltaSeconds = 0.0f;
        config.pacing.targetHz = targetHz;
        return config;
    }
}

TEST_SUITE("engine/frame_pacer") {

    TEST_CASE("frame_pacer - sleeps to the target with a manual clock") {
        core::manual_clock clock(10.0);
        frame_pacer pacer(clock, paced_config(100, false));
        CHECK(pacer.effective_target_hz() == 100);
        CHECK(pacer.is_active());
        pacer.begin_frame();
        clock.advance(0.004);
        const f64 slept = pacer.end_frame(false);
        CHECK(std::abs(slept - 0.006) < 1.0e-9);
        CHECK(std::abs(clock.now_seconds() - 10.01) < 1.0e-9);
        CHECK(pacer.sleep_count() == 1);
        CHECK(pacer.last_sleep_seconds() == slept);
    }

    TEST_CASE("frame_pacer - never reports a negative sleep and skips when behind") {
        core::manual_clock clock(0.0);
        frame_pacer pacer(clock, paced_config(60, false));
        pacer.begin_frame();
        clock.advance(0.05);
        CHECK(pacer.end_frame(false) == 0.0);
        CHECK(pacer.sleep_count() == 0);
        CHECK(std::abs(clock.now_seconds() - 0.05) < 1.0e-9);
    }

    TEST_CASE("regression - review finding 11: a tiny fixed delta clamps the headless target to the u32 range") {
        core::manual_clock clock(0.0);
        engine_config config = paced_config(0, true);
        config.time.fixedDeltaSeconds = 1.0e-12f;
        frame_pacer pacer(clock, config);
        CHECK(pacer.effective_target_hz() == std::numeric_limits<u32>::max());
        CHECK(pacer.is_active());
    }

    TEST_CASE("frame_pacer - inactive with a forced frame delta") {
        core::manual_clock clock(0.0);
        engine_config config = paced_config(60, false);
        config.time.forcedFrameDeltaSeconds = 1.0f / 60.0f;
        frame_pacer pacer(clock, config);
        CHECK_FALSE(pacer.is_active());
        CHECK(pacer.effective_target_hz() == 0);
        pacer.begin_frame();
        CHECK(pacer.end_frame(false) == 0.0);
        CHECK(clock.now_seconds() == 0.0);
    }

    TEST_CASE("frame_pacer - inactive after a vsync present") {
        core::manual_clock clock(0.0);
        frame_pacer pacer(clock, paced_config(60, false));
        pacer.begin_frame();
        CHECK(pacer.end_frame(true) == 0.0);
        CHECK(clock.now_seconds() == 0.0);
    }

    TEST_CASE("frame_pacer - headless defaults to the fixed rate") {
        core::manual_clock clock(0.0);
        engine_config config = paced_config(0, true);
        config.time.fixedDeltaSeconds = 1.0f / 30.0f;
        frame_pacer pacer(clock, config);
        CHECK(pacer.effective_target_hz() == 30);
        pacer.begin_frame();
        CHECK(std::abs(pacer.end_frame(false) - 1.0 / 30.0) < 1.0e-6);
    }

    TEST_CASE("frame_pacer - unpaced when interactive without a target") {
        core::manual_clock clock(0.0);
        frame_pacer pacer(clock, paced_config(0, false));
        CHECK(pacer.effective_target_hz() == 0);
        CHECK_FALSE(pacer.is_active());
    }

    TEST_CASE("frame_pacer - an unfocused window drops to 30 Hz") {
        core::manual_clock clock(0.0);
        frame_pacer pacer(clock, paced_config(240, false));
        pacer.set_unfocused(true);
        CHECK(pacer.is_unfocused());
        CHECK(pacer.effective_target_hz() == frame_pacer::UNFOCUSED_HZ);
        pacer.set_unfocused(false);
        CHECK(pacer.effective_target_hz() == 240);

        engine_config noSleep = paced_config(240, false);
        noSleep.pacing.sleepWhenUnfocused = false;
        frame_pacer stubborn(clock, noSleep);
        stubborn.set_unfocused(true);
        CHECK(stubborn.effective_target_hz() == 240);
    }

    TEST_CASE("frame_pacer - set_target_hz changes the period") {
        core::manual_clock clock(0.0);
        frame_pacer pacer(clock, paced_config(60, false));
        pacer.set_target_hz(10);
        pacer.begin_frame();
        CHECK(std::abs(pacer.end_frame(false) - 0.1) < 1.0e-9);
    }
}
