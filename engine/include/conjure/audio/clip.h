#pragma once

#include "conjure/audio/audio_types.h"

#include "conjure/assets/asset_types.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

// Clips (docs/design/12_audio.md 2.2). A pcm_buffer is immutable once built and shared between the
// clip asset and every voice playing it through std::shared_ptr (A6), so a voice keeps playing the
// old samples after a hot reload swaps the clip. audio_clip is the payload of the
// conjure.audio.clip asset type declared at the end of this header.

namespace conjure::audio {

    /// <summary>
    /// Interleaved f32 frames. Immutable after construction; shared by the clip and its voices.
    /// </summary>
    class pcm_buffer {
        private:
            audio_format mFormat = {};
            u64 mFrameCount = 0;
            std::vector<f32> mSamples = {};   // frameCount * channels

        public:
            pcm_buffer();

            /// <summary>
            /// Takes ownership of interleaved samples. The frame count is samples.size() / channels;
            /// a sample count that is not a multiple of the channel count asserts and drops the
            /// trailing partial frame. A channel count of 0 asserts and is treated as 1.
            /// </summary>
            pcm_buffer(audio_format format, std::vector<f32> samples);
            ~pcm_buffer();

            pcm_buffer(const pcm_buffer &) = default;
            pcm_buffer &operator=(const pcm_buffer &) = default;
            pcm_buffer(pcm_buffer &&) noexcept = default;
            pcm_buffer &operator=(pcm_buffer &&) noexcept = default;

        public:
            /// <summary>
            /// Sample rate, channel count and sample format of the samples.
            /// </summary>
            [[nodiscard]] const audio_format &format() const noexcept {
                return mFormat;
            }

            /// <summary>
            /// Number of frames (one sample per channel each).
            /// </summary>
            [[nodiscard]] u64 frame_count() const noexcept {
                return mFrameCount;
            }

            /// <summary>
            /// Every sample, interleaved by channel.
            /// </summary>
            [[nodiscard]] std::span<const f32> samples() const noexcept {
                return mSamples;
            }

            /// <summary>
            /// frame_count() / sampleRate; 0 for an empty buffer or a zero rate.
            /// </summary>
            [[nodiscard]] f64 duration_seconds() const noexcept;
    };

    /// <summary>
    /// What is known about a clip without touching its samples.
    /// </summary>
    struct clip_info {
        audio_format format = {};
        u64 frameCount = 0;
        clip_encoding encoding = clip_encoding::UNKNOWN;
        clip_storage storage = clip_storage::PCM;

        /// <summary>
        /// frameCount / sampleRate; 0 for a zero rate.
        /// </summary>
        [[nodiscard]] f64 duration_seconds() const noexcept;

        [[nodiscard]] bool operator==(const clip_info &other) const noexcept = default;
    };

    /// <summary>
    /// The conjure.audio.clip payload: info plus one of two shared buffers (PCM samples, or the
    /// original encoded file for STREAMED storage).
    /// </summary>
    struct audio_clip {
        clip_info info = {};
        std::shared_ptr<const pcm_buffer> pcm = nullptr;                   // PCM storage.
        std::shared_ptr<const std::vector<byte>> encoded = nullptr;         // STREAMED storage (original file).

        /// <summary>
        /// True when the clip decodes while playing instead of holding PCM.
        /// </summary>
        [[nodiscard]] bool is_streamed() const noexcept {
            return info.storage == clip_storage::STREAMED;
        }

        /// <summary>
        /// Duration in seconds from info.
        /// </summary>
        [[nodiscard]] f64 duration_seconds() const noexcept {
            return info.duration_seconds();
        }

        /// <summary>
        /// True when the clip carries the buffer its storage kind needs and the info agrees with it.
        /// </summary>
        [[nodiscard]] bool is_valid() const noexcept;
    };

    /// <summary>
    /// Builds a clip around a PCM buffer, filling info from the buffer (encoding WAV, storage PCM).
    /// </summary>
    [[nodiscard]] audio_clip make_pcm_clip(
      std::shared_ptr<const pcm_buffer> pcm,
      clip_encoding encoding = clip_encoding::WAV
    );

    /// <summary>
    /// The silent fallback clip: 0.1 s of mono 48 kHz zeros (docs/design/05_assets.md D14).
    /// </summary>
    [[nodiscard]] audio_clip make_fallback_clip();

    /// <summary>
    /// Registered name of the clip asset type.
    /// </summary>
    inline constexpr std::string_view CLIP_ASSET_TYPE_NAME = "conjure.audio.clip";
}

CONJURE_DECLARE_ASSET_TYPE(conjure::audio::audio_clip, "conjure.audio.clip");
