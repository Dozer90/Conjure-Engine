#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/log_capture.h"

#include "audio_test_support.h"
#include "wav_builder.h"

#include "conjure/audio/audio_engine.h"
#include "conjure/audio/audio_error.h"
#include "conjure/audio/null_backend.h"

#include <algorithm>
#include <vector>

using namespace conjure;
using namespace conjure::audio;

namespace {
    struct recording_callback {
        std::vector<u32> frameCounts = {};
        u64 totalFrames = 0;

        static void render(void *user, f32 *output, u32 frameCount) noexcept {
            recording_callback *self = static_cast<recording_callback *>(user);
            if (self->frameCounts.size() < 4096) {
                self->frameCounts.push_back(frameCount);
            }
            self->totalFrames += frameCount;
            for (u32 sample = 0; sample < frameCount * MIX_CHANNELS; ++sample) {
                output[sample] = 0.25f;
            }
        }
    };

    backend_desc desc_for(recording_callback &recorder, u32 sampleRate = 48000) {
        backend_desc desc = {};
        desc.format.sampleRate = sampleRate;
        desc.callback = &recording_callback::render;
        desc.user = &recorder;
        return desc;
    }
}

TEST_SUITE("audio/null_backend") {

    TEST_CASE("null_backend - update(1/60) renders 800 frames at 48 kHz and 60 updates make exactly 48000") {
        recording_callback recorder = {};
        null_backend backend = {};
        CONJURE_REQUIRE_OK(backend.start(desc_for(recorder)));
        backend.update(testing::FRAME_SECONDS);
        CHECK(recorder.totalFrames == 800);
        CHECK(backend.stats().framesRendered == 800);
        for (u32 frame = 1; frame < 60; ++frame) {
            backend.update(testing::FRAME_SECONDS);
        }
        CHECK(recorder.totalFrames == 48000);
        CHECK(backend.stats().framesRendered == 48000);
        CHECK(backend.virtual_time() == doctest::Approx(1.0));
    }

    TEST_CASE("null_backend - the fractional remainder is carried between updates") {
        recording_callback recorder = {};
        null_backend backend = {};
        CONJURE_REQUIRE_OK(backend.start(desc_for(recorder, 44100)));
        for (u32 frame = 0; frame < 60; ++frame) {
            backend.update(testing::FRAME_SECONDS);   // 735 frames each, no fraction
        }
        CHECK(recorder.totalFrames == 44100);
        for (u32 frame = 0; frame < 7; ++frame) {
            backend.update(0.1f);   // 4410 frames each
        }
        CHECK(recorder.totalFrames == 44100 + 7 * 4410);

        null_backend odd = {};
        recording_callback oddRecorder = {};
        CONJURE_REQUIRE_OK(odd.start(desc_for(oddRecorder, 48000)));
        for (u32 frame = 0; frame < 3; ++frame) {
            odd.update(1.0f / 90.0f);   // 533.33 frames per update: 533, 533, 534 with the carry
        }
        CHECK(oddRecorder.totalFrames == 1600);
        CHECK(oddRecorder.frameCounts.size() == 9);
    }

    TEST_CASE("null_backend - the callback is invoked in MIX_BLOCK_FRAMES blocks") {
        recording_callback recorder = {};
        null_backend backend = {};
        CONJURE_REQUIRE_OK(backend.start(desc_for(recorder)));
        backend.update(testing::FRAME_SECONDS);
        REQUIRE(recorder.frameCounts.size() == 4);
        CHECK(recorder.frameCounts[0] == MIX_BLOCK_FRAMES);
        CHECK(recorder.frameCounts[1] == MIX_BLOCK_FRAMES);
        CHECK(recorder.frameCounts[2] == MIX_BLOCK_FRAMES);
        CHECK(recorder.frameCounts[3] == 800 - 3 * MIX_BLOCK_FRAMES);
        CHECK(backend.stats().renderCalls == 4);
        for (const u32 count : recorder.frameCounts) {
            CHECK(count <= MIX_BLOCK_FRAMES);
        }
    }

    TEST_CASE("null_backend - render_frames renders exactly the requested count and nothing while stopped") {
        recording_callback recorder = {};
        null_backend backend = {};
        backend.render_frames(100);
        CHECK(recorder.totalFrames == 0);
        CONJURE_REQUIRE_OK(backend.start(desc_for(recorder)));
        backend.render_frames(1000);
        CHECK(recorder.totalFrames == 1000);
        CHECK(backend.stats().renderCalls == 4);
        backend.update(0.0f);
        backend.update(-1.0f);
        CHECK(recorder.totalFrames == 1000);
        backend.stop();
        backend.render_frames(100);
        backend.update(1.0f);
        CHECK(recorder.totalFrames == 1000);
    }

    TEST_CASE("null_backend - capture keeps the last N frames in order") {
        recording_callback recorder = {};
        null_backend backend = {};
        CONJURE_REQUIRE_OK(backend.start(desc_for(recorder)));
        CHECK(backend.captured().empty());
        backend.set_capture_frames(300);
        backend.update(testing::FRAME_SECONDS);
        CHECK(backend.captured().size() == 300 * MIX_CHANNELS);
        for (const f32 sample : backend.captured()) {
            CHECK(sample == doctest::Approx(0.25));
        }
        backend.set_capture_frames(0);
        backend.update(testing::FRAME_SECONDS);
        CHECK(backend.captured().empty());
    }

    TEST_CASE("null_backend - simulate_device_loss publishes LOST and stops rendering until the next start") {
        recording_callback recorder = {};
        null_backend backend = {};
        CONJURE_REQUIRE_OK(backend.start(desc_for(recorder)));
        backend.update(testing::FRAME_SECONDS);
        CHECK(recorder.totalFrames == 800);

        backend.simulate_device_loss();
        CHECK(backend.state() == device_state::LOST);
        backend.update(testing::FRAME_SECONDS);
        backend.render_frames(100);
        CHECK(recorder.totalFrames == 800);

        CONJURE_REQUIRE_OK(backend.start(desc_for(recorder)));
        CHECK(backend.state() == device_state::RUNNING);
        backend.update(testing::FRAME_SECONDS);
        CHECK(recorder.totalFrames == 1600);
    }

    TEST_CASE("null_backend - fail_next_starts fails that many starts with DEVICE_START_FAILED") {
        recording_callback recorder = {};
        null_backend backend = {};
        backend.fail_next_starts(2);
        CONJURE_CHECK_ERROR(backend.start(desc_for(recorder)), audio_error::DEVICE_START_FAILED);
        CHECK(backend.state() == device_state::STOPPED);
        CONJURE_CHECK_ERROR(backend.start(desc_for(recorder)), audio_error::DEVICE_START_FAILED);
        CHECK(backend.state() == device_state::STOPPED);
        CONJURE_REQUIRE_OK(backend.start(desc_for(recorder)));
        CHECK(backend.state() == device_state::RUNNING);

        // A failed restart of a running backend leaves it stopped, like a device that will not reopen.
        backend.fail_next_starts(1);
        CONJURE_CHECK_ERROR(backend.start(desc_for(recorder)), audio_error::DEVICE_START_FAILED);
        CHECK(backend.state() == device_state::STOPPED);
        backend.update(testing::FRAME_SECONDS);
        CHECK(recorder.totalFrames == 0);
    }

    TEST_CASE("null_backend - a 1 kHz sine at gain 0.5 peaks at 0.5 through the engine") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        null_backend &backend = *engine.null_backend_instance();
        play_params params = {};
        params.volume = 0.5f;
        params.loop = true;
        params.retain = true;
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(1000.0, 0.5, 1.0f, 2), params);
        CONJURE_REQUIRE_OK(voice);
        backend.set_capture_frames(4800);
        testing::step_frames(engine, 8);
        CHECK(testing::peak_of(backend.captured()) == doctest::Approx(0.5).epsilon(0.002));
        CHECK(backend.stats().activeVoices == 1);
        CHECK(backend.stats().commandsApplied > 0);
    }

    TEST_CASE("null_backend - virtual_time accumulates and the voice stats reflect the cursor") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        null_backend &backend = *engine.null_backend_instance();
        play_params params = {};
        params.retain = true;
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), params);
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 3);
        CHECK(backend.virtual_time() == doctest::Approx(3.0 / 60.0).epsilon(1.0e-6));
        CHECK(backend.stats().framesRendered == 2400);
        const null_voice_stats &slot = testing::slot_stats(engine, 0);
        CHECK(slot.state == voice_state::PLAYING);
        CHECK(slot.cursorFrames == 2400);
        CHECK(slot.gain == doctest::Approx(1.0));
        CHECK(slot.pan == doctest::Approx(0.0));
        CHECK(slot.step == doctest::Approx(1.0));
        CHECK_FALSE(slot.loop);
        CHECK(slot.groupIndex == 0);
        CHECK(backend.stats().listener == engine.listener());
        CHECK(backend.stats().groups.size() == 1);
        CHECK(backend.stats().underruns == 0);
    }

    TEST_CASE("null_backend - rendering 256 voices is allocation-free") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        null_backend &backend = *engine.null_backend_instance();
        const std::shared_ptr<const pcm_buffer> clip = testing::make_sine_pcm(440.0, 2.0, 0.01f, 2);
        play_params params = {};
        params.loop = true;
        params.retain = true;
        for (u32 index = 0; index < MAX_VOICES; ++index) {
            CAPTURE(index);
            CONJURE_REQUIRE_OK(engine.play(clip, params));
        }
        testing::step_frames(engine, 2);
        CHECK(backend.stats().activeVoices == MAX_VOICES);

        testing::allocation_scope scope = {};
        backend.render_frames(480);
        backend.render_frames(480);
        const u64 allocations = scope.allocation_count();
        CHECK(allocations == 0);
    }
}
