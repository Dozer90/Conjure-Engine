#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

// The fixed-step accumulator (docs/design/06_engine_loop.md 2.2, D3): a pure value type without a clock.
//
//   fixed_step_clock clock(1.0f / 60.0f, 8);
//   const fixed_step_result steps = clock.advance(frameDelta);
//   for (u32 step = 0; step < steps.stepCount; ++step) { ... }

namespace conjure::engine {

    struct fixed_step_result {
        u32 stepCount = 0;
        f32 interpolationAlpha = 0.0f;
        bool droppedTime = false;

        [[nodiscard]] bool operator==(const fixed_step_result &) const = default;
    };

    class CONJURE_API fixed_step_clock {
        private:
            f64 mAccumulatorSeconds = 0.0;
            f32 mFixedDeltaSeconds = 1.0f / 60.0f;
            u32 mMaxStepsPerFrame = 8;

        public:
            fixed_step_clock();
            fixed_step_clock(f32 fixedDeltaSeconds, u32 maxStepsPerFrame);
            ~fixed_step_clock();

        public:
            /// <summary>
            /// Adds a scaled frame delta and returns how many fixed steps to run this frame plus the alpha
            /// to use after them. When the step cap is hit the leftover is discarded so a slow frame cannot
            /// snowball (acc += delta; steps = floor(acc / dt); if steps &gt; max { steps = max; acc = 0;
            /// dropped } else { acc -= steps * dt }; alpha = acc / dt). Negative or non-finite deltas add
            /// nothing.
            /// </summary>
            [[nodiscard]] fixed_step_result advance(f32 scaledDeltaSeconds);

            /// <summary>
            /// Zeroes the accumulator; the fixed delta and step cap are kept.
            /// </summary>
            void reset() noexcept;

            /// <summary>
            /// Changes the fixed delta (must be &gt; 0; asserted and ignored otherwise). The accumulator is
            /// kept, so time is conserved across a rate change.
            /// </summary>
            void set_fixed_delta(f32 fixedDeltaSeconds);

            /// <summary>
            /// Changes the step cap (must be &gt; 0; asserted and ignored otherwise).
            /// </summary>
            void set_max_steps_per_frame(u32 maxStepsPerFrame);

            [[nodiscard]] f32 fixed_delta() const noexcept {
                return mFixedDeltaSeconds;
            }

            [[nodiscard]] u32 max_steps_per_frame() const noexcept {
                return mMaxStepsPerFrame;
            }

            /// <summary>
            /// Seconds accumulated and not yet consumed by a step.
            /// </summary>
            [[nodiscard]] f64 accumulator() const noexcept {
                return mAccumulatorSeconds;
            }
    };
}
