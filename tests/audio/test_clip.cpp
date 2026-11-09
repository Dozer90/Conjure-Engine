#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "wav_builder.h"

#include "conjure/audio/clip.h"

#include <memory>
#include <vector>

using namespace conjure;
using namespace conjure::audio;

TEST_SUITE("audio/clip") {

    TEST_CASE("pcm_buffer - a default buffer is empty") {
        const pcm_buffer buffer = {};
        CHECK(buffer.frame_count() == 0);
        CHECK(buffer.samples().empty());
        CHECK(buffer.duration_seconds() == doctest::Approx(0.0));
        CHECK(buffer.format() == audio_format{});
    }

    TEST_CASE("pcm_buffer - frame count and duration follow the format") {
        audio_format format = {};
        format.sampleRate = 44100;
        format.channels = 2;
        const pcm_buffer buffer(format, std::vector<f32>(44100 * 2, 0.25f));
        CHECK(buffer.frame_count() == 44100);
        CHECK(buffer.samples().size() == 88200);
        CHECK(buffer.duration_seconds() == doctest::Approx(1.0));
        CHECK(buffer.format().channels == 2);
        CHECK(buffer.samples()[17] == doctest::Approx(0.25));
    }

    TEST_CASE("pcm_buffer - a sample count that is not a whole number of frames asserts and is trimmed") {
        audio_format format = {};
        format.channels = 2;
        testing::expect_assert guard = {};
        const pcm_buffer buffer(format, std::vector<f32>(7, 0.0f));
        CHECK(guard.count() == 1);
        CHECK(buffer.frame_count() == 3);
        CHECK(buffer.samples().size() == 6);
    }

    TEST_CASE("pcm_buffer - a zero channel format asserts and is treated as mono") {
        audio_format format = {};
        format.channels = 0;
        testing::expect_assert guard = {};
        const pcm_buffer buffer(format, std::vector<f32>(10, 0.0f));
        CHECK(guard.count() == 1);
        CHECK(buffer.format().channels == 1);
        CHECK(buffer.frame_count() == 10);
    }

    TEST_CASE("pcm_buffer - a zero sample rate has no duration") {
        audio_format format = {};
        format.sampleRate = 0;
        const pcm_buffer buffer(format, std::vector<f32>(100, 0.0f));
        CHECK(buffer.duration_seconds() == doctest::Approx(0.0));
    }

    TEST_CASE("clip_info - duration and equality") {
        clip_info info = {};
        info.format.sampleRate = 48000;
        info.frameCount = 24000;
        CHECK(info.duration_seconds() == doctest::Approx(0.5));
        clip_info other = info;
        CHECK(other == info);
        other.encoding = clip_encoding::MP3;
        CHECK(other != info);
        info.format.sampleRate = 0;
        CHECK(info.duration_seconds() == doctest::Approx(0.0));
    }

    TEST_CASE("audio_clip - copies share the PCM buffer") {
        const std::shared_ptr<const pcm_buffer> pcm = testing::make_sine_pcm(440.0, 0.1);
        const audio_clip clip = make_pcm_clip(pcm);
        CHECK(pcm.use_count() == 2);
        const audio_clip copy = clip;
        CHECK(pcm.use_count() == 3);
        CHECK(copy.pcm.get() == clip.pcm.get());
        CHECK_FALSE(copy.is_streamed());
        CHECK(copy.is_valid());
        CHECK(copy.info.frameCount == 4800);
        CHECK(copy.info.format.channels == 1);
        CHECK(copy.info.encoding == clip_encoding::WAV);
        CHECK(copy.duration_seconds() == doctest::Approx(0.1));
    }

    TEST_CASE("audio_clip - is_streamed follows the storage kind and validity needs the matching buffer") {
        audio_clip streamed = {};
        streamed.info.storage = clip_storage::STREAMED;
        streamed.info.frameCount = 100;
        CHECK(streamed.is_streamed());
        CHECK_FALSE(streamed.is_valid());
        streamed.encoded = std::make_shared<const std::vector<byte>>(16, byte{0});
        CHECK(streamed.is_valid());

        audio_clip pcmClip = {};
        CHECK_FALSE(pcmClip.is_valid());
        pcmClip.pcm = testing::make_sine_pcm(440.0, 0.01);
        CHECK_FALSE(pcmClip.is_valid());   // info disagrees with the buffer
        pcmClip.info.frameCount = pcmClip.pcm->frame_count();
        pcmClip.info.format = pcmClip.pcm->format();
        CHECK(pcmClip.is_valid());
    }

    TEST_CASE("audio_clip - make_pcm_clip with a null buffer is invalid") {
        const audio_clip clip = make_pcm_clip(nullptr);
        CHECK_FALSE(clip.is_valid());
        CHECK(clip.info.frameCount == 0);
    }

    TEST_CASE("audio_clip - the fallback clip is 0.1 s of mono 48 kHz zeros") {
        const audio_clip fallback = make_fallback_clip();
        REQUIRE(fallback.pcm != nullptr);
        CHECK(fallback.is_valid());
        CHECK(fallback.info.format.sampleRate == 48000);
        CHECK(fallback.info.format.channels == 1);
        CHECK(fallback.info.frameCount == 4800);
        CHECK(fallback.duration_seconds() == doctest::Approx(0.1));
        CHECK(testing::peak_of(fallback.pcm->samples()) == doctest::Approx(0.0));
        CHECK(CLIP_ASSET_TYPE_NAME == "conjure.audio.clip");
    }
}
