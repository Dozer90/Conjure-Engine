#include "renderer/timer_ring.h"

using namespace conjure;
using namespace conjure::renderer;

timer_ring::timer_ring(nvrhi::IDevice &device) :
    mDevice(device) {}

timer_ring::~timer_ring() {}

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
    if (frame.queries.size() <= frame.used) {
        nvrhi::TimerQueryHandle query = mDevice.createTimerQuery();
        if (query == nullptr) {
            return;
        }
        frame.queries.push_back(std::move(query));
        frame.names.push_back(name);
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
