#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"
#include "conjure/ecs/system_phase.h"

#include <array>
#include <span>

// Frame statistics (docs/design/06_engine_loop.md 2.7): one frame_sample per frame committed at step 15
// into a ring of HISTORY_SIZE samples. The editor profiler panel and diagnostics_frame_stats (R51) read
// it through engine_context::stats; written only by the main thread between phases.

namespace conjure::engine {

    struct frame_sample {
        f32 frameMilliseconds = 0.0f;
        std::array<f32, ecs::SYSTEM_PHASE_COUNT> phaseMilliseconds = {};
        f32 submitMilliseconds = 0.0f;
        f32 pacerSleepMilliseconds = 0.0f;
        u32 fixedSteps = 0;
        u32 jobsExecuted = 0;
        usize frameMemoryHighWater = 0;
        bool droppedFixedTime = false;

        [[nodiscard]] bool operator==(const frame_sample &) const = default;
    };

    class CONJURE_API frame_stats {
        public:
            /// <summary>
            /// Samples kept (four seconds at 60 Hz).
            /// </summary>
            static constexpr usize HISTORY_SIZE = 240;

        private:
            std::array<frame_sample, HISTORY_SIZE> mSamples = {};
            usize mWriteIndex = 0;              // slot the next commit fills
            usize mCount = 0;                   // samples committed, saturating at HISTORY_SIZE
            u64 mTotalCommitted = 0;
            u64 mTotalFramesWithDroppedTime = 0;

        public:
            frame_stats();
            ~frame_stats();

            frame_stats(const frame_stats &) = default;
            frame_stats &operator=(const frame_stats &) = default;

        public:
            /// <summary>
            /// Stores a sample, overwriting the oldest once the ring is full.
            /// </summary>
            void commit(const frame_sample &sample);

            /// <summary>
            /// The last committed sample (a default sample before the first commit).
            /// </summary>
            [[nodiscard]] const frame_sample &latest() const noexcept;

            /// <summary>
            /// The ring in storage order: HISTORY_SIZE slots once full, the committed prefix before that.
            /// Use sample_at() for chronological access.
            /// </summary>
            [[nodiscard]] std::span<const frame_sample> history() const noexcept;

            /// <summary>
            /// The sample 'age' frames before the latest (0 = latest); a default sample beyond the history.
            /// </summary>
            [[nodiscard]] const frame_sample &sample_at(usize age) const noexcept;

            /// <summary>
            /// Mean frameMilliseconds over the latest min(frameCount, sample_count()) samples; 0 when empty.
            /// </summary>
            [[nodiscard]] f32 average_frame_milliseconds(usize frameCount) const noexcept;

            /// <summary>
            /// Samples currently in the ring (at most HISTORY_SIZE).
            /// </summary>
            [[nodiscard]] usize sample_count() const noexcept {
                return mCount;
            }

            /// <summary>
            /// Samples committed since construction.
            /// </summary>
            [[nodiscard]] u64 total_committed() const noexcept {
                return mTotalCommitted;
            }

            /// <summary>
            /// Frames in which the spiral-of-death guard dropped fixed time, since construction (one per frame,
            /// however much time that frame dropped).
            /// </summary>
            [[nodiscard]] u64 total_frames_with_dropped_time() const noexcept {
                return mTotalFramesWithDroppedTime;
            }

            /// <summary>
            /// Forgets every sample.
            /// </summary>
            void clear() noexcept;
    };
}
