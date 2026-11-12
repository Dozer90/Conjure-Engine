#include "conjure/audio/decoder.h"
#include "conjure/audio/audio_error.h"
#include "conjure/audio/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/error.h"

#include <miniaudio.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

// The decoder implementation: the first of the two translation units of the module that include
// miniaudio.h (the other is miniaudio_backend.cpp). Decoding is identical on every backend.

using namespace conjure;
using namespace conjure::audio;

namespace {
    constexpr usize READ_CHUNK_FRAMES = 4096;

    // Upper bound on how many f32 samples one encoded byte can plausibly turn into: 8-bit PCM gives
    // one, 4-bit ADPCM two, Vorbis at its lowest quality around eleven. Only a reserve hint and a
    // sanity check depend on it; decoding itself reads until the container ends.
    constexpr u64 MAX_SAMPLES_PER_ENCODED_BYTE = 8;
    constexpr u64 MAX_WAV_SAMPLES_PER_BYTE = 2;

    [[nodiscard]] bool starts_with(
      std::span<const byte> bytes,
      const char *magic,
      usize length,
      usize offset = 0
    ) noexcept {
        if (bytes.size() < offset + length) {
            return false;
        }
        return std::memcmp(bytes.data() + offset, magic, length) == 0;
    }

    [[nodiscard]] ma_encoding_format to_miniaudio(clip_encoding encoding) noexcept {
        switch (encoding) {
            case clip_encoding::WAV:
                return ma_encoding_format_wav;
            case clip_encoding::FLAC:
                return ma_encoding_format_flac;
            case clip_encoding::MP3:
                return ma_encoding_format_mp3;
            case clip_encoding::OGG_VORBIS:
                return ma_encoding_format_vorbis;
            case clip_encoding::UNKNOWN:
                break;
        }
        return ma_encoding_format_unknown;
    }

    [[nodiscard]] core::error decode_error(const char *what, ma_result code) {
        return core::make_error(audio_error::DECODE_FAILED, "{}: {}", what, ma_result_description(code));
    }

    /// <summary>
    /// Opens a miniaudio decoder over memory at the native channel count and rate, output f32.
    /// </summary>
    [[nodiscard]] core::result<clip_encoding> open_decoder(std::span<const byte> encoded, ma_decoder &decoder) {
        if (encoded.empty()) {
            return core::make_error(audio_error::INVALID_CLIP, "empty clip data");
        }
        const clip_encoding encoding = sniff_encoding(encoded);
        if (encoding == clip_encoding::UNKNOWN) {
            return core::make_error(
              audio_error::UNSUPPORTED_FORMAT,
              "unrecognised audio container (not WAV, Ogg, MP3 or FLAC)"
            );
        }

        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
        config.encodingFormat = to_miniaudio(encoding);
        const ma_result opened = ma_decoder_init_memory(encoded.data(), encoded.size(), &config, &decoder);
        if (opened != MA_SUCCESS) {
            // MA_NO_BACKEND here means the container's decoder rejected the bytes (the backend
            // itself is compiled in), so it is a decode failure of a recognised format.
            return core::make_error(
              audio_error::DECODE_FAILED,
              "{} decoder rejected the data: {}",
              to_string(encoding),
              ma_result_description(opened)
            );
        }
        return encoding;
    }

    /// <summary>
    /// Counts the frames of a stream whose container does not report its length (Vorbis in push
    /// mode) by decoding it once, then rewinds.
    /// </summary>
    [[nodiscard]] core::result<u64> count_frames(ma_decoder &decoder, u32 channels) {
        std::vector<f32> scratch(READ_CHUNK_FRAMES * channels, 0.0f);
        u64 total = 0;
        while (true) {
            ma_uint64 read = 0;
            const ma_result result = ma_decoder_read_pcm_frames(&decoder, scratch.data(), READ_CHUNK_FRAMES, &read);
            total += read;
            if (result == MA_AT_END || read == 0) {
                break;
            }
            if (result != MA_SUCCESS) {
                return decode_error("ma_decoder_read_pcm_frames", result);
            }
        }
        const ma_result rewound = ma_decoder_seek_to_pcm_frame(&decoder, 0);
        if (rewound != MA_SUCCESS) {
            return decode_error("ma_decoder_seek_to_pcm_frame", rewound);
        }
        return total;
    }

