#pragma once

#include "conjure/audio/audio_backend.h"
#include "conjure/audio/audio_device.h"
#include "conjure/audio/audio_types.h"
#include "conjure/audio/mix_groups.h"
#include "conjure/audio/spatialisation.h"

#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <array>
#include <atomic>
#include <span>
#include <vector>

// The headless backend (docs/design/12_audio.md 4.3, 7): no device, no thread, no OS calls. Its
// update(dt) advances a virtual clock and calls the real mixer for round(dt * sampleRate) frames on
// the main thread (carrying the fractional remainder), so cursors, loops, ramps and group gains
// behave exactly as on hardware. The engine writes a snapshot of its voices, groups and listener
// into stats() every update, which is what headless tests assert on.

namespace conjure::audio {

    class audio_engine;

    namespace detail {
        struct audio_engine_helpers;
    }

    /// <summary>
    /// Snapshot of one mixer slot.
    /// </summary>
    struct null_voice_stats {
        voice_state state = voice_state::FREE;
        u64 cursorFrames = 0;
        f32 gain = 0.0f;
        f32 pan = 0.0f;
        f64 step = 0.0;
        bool loop = false;
        u8 groupIndex = 0;
    };

    /// <summary>
    /// Snapshot of one group.
    /// </summary>
    struct null_group_stats {
        group_id id = {};
        core::string_id name = {};
        f32 gain = 1.0f;
        f32 effectiveGain = 1.0f;
        bool paused = false;
    };

    /// <summary>
    /// Everything the null backend and the engine publish for tests and the profiler.
    /// </summary>
    struct null_backend_stats {
        f64 virtualTimeSeconds = 0.0;
        u64 framesRendered = 0;
        u32 renderCalls = 0;
        u32 activeVoices = 0;
        u32 underruns = 0;
        u32 commandsApplied = 0;
        listener_state listener = {};
        std::array<null_voice_stats, MAX_VOICES> voices = {};
        std::vector<null_group_stats> groups = {};
    };

    class null_backend final : public audio_backend {
        private:
            friend class audio_engine;                     // owns the backend
            friend struct detail::audio_engine_helpers;   // writes the voice / group / listener snapshot

            backend_desc mDesc = {};
            std::atomic<device_state> mState = device_state::STOPPED;
            f64 mFractionalFrames = 0.0;
            u32 mFailingStarts = 0;              // start() calls left to fail (fail_next_starts)
            u32 mCaptureFrames = 0;
            std::vector<f32> mCaptured = {};     // the last mCaptureFrames rendered stereo frames
            std::vector<f32> mRenderBuffer = {}; // MIX_BLOCK_FRAMES * MIX_CHANNELS, allocated once
            null_backend_stats mStats = {};

        public:
            null_backend();
            ~null_backend() override;

        public:
            [[nodiscard]] audio_backend_kind kind() const noexcept override;

            /// <summary>
            /// Resets the virtual clock. Fails with INVALID_SETTINGS only when desc.callback is null
            /// or desc.format.sampleRate is outside [MIN_SAMPLE_RATE, MAX_SAMPLE_RATE], and with
            /// DEVICE_START_FAILED while a fail_next_starts() budget remains.
            /// </summary>
            [[nodiscard]] core::result<void> start(const backend_desc &desc) override;
            void stop() noexcept override;
            [[nodiscard]] device_state state() const noexcept override;
            [[nodiscard]] audio_format output_format() const noexcept override;

            /// <summary>
            /// One device, "Null Output", 48000 Hz stereo, default.
            /// </summary>
            [[nodiscard]] core::result<std::vector<audio_device_info>> enumerate_devices() const override;

            /// <summary>
            /// Renders round(deltaSeconds * sampleRate) frames (fraction carried to the next call)
            /// through the callback in MIX_BLOCK_FRAMES blocks. Nothing happens while stopped or for
            /// a non-positive delta.
            /// </summary>
            void update(f32 deltaSeconds) override;

            /// <summary>
            /// Renders exactly 'frameCount' frames, ignoring the clock. Tests use it to step by
            /// block.
            /// </summary>
            void render_frames(u32 frameCount);

            /// <summary>
            /// The snapshot written by the last update().
            /// </summary>
            [[nodiscard]] const null_backend_stats &stats() const noexcept {
                return mStats;
            }

            /// <summary>
            /// Tests: keep the last 'frames' rendered stereo f32 frames for inspection (0 disables).
            /// </summary>
            void set_capture_frames(u32 frames);

            /// <summary>
            /// Tests: publishes device_state::LOST, as the miniaudio notification callback does when
            /// the OS stops a device. Nothing renders until the next successful start(), which is
            /// how the engine's recovery path (docs/design/12_audio.md 3.8) is exercised headlessly.
            /// </summary>
            void simulate_device_loss() noexcept;

            /// <summary>
            /// Tests: the next 'count' start() calls fail with DEVICE_START_FAILED and leave the
            /// backend STOPPED, like a device that cannot be reopened after a loss.
            /// </summary>
            void fail_next_starts(u32 count) noexcept;

            /// <summary>
            /// The captured frames, oldest first, interleaved stereo.
            /// </summary>
            [[nodiscard]] std::span<const f32> captured() const noexcept {
                return mCaptured;
            }

            /// <summary>
            /// Seconds of audio rendered since start().
            /// </summary>
            [[nodiscard]] f64 virtual_time() const noexcept {
                return mStats.virtualTimeSeconds;
            }

        private:
            [[nodiscard]] null_backend_stats &mutable_stats() noexcept {
                return mStats;
            }
    };
}
