#include "conjure/engine/frame_stats.h"

#include <algorithm>

using namespace conjure;
using namespace conjure::engine;

namespace {
    const frame_sample EMPTY_SAMPLE = {};
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

frame_stats::frame_stats() {}

frame_stats::~frame_stats() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void frame_stats::commit(const frame_sample &sample) {
    mSamples[mWriteIndex] = sample;
    mWriteIndex = (mWriteIndex + 1) % HISTORY_SIZE;
    mCount = std::min(mCount + 1, HISTORY_SIZE);
    ++mTotalCommitted;
    if (sample.droppedFixedTime) {
        ++mTotalFramesWithDroppedTime;
    }
}

const frame_sample &frame_stats::latest() const noexcept {
    return sample_at(0);
}

std::span<const frame_sample> frame_stats::history() const noexcept {
    return std::span<const frame_sample>(mSamples.data(), mCount);
}

const frame_sample &frame_stats::sample_at(usize age) const noexcept {
    if (age >= mCount) {
        return EMPTY_SAMPLE;
    }
    // The latest sample sits one before the write index; older samples walk backwards around the ring.
    const usize index = (mWriteIndex + HISTORY_SIZE - 1 - age) % HISTORY_SIZE;
    return mSamples[index];
}

f32 frame_stats::average_frame_milliseconds(usize frameCount) const noexcept {
    const usize count = std::min(frameCount, mCount);
    if (count == 0) {
        return 0.0f;
    }
    f64 total = 0.0;
    for (usize age = 0; age < count; ++age) {
        total += static_cast<f64>(sample_at(age).frameMilliseconds);
    }
    return static_cast<f32>(total / static_cast<f64>(count));
}

void frame_stats::clear() noexcept {
    mSamples.fill(frame_sample{});
    mWriteIndex = 0;
    mCount = 0;
    mTotalCommitted = 0;
    mTotalFramesWithDroppedTime = 0;
}