    /// <summary>
    /// Format and length of an open decoder. countUnknownLength decodes a stream whose container
    /// reports no length (Vorbis in push mode) once to count it; 'encoded' is the span the decoder
    /// reads so a declared length can be checked against the bytes that actually exist.
    /// </summary>
    [[nodiscard]] core::result<clip_info> info_of(
      ma_decoder &decoder,
      clip_encoding encoding,
      std::span<const byte> encoded,
      bool countUnknownLength
    ) {
        ma_format format = ma_format_unknown;
        ma_uint32 channels = 0;
        ma_uint32 sampleRate = 0;
        const ma_result formatResult =
          ma_decoder_get_data_format(&decoder, &format, &channels, &sampleRate, nullptr, 0);
        if (formatResult != MA_SUCCESS) {
            return decode_error("ma_decoder_get_data_format", formatResult);
        }
        if (channels == 0 || channels > 2) {
            return core::make_error(
              audio_error::UNSUPPORTED_FORMAT,
              "{} channels are not supported; clips are mono or stereo",
              channels
            );
        }
        if (sampleRate < MIN_SAMPLE_RATE || sampleRate > MAX_SAMPLE_RATE) {
            return core::make_error(
              audio_error::UNSUPPORTED_FORMAT,
              "sample rate {} is outside the supported range",
              sampleRate
            );
        }

        ma_uint64 length = 0;
        const ma_result lengthResult = ma_decoder_get_length_in_pcm_frames(&decoder, &length);
        if (lengthResult != MA_SUCCESS && lengthResult != MA_NOT_IMPLEMENTED) {
            return decode_error("ma_decoder_get_length_in_pcm_frames", lengthResult);
        }
        if (length == 0 && countUnknownLength) {
            CONJURE_TRY_ASSIGN(const u64 counted, count_frames(decoder, channels));
            length = counted;
        }

        // dr_wav takes the frame count from the data chunk header without comparing it to the file
        // size, so a forged or truncated header can declare gigabytes for a few bytes of data. Even
        // 4-bit ADPCM needs half a byte per sample, which bounds what a WAV of this size can hold.
        if (encoding == clip_encoding::WAV) {
            const u64 maxFrames = (static_cast<u64>(encoded.size()) * MAX_WAV_SAMPLES_PER_BYTE) / channels;
            if (length > maxFrames) {
                return core::make_error(
                  audio_error::DECODE_FAILED,
                  "WAV header declares {} frames but the file holds only {} bytes",
                  length,
                  encoded.size()
                );
            }
        }

        clip_info info = {};
        info.format.sampleRate = sampleRate;
        info.format.channels = static_cast<u16>(channels);
        info.format.format = sample_format::F32;
        info.frameCount = length;
        info.encoding = encoding;
        info.storage = clip_storage::STREAMED;
        return info;
    }

    /// <summary>
    /// Averages the channels of an interleaved stereo buffer into mono.
    /// </summary>
    [[nodiscard]] std::vector<f32> downmix_to_mono(std::span<const f32> stereo) {
        std::vector<f32> mono(stereo.size() / 2, 0.0f);
        for (usize frame = 0; frame < mono.size(); ++frame) {
            mono[frame] = 0.5f * (stereo[frame * 2] + stereo[frame * 2 + 1]);
        }
        return mono;
    }

    /// <summary>
    /// Resamples interleaved f32 frames with miniaudio's linear resampler.
    /// </summary>
    [[nodiscard]] core::result<std::vector<f32>> resample(
      std::span<const f32> input,
      u32 channels,
      u32 rateIn,
      u32 rateOut
    ) {
        ma_data_converter_config config =
          ma_data_converter_config_init(ma_format_f32, ma_format_f32, channels, channels, rateIn, rateOut);
        config.resampling.algorithm = ma_resample_algorithm_linear;
        ma_data_converter converter = {};
        const ma_result initialised = ma_data_converter_init(&config, nullptr, &converter);
        if (initialised != MA_SUCCESS) {
            return decode_error("ma_data_converter_init", initialised);
        }

        const ma_uint64 inputFrames = static_cast<ma_uint64>(input.size() / channels);
        ma_uint64 expectedOutput = 0;
        const ma_result expected =
          ma_data_converter_get_expected_output_frame_count(&converter, inputFrames, &expectedOutput);
        if (expected != MA_SUCCESS) {
            ma_data_converter_uninit(&converter, nullptr);
            return decode_error("ma_data_converter_get_expected_output_frame_count", expected);
        }

        // The converter may hold a few frames of latency; room for an extra chunk lets it flush.
        std::vector<f32> output(static_cast<usize>(expectedOutput + READ_CHUNK_FRAMES) * channels, 0.0f);
        ma_uint64 consumedTotal = 0;
        ma_uint64 producedTotal = 0;
        while (consumedTotal < inputFrames && producedTotal < expectedOutput) {
            ma_uint64 consumed = inputFrames - consumedTotal;
            ma_uint64 produced = expectedOutput - producedTotal;
            const ma_result processed = ma_data_converter_process_pcm_frames(
              &converter,
              input.data() + static_cast<usize>(consumedTotal) * channels,
              &consumed,
              output.data() + static_cast<usize>(producedTotal) * channels,
              &produced
            );
            if (processed != MA_SUCCESS) {
                ma_data_converter_uninit(&converter, nullptr);
                return decode_error("ma_data_converter_process_pcm_frames", processed);
            }
            if (consumed == 0 && produced == 0) {
                break;
            }
            consumedTotal += consumed;
            producedTotal += produced;
        }

        // Flush the converter's internal latency with silence so the tail is not lost.
        while (producedTotal < expectedOutput) {
            std::array<f32, READ_CHUNK_FRAMES> silence = {};
            ma_uint64 consumed = static_cast<ma_uint64>(silence.size() / channels);
            ma_uint64 produced = expectedOutput - producedTotal;
            const ma_result processed = ma_data_converter_process_pcm_frames(
              &converter,
              silence.data(),
              &consumed,
              output.data() + static_cast<usize>(producedTotal) * channels,
              &produced
            );
            if (processed != MA_SUCCESS || produced == 0) {
                break;
            }
            producedTotal += produced;
        }

        ma_data_converter_uninit(&converter, nullptr);
        output.resize(static_cast<usize>(producedTotal) * channels);
        return output;
    }

