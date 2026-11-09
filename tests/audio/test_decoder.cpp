#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_assets.h"
#include "conjure/testing/test_environment.h"

#include "wav_builder.h"

#include "conjure/audio/audio_error.h"
#include "conjure/audio/decoder.h"

#include "conjure/core/random.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::audio;

namespace {
    struct wav_case {
        u16 channels = 1;
        testing::wav_sample_format format = testing::wav_sample_format::FLOAT32;
        u32 sampleRate = 48000;
        u64 frameCount = 48000;
    };

    constexpr std::array<wav_case, 8> WAV_CASES = {{
        {1, testing::wav_sample_format::FLOAT32, 48000, 48000},
        {2, testing::wav_sample_format::FLOAT32, 44100, 4410},
        {1, testing::wav_sample_format::PCM16, 22050, 2205},
        {2, testing::wav_sample_format::PCM16, 48000, 48000},
        {1, testing::wav_sample_format::FLOAT32, 22050, 1},
        {2, testing::wav_sample_format::PCM16, 44100, 1},
        {1, testing::wav_sample_format::PCM16, 48000, 48000},
        {2, testing::wav_sample_format::FLOAT32, 48000, 1000},
    }};

    testing::wav_spec spec_for(const wav_case &item) {
        testing::wav_spec spec = {};
        spec.channels = item.channels;
        spec.format = item.format;
        spec.sampleRate = item.sampleRate;
        spec.frameCount = item.frameCount;
        return spec;
    }

    f32 max_difference(std::span<const f32> left, std::span<const f32> right) {
        f32 largest = 0.0f;
        const usize count = std::min(left.size(), right.size());
        for (usize index = 0; index < count; ++index) {
            largest = std::max(largest, std::abs(left[index] - right[index]));
        }
        return largest;
    }

    std::vector<byte> read_whole_file(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        std::vector<char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        std::vector<byte> result(bytes.size());
        for (usize index = 0; index < bytes.size(); ++index) {
            result[index] = static_cast<byte>(bytes[index]);
        }
        return result;
    }
}

