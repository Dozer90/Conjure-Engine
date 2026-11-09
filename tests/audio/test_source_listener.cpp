#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "audio_test_support.h"
#include "wav_builder.h"

#include "conjure/audio/audio_engine.h"
#include "conjure/audio/components.h"

#include "conjure/core/math.h"

#include <array>
#include <vector>

// Drives the engine the way engine.audio_sync will (docs/design/12_audio.md 8): sources described by
// component-like data plus world positions, one set_positions() per frame, a moving listener, and
// end_frame() publishing the finished one-shots.

using namespace conjure;
using namespace conjure::audio;

namespace {
    struct source_sample {
        play_params params = {};
        math::vec3 position = math::vec3(0.0f);
        math::vec3 velocity = math::vec3(0.0f);
        voice_id voice = {};
        bool isPlaying = false;
    };

    play_params params_from(f32 volume, bool loop, bool spatial, f32 minDistance, f32 maxDistance) {
        play_params params = {};
        params.volume = volume;
        params.loop = loop;
        params.spatial = spatial;
        params.retain = true;      // component voices are retained; the sync releases them
        params.minDistance = minDistance;
        params.maxDistance = maxDistance;
        params.attenuation = attenuation_model::INVERSE;
        return params;
    }

    listener_state listener_at(math::vec3 position, math::vec3 previous, f32 deltaSeconds) {
        listener_state listener = {};
        listener.position = position;
        listener.velocity = (position - previous) / deltaSeconds;
        return listener;
    }
}

