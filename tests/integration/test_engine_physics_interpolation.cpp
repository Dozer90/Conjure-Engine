#include "conjure/testing/doctest.h"

#include "conjure/testing/headless_engine.h"
#include "conjure/testing/world_builder.h"
#include "integration_engine_support.h"

#include "conjure/core/types.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/engine_time.h"
#include "conjure/physics/components.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"

#include <algorithm>
#include <cmath>

// docs/design/06_engine_loop.md 10 test_engine_physics_interpolation: a falling body stepped for 300 frames renders
// a height that never rises, always lies between the two most recent fixed-step poses and equals their lerp at the
// frame's interpolationAlpha (R47, 3.2). Two cadences: the design's forced 1/30 (two fixed steps of 1/60 per frame,
// so every frame ends on a step and the rendered pose is the previous one), and a 1/40 frame over a 1/64 step (a
// binary-exact step the frame never divides: alpha cycles 0.6, 0.2, 0.8, 0.4, 0, so the lerp is exercised at four
// mid-range alphas and on the frames that land on a step).

using namespace conjure;
using namespace conjure::testing;

namespace {
    constexpr u32 FRAME_COUNT = 300;
    constexpr f64 TWO_STEP_FRAME_DELTA = 1.0 / 30.0;     // two 1/60 steps per frame: alpha is 0 at every frame end
    constexpr f64 MID_STEP_FRAME_DELTA = 1.0 / 40.0;     // 1.6 steps of 1/64 per frame: 8 steps per 5 frames
    constexpr f64 MID_STEP_FIXED_DELTA = 1.0 / 64.0;
    constexpr f32 START_HEIGHT = 2000.0f;
    constexpr f32 EPSILON = 1.0e-3f;

    // Frame 0 runs one fixed step (06 3.2); the forced 1/30 runs two per frame after it.
    constexpr u64 TWO_STEP_FIXED_STEPS = 1 + 2 * (FRAME_COUNT - 1);
    // Frame 0 runs one step; afterwards five 1/40 frames hold eight 1/64 steps (1, 2, 1, 2, 2), and the four
    // frames left over from 299 run 1 + 2 + 1 + 2.
    constexpr u64 MID_STEP_FIXED_STEPS = 1 + ((FRAME_COUNT - 1) / 5) * 8 + 6;

    struct fall_summary {
        f32 lastRendered = START_HEIGHT;
        u32 strictlyBetween = 0;    // frames whose rendered height lay strictly between the two poses
        u32 onStep = 0;             // frames with alpha 0 whose rendered height was the previous pose
    };

    /// <summary>
    /// Steps the falling body for the remaining frames at 'frameDelta' and checks every frame against the two
    /// fixed poses the body carries and the frame's interpolationAlpha.
    /// </summary>
    [[nodiscard]] fall_summary run_falling_body(headless_engine &engine, ecs::entity body, f64 frameDelta) {
        const scene::scene &draft = engine.active_scene();
        fall_summary summary = {};
        for (u32 frame = 1; frame < FRAME_COUNT; ++frame) {
            CAPTURE(frame);
            engine.step_frames(1, frameDelta);
            const scene::interpolated_transform *poses = draft.world().get<scene::interpolated_transform>(body);
            REQUIRE(poses != nullptr);
            const f32 alpha = engine.application().time().interpolationAlpha;
            CAPTURE(alpha);
            const f32 previous = poses->previous.position.y;
            const f32 current = poses->current.position.y;
            const f32 rendered = integration_test::rendered_position(draft, body).y;
            const f32 simulated = integration_test::propagated_position(draft, body).y;
            const f32 low = std::min(previous, current) - EPSILON;
            const f32 high = std::max(previous, current) + EPSILON;
            const f32 stepDistance = std::abs(current - previous);

            CHECK(alpha >= 0.0f);
            CHECK(alpha < 1.0f);
            CHECK(rendered <= summary.lastRendered + EPSILON);      // never rises
            CHECK(rendered >= low);
            CHECK(rendered <= high);
            CHECK(std::abs(simulated - current) <= EPSILON);        // simulation truth is the latest pose
            CHECK(std::abs(rendered - simulated) <= stepDistance + EPSILON);
            // The rendered pose is the lerp of the two poses at the frame's alpha (06 3.2), not the current pose
            // (interpolation off) and not a pose at some other alpha.
            const f32 expected = std::lerp(previous, current, alpha);
            CHECK(std::abs(rendered - expected) <= EPSILON);
            if (alpha <= EPSILON) {
                CHECK(std::abs(rendered - previous) <= EPSILON);
                if (stepDistance > 2.0f * EPSILON) {
                    CHECK(std::abs(rendered - current) > EPSILON);  // the previous pose, not the current one
                }
                ++summary.onStep;
            }
            if (rendered > low + 2.0f * EPSILON && rendered < high - 2.0f * EPSILON) {
                ++summary.strictlyBetween;   // strictly inside the pose interval, whichever way the body moves
            }
            summary.lastRendered = rendered;
        }
        return summary;
    }

