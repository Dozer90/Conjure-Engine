#include "conjure/engine/fixed_step_clock.h"

#include "conjure/core/assert.h"

#include <cmath>

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

fixed_step_clock::fixed_step_clock() {}

fixed_step_clock::fixed_step_clock(f32 fixedDeltaSeconds, u32 maxStepsPerFrame) {
    set_fixed_delta(fixedDeltaSeconds);
    set_max_steps_per_frame(maxStepsPerFrame);
}

fixed_step_clock::~fixed_step_clock() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

fixed_step_result fixed_step_clock::advance(f32 scaledDeltaSeconds) {
    fixed_step_result result = {};
    if (std::isfinite(scaledDeltaSeconds) && scaledDeltaSeconds > 0.0f) {
        mAccumulatorSeconds += static_cast<f64>(scaledDeltaSeconds);
    }

    const f64 fixedDelta = static_cast<f64>(mFixedDeltaSeconds);
    const f64 rawSteps = std::floor(mAccumulatorSeconds / fixedDelta);
    const u64 steps = rawSteps > 0.0 ? static_cast<u64>(rawSteps) : 0;

    if (steps > mMaxStepsPerFrame) {
        // Spiral-of-death guard (D3): the leftover is discarded so one slow frame cannot snowball into
        // ever more steps on the next.
        result.stepCount = mMaxStepsPerFrame;
        result.droppedTime = true;
        mAccumulatorSeconds = 0.0;
    } else {
        result.stepCount = static_cast<u32>(steps);
        mAccumulatorSeconds -= static_cast<f64>(steps) * fixedDelta;
        if (mAccumulatorSeconds < 0.0) {
            mAccumulatorSeconds = 0.0;
        }
    }

    result.interpolationAlpha = static_cast<f32>(mAccumulatorSeconds / fixedDelta);
    if (result.interpolationAlpha >= 1.0f) {
        // Floating-point rounding of the subtraction can leave the ratio a hair above one.
        result.interpolationAlpha = std::nextafter(1.0f, 0.0f);
    }
    return result;
}

void fixed_step_clock::reset() noexcept {
    mAccumulatorSeconds = 0.0;
}

void fixed_step_clock::set_fixed_delta(f32 fixedDeltaSeconds) {
    CONJURE_ASSERT(
      std::isfinite(fixedDeltaSeconds) && fixedDeltaSeconds > 0.0f,
      "fixed_step_clock: the fixed delta must be positive ({})",
      fixedDeltaSeconds
    );
    if (!std::isfinite(fixedDeltaSeconds) || fixedDeltaSeconds <= 0.0f) {
        return;
    }
    mFixedDeltaSeconds = fixedDeltaSeconds;
}

void fixed_step_clock::set_max_steps_per_frame(u32 maxStepsPerFrame) {
    CONJURE_ASSERT(maxStepsPerFrame > 0, "fixed_step_clock: the step cap must be positive");
    if (maxStepsPerFrame == 0) {
        return;
    }
    mMaxStepsPerFrame = maxStepsPerFrame;
}
