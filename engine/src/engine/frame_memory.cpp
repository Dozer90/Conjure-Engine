#include "conjure/engine/frame_memory.h"

#include "conjure/engine/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"
#include "conjure/core/string_utils.h"

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

frame_memory::frame_memory(const memory_settings &settings) :
    mAllocator(settings.frameArenaBytes, settings.frameThreadBlockBytes) {
    mEpoch = mAllocator.epoch();
}

frame_memory::~frame_memory() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void *frame_memory::allocate(usize bytes, usize alignment) noexcept {
    return mAllocator.allocate(bytes, alignment);
}

void frame_memory::reset() {
    // Resetting under live scheduler jobs would hand their memory to the next frame (D6).
    CONJURE_ASSERT(!mPhaseInFlight, "frame_memory::reset() while a scheduler phase is in flight");
    if (mPhaseInFlight) {
        return;
    }
    mOverflowCount = mAllocator.overflow_count();
    ++mResetCount;

    // The heap fallback is never fatal; it is reported at most once per interval so a frame that
    // overflows every time does not flood the log (06_engine_loop.md 6).
    if (mOverflowCount > 0 && (!mWarnedOnce || mResetCount - mLastWarningReset >= OVERFLOW_WARNING_INTERVAL)) {
        mWarnedOnce = true;
        mLastWarningReset = mResetCount;
        CONJURE_LOG_WARN(
          LOG_ENGINE,
          "frame memory overflowed {} time(s) this frame ({} on the heap); high-water mark {} of {}",
          mOverflowCount,
          core::str::format_bytes(mAllocator.overflow_bytes()),
          core::str::format_bytes(mAllocator.high_water_bytes()),
          core::str::format_bytes(mAllocator.capacity_bytes())
        );
    }

    mAllocator.reset();
    mEpoch = mAllocator.epoch();
}

frame_memory_stats frame_memory::stats() const noexcept {
    frame_memory_stats stats = {};
    stats.highWaterBytes = mAllocator.high_water_bytes();
    stats.capacityBytes = mAllocator.capacity_bytes();
    stats.overflowCount = mAllocator.overflow_count();
    return stats;
}

usize frame_memory::used_bytes() const noexcept {
    return mAllocator.used_bytes();
}
