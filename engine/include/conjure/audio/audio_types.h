#pragma once

#include "conjure/core/types.h"

#include <format>
#include <string_view>

// Formats, identifiers and constants shared by every audio header (docs/design/12_audio.md 2.1).
// The mixer renders interleaved f32 stereo at audio_settings::sampleRate; every other format is
// converted by the backend (miniaudio) or at import (decoder).

namespace conjure::audio {

    inline constexpr u32 MAX_VOICES = 256;
    inline constexpr u32 MAX_GROUPS = 32;
    inline constexpr u32 MAX_GROUP_DEPTH = 8;
    inline constexpr u32 MAX_STREAMING_VOICES = 32;
    inline constexpr u32 MIX_CHANNELS = 2;
    inline constexpr u32 MIX_BLOCK_FRAMES = 256;            // Ramp granularity inside the callback.
    inline constexpr u32 STREAM_RING_FRAMES = 16384;        // Per streaming voice, stereo f32 (128 KiB).
    inline constexpr u32 STREAM_CHUNK_FRAMES = 4096;        // One decode job fills this many frames.
    inline constexpr u32 COMMAND_RING_CAPACITY = 8192;      // Commands per ring; spill beyond.
    inline constexpr u32 EVENT_RING_CAPACITY = 4096;        // Events per ring; reconciled from atomics beyond.
    inline constexpr f32 DEFAULT_RAMP_SECONDS = 0.005f;     // Gain / stop ramps (click suppression).
    inline constexpr f32 STEAL_FADE_SECONDS = 0.002f;       // Fade applied to a stolen voice.
    inline constexpr f32 DEFAULT_SPEED_OF_SOUND = 343.0f;
    inline constexpr u32 MIN_SAMPLE_RATE = 8000;
    inline constexpr u32 MAX_SAMPLE_RATE = 192000;

    /// <summary>
    /// Sample format of pcm_buffer and of the mixer. F32 is the only one in v1.
    /// </summary>
    enum class sample_format : u8 {
        F32
    };

    /// <summary>
    /// How a clip keeps its samples: decoded at import (PCM) or decoded incrementally while
    /// playing from the compressed bytes kept in memory (STREAMED).
    /// </summary>
    enum class clip_storage : u8 {
        PCM,
        STREAMED
    };

    /// <summary>
    /// Container / codec of the source file a clip was imported from.
    /// </summary>
    enum class clip_encoding : u8 {
        UNKNOWN,
        WAV,
        OGG_VORBIS,
        MP3,
        FLAC
    };

    /// <summary>
    /// Which backend audio_engine talks to. AUTO picks MINIAUDIO when it is compiled in and falls
    /// back to NULL_BACKEND when no device can be opened.
    /// </summary>
    enum class audio_backend_kind : u8 {
        AUTO,
        MINIAUDIO,
        NULL_BACKEND
    };

    /// <summary>
    /// Device state published by a backend (atomic, readable from any thread).
    /// </summary>
    enum class device_state : u8 {
        STOPPED,
        STARTING,
        RUNNING,
        LOST
    };

    /// <summary>
    /// Lifecycle of a voice. PRIMING covers a streamed voice waiting for its first chunk and a
    /// voice waiting for a stolen mixer slot; FINISHED and STOPPED are terminal.
    /// </summary>
    enum class voice_state : u8 {
        FREE,
        PRIMING,
        PLAYING,
        PAUSED,
        STOPPING,
        FINISHED,
        STOPPED
    };

    /// <summary>
    /// Distance attenuation curve of a spatial voice (docs/design/12_audio.md 2.5).
    /// </summary>
    enum class attenuation_model : u8 {
        NONE,
        INVERSE,
        LINEAR,
        EXPONENTIAL
    };

    /// <summary>
    /// Sample rate, channel count and sample format of a buffer or of the mixer output.
    /// </summary>
    struct audio_format {
        u32 sampleRate = 48000;
        u16 channels = static_cast<u16>(MIX_CHANNELS);
        sample_format format = sample_format::F32;

        [[nodiscard]] bool operator==(const audio_format &other) const noexcept = default;
    };

    /// <summary>
    /// Enumerator names ("F32", "PCM", "PLAYING", ...); "UNKNOWN" for out-of-range values.
    /// </summary>
    [[nodiscard]] std::string_view to_string(sample_format value) noexcept;
    [[nodiscard]] std::string_view to_string(clip_storage value) noexcept;
    [[nodiscard]] std::string_view to_string(clip_encoding value) noexcept;
    [[nodiscard]] std::string_view to_string(audio_backend_kind value) noexcept;
    [[nodiscard]] std::string_view to_string(device_state value) noexcept;
    [[nodiscard]] std::string_view to_string(voice_state value) noexcept;
    [[nodiscard]] std::string_view to_string(attenuation_model value) noexcept;

    /// <summary>
    /// True for PRIMING, PLAYING and PAUSED: the states in which audio_engine::is_playing() holds.
    /// </summary>
    [[nodiscard]] constexpr bool is_active_state(voice_state state) noexcept {
        return state == voice_state::PRIMING || state == voice_state::PLAYING || state == voice_state::PAUSED;
    }

    /// <summary>
    /// True for FINISHED and STOPPED: the voice has been acknowledged as ended by the mixer.
    /// </summary>
    [[nodiscard]] constexpr bool is_terminal_state(voice_state state) noexcept {
        return state == voice_state::FINISHED || state == voice_state::STOPPED;
    }
}

#define CONJURE_AUDIO_ENUM_FORMATTER(enum_type)                                                             \
    template <>                                                                                             \
    struct std::formatter<enum_type> : std::formatter<std::string_view> {                                   \
        template <typename FormatContext>                                                                   \
        auto format(enum_type value, FormatContext &context) const {                                        \
            return std::formatter<std::string_view>::format(conjure::audio::to_string(value), context);     \
        }                                                                                                   \
    }

CONJURE_AUDIO_ENUM_FORMATTER(conjure::audio::sample_format);
CONJURE_AUDIO_ENUM_FORMATTER(conjure::audio::clip_storage);
CONJURE_AUDIO_ENUM_FORMATTER(conjure::audio::clip_encoding);
CONJURE_AUDIO_ENUM_FORMATTER(conjure::audio::audio_backend_kind);
CONJURE_AUDIO_ENUM_FORMATTER(conjure::audio::device_state);
CONJURE_AUDIO_ENUM_FORMATTER(conjure::audio::voice_state);
CONJURE_AUDIO_ENUM_FORMATTER(conjure::audio::attenuation_model);

#undef CONJURE_AUDIO_ENUM_FORMATTER
