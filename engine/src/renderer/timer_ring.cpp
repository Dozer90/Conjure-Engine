#include "renderer/timer_ring.h"
#include "conjure/renderer/log_category.h"

#include "conjure/core/log.h"

using namespace conjure;
using namespace conjure::renderer;

timer_ring::timer_ring(nvrhi::IDevice &device) :
    mDevice(device) {
    // The handle vectors are sized once so growing the slots later never reallocates; the queries
    // themselves are created by begin() for all slots at once, so a frame that needs more than any
    // frame before it pays for the next two frames as well and a steady pass count pays nothing.
    for (frame_queries &frame : mFrames) {
        frame.queries.reserve(MAX_QUERIES_PER_FRAME);
        frame.names.reserve(MAX_QUERIES_PER_FRAME);
    }
}

timer_ring::~timer_ring() {}

usize timer_ring::query_count() const noexcept {
    usize count = 0;
    for (const frame_queries &frame : mFrames) {
        count += frame.queries.size();
    }
    return count;
}

void timer_ring::begin_frame() {
    mCurrent = (mCurrent + 1) % FRAME_COUNT;
    frame_queries &frame = mFrames[mCurrent];
    mResolved.clear();
    if (frame.pending) {
        for (usize index = 0; index < frame.used; ++index) {
            nvrhi::ITimerQuery *query = frame.queries[index].Get();
            if (query == nullptr || !mDevice.pollTimerQuery(query)) {
                continue;
            }
            pass_timing timing = {};
            timing.name = frame.names[index];
            timing.gpuMilliseconds = mDevice.getTimerQueryTime(query) * 1000.0f;
            if (mResolved.size() < mResolved.INLINE_CAPACITY) {
                mResolved.push_back(timing);
            }
            mDevice.resetTimerQuery(query);
        }
    }
    frame.used = 0;
    frame.pending = false;
    mOpen = false;
}

void timer_ring::begin(nvrhi::ICommandList &commandList, const char *name) {
    frame_queries &frame = mFrames[mCurrent];
    if (mOpen || frame.used >= MAX_QUERIES_PER_FRAME) {
        return;
    }
    if (frame.queries.size() <= frame.used && !mDeviceRefused) {
        // No frame needed this many queries before: give every slot one more, so the two frames
        // that follow (the other slots) find theirs and create nothing.
        const usize needed = frame.used + 1;
        for (frame_queries &slot : mFrames) {
            while (slot.queries.size() < needed) {
                nvrhi::TimerQueryHandle query = mDevice.createTimerQuery();
                if (query == nullptr) {
                    mDeviceRefused = true;
                    CONJURE_LOG_WARN(
                      LOG_RENDERER,
                      "timer_ring: the device refused timer query {} of {} per frame; passes beyond {} go "
                      "untimed (the device's timer-query budget is shared by every renderer on it)",
                      slot.queries.size() + 1,
                      MAX_QUERIES_PER_FRAME,
                      slot.queries.size()
                    );
                    break;
                }
                slot.queries.push_back(std::move(query));
                slot.names.push_back("");
            }
            if (mDeviceRefused) {
                break;
            }
        }
    }
    if (frame.queries.size() <= frame.used) {
        // The device refused this query; the pass goes untimed.
        return;
    }
    frame.names[frame.used] = name;
    commandList.beginTimerQuery(frame.queries[frame.used].Get());
    mOpen = true;
}

void timer_ring::end(nvrhi::ICommandList &commandList) {
    frame_queries &frame = mFrames[mCurrent];
    if (!mOpen) {
        return;
    }
    commandList.endTimerQuery(frame.queries[frame.used].Get());
    ++frame.used;
    frame.pending = true;
    mOpen = false;
}
