#pragma once

#include "conjure/engine/engine_config.h"

#include "conjure/core/clock.h"
#include "conjure/core/types.h"

// Frame pacing (docs/design/06_engine_loop.md 3.3): active for a frame iff no forced frame delta, no vsync
// present happened and the effective target rate is non-zero. The effective target is pacing.targetHz when
// non-zero, else 1 / fixedDeltaSeconds when headless (servers tick at simulation rate), else unpaced;
// sleepWhenUnfocused on an unfocused window forces 30 Hz. The sleep is the clock's sleep_until (the steady
// clock sleeps until 2 ms before the deadline and spins the rest; a manual clock jumps), never when behind.

namespace conjure::engine {

    class frame_pacer {
        public:
            /// <summary>
            /// Rate forced while the window is unfocused and sleepWhenUnfocused is set.
            /// </summary>
            static constexpr u32 UNFOCUSED_HZ = 30;

        private:
            core::clock *mClock = nullptr;
            pacing_settings mPacing = {};
            f32 mFixedDeltaSeconds = 1.0f / 60.0f;
            bool mHeadless = false;
            bool mForcedDelta = false;
            bool mUnfocused = false;
            f64 mFrameStartSeconds = 0.0;
            f64 mLastSleepSeconds = 0.0;
            u64 mSleepCount = 0;

        public:
            frame_pacer(core::clock &clock, const engine_config &config);
            ~frame_pacer();

            frame_pacer(const frame_pacer &) = delete;
            frame_pacer &operator=(const frame_pacer &) = delete;

        public:
            /// <summary>
            /// Marks the start of a frame (reads the clock).
            /// </summary>
            void begin_frame();

            /// <summary>
            /// Sleeps until the frame's deadline when pacing is active and the frame finished early. Returns
            /// the seconds slept (0 when inactive or behind).
            /// </summary>
            f64 end_frame(bool presentHappened);

            void set_unfocused(bool unfocused) noexcept {
                mUnfocused = unfocused;
            }

            void set_target_hz(u32 targetHz) noexcept {
                mPacing.targetHz = targetHz;
            }

            /// <summary>
            /// The rate pacing aims at right now (0 = unpaced): the forced delta, the unfocused rule and the
            /// headless default applied.
            /// </summary>
            [[nodiscard]] u32 effective_target_hz() const noexcept;

            /// <summary>
            /// True when end_frame(false) would consider sleeping.
            /// </summary>
            [[nodiscard]] bool is_active() const noexcept {
                return !mForcedDelta && effective_target_hz() != 0;
            }

            [[nodiscard]] f64 last_sleep_seconds() const noexcept {
                return mLastSleepSeconds;
            }

            [[nodiscard]] u64 sleep_count() const noexcept {
                return mSleepCount;
            }

            [[nodiscard]] bool is_unfocused() const noexcept {
                return mUnfocused;
            }
    };
}
