#include "audio/stream_ring.h"

#include "conjure/core/assert.h"

#include <algorithm>
#include <cstring>

using namespace conjure;
using namespace conjure::audio;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

stream_ring::stream_ring(u32 capacityFrames) :
    mSamples(static_cast<usize>(capacityFrames) * MIX_CHANNELS, 0.0f),
    mCapacityFrames(capacityFrames) {
    CONJURE_ASSERT(capacityFrames > 0, "stream_ring needs a positive capacity");
}

stream_ring::~stream_ring() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void stream_ring::reset(u16 channels) noexcept {
    CONJURE_ASSERT(channels == 1 || channels == 2, "stream_ring supports 1 or 2 channels, got {}", channels);
    mChannels = static_cast<u16>(channels == 2 ? 2 : 1);
    mWriteFrame.store(0, std::memory_order_relaxed);
    mReadFrame.store(0, std::memory_order_relaxed);
    mEndOfStream.store(false, std::memory_order_relaxed);
}

u32 stream_ring::free_frames() const noexcept {
    const u64 written = mWriteFrame.load(std::memory_order_relaxed);
    const u64 read = mReadFrame.load(std::memory_order_acquire);
    return mCapacityFrames - static_cast<u32>(written - read);
}

u32 stream_ring::available_frames() const noexcept {
    const u64 read = mReadFrame.load(std::memory_order_relaxed);
    const u64 written = mWriteFrame.load(std::memory_order_acquire);
    return static_cast<u32>(written - read);
}

u32 stream_ring::push_frames(std::span<const f32> interleaved) noexcept {
    const u32 wanted = static_cast<u32>(interleaved.size() / mChannels);
    const u32 count = std::min(wanted, free_frames());
    if (count == 0) {
        return 0;
    }

    const u64 written = mWriteFrame.load(std::memory_order_relaxed);
    const u32 start = static_cast<u32>(written % mCapacityFrames);
    const u32 firstPart = std::min(count, mCapacityFrames - start);
    const u32 secondPart = count - firstPart;

    std::memcpy(
      mSamples.data() + static_cast<usize>(start) * mChannels,
      interleaved.data(),
      static_cast<usize>(firstPart) * mChannels * sizeof(f32)
    );
    if (secondPart > 0) {
        std::memcpy(
          mSamples.data(),
          interleaved.data() + static_cast<usize>(firstPart) * mChannels,
          static_cast<usize>(secondPart) * mChannels * sizeof(f32)
        );
    }

    mWriteFrame.store(written + count, std::memory_order_release);
    return count;
}

bool stream_ring::pop_frame(f32 *out) noexcept {
    const u64 read = mReadFrame.load(std::memory_order_relaxed);
    const u64 written = mWriteFrame.load(std::memory_order_acquire);
    if (read == written) {
        return false;
    }

    const usize offset = static_cast<usize>(read % mCapacityFrames) * mChannels;
    for (u16 channel = 0; channel < mChannels; ++channel) {
        out[channel] = mSamples[offset + channel];
    }
    mReadFrame.store(read + 1, std::memory_order_release);
    return true;
}