    [[nodiscard]] ecs::entity spawn_falling_body(headless_engine &engine, f64 firstFrameDelta) {
        REQUIRE(engine.context().physics != nullptr);
        scene::scene &draft = engine.active_scene();
        const core::result<ecs::entity> body =
          spawn_box_body(draft, math::vec3(0.0f, START_HEIGHT, 0.0f), physics::motion_type::DYNAMIC);
        CONJURE_REQUIRE_OK(body);
        // The first frame creates the Jolt body and the interpolated_transform (R47); interpolation is observed
        // from the second frame on.
        engine.step_frames(1, firstFrameDelta);
        REQUIRE(draft.world().get<scene::interpolated_transform>(body.value()) != nullptr);
        return body.value();
    }
}

TEST_SUITE("integration/engine_physics_interpolation") {

    TEST_CASE("engine_physics_interpolation - a falling body at a two-step frame renders its previous fixed pose") {
        headless_engine engine(integration_test::physics_options());
        const ecs::entity body = spawn_falling_body(engine, TWO_STEP_FRAME_DELTA);

        const fall_summary summary = run_falling_body(engine, body, TWO_STEP_FRAME_DELTA);
        // f32(1/30) is exactly two f32(1/60) steps, so the accumulator is empty at every frame end: alpha 0, the
        // rendered pose is the previous one on every frame, and the body nevertheless fell the whole way.
        CHECK(summary.onStep == FRAME_COUNT - 1);
        CHECK(summary.strictlyBetween == 0);
        CHECK(summary.lastRendered < START_HEIGHT - 100.0f);
        CHECK(engine.fixed_update_count() == TWO_STEP_FIXED_STEPS);
    }

    TEST_CASE("engine_physics_interpolation - a falling body at a mid-step frame renders the lerp at its alpha") {
        headless_engine_options options = integration_test::physics_options();
        options.fixedDeltaSeconds = MID_STEP_FIXED_DELTA;
        headless_engine engine(options);
        const ecs::entity body = spawn_falling_body(engine, MID_STEP_FRAME_DELTA);

        const fall_summary summary = run_falling_body(engine, body, MID_STEP_FRAME_DELTA);
        // Four of every five frames end mid-step (alpha 0.6, 0.2, 0.8, 0.4) and the fifth lands on a step.
        REQUIRE(summary.strictlyBetween > 0);
        CHECK(summary.onStep > 0);
        CHECK(summary.strictlyBetween + summary.onStep == FRAME_COUNT - 1);
        CHECK(summary.strictlyBetween == ((FRAME_COUNT - 1) / 5) * 4 + 4);
        CHECK(summary.lastRendered < START_HEIGHT - 100.0f);
        CHECK(engine.fixed_update_count() == MID_STEP_FIXED_STEPS);
    }
}