TEST_SUITE("audio/decoder") {

    TEST_CASE("sniff_encoding - recognises the four containers and nothing else") {
        const std::vector<byte> wav = testing::build_wav(testing::wav_spec{});
        CHECK(sniff_encoding(wav) == clip_encoding::WAV);
        const std::array<byte, 8> ogg = {
            byte{'O'}, byte{'g'}, byte{'g'}, byte{'S'}, byte{0}, byte{2}, byte{0}, byte{0},
        };
        CHECK(sniff_encoding(ogg) == clip_encoding::OGG_VORBIS);
        const std::array<byte, 4> flac = {byte{'f'}, byte{'L'}, byte{'a'}, byte{'C'}};
        CHECK(sniff_encoding(flac) == clip_encoding::FLAC);
        const std::array<byte, 4> id3 = {byte{'I'}, byte{'D'}, byte{'3'}, byte{4}};
        CHECK(sniff_encoding(id3) == clip_encoding::MP3);
        const std::array<byte, 4> sync = {byte{0xFF}, byte{0xFB}, byte{0x90}, byte{0x00}};
        CHECK(sniff_encoding(sync) == clip_encoding::MP3);
        const std::array<byte, 4> text = {byte{'t'}, byte{'e'}, byte{'x'}, byte{'t'}};
        CHECK(sniff_encoding(text) == clip_encoding::UNKNOWN);
        CHECK(sniff_encoding({}) == clip_encoding::UNKNOWN);
        const std::array<byte, 3> short_riff = {byte{'R'}, byte{'I'}, byte{'F'}};
        CHECK(sniff_encoding(short_riff) == clip_encoding::UNKNOWN);
    }

    TEST_CASE("probe - reports format and length of generated WAV files") {
        for (usize index = 0; index < WAV_CASES.size(); ++index) {
            CAPTURE(index);
            const wav_case item = WAV_CASES[index];
            const std::vector<byte> wav = testing::build_wav(spec_for(item));
            const core::result<clip_info> info = probe(wav);
            CONJURE_REQUIRE_OK(info);
            CHECK(info.value().format.sampleRate == item.sampleRate);
            CHECK(info.value().format.channels == item.channels);
            CHECK(info.value().format.format == sample_format::F32);
            CHECK(info.value().frameCount == item.frameCount);
            CHECK(info.value().encoding == clip_encoding::WAV);
            CHECK(info.value().storage == clip_storage::STREAMED);
        }
    }

    TEST_CASE("decode_to_pcm - reproduces the sine within 1e-4 for 16-bit and 1e-6 for float") {
        for (usize index = 0; index < WAV_CASES.size(); ++index) {
            CAPTURE(index);
            const wav_case item = WAV_CASES[index];
            const testing::wav_spec spec = spec_for(item);
            const std::vector<f32> expected = testing::generate_samples(spec);
            const core::result<pcm_buffer> decoded = decode_to_pcm(testing::build_wav(spec), decode_options{});
            CONJURE_REQUIRE_OK(decoded);
            CHECK(decoded.value().frame_count() == item.frameCount);
            CHECK(decoded.value().format().channels == item.channels);
            CHECK(decoded.value().format().sampleRate == item.sampleRate);
            REQUIRE(decoded.value().samples().size() == expected.size());
            const f32 tolerance = item.format == testing::wav_sample_format::PCM16 ? 1.0e-4f : 1.0e-6f;
            CHECK(max_difference(decoded.value().samples(), expected) <= tolerance);
        }
    }

    TEST_CASE("decode_to_pcm - forceMono averages the channels") {
        testing::wav_spec spec = {};
        spec.channels = 2;
        spec.frameCount = 1000;
        std::vector<f32> samples = testing::generate_samples(spec);
        for (usize frame = 0; frame < spec.frameCount; ++frame) {
            samples[frame * 2] = 0.8f;      // left
            samples[frame * 2 + 1] = 0.2f;  // right
        }
        const std::vector<byte> wav =
          testing::build_wav_from_samples(samples, 48000, 2, testing::wav_sample_format::FLOAT32);
        decode_options options = {};
        options.forceMono = true;
        const core::result<pcm_buffer> decoded = decode_to_pcm(wav, options);
        CONJURE_REQUIRE_OK(decoded);
        CHECK(decoded.value().format().channels == 1);
        CHECK(decoded.value().frame_count() == 1000);
        CHECK(decoded.value().samples()[0] == doctest::Approx(0.5));
        CHECK(decoded.value().samples()[999] == doctest::Approx(0.5));

        testing::wav_spec mono = {};
        mono.frameCount = 100;
        const core::result<pcm_buffer> alreadyMono = decode_to_pcm(testing::build_wav(mono), options);
        CONJURE_REQUIRE_OK(alreadyMono);
        CHECK(alreadyMono.value().format().channels == 1);
    }

    TEST_CASE("decode_to_pcm - a target sample rate scales the frame count") {
        testing::wav_spec spec = {};
        spec.sampleRate = 48000;
        spec.frameCount = 48000;
        spec.frequency = 100.0;
        const std::vector<byte> wav = testing::build_wav(spec);

        decode_options options = {};
        options.sampleRate = 24000;
        const core::result<pcm_buffer> halved = decode_to_pcm(wav, options);
        CONJURE_REQUIRE_OK(halved);
        CHECK(halved.value().format().sampleRate == 24000);
        CHECK(halved.value().frame_count() == doctest::Approx(24000.0).epsilon(0.01));
        CHECK(halved.value().duration_seconds() == doctest::Approx(1.0).epsilon(0.01));
        CHECK(testing::peak_of(halved.value().samples()) == doctest::Approx(0.5).epsilon(0.05));

        options.sampleRate = 96000;
        const core::result<pcm_buffer> doubled = decode_to_pcm(wav, options);
        CONJURE_REQUIRE_OK(doubled);
        CHECK(doubled.value().frame_count() == doctest::Approx(96000.0).epsilon(0.01));

        options.sampleRate = 48000;
        const core::result<pcm_buffer> same = decode_to_pcm(wav, options);
        CONJURE_REQUIRE_OK(same);
        CHECK(same.value().frame_count() == 48000);

        options.sampleRate = 1000;
        CONJURE_CHECK_ERROR(decode_to_pcm(wav, options), audio_error::INVALID_SETTINGS);
    }

    TEST_CASE("decode_to_pcm - normalize scales the peak to 1") {
        testing::wav_spec spec = {};
        spec.amplitude = 0.25f;
        spec.frameCount = 4800;
        decode_options options = {};
        options.normalize = true;
        const core::result<pcm_buffer> decoded = decode_to_pcm(testing::build_wav(spec), options);
        CONJURE_REQUIRE_OK(decoded);
        CHECK(testing::peak_of(decoded.value().samples()) == doctest::Approx(1.0).epsilon(1.0e-5));

        spec.shape = testing::wave_shape::SILENCE;
        const core::result<pcm_buffer> silent = decode_to_pcm(testing::build_wav(spec), options);
        CONJURE_REQUIRE_OK(silent);
        CHECK(testing::peak_of(silent.value().samples()) == doctest::Approx(0.0));
    }

    TEST_CASE("decoder - a truncated header fails with DECODE_FAILED") {
        const std::vector<byte> wav = testing::build_wav(testing::wav_spec{});
        const std::vector<byte> truncated(wav.begin(), wav.begin() + 20);
        CONJURE_CHECK_ERROR(probe(truncated), audio_error::DECODE_FAILED);
        CONJURE_CHECK_ERROR(decode_to_pcm(truncated, decode_options{}), audio_error::DECODE_FAILED);
        const core::result<clip_info> failure = probe(truncated);
        REQUIRE_FALSE(failure.has_value());
        CHECK_FALSE(failure.get_error().message().empty());
    }

    TEST_CASE("decoder - an empty span fails with INVALID_CLIP") {
        CONJURE_CHECK_ERROR(probe({}), audio_error::INVALID_CLIP);
        CONJURE_CHECK_ERROR(decode_to_pcm({}, decode_options{}), audio_error::INVALID_CLIP);
        streaming_decoder decoder = {};
        CONJURE_CHECK_ERROR(decoder.open({}), audio_error::INVALID_CLIP);
        CHECK_FALSE(decoder.is_open());
    }

    TEST_CASE("decoder - random bytes fail with an error and never assert") {
        testing::expect_assert guard = {};
        core::random::engine &generator = core::random::default_engine();
        generator.seed(testing::environment().seed);
        for (u32 attempt = 0; attempt < 16; ++attempt) {
            CAPTURE(attempt);
            std::vector<byte> garbage(256 + attempt * 37, byte{0});
            for (byte &value : garbage) {
                value = static_cast<byte>(core::random::uniform(generator, 0, 255));
            }
            // Half the attempts wear a valid magic so the decoder itself has to reject the body.
            if (attempt % 2 == 0) {
                garbage[0] = byte{'R'};
                garbage[1] = byte{'I'};
                garbage[2] = byte{'F'};
                garbage[3] = byte{'F'};
                garbage[8] = byte{'W'};
                garbage[9] = byte{'A'};
                garbage[10] = byte{'V'};
                garbage[11] = byte{'E'};
            }
            const core::result<clip_info> probed = probe(garbage);
            CHECK_FALSE(probed.has_value());
            const core::result<pcm_buffer> decoded = decode_to_pcm(garbage, decode_options{});
            CHECK_FALSE(decoded.has_value());
        }
        CHECK(guard.count() == 0);
    }

    TEST_CASE("streaming_decoder - reading in 1000-frame chunks equals the full decode") {
        testing::wav_spec spec = {};
        spec.channels = 2;
        spec.frameCount = 12345;
        const std::vector<byte> wav = testing::build_wav(spec);
        const core::result<pcm_buffer> whole = decode_to_pcm(wav, decode_options{});
        CONJURE_REQUIRE_OK(whole);

        streaming_decoder decoder = {};
        CONJURE_REQUIRE_OK(decoder.open(wav));
        CHECK(decoder.is_open());
        CHECK(decoder.info().frameCount == 12345);
        CHECK(decoder.info().format.channels == 2);
        CHECK(decoder.position() == 0);

        std::vector<f32> streamed = {};
        std::vector<f32> chunk(1000 * 2, 0.0f);
        usize read = 0;
        u32 iterations = 0;
        while ((read = decoder.read(chunk)) > 0 && iterations < 100) {
            streamed.insert(streamed.end(), chunk.begin(), chunk.begin() + static_cast<isize>(read * 2));
            ++iterations;
        }
        CHECK(iterations == 13);
        CHECK(decoder.position() == 12345);
        REQUIRE(streamed.size() == whole.value().samples().size());
        CHECK(max_difference(streamed, whole.value().samples()) == doctest::Approx(0.0));
        CHECK(decoder.read(chunk) == 0);
    }

    TEST_CASE("streaming_decoder - seek to the middle and read past the end") {
        testing::wav_spec spec = {};
        spec.frameCount = 10000;
        spec.shape = testing::wave_shape::RAMP;
        spec.amplitude = 1.0f;
        const std::vector<byte> wav = testing::build_wav(spec);
        const std::vector<f32> expected = testing::generate_samples(spec);

        streaming_decoder decoder = {};
        CONJURE_REQUIRE_OK(decoder.open(wav));
        CONJURE_REQUIRE_OK(decoder.seek(5000));
        CHECK(decoder.position() == 5000);
        std::vector<f32> chunk(100, 0.0f);
        CHECK(decoder.read(chunk) == 100);
        CHECK(chunk[0] == doctest::Approx(static_cast<f64>(expected[5000])));
        CHECK(chunk[99] == doctest::Approx(static_cast<f64>(expected[5099])));
        CHECK(decoder.position() == 5100);

        CONJURE_REQUIRE_OK(decoder.seek(9990));
        CHECK(decoder.read(chunk) == 10);
        CHECK(decoder.read(chunk) == 0);
        CHECK(decoder.read(chunk) == 0);
        CHECK(decoder.position() == 10000);

        CONJURE_REQUIRE_OK(decoder.seek(0));
        CHECK(decoder.read(chunk) == 100);
        CHECK(chunk[0] == doctest::Approx(static_cast<f64>(expected[0])));

        decoder.close();
        CHECK_FALSE(decoder.is_open());
        CHECK(decoder.read(chunk) == 0);
        CONJURE_CHECK_ERROR(decoder.seek(0), core::error_code::INVALID_STATE);
        decoder.close();
    }

    TEST_CASE("streaming_decoder - move transfers the open stream") {
        const std::vector<byte> wav = testing::build_wav(testing::wav_spec{});
        streaming_decoder first = {};
        CONJURE_REQUIRE_OK(first.open(wav));
        streaming_decoder second(std::move(first));
        CHECK(second.is_open());
        CHECK(second.info().frameCount == 48000);
        streaming_decoder third = {};
        third = std::move(second);
        CHECK(third.is_open());
        std::vector<f32> chunk(64, 0.0f);
        CHECK(third.read(chunk) == 64);
    }

    TEST_CASE("streaming_decoder - WAV decoding is allocation-free after open()") {
        const std::vector<byte> wav = testing::build_wav(testing::wav_spec{});
        streaming_decoder decoder = {};
        CONJURE_REQUIRE_OK(decoder.open(wav));
        std::vector<f32> chunk(4096, 0.0f);
        usize total = 0;
        testing::allocation_scope scope = {};
        for (u32 iteration = 0; iteration < 8; ++iteration) {
            total += decoder.read(chunk);
        }
        const u64 allocations = scope.allocation_count();
        CHECK(allocations == 0);
        CHECK(total == 8 * 4096);
    }

    TEST_CASE("regression - audio review 3 a WAV header declaring more frames than the file holds is rejected") {
        testing::wav_spec spec = {};
        spec.format = testing::wav_sample_format::PCM16;
        spec.frameCount = 16;
        const std::vector<byte> wav = testing::build_wav(spec);
        REQUIRE(wav.size() == 44 + 32);

        // dr_wav derives the frame count from this field alone, so a forged header used to make
        // decode_to_pcm() reserve hundreds of megabytes (or throw) for 32 bytes of samples.
        std::vector<byte> forged = wav;
        forged[40] = byte{0xF0};
        forged[41] = byte{0xFF};
        forged[42] = byte{0xFF};
        forged[43] = byte{0x0F};
        CONJURE_CHECK_ERROR(probe(forged), audio_error::DECODE_FAILED);
        testing::allocation_scope scope = {};
        CONJURE_CHECK_ERROR(decode_to_pcm(forged, decode_options{}), audio_error::DECODE_FAILED);
        const u64 allocatedBytes = scope.allocated_bytes();
        CHECK(allocatedBytes < 1024 * 1024);
        streaming_decoder decoder = {};
        CONJURE_CHECK_ERROR(decoder.open(forged), audio_error::DECODE_FAILED);
        CHECK_FALSE(decoder.is_open());

        // A declared size the bytes could hold is tolerated: what exists is decoded.
        std::vector<byte> padded = wav;
        padded[40] = byte{150};
        padded[41] = byte{0};
        const core::result<clip_info> info = probe(padded);
        CONJURE_REQUIRE_OK(info);
        CHECK(info.value().frameCount == 75);
        const core::result<pcm_buffer> decoded = decode_to_pcm(padded, decode_options{});
        CONJURE_REQUIRE_OK(decoded);
        CHECK(decoded.value().frame_count() == 16);
    }

    TEST_CASE("streaming_decoder - open with a known frame count uses it only when the container reports none") {
        const std::vector<byte> wav = testing::build_wav(testing::wav_spec{});
        streaming_decoder decoder = {};
        CONJURE_REQUIRE_OK(decoder.open(wav, 5));
        CHECK(decoder.info().frameCount == 48000);   // the container's own length wins
        CHECK(decoder.position() == 0);
        std::vector<f32> chunk(64, 0.0f);
        CHECK(decoder.read(chunk) == 64);

        // An empty data chunk reports no length, like a Vorbis stream in push mode: the supplied
        // count stands in and nothing is decoded before the first read().
        testing::wav_spec empty = {};
        empty.frameCount = 0;
        const std::vector<byte> emptyWav = testing::build_wav(empty);
        CONJURE_REQUIRE_OK(decoder.open(emptyWav));
        CHECK(decoder.info().frameCount == 0);
        CONJURE_REQUIRE_OK(decoder.open(emptyWav, 500));
        CHECK(decoder.info().frameCount == 500);
        CHECK(decoder.position() == 0);
        CHECK(decoder.read(chunk) == 0);
        CONJURE_CHECK_ERROR(decoder.open({}, 500), audio_error::INVALID_CLIP);
    }

    TEST_CASE("decoder - [downloaded] ogg, mp3 and flac decode to the expected length within 1 %") {
        CONJURE_TEST_REQUIRE(testing::precondition::DOWNLOADED_ASSETS);
        testing::log_capture logs = {};
        const std::array<std::string, 3> names = {"audio/tone.ogg", "audio/tone.mp3", "audio/tone.flac"};
        const std::array<clip_encoding, 3> encodings = {
            clip_encoding::OGG_VORBIS, clip_encoding::MP3, clip_encoding::FLAC,
        };
        for (usize index = 0; index < names.size(); ++index) {
            CAPTURE(names[index]);
            const std::filesystem::path path = testing::downloaded_asset(names[index]);
            // The downloaded set is present (precondition above), so a missing tone file means
            // tools/test_assets.json does not list it: the Vorbis path would silently go untested.
            REQUIRE_FALSE(path.empty());
            REQUIRE(std::filesystem::exists(path));
            const std::vector<byte> bytes = read_whole_file(path);
            const core::result<clip_info> info = probe(bytes);
            CONJURE_REQUIRE_OK(info);
            CHECK(info.value().encoding == encodings[index]);
            CHECK(info.value().frameCount > 0);
            const core::result<pcm_buffer> decoded = decode_to_pcm(bytes, decode_options{});
            CONJURE_REQUIRE_OK(decoded);
            const f64 expectedFrames = static_cast<f64>(info.value().frameCount);
            CHECK(static_cast<f64>(decoded.value().frame_count()) == doctest::Approx(expectedFrames).epsilon(0.01));
        }
    }
}
