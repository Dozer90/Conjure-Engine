#pragma once

#include "conjure/audio/audio_types.h"

#include "conjure/core/types.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

// Per streaming voice ring of decoded interleaved f32 frames (docs/design/12_audio.md 3.7). Single
// producer (the decode job, one at a time per slot) and single consumer (the mixer callback). The
// capacity is fixed at construction so nothing allocates after the engine is built.

namespace conjure::audio {

    class stream_ring {
        private:
            static constexpr usize COUNTER_PADDING = CACHE_LINE_SIZE - sizeof(std::atomic<u64>);

            std::vector<f32> mSamples = {};
            u32 mCapacityFrames = 0;
            u16 mChannels = 1;
            // Explicit padding keeps the two counters on separate cache lines without alignas (C4324).
            [[maybe_unused]] std::array<byte, CACHE_LINE_SIZE> mLeadPadding = {};
            std::atomic<u64> mWriteFrame = 0;                     // frames ever pushed
            [[maybe_unused]] std::array<byte, COUNTER_PADDING> mWritePadding = {};
            std::atomic<u64> mReadFrame = 0;                      // frames ever popped
            [[maybe_unused]] std::array<byte, COUNTER_PADDING> mReadPadding = {};
            std::atomic<bool> mEndOfStream = false;

        public:
            explicit stream_ring(u32 capacityFrames);
            ~stream_ring();

            stream_ring(const stream_ring &) = delete;
            stream_ring &operator=(const stream_ring &) = delete;

        public:
            /// <summary>
            /// Empties the ring for a new stream of 'channels' channels (1 or 2). Both sides idle.
            /// </summary>
            void reset(u16 channels) noexcept;

            [[nodiscard]] u32 capacity_frames() const noexcept {
                return mCapacityFrames;
            }

            [[nodiscard]] u16 channels() const noexcept {
                return mChannels;
            }

            /// <summary>
            /// Frames the producer may still push.
            /// </summary>
            [[nodiscard]] u32 free_frames() const noexcept;

            /// <summary>
            /// Frames the consumer may still pop.
            /// </summary>
            [[nodiscard]] u32 available_frames() const noexcept;

            /// <summary>
            /// Producer: appends whole frames (interleaved.size() / channels of them, capped by the
            /// free space). Returns the number of frames pushed.
            /// </summary>
            u32 push_frames(std::span<const f32> interleaved) noexcept;

            /// <summary>
            /// Consumer: copies one frame (channels() samples) into 'out'. False when empty.
            /// </summary>
            [[nodiscard]] bool pop_frame(f32 *out) noexcept;

            /// <summary>
            /// Producer: marks that no further frame will ever be pushed for this stream.
            /// </summary>
            void set_end_of_stream(bool value) noexcept {
                mEndOfStream.store(value, std::memory_order_release);
            }

            [[nodiscard]] bool end_of_stream() const noexcept {
                return mEndOfStream.load(std::memory_order_acquire);
            }
    };
}
