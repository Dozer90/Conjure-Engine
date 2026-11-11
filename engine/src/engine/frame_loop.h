#pragma once

#include "engine/engine_services.h"

#include "conjure/engine/application.h"
#include "conjure/engine/frame_stats.h"

#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/system_phase.h"

#include <optional>
#include <string>

// The frame sequence of docs/design/06_engine_loop.md 3.1, steps 1 to 16, run once per
// application::step_frame(). A friend of application: it owns no state the application does not, only the
// bookkeeping of the failure rules in section 6 (render failures, the one resize retry, the rate-limited
// spiral-of-death warning) and the per-phase timing that becomes the frame_sample.

namespace conjure::engine {

    class frame_loop {
        public:
            static constexpr u32 MAX_CONSECUTIVE_RENDER_FAILURES = 3;
            static constexpr f64 DROPPED_TIME_WARNING_INTERVAL_SECONDS = 1.0;

        private:
            application *mApp = nullptr;
            engine_services *mServices = nullptr;
            frame_sample mSample = {};
            std::string mLastRenderError = {};
            std::optional<core::error> mInjectedRenderFailure = std::nullopt;   // replaces render_frame's result
            u32 mInjectedRenderFailureFrames = 0;
            std::optional<math::uvec2> mPendingResize = std::nullopt;   // a resize that failed once
            f64 mLastFrameClockSeconds = 0.0;
            f64 mLastDroppedWarningSeconds = -1.0e9;
            u64 mJobsExecutedAtFrameStart = 0;
            u32 mConsecutiveRenderFailures = 0;
            u32 mResizeFailures = 0;
            bool mHasPreviousFrame = false;

        public:
            frame_loop(application &app, engine_services &services);
            ~frame_loop();

            frame_loop(const frame_loop &) = delete;
            frame_loop &operator=(const frame_loop &) = delete;

        public:
            /// <summary>
            /// Runs one frame. 'deltaOverride' replaces the clock and the forced delta for this frame
            /// (application::step_frame(f64), R13). Returns an error only for unrecoverable failures.
            /// </summary>
            [[nodiscard]] core::result<void> run_frame(std::optional<f64> deltaOverride);

            /// <summary>
            /// Forgets the previous frame's clock reading so the next frame measures from now.
            /// </summary>
            void reset_clock() noexcept;

            /// <summary>
            /// The sample of the last frame (what was committed to frame_stats).
            /// </summary>
            [[nodiscard]] const frame_sample &last_sample() const noexcept {
                return mSample;
            }

            [[nodiscard]] u32 consecutive_render_failures() const noexcept {
                return mConsecutiveRenderFailures;
            }

            /// <summary>
            /// The next frameCount render_frame results are replaced by 'error' (application::inject_render_failure).
            /// </summary>
            void inject_render_failure(core::error error, u32 frameCount);

        private:
            [[nodiscard]] f64 measure_raw_delta(std::optional<f64> deltaOverride);
            void pump_events();
            void handle_window_event(const platform::event &event);
            void run_phase(ecs::system_phase phase);
            void run_phase_filtered(ecs::system_phase phase, ecs::system_flags required);
            void set_phase_in_flight(bool inFlight) noexcept;
            void run_fixed_steps(u32 stepCount);
            [[nodiscard]] bool render();
            void handle_render_failure(const core::error &error);
            void apply_pending_resize();
            void apply_resize(math::uvec2 size);
    };
}
