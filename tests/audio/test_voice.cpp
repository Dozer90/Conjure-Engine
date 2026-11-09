#include "conjure/testing/doctest.h"

#include "conjure/audio/voice.h"

#include <cmath>
#include <limits>

using namespace conjure;
using namespace conjure::audio;

TEST_SUITE("audio/voice") {

    TEST_CASE("play_params - defaults match the design") {
        const play_params params = {};
        CHECK(params.volume == doctest::Approx(1.0));
        CHECK(params.pitch == doctest::Approx(1.0));
        CHECK(params.pan == doctest::Approx(0.0));
        CHECK_FALSE(params.loop);
        CHECK_FALSE(params.spatial);
        CHECK_FALSE(params.retain);
        CHECK_FALSE(params.startPaused);
        CHECK(params.priority == 128);
        CHECK(params.group == MASTER_GROUP);
        CHECK(params.startSeconds == doctest::Approx(0.0));
        CHECK(params.attenuation == attenuation_model::INVERSE);
        CHECK(params.minDistance == doctest::Approx(1.0));
        CHECK(params.maxDistance == doctest::Approx(100.0));
        CHECK(params.rolloff == doctest::Approx(1.0));
        CHECK(clamp_play_params(params) == params);
    }

    TEST_CASE("play_params - every field is clamped to its range") {
        play_params params = {};
        params.volume = 5.0f;
        params.pitch = 0.0f;
        params.pan = -3.0f;
        params.minDistance = 0.0f;
        params.maxDistance = 0.5f;
        params.rolloff = 42.0f;
        params.startSeconds = -1.0;

        const play_params clamped = clamp_play_params(params);
        CHECK(clamped.volume == doctest::Approx(2.0));
        CHECK(clamped.pitch == doctest::Approx(0.25));
        CHECK(clamped.pan == doctest::Approx(-1.0));
        CHECK(clamped.minDistance == doctest::Approx(0.01));
        CHECK(clamped.maxDistance == doctest::Approx(0.5));
        CHECK(clamped.rolloff == doctest::Approx(10.0));
        CHECK(clamped.startSeconds == doctest::Approx(0.0));

        params.pitch = 9.0f;
        params.pan = 1.5f;
        CHECK(clamp_play_params(params).pitch == doctest::Approx(4.0));
        CHECK(clamp_play_params(params).pan == doctest::Approx(1.0));
    }

    TEST_CASE("play_params - maxDistance below minDistance is raised to minDistance") {
        play_params params = {};
        params.minDistance = 10.0f;
        params.maxDistance = 2.0f;
        const play_params clamped = clamp_play_params(params);
        CHECK(clamped.minDistance == doctest::Approx(10.0));
        CHECK(clamped.maxDistance == doctest::Approx(10.0));
    }

    TEST_CASE("play_params - NaN and infinity become the defaults") {
        const f32 nan = std::numeric_limits<f32>::quiet_NaN();
        const f32 infinity = std::numeric_limits<f32>::infinity();
        play_params params = {};
        params.volume = nan;
        params.pitch = nan;
        params.pan = nan;
        params.minDistance = nan;
        params.maxDistance = nan;
        params.rolloff = nan;
        params.startSeconds = std::numeric_limits<f64>::quiet_NaN();

        const play_params clamped = clamp_play_params(params);
        CHECK(clamped.volume == doctest::Approx(1.0));
        CHECK(clamped.pitch == doctest::Approx(1.0));
        CHECK(clamped.pan == doctest::Approx(0.0));
        CHECK(clamped.minDistance == doctest::Approx(1.0));
        CHECK(clamped.maxDistance == doctest::Approx(100.0));
        CHECK(clamped.rolloff == doctest::Approx(1.0));
        CHECK(clamped.startSeconds == doctest::Approx(0.0));

        CHECK(clamp_volume(infinity) == doctest::Approx(1.0));
        CHECK(clamp_volume(-infinity) == doctest::Approx(1.0));
        CHECK(clamp_pitch(infinity) == doctest::Approx(1.0));
        CHECK(clamp_pan(-infinity) == doctest::Approx(0.0));
    }

    TEST_CASE("play_params - the individual clamp helpers agree with the design ranges") {
        CHECK(clamp_volume(-1.0f) == doctest::Approx(0.0));
        CHECK(clamp_volume(1.5f) == doctest::Approx(1.5));
        CHECK(clamp_pitch(0.1f) == doctest::Approx(0.25));
        CHECK(clamp_pitch(2.0f) == doctest::Approx(2.0));
        CHECK(clamp_pan(0.25f) == doctest::Approx(0.25));
        CHECK(clamp_pan(2.0f) == doctest::Approx(1.0));
    }

    TEST_CASE("voice_info - defaults") {
        const voice_info info = {};
        CHECK(info.state == voice_state::FREE);
        CHECK(info.cursorFrames == 0);
        CHECK(info.clipFrames == 0);
        CHECK(info.volume == doctest::Approx(1.0));
        CHECK(info.pitch == doctest::Approx(1.0));
        CHECK(info.effectiveGain == doctest::Approx(1.0));
        CHECK(info.group == MASTER_GROUP);
        CHECK_FALSE(info.loop);
        CHECK_FALSE(info.spatial);
        CHECK_FALSE(info.retain);
        CHECK_FALSE(info.streamed);
        CHECK(info.clipVersion == 0);
        CHECK(info == voice_info{});
    }
}
