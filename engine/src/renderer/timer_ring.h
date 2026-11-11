#pragma once

#include "conjure/renderer/renderer.h"

#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

#include <array>
#include <vector>

// GPU timer queries per pass (docs/design/02_renderer.md 3.1 step 1, 4.2 render_stats::passTimings).
// A ring of three frames: the queries of frame N are resolved when frame N + 3 begins, so the GPU
// never blocks the CPU. Enabled only in Development and Debug builds on devices that report timer
// queries (the null device reports them and returns 0, which keeps this path covered headless).
// Queries are created for all three slots together the first time a frame needs one more than any
// frame before it, so a steady pass count creates none after its first frame and an idle ring holds
// none: the device's timer-query budget (NVRHI's default is 256) is shared by every renderer on it.

namespace conjure::renderer {

    class timer_ring {
        public:
            static constexpr usize FRAME_COUNT = 3;
            static constexpr usize MAX_QUERIES_PER_FRAME = 48;    // passes timed per frame, the rest untimed

        private:
            struct frame_queries {
                std::vector<nvrhi::TimerQueryHandle> queries = {};
                std::vector<const char *> names = {};
                usize used = 0;
                bool pending = false;
            };

            nvrhi::IDevice &mDevice;    // set by the only constructor; outlives the ring
            std::array<frame_queries, FRAME_COUNT> mFrames = {};
            usize mCurrent = 0;
            bool mOpen = false;
            bool mDeviceRefused = false;    // createTimerQuery returned nullptr once; warned, no retry
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
            /// Starts a timer for the pass; ignored past MAX_QUERIES_PER_FRAME and for a query the
            /// device refused to create. Creates the pass's query in every slot when no frame before
            /// needed this many.
            /// </summary>
            void begin(nvrhi::ICommandList &commandList, const char *name);

            void end(nvrhi::ICommandList &commandList);

            /// <summary>
            /// Timer queries created so far over the three slots (for tests and the module README).
            /// </summary>
            [[nodiscard]] usize query_count() const noexcept;

            /// <summary>
            /// The timings of the frame resolved by the last begin_frame.
            /// </summary>
            [[nodiscard]] std::span<const pass_timing> resolved() const noexcept {
                return mResolved;
            }
    };
}
