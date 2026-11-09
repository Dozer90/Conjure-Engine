#pragma once

#include "conjure/audio/clip.h"

#include "conjure/core/types.h"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

// In-test clip data (docs/design/12_audio.md 10): canonical 16-bit PCM or 32-bit float WAV files
// (RIFF + fmt + data) and ready-made pcm_buffers built from a sine, square or ramp, so no binary
// asset is needed.

namespace conjure::testing {

    enum class wave_shape : u8 {
        SINE,
        SQUARE,
        RAMP,       // -amplitude .. +amplitude over the whole clip
        SILENCE
    };

    enum class wav_sample_format : u8 {
        PCM16,
        FLOAT32
    };

    struct wav_spec {
        u32 sampleRate = 48000;
        u16 channels = 1;
        wav_sample_format format = wav_sample_format::FLOAT32;
        wave_shape shape = wave_shape::SINE;
        f64 frequency = 440.0;
        f32 amplitude = 0.5f;
        u64 frameCount = 48000;
    };

    /// <summary>
    /// Interleaved f32 samples for the spec; every channel carries the same signal.
    /// </summary>
    [[nodiscard]] std::vector<f32> generate_samples(const wav_spec &spec);

    /// <summary>
    /// A WAV file holding generate_samples(spec) in the spec's sample format.
    /// </summary>
    [[nodiscard]] std::vector<byte> build_wav(const wav_spec &spec);

    /// <summary>
    /// A WAV file around caller-provided interleaved samples.
    /// </summary>
    [[nodiscard]] std::vector<byte> build_wav_from_samples(
      std::span<const f32> samples,
      u32 sampleRate,
      u16 channels,
      wav_sample_format format
    );

    /// <summary>
    /// A pcm_buffer holding generate_samples(spec) directly (no WAV round trip).
    /// </summary>
    [[nodiscard]] std::shared_ptr<const audio::pcm_buffer> make_pcm(const wav_spec &spec);

    /// <summary>
    /// Shorthand for a sine pcm_buffer.
    /// </summary>
    [[nodiscard]] std::shared_ptr<const audio::pcm_buffer> make_sine_pcm(
      f64 frequency,
      f64 seconds,
      f32 amplitude = 0.5f,
      u16 channels = 1,
      u32 sampleRate = 48000
    );

    /// <summary>
    /// Largest absolute sample value in a buffer (0 for an empty span).
    /// </summary>
    [[nodiscard]] f32 peak_of(std::span<const f32> samples) noexcept;

    /// <summary>
    /// Largest absolute difference between consecutive samples of one channel.
    /// </summary>
    [[nodiscard]] f32 largest_step(std::span<const f32> interleaved, u16 channels, u16 channel) noexcept;
}