    void normalize_peak(std::span<f32> samples) noexcept {
        f32 peak = 0.0f;
        for (const f32 sample : samples) {
            peak = std::max(peak, std::abs(sample));
        }
        if (peak <= 0.0f || !std::isfinite(peak)) {
            return;
        }
        const f32 scale = 1.0f / peak;
        for (f32 &sample : samples) {
            sample *= scale;
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Free functions

clip_encoding conjure::audio::sniff_encoding(std::span<const byte> encoded) noexcept {
    if (starts_with(encoded, "RIFF", 4) && starts_with(encoded, "WAVE", 4, 8)) {
        return clip_encoding::WAV;
    }
    if (starts_with(encoded, "OggS", 4)) {
        return clip_encoding::OGG_VORBIS;
    }
    if (starts_with(encoded, "fLaC", 4)) {
        return clip_encoding::FLAC;
    }
    if (starts_with(encoded, "ID3", 3)) {
        return clip_encoding::MP3;
    }
    if (encoded.size() >= 2) {
        const u8 first = static_cast<u8>(encoded[0]);
        const u8 second = static_cast<u8>(encoded[1]);
        if (first == 0xFF && (second & 0xE0) == 0xE0) {
            return clip_encoding::MP3;
        }
    }
    return clip_encoding::UNKNOWN;
}

core::result<clip_info> conjure::audio::probe(std::span<const byte> encoded) {
    ma_decoder decoder = {};
    CONJURE_TRY_ASSIGN(const clip_encoding encoding, open_decoder(encoded, decoder));
    core::result<clip_info> info = info_of(decoder, encoding, encoded, true);
    ma_decoder_uninit(&decoder);
    return info;
}

core::result<pcm_buffer> conjure::audio::decode_to_pcm(std::span<const byte> encoded, const decode_options &options) {
    ma_decoder decoder = {};
    CONJURE_TRY_ASSIGN(const clip_encoding encoding, open_decoder(encoded, decoder));

    core::result<clip_info> probed = info_of(decoder, encoding, encoded, false);
    if (!probed.has_value()) {
        ma_decoder_uninit(&decoder);
        return std::move(probed).get_error();
    }
    const clip_info info = probed.value();
    const u32 channels = info.format.channels;

    // The declared length is only a reserve hint: a compressed container's header can claim any
    // count, so the reservation is capped by what the bytes could plausibly expand to and the
    // vector grows past it if the stream really is longer. Project files are untrusted input.
    const u64 plausibleFrames = static_cast<u64>(encoded.size()) * MAX_SAMPLES_PER_ENCODED_BYTE / channels;
    const u64 reservedFrames = std::min(info.frameCount, plausibleFrames) + READ_CHUNK_FRAMES;
    std::vector<f32> samples = {};
    samples.reserve(static_cast<usize>(reservedFrames) * channels);
    std::vector<f32> scratch(READ_CHUNK_FRAMES * channels, 0.0f);
    while (true) {
        ma_uint64 read = 0;
        const ma_result result = ma_decoder_read_pcm_frames(&decoder, scratch.data(), READ_CHUNK_FRAMES, &read);
        samples.insert(samples.end(), scratch.begin(), scratch.begin() + static_cast<isize>(read * channels));
        if (result == MA_AT_END || read == 0) {
            break;
        }
        if (result != MA_SUCCESS) {
            ma_decoder_uninit(&decoder);
            return decode_error("ma_decoder_read_pcm_frames", result);
        }
    }
    ma_decoder_uninit(&decoder);

    audio_format format = info.format;
    if (options.forceMono && channels == 2) {
        samples = downmix_to_mono(samples);
        format.channels = 1;
    }

    if (options.sampleRate != 0 && options.sampleRate != format.sampleRate) {
        if (options.sampleRate < MIN_SAMPLE_RATE || options.sampleRate > MAX_SAMPLE_RATE) {
            return core::make_error(
              audio_error::INVALID_SETTINGS,
              "target sample rate {} outside [{}, {}]",
              options.sampleRate,
              MIN_SAMPLE_RATE,
              MAX_SAMPLE_RATE
            );
        }
        CONJURE_TRY_ASSIGN(
          std::vector<f32> resampled,
          resample(samples, format.channels, format.sampleRate, options.sampleRate)
        );
        samples = std::move(resampled);
        format.sampleRate = options.sampleRate;
    }

    if (options.normalize) {
        normalize_peak(samples);
    }

    return pcm_buffer(format, std::move(samples));
}


/*===========================================================================*/
/*===========================================================================*/
// streaming_decoder

struct streaming_decoder::state {
    ma_decoder decoder = {};
};

streaming_decoder::streaming_decoder() {}

streaming_decoder::~streaming_decoder() {
    close();
}

streaming_decoder::streaming_decoder(streaming_decoder &&other) noexcept :
    mState(std::move(other.mState)),
    mInfo(other.mInfo),
    mPosition(other.mPosition) {
    other.mInfo = {};
    other.mPosition = 0;
}

streaming_decoder &streaming_decoder::operator=(streaming_decoder &&other) noexcept {
    if (this != &other) {
        close();
        mState = std::move(other.mState);
        mInfo = other.mInfo;
        mPosition = other.mPosition;
        other.mInfo = {};
        other.mPosition = 0;
    }
    return *this;
}

core::result<void> streaming_decoder::open(std::span<const byte> encoded) {
    return open(encoded, 0);
}

core::result<void> streaming_decoder::open(std::span<const byte> encoded, u64 knownFrameCount) {
    close();
    std::unique_ptr<state> opened = std::make_unique<state>();
    CONJURE_TRY_ASSIGN(const clip_encoding encoding, open_decoder(encoded, opened->decoder));

    // A caller that already knows the length (a cooked clip's header carries it) spares the full decode
    // that counting a Vorbis stream costs; play() runs this on the main thread (A16).
    core::result<clip_info> info = info_of(opened->decoder, encoding, encoded, knownFrameCount == 0);
    if (!info.has_value()) {
        ma_decoder_uninit(&opened->decoder);
        return std::move(info).get_error();
    }

    mInfo = info.value();
    if (mInfo.frameCount == 0) {
        mInfo.frameCount = knownFrameCount;
    }
    mPosition = 0;
    mState = std::move(opened);
    return {};
}

usize streaming_decoder::read(std::span<f32> out) noexcept {
    if (mState == nullptr || mInfo.format.channels == 0) {
        return 0;
    }
    const ma_uint64 wanted = static_cast<ma_uint64>(out.size() / mInfo.format.channels);
    if (wanted == 0) {
        return 0;
    }
    ma_uint64 read = 0;
    const ma_result result = ma_decoder_read_pcm_frames(&mState->decoder, out.data(), wanted, &read);
    if (result != MA_SUCCESS && result != MA_AT_END) {
        CONJURE_LOG_WARN(LOG_AUDIO, "streaming decoder read failed: {}", ma_result_description(result));
        return 0;
    }
    mPosition += read;
    return static_cast<usize>(read);
}

core::result<void> streaming_decoder::seek(u64 frame) noexcept {
    if (mState == nullptr) {
        return core::make_error(core::error_code::INVALID_STATE, "streaming decoder is closed");
    }
    const ma_result result = ma_decoder_seek_to_pcm_frame(&mState->decoder, frame);
    if (result != MA_SUCCESS) {
        return decode_error("ma_decoder_seek_to_pcm_frame", result);
    }
    mPosition = frame;
    return {};
}

void streaming_decoder::close() noexcept {
    if (mState == nullptr) {
        return;
    }
    ma_decoder_uninit(&mState->decoder);
    mState.reset();
    mInfo = {};
    mPosition = 0;
}
