#pragma once

#include "conjure/core/platform_detect.h"

// The frame statistics overlay (docs/design/06_engine_loop.md 2.8): compiled in Debug and Development
// only; the header is empty in Distribution so it still compiles alone. Every frame it formats the latest
// frame_sample (frame time, phase times, fixed steps, frame memory, dropped time) into text(); in editor
// builds with an ImGui frame open it also draws that text in a small window, and it logs the text at INFO
// every log_interval_frames() frames (0 disables) so a headless run still shows its timing.

#if !CONJURE_DISTRIBUTION

#include "conjure/engine/engine_context.h"
#include "conjure/engine/layer.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <string>
#include <string_view>

namespace conjure::engine {

    class CONJURE_API debug_overlay_layer final : public layer {
        public:
            /// <summary>
            /// Default logging interval (five seconds at 60 Hz).
            /// </summary>
            static constexpr u64 DEFAULT_LOG_INTERVAL_FRAMES = 300;

        private:
            std::string mText = {};
            u64 mLogIntervalFrames = DEFAULT_LOG_INTERVAL_FRAMES;
            u64 mFramesSeen = 0;
            bool mVisible = true;

        public:
            debug_overlay_layer();
            ~debug_overlay_layer() override;

        public:
            /// <summary>
            /// Draws the text in an ImGui window when a frame is open (editor builds) and the overlay is visible.
            /// </summary>
            void on_imgui(engine_context &context) override;

            /// <summary>
            /// Formats the latest committed sample and logs it when the interval elapsed.
            /// </summary>
            void on_frame_end(engine_context &context) override;

            /// <summary>
            /// The text of the last frame (empty before the first on_frame_end).
            /// </summary>
            [[nodiscard]] std::string_view text() const noexcept {
                return mText;
            }

            /// <summary>
            /// Frames between two INFO log lines; 0 never logs.
            /// </summary>
            void set_log_interval_frames(u64 frames) noexcept {
                mLogIntervalFrames = frames;
            }

            [[nodiscard]] u64 log_interval_frames() const noexcept {
                return mLogIntervalFrames;
            }

            /// <summary>
            /// Shows or hides the ImGui window; the text is formatted either way.
            /// </summary>
            void set_visible(bool visible) noexcept {
                mVisible = visible;
            }

            [[nodiscard]] bool is_visible() const noexcept {
                return mVisible;
            }

            /// <summary>
            /// Frames observed by on_frame_end.
            /// </summary>
            [[nodiscard]] u64 frames_seen() const noexcept {
                return mFramesSeen;
            }

            /// <summary>
            /// The text the overlay would show for a sample: the formatting without the layer.
            /// </summary>
            [[nodiscard]] static std::string format_sample(const engine_context &context);
    };
}

#endif