TEST_SUITE("audio/source_listener") {

    TEST_CASE("source_listener - 120 frames of sync drive voices, one-shots and the moving listener") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const std::shared_ptr<const pcm_buffer> music = testing::make_sine_pcm(220.0, 1.0, 0.5f, 2);
        const std::shared_ptr<const pcm_buffer> beep = testing::make_sine_pcm(880.0, 0.1, 0.5f);

        std::array<source_sample, 3> sources = {};
        sources[0].params = params_from(0.8f, true, true, 1.0f, 100.0f);        // MusicLoop, 10 m ahead
        sources[0].position = math::vec3(0.0f, 0.0f, -10.0f);
        sources[1].params = params_from(1.0f, false, true, 1.0f, 100.0f);       // OneShotEmitter, to the right
        sources[1].position = math::vec3(5.0f, 0.0f, 0.0f);
        sources[2].params = params_from(0.5f, true, false, 1.0f, 100.0f);       // 2D UI loop

        // Frame 0: the sync sees every source for the first time with playOnStart set.
        for (usize index = 0; index < sources.size(); ++index) {
            CAPTURE(index);
            const core::result<voice_id> voice = engine.play(index == 1 ? beep : music, sources[index].params);
            CONJURE_REQUIRE_OK(voice);
            sources[index].voice = voice.value();
        }
        CHECK(engine.active_voice_count() == 3);

        math::vec3 listenerPosition = math::vec3(0.0f);
        math::vec3 previousListener = listenerPosition;
        u32 oneShotFinishedFrame = 0;
        f32 gainAtStart = 0.0f;
        f32 gainAtEnd = 0.0f;
        for (u32 frame = 0; frame < 120; ++frame) {
            CAPTURE(frame);
            // 1. listener: walks away from the music source along +Z at 10 m/s.
            previousListener = listenerPosition;
            listenerPosition = math::vec3(0.0f, 0.0f, static_cast<f32>(frame) * 10.0f / 60.0f);
            engine.set_listener(listener_at(listenerPosition, previousListener, testing::FRAME_SECONDS));

            // 2. component pass: mirror is_playing, re-apply edited fields on a "changed" chunk.
            for (source_sample &source : sources) {
                source.isPlaying = engine.is_playing(source.voice);
            }
            if (frame == 30) {
                sources[2].params.volume = 0.25f;
                engine.apply_params(sources[2].voice, sources[2].params);
            }

            // 3. positions, then update.
            std::vector<voice_transform> transforms = {};
            for (const source_sample &source : sources) {
                if (source.params.spatial && engine.is_valid(source.voice)) {
                    voice_transform transform = {};
                    transform.voice = source.voice;
                    transform.position = source.position;
                    transform.velocity = source.velocity;
                    transforms.push_back(transform);
                }
            }
            engine.set_positions(transforms);
            engine.update(testing::FRAME_SECONDS);

            // FRAME_END: finished voices lose their handle; one-shot entities would be destroyed.
            engine.end_frame();
            for (const voice_id finished : engine.finished_voices()) {
                for (source_sample &source : sources) {
                    if (source.voice == finished) {
                        CHECK(oneShotFinishedFrame == 0);
                        oneShotFinishedFrame = frame + 1;
                        engine.release(source.voice);
                        source.voice = {};
                        source.isPlaying = false;
                    }
                }
            }
            if (frame == 2) {
                gainAtStart = testing::slot_stats(engine, 0).gain;
            }
            if (frame == 119) {
                gainAtEnd = testing::slot_stats(engine, 0).gain;
            }
        }

        // The 0.1 s one-shot ended on frame 6 and was reported exactly once.
        CHECK(oneShotFinishedFrame == 6);
        CHECK_FALSE(sources[1].voice.is_valid());
        CHECK_FALSE(sources[1].isPlaying);

        // The loops are still going and the voice count dropped to two.
        CHECK(engine.active_voice_count() == 2);
        CHECK(sources[0].isPlaying);
        CHECK(sources[2].isPlaying);
        CHECK(engine.stats().loops >= 1);

        // The listener position reached the stats and the music attenuated as the listener walked away.
        const listener_state &reported = engine.null_backend_instance()->stats().listener;
        CHECK(reported.position.z == doctest::Approx(119.0 * 10.0 / 60.0));
        CHECK(reported.velocity.z == doctest::Approx(10.0).epsilon(1.0e-3));
        CHECK(gainAtStart == doctest::Approx(0.08).epsilon(0.02));   // 0.8 * 1 / 10
        CHECK(gainAtEnd < gainAtStart * 0.5f);
        CHECK(gainAtEnd == doctest::Approx(0.8 / (1.0 + (10.0 + 119.0 * 10.0 / 60.0 - 1.0))).epsilon(0.05));

        // The 2D loop took the edited volume on the "changed" frame.
        CHECK(testing::slot_stats(engine, 2).gain == doctest::Approx(0.25));
        CHECK(testing::slot_stats(engine, 2).pan == doctest::Approx(0.0));

        // Releasing the component voices (world::observe REMOVED) empties the engine.
        engine.release(sources[0].voice);
        engine.release(sources[2].voice);
        testing::step_frames(engine, 2);
        CHECK(engine.active_voice_count() == 0);
    }

    TEST_CASE("source_listener - a hidden source_handle round trips the voice id and version") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const play_params loopParams = params_from(1.0f, true, false, 1.0f, 10.0f);
        const audio_clip clip = make_pcm_clip(testing::make_sine_pcm(440.0, 1.0));
        const core::result<voice_id> voice = engine.play(clip, loopParams, 42);
        CONJURE_REQUIRE_OK(voice);

        source_handle handle = {};
        handle.voice = voice.value();
        handle.clipVersion = engine.voice(voice.value())->clipVersion;
        handle.flags |= source_handle::FLAG_STARTED_ONCE;
        CHECK(handle.clipVersion == 42);
        CHECK(handle.started_once());
        CHECK(engine.is_playing(handle.voice));

        // A reload bumps the version: the sync compares and restarts the voice.
        const u32 newVersion = 43;
        CHECK(newVersion != handle.clipVersion);
        engine.stop(handle.voice);
        const core::result<voice_id> replacement = engine.play(clip, loopParams, newVersion);
        CONJURE_REQUIRE_OK(replacement);
        handle.voice = replacement.value();
        handle.clipVersion = newVersion;
        testing::step_frames(engine, 2);
        CHECK(engine.is_playing(handle.voice));
        CHECK_FALSE(engine.is_playing(voice.value()));
        engine.release(handle.voice);
    }

    TEST_CASE("source_listener - the listener component gain and active flag map onto listener_state") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        audio_listener component = {};
        component.gain = 0.5f;
        component.active = true;
        listener_state listener = {};
        listener.gain = component.gain;
        listener.position = math::vec3(1.0f, 2.0f, 3.0f);
        engine.set_listener(listener);
        CHECK(engine.listener().gain == doctest::Approx(0.5));
        CHECK(engine.listener().position.y == doctest::Approx(2.0));

        // An inactive listener leaves the engine's listener unchanged.
        component.active = false;
        if (component.active) {
            engine.set_listener(listener_state{});
        }
        CHECK(engine.listener().gain == doctest::Approx(0.5));
        engine.update(testing::FRAME_SECONDS);
        CHECK(engine.null_backend_instance()->stats().listener.position.x == doctest::Approx(1.0));
    }
}
