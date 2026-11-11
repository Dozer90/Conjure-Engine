#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "conjure/testing/assert_capture.h"

#include "conjure/core/random.h"
#include "conjure/core/types.h"
#include "conjure/engine/fixed_step_clock.h"

#include <cmath>

// fixed_step_clock.h (docs/design/06_engine_loop.md 2.2, D3): the pure accumulator with its clamped step
// count and render-side alpha.

using namespace conjure;
using namespace conjure::engine;

namespace {

    constexpr f32 STEP = 1.0f / 60.0f;
}

TEST_SUITE("engine/fixed_step_clock") {

    TEST_CASE("fixed_step_clock - zero delta gives zero steps") {
        fixed_step_clock clock(STEP, 8);
        const fixed_step_result result = clock.advance(0.0f);
        CHECK(result.stepCount == 0);
        CHECK(result.interpolationAlpha == 0.0f);
        CHECK_FALSE(result.droppedTime);
    }

    TEST_CASE("fixed_step_clock - exact delta gives one step with alpha zero") {
        fixed_step_clock clock(STEP, 8);
        const fixed_step_result result = clock.advance(STEP);
        CHECK(result.stepCount == 1);
        CHECK(result.interpolationAlpha < 1.0e-4f);
    }

    TEST_CASE("fixed_step_clock - two half deltas give one step on the second") {
        fixed_step_clock clock(STEP, 8);
        CHECK(clock.advance(STEP * 0.5f).stepCount == 0);
        CHECK(clock.advance(STEP * 0.5f).stepCount == 1);
    }

    TEST_CASE("fixed_step_clock - two and a half steps give two steps and alpha one half") {
        fixed_step_clock clock(STEP, 8);
        const fixed_step_result result = clock.advance(STEP * 2.5f);
        CHECK(result.stepCount == 2);
        CHECK(std::abs(result.interpolationAlpha - 0.5f) < 1.0e-3f);
    }

    TEST_CASE("fixed_step_clock - alpha stays in the unit interval") {
        fixed_step_clock clock(STEP, 8);
        core::random::random_engine random(42);
        for (u32 frame = 0; frame < 2000; ++frame) {
            const f32 delta = core::random::uniform(random, 0.0f, STEP * 3.0f);
            const fixed_step_result result = clock.advance(delta);
            CHECK(result.interpolationAlpha >= 0.0f);
            CHECK(result.interpolationAlpha < 1.0f);
        }
    }

    TEST_CASE("fixed_step_clock - step cap drops the remainder and flags it") {
        fixed_step_clock clock(STEP, 4);
        const fixed_step_result result = clock.advance(STEP * 10.0f);
        CHECK(result.stepCount == 4);
        CHECK(result.droppedTime);
        CHECK(clock.accumulator() == 0.0);
        CHECK(result.interpolationAlpha == 0.0f);
    }

    TEST_CASE("fixed_step_clock - the dropped flag clears on the next advance") {
        fixed_step_clock clock(STEP, 2);
        CHECK(clock.advance(STEP * 5.0f).droppedTime);
        const fixed_step_result next = clock.advance(STEP);
        CHECK_FALSE(next.droppedTime);
        CHECK(next.stepCount == 1);
    }

    TEST_CASE("fixed_step_clock - set_fixed_delta keeps the accumulator") {
        fixed_step_clock clock(STEP, 8);
        CHECK(clock.advance(STEP * 0.5f).stepCount == 0);
        const f64 before = clock.accumulator();
        clock.set_fixed_delta(STEP * 0.25f);
        CHECK(clock.accumulator() == before);
        CHECK(clock.fixed_delta() == STEP * 0.25f);
        CHECK(clock.advance(0.0f).stepCount == 2);
    }

    TEST_CASE("fixed_step_clock - reset zeroes the accumulator") {
        fixed_step_clock clock(STEP, 8);
        CHECK(clock.advance(STEP * 0.75f).stepCount == 0);
        clock.reset();
        CHECK(clock.accumulator() == 0.0);
        CHECK(clock.advance(STEP * 0.5f).stepCount == 0);
    }

    TEST_CASE("fixed_step_clock - random deltas conserve time") {
        fixed_step_clock clock(STEP, 1000);
        core::random::random_engine random(7);
        f64 fed = 0.0;
        u64 steps = 0;
        for (u32 frame = 0; frame < 5000; ++frame) {
            const f32 delta = core::random::uniform(random, 0.0f, STEP * 2.0f);
            fed += static_cast<f64>(delta);
            steps += clock.advance(delta).stepCount;
        }
        const f64 consumed = static_cast<f64>(steps) * static_cast<f64>(STEP) + clock.accumulator();
        CHECK(std::abs(consumed - fed) < 1.0e-3);
    }

    TEST_CASE("fixed_step_clock - invalid settings assert and keep the previous values") {
        fixed_step_clock clock(STEP, 8);
        CONJURE_CHECK_ASSERTS(clock.set_fixed_delta(0.0f));
        CHECK(clock.fixed_delta() == STEP);
        CONJURE_CHECK_ASSERTS(clock.set_max_steps_per_frame(0));
        CHECK(clock.max_steps_per_frame() == 8);
    }
}
