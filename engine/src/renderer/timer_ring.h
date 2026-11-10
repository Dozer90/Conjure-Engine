#pragma once

#include "conjure/renderer/renderer.h"

#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

#include <array>
#include <vector>

// GPU timer queries per pass (docs/design/02_renderer.md 3.1 step 1, 4.2 render_stats::passTimings).
// A ring of three frames: the queries of frame N are resolved when frame N + 3 begins, so the GPU
// never blocks the CPU. Enabled only in Development and Debug builds on devices that report timer
// queries (never on the null device, where getTimerQueryTime returns 0 anyway).

namespace conjure::renderer {

    class timer_ring {
        private:
            struct frame_queries {
                std::vector<nvrhi::TimerQueryHandle> queries = {};
                std::vector<const char *> names = {};
                usize used = 0;
                bool pending = false;
            };

            static constexpr usize FRAME_COUNT = 3;
            static constexpr usize MAX_QUERIES_PER_FRAME = 48;

            nvrhi::IDevice &mDevice;    // set by the only constructor; outlives the ring
            std::array<frame_queries, FRAME_COUNT> mFrames = {};
            usize mCurrent = 0;
            bool mOpen = false;
            core::small_vector<pass_timing, 48> mResolved = {};

        public:
            explicit timer_ring(nvrhi::IDevice &device);
            ~timer_ring();

            timer_ring(const timer_ring &) = delete;
            timer_ring &operator=(const timer_ring &) = delete;

        public:
            /// <summary>
            /// Resolves the oldest frame's queries into resolved() and makes its slot current.
            /// </summary>
            void begin_frame();

            /// <summary>
            /// Starts a timer for the pass; ignored past MAX_QUERIES_PER_FRAME.
            /// </summary>
            void begin(nvrhi::ICommandList &commandList, const char *name);

            void end(nvrhi::ICommandList &commandList);

            /// <summary>
            /// The timings of the frame resolved by the last begin_frame.
            /// </summary>
            [[nodiscard]] std::span<const pass_timing> resolved() const noexcept {
                return mResolved;
            }
    };
}
