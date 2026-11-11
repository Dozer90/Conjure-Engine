#include "engine/frame_pacer.h"

#include <algorithm>
#include <limits>

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

frame_pacer::frame_pacer(core::clock &clock, const engine_config &config) :
    mClock(&clock),
    mPacing(config.pacing),
    mFixedDeltaSeconds(config.time.fixedDeltaSeconds),
    mHeadless(config.headless),
    mForcedDelta(config.time.forcedFrameDeltaSeconds > 0.0f) {
    mFrameStartSeconds = mClock->now_seconds();
}

frame_pacer::~frame_pacer() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void frame_pacer::begin_frame() {
    mFrameStartSeconds = mClock->now_seconds();
}

f64 frame_pacer::end_frame(bool presentHappened) {
    mLastSleepSeconds = 0.0;
    if (presentHappened || !is_active()) {
        // With vsync the present inside render_frame is the back-pressure (3.3).
        return 0.0;
    }

    const f64 period = 1.0 / static_cast<f64>(effective_target_hz());
    const f64 deadline = mFrameStartSeconds + period;
    const f64 now = mClock->now_seconds();
    if (now >= deadline) {
        return 0.0;
    }

    mClock->sleep_until(deadline);
    mLastSleepSeconds = mClock->now_seconds() - now;
    if (mLastSleepSeconds < 0.0) {
        mLastSleepSeconds = 0.0;
    }
    ++mSleepCount;
    return mLastSleepSeconds;
}

u32 frame_pacer::effective_target_hz() const noexcept {
    if (mForcedDelta) {
        return 0;
    }
    u32 target = mPacing.targetHz;
    if (target == 0 && mHeadless && mFixedDeltaSeconds > 0.0f) {
        // Clamped before the conversion: a tiny fixed delta would otherwise convert a value above u32's
        // range, which is undefined behaviour.
        const f64 rate = 1.0 / static_cast<f64>(mFixedDeltaSeconds);
        const f64 maxRate = static_cast<f64>(std::numeric_limits<u32>::max());
        target = rate >= 1.0 ? static_cast<u32>(std::min(rate + 0.5, maxRate)) : 1u;
    }
    if (mUnfocused && mPacing.sleepWhenUnfocused) {
        target = target == 0 ? UNFOCUSED_HZ : std::min(target, UNFOCUSED_HZ);
    }
    return target;
}
