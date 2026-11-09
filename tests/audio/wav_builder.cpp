#include "wav_builder.h"

#include "conjure/core/math.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace conjure;
using namespace conjure::testing;

namespace {
    constexpr f64 TWO_PI = 2.0 * static_cast<f64>(math::PI);

    void append_u32(std::vector<byte> &out, u32 value) {
        for (u32 shift = 0; shift < 32; shift += 8) {
            out.push_back(static_cast<byte>((value >> shift) & 0xFFu));
        }
    }

    void append_u16(std::vector<byte> &out, u16 value) {
        out.push_back(static_cast<byte>(value & 0xFFu));
        out.push_back(static_cast<byte>((value >> 8) & 0xFFu));
    }

    void append_tag(std::vector<byte> &out, const char *tag) {
        for (usize index = 0; index < 4; ++index) {
            out.push_back(static_cast<byte>(tag[index]));
        }
    }
}

std::vector<f32> conjure::testing::generate_samples(const wav_spec &spec) {
    std::vector<f32> samples(static_cast<usize>(spec.frameCount) * spec.channels, 0.0f);
    const f64 rate = static_cast<f64>(spec.sampleRate);
    for (u64 frame = 0; frame < spec.frameCount; ++frame) {
        f32 value = 0.0f;
        const f64 time = static_cast<f64>(frame) / rate;
        switch (spec.shape) {
            case wave_shape::SINE:
                value = spec.amplitude * static_cast<f32>(std::sin(TWO_PI * spec.frequency * time));
                break;
            case wave_shape::SQUARE:
                value = std::sin(2.0 * static_cast<f64>(math::PI) * spec.frequency * time) >= 0.0
                  ? spec.amplitude
                  : -spec.amplitude;
                break;
            case wave_shape::RAMP: {
                const f64 lastFrame = static_cast<f64>(spec.frameCount > 1 ? spec.frameCount - 1 : 1);
                const f64 progress = spec.frameCount > 1 ? static_cast<f64>(frame) / lastFrame : 0.0;
                value = spec.amplitude * static_cast<f32>(progress * 2.0 - 1.0);
                break;
            }
            case wave_shape::SILENCE:
                value = 0.0f;
                break;
        }
        for (u16 channel = 0; channel < spec.channels; ++channel) {
            samples[static_cast<usize>(frame) * spec.channels + channel] = value;
        }
    }
    return samples;
}

std::vector<byte> conjure::testing::build_wav(const wav_spec &spec) {
    return build_wav_from_samples(generate_samples(spec), spec.sampleRate, spec.channels, spec.format);
}

std::vector<byte> conjure::testing::build_wav_from_samples(
  std::span<const f32> samples,
  u32 sampleRate,
  u16 channels,
  wav_sample_format format
) {
    const u16 bitsPerSample = format == wav_sample_format::PCM16 ? 16 : 32;
    const u16 blockAlign = static_cast<u16>(channels * (bitsPerSample / 8));
    const u32 dataBytes = static_cast<u32>(samples.size() * (bitsPerSample / 8));
    const u16 formatTag = format == wav_sample_format::PCM16 ? 1 : 3;

    std::vector<byte> out = {};
    out.reserve(44 + dataBytes);
    append_tag(out, "RIFF");
    append_u32(out, 36 + dataBytes);
    append_tag(out, "WAVE");
    append_tag(out, "fmt ");
    append_u32(out, 16);
    append_u16(out, formatTag);
    append_u16(out, channels);
    append_u32(out, sampleRate);
    append_u32(out, sampleRate * blockAlign);
    append_u16(out, blockAlign);
    append_u16(out, bitsPerSample);
    append_tag(out, "data");
    append_u32(out, dataBytes);

    for (const f32 sample : samples) {
        if (format == wav_sample_format::PCM16) {
            const f32 clamped = std::clamp(sample, -1.0f, 1.0f);
            const i16 quantised = static_cast<i16>(std::lround(clamped * 32767.0f));
            append_u16(out, static_cast<u16>(quantised));
        } else {
            u32 bits = 0;
            std::memcpy(&bits, &sample, sizeof(bits));
            append_u32(out, bits);
        }
    }
    return out;
}

std::shared_ptr<const audio::pcm_buffer> conjure::testing::make_pcm(const wav_spec &spec) {
    audio::audio_format format = {};
    format.sampleRate = spec.sampleRate;
    format.channels = spec.channels;
    return std::make_shared<const audio::pcm_buffer>(format, generate_samples(spec));
}

std::shared_ptr<const audio::pcm_buffer> conjure::testing::make_sine_pcm(
  f64 frequency,
  f64 seconds,
  f32 amplitude,
  u16 channels,
  u32 sampleRate
) {
    wav_spec spec = {};
    spec.sampleRate = sampleRate;
    spec.channels = channels;
    spec.frequency = frequency;
    spec.amplitude = amplitude;
    spec.frameCount = static_cast<u64>(std::llround(seconds * static_cast<f64>(sampleRate)));
    return make_pcm(spec);
}

f32 conjure::testing::peak_of(std::span<const f32> samples) noexcept {
    f32 peak = 0.0f;
    for (const f32 sample : samples) {
        peak = std::max(peak, std::abs(sample));
    }
    return peak;
}

f32 conjure::testing::largest_step(std::span<const f32> interleaved, u16 channels, u16 channel) noexcept {
    f32 largest = 0.0f;
    const usize frames = channels == 0 ? 0 : interleaved.size() / channels;
    for (usize frame = 1; frame < frames; ++frame) {
        const f32 previous = interleaved[(frame - 1) * channels + channel];
        const f32 current = interleaved[frame * channels + channel];
        largest = std::max(largest, std::abs(current - previous));
    }
    return largest;
}
