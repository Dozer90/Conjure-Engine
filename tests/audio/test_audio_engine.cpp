#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"

#include "audio_test_support.h"
#include "wav_builder.h"

#include "conjure/audio/audio_engine.h"
#include "conjure/audio/audio_error.h"
#include "conjure/audio/clip_asset.h"
#include "conjure/audio/decoder.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::audio;

namespace {
    play_params retained(bool loop = false) {
        play_params params = {};
        params.retain = true;
        params.loop = loop;
        return params;
    }

    audio_clip streamed_clip(f64 seconds, u16 channels = 1) {
        testing::wav_spec spec = {};
        spec.channels = channels;
        spec.frameCount = static_cast<u64>(seconds * 48000.0);
        spec.amplitude = 0.5f;
        clip_import_settings settings = {};
        settings.storage = clip_storage_setting::STREAMED;
        const core::result<std::vector<byte>> blob = cook_clip(testing::build_wav(spec), settings);
        REQUIRE(blob.has_value());
        const core::result<audio_clip> clip = load_clip_blob(blob.value());
        REQUIRE(clip.has_value());
        return clip.value();
    }

    audio_clip pcm_clip_of(const audio_clip &streamed) {
        const core::result<pcm_buffer> decoded = decode_to_pcm(*streamed.encoded, decode_options{});
        REQUIRE(decoded.has_value());
        return make_pcm_clip(std::make_shared<const pcm_buffer>(std::move(decoded).value()));
    }

    std::vector<f32> render_whole(audio_engine &engine, voice_id voice, u32 maxFrames) {
        null_backend &backend = *engine.null_backend_instance();
        backend.set_capture_frames(maxFrames * testing::FRAME_FRAMES);
        testing::step_until_ended(engine, voice, maxFrames);
        return std::vector<f32>(backend.captured().begin(), backend.captured().end());
    }

    struct stereo_peaks {
        f32 left = 0.0f;
        f32 right = 0.0f;
    };

    stereo_peaks peaks_of(std::span<const f32> interleaved) {
        stereo_peaks peaks = {};
        for (usize frame = 0; frame < interleaved.size() / 2; ++frame) {
            peaks.left = std::max(peaks.left, std::abs(interleaved[frame * 2]));
            peaks.right = std::max(peaks.right, std::abs(interleaved[frame * 2 + 1]));
        }
        return peaks;
    }

    /// <summary>
    /// A 1 s stereo clip whose channels differ (left 0.8, right 0.2), so the pan law is visible.
    /// </summary>
    std::shared_ptr<const pcm_buffer> make_unbalanced_stereo() {
        audio_format format = {};
        format.channels = 2;
        std::vector<f32> samples(static_cast<usize>(48000) * 2, 0.0f);
        for (usize frame = 0; frame < 48000; ++frame) {
            samples[frame * 2] = 0.8f;
            samples[frame * 2 + 1] = 0.2f;
        }
        return std::make_shared<const pcm_buffer>(format, std::move(samples));
    }

    /// <summary>
    /// A STREAMED clip whose bytes wear the WAV magic but hold no decodable chunks.
    /// </summary>
    audio_clip undecodable_streamed_clip() {
        std::vector<byte> bytes(64, byte{0});
        const char *riff = "RIFF";
        const char *wave = "WAVE";
        for (usize index = 0; index < 4; ++index) {
            bytes[index] = static_cast<byte>(riff[index]);
            bytes[8 + index] = static_cast<byte>(wave[index]);
        }
        audio_clip clip = {};
        clip.info.storage = clip_storage::STREAMED;
        clip.info.encoding = clip_encoding::WAV;
        clip.info.frameCount = 4800;
        clip.info.format.channels = 1;
        clip.encoded = std::make_shared<const std::vector<byte>>(std::move(bytes));
        return clip;
    }
}

TEST_SUITE("audio/audio_engine") {

    /*-----------------------------------------------------------------------*/
    // Construction

    TEST_CASE("audio_engine - construction with NULL_BACKEND is headless and never fails") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        CHECK(engine.backend_kind() == audio_backend_kind::NULL_BACKEND);
        CHECK(engine.null_backend_instance() != nullptr);
        CHECK(engine.last_backend_error() == nullptr);
        CHECK_FALSE(engine.stats().fellBackToNull);
        CHECK(engine.output_format().sampleRate == 48000);
        CHECK(engine.output_format().channels == MIX_CHANNELS);
        CHECK(engine.stats().deviceState == device_state::RUNNING);
        CHECK(engine.active_voice_count() == 0);
        CHECK(engine.finished_voices().empty());
        CHECK(engine.settings().backend == audio_backend_kind::NULL_BACKEND);
        const core::result<std::vector<audio_device_info>> devices = engine.enumerate_devices();
        CONJURE_REQUIRE_OK(devices);
        CHECK(devices.value().size() == 1);
        CHECK(logs.contains(core::log_level::INFO, "audio engine ready"));
    }

    TEST_CASE("audio_engine - AUTO resolves to a concrete backend and falls back to null without an error") {
        testing::log_capture logs = {};
        audio_settings settings = {};
        settings.backend = audio_backend_kind::AUTO;
        audio_engine engine(settings);
        CHECK(engine.backend_kind() != audio_backend_kind::AUTO);
        if (engine.backend_kind() == audio_backend_kind::NULL_BACKEND) {
            CHECK(engine.stats().fellBackToNull);
            CHECK(engine.last_backend_error() != nullptr);
            CHECK(engine.null_backend_instance() != nullptr);
        } else {
            CHECK(engine.backend_kind() == audio_backend_kind::MINIAUDIO);
            CHECK_FALSE(engine.stats().fellBackToNull);
            CHECK(engine.null_backend_instance() == nullptr);
        }
        engine.update(testing::FRAME_SECONDS);
        engine.end_frame();
    }

    TEST_CASE("audio_engine - invalid settings construct on null with last_backend_error INVALID_SETTINGS") {
        testing::log_capture logs = {};
        logs.allow_errors();
        audio_settings settings = {};
        settings.backend = audio_backend_kind::AUTO;
        settings.sampleRate = 12;
        CONJURE_CHECK_ERROR(settings.validate(), audio_error::INVALID_SETTINGS);
        audio_engine engine(settings);
        CHECK(engine.backend_kind() == audio_backend_kind::NULL_BACKEND);
        REQUIRE(engine.last_backend_error() != nullptr);
        CHECK(engine.last_backend_error()->is(audio_error::INVALID_SETTINGS));
        CHECK(engine.output_format().sampleRate == MIN_SAMPLE_RATE);
        CHECK(logs.contains(core::log_level::ERR, "invalid audio settings"));
        engine.update(testing::FRAME_SECONDS);
        engine.end_frame();
    }

    TEST_CASE("audio_settings - validate rejects every documented problem") {
        audio_settings settings = testing::null_settings();
        CONJURE_CHECK_OK(settings.validate());
        settings.periodFrames = 0;
        CONJURE_CHECK_ERROR(settings.validate(), audio_error::INVALID_SETTINGS);
        settings = testing::null_settings();
        settings.sampleRate = MAX_SAMPLE_RATE + 1;
        CONJURE_CHECK_ERROR(settings.validate(), audio_error::INVALID_SETTINGS);
        settings = testing::null_settings();
        settings.dopplerFactor = -1.0f;
        CONJURE_CHECK_ERROR(settings.validate(), audio_error::INVALID_SETTINGS);
        settings = testing::null_settings();
        settings.speedOfSound = std::numeric_limits<f32>::quiet_NaN();
        CONJURE_CHECK_ERROR(settings.validate(), audio_error::INVALID_SETTINGS);

        settings = testing::null_settings();
        group_desc unnamed = {};
        settings.groups = {unnamed};
        CONJURE_CHECK_ERROR(settings.validate(), audio_error::INVALID_SETTINGS);

        group_desc music = {};
        music.name = core::string_id("Music");
        settings.groups = {music, music};
        CONJURE_CHECK_ERROR(settings.validate(), audio_error::INVALID_SETTINGS);

        group_desc orphan = {};
        orphan.name = core::string_id("Orphan");
        orphan.parentName = core::string_id("Missing");
        settings.groups = {orphan};
        CONJURE_CHECK_ERROR(settings.validate(), audio_error::INVALID_SETTINGS);

        group_desc first = {};
        first.name = core::string_id("A");
        first.parentName = core::string_id("B");
        group_desc second = {};
        second.name = core::string_id("B");
        second.parentName = core::string_id("A");
        settings.groups = {first, second};
        CONJURE_CHECK_ERROR(settings.validate(), audio_error::INVALID_SETTINGS);

        settings.groups.clear();
        for (u32 index = 0; index < MAX_GROUPS; ++index) {
            group_desc desc = {};
            desc.name = core::string_id("g" + std::to_string(index));
            settings.groups.push_back(desc);
        }
        CONJURE_CHECK_ERROR(settings.validate(), audio_error::INVALID_SETTINGS);
    }

    TEST_CASE("audio_engine - select_device restarts the null backend and keeps playing") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), retained(true));
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 2);
        CONJURE_REQUIRE_OK(engine.select_device("Null Output"));
        CHECK(engine.settings().deviceName == "Null Output");
        CHECK(engine.stats().deviceRestarts == 1);
        testing::step_frames(engine, 2);
        CHECK(engine.is_playing(voice.value()));
        CHECK(engine.stats().deviceState == device_state::RUNNING);
    }

    /*-----------------------------------------------------------------------*/
    // Playback lifecycle

    TEST_CASE("audio_engine - play records clip, gain and state in null_backend_stats and is_playing is immediate") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        play_params params = {};
        params.volume = 0.5f;
        params.retain = true;
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), params);
        CONJURE_REQUIRE_OK(voice);
        CHECK(engine.is_valid(voice.value()));
        CHECK(engine.is_playing(voice.value()));
        CHECK(engine.active_voice_count() == 1);
        const std::optional<voice_info> info = engine.voice(voice.value());
        REQUIRE(info.has_value());
        CHECK(info->state == voice_state::PLAYING);
        CHECK(info->volume == doctest::Approx(0.5));
        CHECK(info->clipFrames == 48000);
        CHECK(info->retain);

        testing::step_frames(engine, 1);
        const null_voice_stats &slot = testing::slot_stats(engine, 0);
        CHECK(slot.state == voice_state::PLAYING);
        CHECK(slot.gain == doctest::Approx(0.5));
        CHECK(slot.cursorFrames == 800);
        CHECK(slot.step == doctest::Approx(1.0));
        CHECK(engine.stats().activeVoices == 1);
        CHECK(engine.stats().peakVoices == 1);
        CHECK(engine.stats().commandsSent >= 1);
    }

    TEST_CASE("audio_engine - play rejects empty buffers and invalid clips") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        CONJURE_CHECK_ERROR(engine.play(std::shared_ptr<const pcm_buffer>(), play_params{}), audio_error::INVALID_CLIP);
        audio_format format = {};
        CONJURE_CHECK_ERROR(
          engine.play(std::make_shared<const pcm_buffer>(format, std::vector<f32>{}), play_params{}),
          audio_error::INVALID_CLIP
        );
        audio_clip broken = {};
        broken.info.frameCount = 10;
        CONJURE_CHECK_ERROR(engine.play(broken, play_params{}), audio_error::INVALID_CLIP);
        CHECK(engine.active_voice_count() == 0);
    }

    TEST_CASE("audio_engine - play from an asset_ref needs a loaded clip") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const assets::asset_ref<audio_clip> unbound = {};
        CONJURE_CHECK_ERROR(engine.play(unbound, play_params{}), audio_error::CLIP_NOT_LOADED);

        assets::asset_slot slot = {};
        audio_clip clip = make_pcm_clip(testing::make_sine_pcm(440.0, 0.1));
        slot.object.store(&clip, std::memory_order_release);
        slot.version.store(7, std::memory_order_release);
        slot.state.store(assets::asset_state::LOADED, std::memory_order_release);
        const assets::asset_ref<audio_clip> bound(core::uuid::generate(), &slot);
        const core::result<voice_id> voice = engine.play(bound, retained());
        CONJURE_REQUIRE_OK(voice);
        CHECK(engine.voice(voice.value())->clipVersion == 7);
        CHECK(engine.is_playing(voice.value()));
        slot.object.store(nullptr, std::memory_order_release);
    }

    TEST_CASE("audio_engine - a 0.5 s clip finishes after 0.5 s of updates and appears in finished_voices once") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 0.5), retained());
        CONJURE_REQUIRE_OK(voice);
        u32 reports = 0;
        for (u32 frame = 0; frame < 29; ++frame) {
            engine.update(testing::FRAME_SECONDS);
            engine.end_frame();
            reports += static_cast<u32>(engine.finished_voices().size());
        }
        CHECK(engine.is_playing(voice.value()));
        CHECK(reports == 0);

        engine.update(testing::FRAME_SECONDS);
        engine.end_frame();
        REQUIRE(engine.finished_voices().size() == 1);
        CHECK(engine.finished_voices()[0] == voice.value());
        CHECK_FALSE(engine.is_playing(voice.value()));
        CHECK(engine.voice(voice.value())->state == voice_state::FINISHED);
        CHECK(engine.voice(voice.value())->cursorFrames == 24000);

        engine.update(testing::FRAME_SECONDS);
        engine.end_frame();
        CHECK(engine.finished_voices().empty());
        CHECK(engine.is_valid(voice.value()));   // retained: stays until release
    }

    TEST_CASE("audio_engine - a one-shot slot is FREE after end_frame and its id invalid") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 0.1), play_params{});
        CONJURE_REQUIRE_OK(voice);
        CHECK_FALSE(engine.voice(voice.value())->retain);
        const u32 frames = testing::step_until_ended(engine, voice.value(), 20);
        CHECK(frames == 6);
        CHECK_FALSE(engine.is_valid(voice.value()));
        CHECK_FALSE(engine.voice(voice.value()).has_value());
        CHECK(engine.active_voice_count() == 0);
        testing::step_frames(engine, 1);
        CHECK(testing::slot_stats(engine, 0).state == voice_state::FINISHED);   // mixer slot idle until reuse
        const core::result<voice_id> next = engine.play(testing::make_sine_pcm(440.0, 0.1), retained());
        CONJURE_REQUIRE_OK(next);
        CHECK(next.value() != voice.value());
        testing::step_frames(engine, 1);
        CHECK(testing::slot_stats(engine, 0).state == voice_state::PLAYING);
        engine.release(next.value());
    }

    TEST_CASE("audio_engine - a retained voice stays FINISHED until release") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 0.05), retained());
        CONJURE_REQUIRE_OK(voice);
        testing::step_until_ended(engine, voice.value(), 20);
        testing::step_frames(engine, 5);
        CHECK(engine.is_valid(voice.value()));
        CHECK(engine.voice(voice.value())->state == voice_state::FINISHED);
        engine.release(voice.value());
        CHECK_FALSE(engine.is_valid(voice.value()));
        engine.release(voice.value());   // idempotent on a stale id
        CHECK(logs.contains(core::log_level::TRACE, "invalid voice"));
    }

    TEST_CASE("audio_engine - release on a playing voice stops it and erases it at the acknowledgement") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), retained(true));
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 1);
        engine.release(voice.value());
        CHECK(engine.is_valid(voice.value()));
        CHECK_FALSE(engine.is_playing(voice.value()));
        engine.update(testing::FRAME_SECONDS);
        engine.end_frame();
        CHECK_FALSE(engine.is_valid(voice.value()));
        CHECK(engine.finished_voices().size() == 1);
        CHECK(engine.active_voice_count() == 0);
    }

    TEST_CASE("audio_engine - loop wraps, counts LOOPED and keeps the cursor inside the clip") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 0.1), retained(true));
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 30);   // 0.5 s over a 0.1 s clip
        CHECK(engine.is_playing(voice.value()));
        CHECK(engine.stats().loops >= 4);
        CHECK(engine.voice(voice.value())->cursorFrames < 4800);
        CHECK(testing::slot_stats(engine, 0).loop);
        engine.set_loop(voice.value(), false);
        testing::step_frames(engine, 7);
        CHECK_FALSE(engine.is_playing(voice.value()));
    }

    TEST_CASE("audio_engine - stop ramps over 5 ms without a sample jump") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        null_backend &backend = *engine.null_backend_instance();
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(100.0, 1.0, 0.5f, 2), retained(true));
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 2);
        backend.set_capture_frames(1600);
        engine.stop(voice.value());
        CHECK_FALSE(engine.is_playing(voice.value()));
        CHECK(engine.voice(voice.value())->state == voice_state::STOPPING);
        engine.update(testing::FRAME_SECONDS);
        engine.end_frame();
        CHECK(engine.voice(voice.value())->state == voice_state::STOPPED);
        CHECK(engine.finished_voices().size() == 1);
        testing::step_frames(engine, 1);
        CHECK(testing::largest_step(backend.captured(), 2, 0) < 0.1f);
        CHECK(testing::peak_of(backend.captured()) > 0.1f);   // the ramp is audible, not an instant cut
        const std::span<const f32> tail = backend.captured().subspan(backend.captured().size() - 200);
        CHECK(testing::peak_of(tail) == doctest::Approx(0.0));
        CHECK(engine.finished_voices().empty());
        engine.stop(voice.value());   // already ended: no-op
    }

    TEST_CASE("audio_engine - stop_all ends every voice") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        for (u32 index = 0; index < 10; ++index) {
            CONJURE_REQUIRE_OK(engine.play(testing::make_sine_pcm(440.0, 1.0), retained(true)));
        }
        testing::step_frames(engine, 1);
        engine.stop_all();
        testing::step_frames(engine, 2);
        CHECK(engine.active_voice_count() == 0);
        CHECK(engine.stats().activeVoices == 0);
    }

    TEST_CASE("audio_engine - pause and resume freeze and continue the cursor") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), retained());
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 2);
        const u64 cursor = engine.voice(voice.value())->cursorFrames;
        CHECK(cursor == 1600);
        engine.pause(voice.value());
        CHECK(engine.is_playing(voice.value()));
        CHECK(engine.voice(voice.value())->state == voice_state::PAUSED);
        testing::step_frames(engine, 5);
        CHECK(engine.voice(voice.value())->cursorFrames == cursor);
        CHECK(testing::slot_stats(engine, 0).state == voice_state::PAUSED);
        engine.pause(voice.value());   // idempotent
        engine.resume(voice.value());
        testing::step_frames(engine, 1);
        CHECK(engine.voice(voice.value())->state == voice_state::PLAYING);
        CHECK(engine.voice(voice.value())->cursorFrames == cursor + 800);
        engine.resume(voice.value());   // idempotent
    }

    TEST_CASE("audio_engine - startPaused begins in PAUSED") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        play_params params = retained();
        params.startPaused = true;
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), params);
        CONJURE_REQUIRE_OK(voice);
        CHECK(engine.is_playing(voice.value()));
        testing::step_frames(engine, 2);
        CHECK(engine.voice(voice.value())->state == voice_state::PAUSED);
        CHECK(engine.voice(voice.value())->cursorFrames == 0);
        engine.resume(voice.value());
        testing::step_frames(engine, 1);
        CHECK(engine.voice(voice.value())->cursorFrames == 800);
    }

    TEST_CASE("audio_engine - seek moves the cursor and startSeconds seeks before start") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), retained());
        CONJURE_REQUIRE_OK(voice);
        engine.seek(voice.value(), 0.5);
        testing::step_frames(engine, 1);
        CHECK(engine.voice(voice.value())->cursorFrames == 24000 + 800);
        engine.seek(voice.value(), 99.0);   // clamped to the last frame
        testing::step_frames(engine, 1);
        CHECK_FALSE(engine.is_playing(voice.value()));

        play_params params = retained();
        params.startSeconds = 0.75;
        const core::result<voice_id> late = engine.play(testing::make_sine_pcm(440.0, 1.0), params);
        CONJURE_REQUIRE_OK(late);
        CHECK(engine.voice(late.value())->cursorFrames == 36000);
        testing::step_frames(engine, 1);
        CHECK(engine.voice(late.value())->cursorFrames == 36800);
    }

    /*-----------------------------------------------------------------------*/
    // Setters

    TEST_CASE("audio_engine - set_volume changes the gain the mixer sees and clamps") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), retained(true));
        CONJURE_REQUIRE_OK(voice);
        engine.set_volume(voice.value(), 0.25f);
        testing::step_frames(engine, 2);
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(0.25));
        CHECK(engine.voice(voice.value())->volume == doctest::Approx(0.25));
        engine.set_volume(voice.value(), 9.0f);
        testing::step_frames(engine, 2);
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(2.0));
        CHECK(engine.voice(voice.value())->effectiveGain == doctest::Approx(2.0));
    }

    TEST_CASE("audio_engine - pitch 2 finishes in half the time") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> normal = engine.play(testing::make_sine_pcm(440.0, 0.5), retained());
        const core::result<voice_id> fast = engine.play(testing::make_sine_pcm(440.0, 0.5), retained());
        CONJURE_REQUIRE_OK(normal);
        CONJURE_REQUIRE_OK(fast);
        engine.set_pitch(fast.value(), 2.0f);
        const u32 fastFrames = testing::step_until_ended(engine, fast.value(), 60);
        CHECK(fastFrames == 15);
        CHECK(engine.is_playing(normal.value()));
        const u32 normalFrames = fastFrames + testing::step_until_ended(engine, normal.value(), 60);
        CHECK(normalFrames == 30);
        CHECK(engine.voice(fast.value())->pitch == doctest::Approx(2.0));
        CHECK(testing::slot_stats(engine, 1).step == doctest::Approx(2.0));
    }

    TEST_CASE("audio_engine - pitch follows the clip rate: a 24 kHz clip steps at 0.5") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const std::shared_ptr<const pcm_buffer> lowRate = testing::make_sine_pcm(440.0, 0.5, 0.5f, 1, 24000);
        const core::result<voice_id> voice = engine.play(lowRate, retained());
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 1);
        CHECK(testing::slot_stats(engine, 0).step == doctest::Approx(0.5));
        CHECK(engine.voice(voice.value())->cursorFrames == 400);
        CHECK(testing::step_until_ended(engine, voice.value(), 60) == 29);
    }

    TEST_CASE("audio_engine - set_pan hard left silences the right channel") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        null_backend &backend = *engine.null_backend_instance();
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), retained(true));
        CONJURE_REQUIRE_OK(voice);
        engine.set_pan(voice.value(), -1.0f);
        testing::step_frames(engine, 2);
        backend.set_capture_frames(800);
        testing::step_frames(engine, 1);
        f32 leftPeak = 0.0f;
        f32 rightPeak = 0.0f;
        const std::span<const f32> captured = backend.captured();
        for (usize frame = 0; frame < captured.size() / 2; ++frame) {
            leftPeak = std::max(leftPeak, std::abs(captured[frame * 2]));
            rightPeak = std::max(rightPeak, std::abs(captured[frame * 2 + 1]));
        }
        CHECK(leftPeak == doctest::Approx(0.5).epsilon(0.01));
        CHECK(rightPeak == doctest::Approx(0.0).epsilon(1.0e-4));
        CHECK(testing::slot_stats(engine, 0).pan == doctest::Approx(-1.0));
    }

    TEST_CASE("audio_engine - a 3D voice at maxDistance renders at the attenuation floor and at 0 at full gain") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        play_params params = retained(true);
        params.spatial = true;
        params.minDistance = 1.0f;
        params.maxDistance = 100.0f;
        params.attenuation = attenuation_model::INVERSE;
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), params);
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 2);
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(1.0));   // defaults to the listener position

        engine.set_position(voice.value(), math::vec3(0.0f, 0.0f, -100.0f));
        testing::step_frames(engine, 3);
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(0.01));
        CHECK(engine.voice(voice.value())->effectiveGain == doctest::Approx(0.01));

        engine.set_position(voice.value(), math::vec3(0.0f, 0.0f, -5000.0f));
        testing::step_frames(engine, 3);
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(0.01));

        engine.set_position(voice.value(), math::vec3(0.0f));
        testing::step_frames(engine, 3);
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(1.0));

        engine.set_position(voice.value(), math::vec3(10.0f, 0.0f, 0.0f));
        testing::step_frames(engine, 3);
        CHECK(testing::slot_stats(engine, 0).pan == doctest::Approx(1.0));
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(0.1));
    }

    TEST_CASE("audio_engine - set_positions equals individual set_position calls") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        play_params params = retained(true);
        params.spatial = true;
        const core::result<voice_id> first = engine.play(testing::make_sine_pcm(440.0, 1.0), params);
        const core::result<voice_id> second = engine.play(testing::make_sine_pcm(440.0, 1.0), params);
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(second);
        engine.set_position(first.value(), math::vec3(0.0f, 0.0f, -10.0f));
        voice_transform transform = {};
        transform.voice = second.value();
        transform.position = math::vec3(0.0f, 0.0f, -10.0f);
        const std::vector<voice_transform> transforms = {transform};
        engine.set_positions(transforms);
        testing::step_frames(engine, 3);
        const f64 secondGain = static_cast<f64>(testing::slot_stats(engine, 1).gain);
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(secondGain));
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(0.1));
    }

    TEST_CASE("audio_engine - set_attenuation and apply_params change the model") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        play_params params = retained(true);
        params.spatial = true;
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), params);
        CONJURE_REQUIRE_OK(voice);
        engine.set_position(voice.value(), math::vec3(0.0f, 0.0f, -50.5f));
        attenuation_params linear = {};
        linear.model = attenuation_model::LINEAR;
        engine.set_attenuation(voice.value(), linear);
        testing::step_frames(engine, 3);
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(0.5));

        play_params replacement = params;
        replacement.attenuation = attenuation_model::NONE;
        replacement.volume = 0.5f;
        replacement.pitch = 0.5f;
        engine.apply_params(voice.value(), replacement);
        testing::step_frames(engine, 3);
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(0.5));
        CHECK(testing::slot_stats(engine, 0).step == doctest::Approx(0.5));
        CHECK(engine.voice(voice.value())->spatial);

        engine.set_spatial(voice.value(), false);
        testing::step_frames(engine, 3);
        CHECK_FALSE(engine.voice(voice.value())->spatial);
        CHECK(testing::slot_stats(engine, 0).pan == doctest::Approx(0.0));
    }

    TEST_CASE("audio_engine - apply_source applies every authored field of a component at once") {
        testing::log_capture logs = {};
        audio_settings settings = testing::null_settings();
        group_desc music = {};
        music.name = core::string_id("Music");
        music.gain = 0.5f;
        settings.groups.push_back(music);
        audio_engine engine(settings);
        const group_id musicGroup = engine.find_group(core::string_id("Music"));
        REQUIRE(musicGroup.is_valid());

        play_params params = retained(true);
        params.spatial = true;
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), params);
        CONJURE_REQUIRE_OK(voice);
        engine.set_position(voice.value(), math::vec3(0.0f, 0.0f, -50.5f));
        testing::step_frames(engine, 3);

        audio_source source = {};
        source.volume = 0.5f;
        source.pitch = 0.5f;
        source.loop = false;
        source.spatial = false;
        source.group = core::string_id("Music");
        source.attenuation = attenuation_model::NONE;
        engine.apply_source(voice.value(), source, engine.find_group(source.group));
        testing::step_frames(engine, 3);

        const std::optional<voice_info> info = engine.voice(voice.value());
        REQUIRE(info.has_value());
        CHECK(info->volume == doctest::Approx(0.5));
        CHECK(info->pitch == doctest::Approx(0.5));
        CHECK_FALSE(info->loop);
        CHECK_FALSE(info->spatial);
        CHECK(info->group == musicGroup);
        CHECK(info->retain);
        // The slot carries the voice gain (0.5, no attenuation) and its group; the group gain (0.5)
        // is applied by the mixer from the group table.
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(0.5));
        CHECK(testing::slot_stats(engine, 0).step == doctest::Approx(0.5));
        CHECK(testing::slot_stats(engine, 0).groupIndex == musicGroup.index);
        CHECK_FALSE(testing::slot_stats(engine, 0).loop);

        // The null name resolves to MASTER; an invalid group leaves the voice's group unchanged.
        source.group = {};
        engine.apply_source(voice.value(), source, engine.find_group(source.group));
        testing::step_frames(engine, 1);
        CHECK(engine.voice(voice.value())->group == MASTER_GROUP);
        engine.apply_source(voice.value(), source, group_id{});
        testing::step_frames(engine, 1);
        CHECK(engine.voice(voice.value())->group == MASTER_GROUP);

        // A stale id is a no-op with a TRACE.
        engine.stop(voice.value(), 0.0f);
        engine.release(voice.value());
        testing::step_frames(engine, 3);
        const usize tracesBefore = logs.count(core::log_level::TRACE);
        engine.apply_source(voice.value(), source, MASTER_GROUP);
        CHECK(logs.count(core::log_level::TRACE) > tracesBefore);
    }

    TEST_CASE("audio_engine - Doppler changes the step when enabled") {
        testing::log_capture logs = {};
        audio_settings settings = testing::null_settings();
        settings.dopplerFactor = 1.0f;
        audio_engine engine(settings);
        play_params params = retained(true);
        params.spatial = true;
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), params);
        CONJURE_REQUIRE_OK(voice);
        engine.set_position(voice.value(), math::vec3(0.0f, 0.0f, -10.0f), math::vec3(0.0f, 0.0f, 34.3f));
        testing::step_frames(engine, 3);
        CHECK(testing::slot_stats(engine, 0).step == doctest::Approx(1.0 / 0.9).epsilon(1.0e-3));
    }

    TEST_CASE("audio_engine - listener gain scales everything") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> flat = engine.play(testing::make_sine_pcm(440.0, 1.0), retained(true));
        play_params spatialParams = retained(true);
        spatialParams.spatial = true;
        const core::result<voice_id> spatial = engine.play(testing::make_sine_pcm(440.0, 1.0), spatialParams);
        CONJURE_REQUIRE_OK(flat);
        CONJURE_REQUIRE_OK(spatial);
        listener_state listener = {};
        listener.gain = 0.5f;
        engine.set_listener(listener);
        CHECK(engine.listener().gain == doctest::Approx(0.5));
        testing::step_frames(engine, 3);
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(0.5));
        CHECK(testing::slot_stats(engine, 1).gain == doctest::Approx(0.5));
        CHECK(engine.null_backend_instance()->stats().listener.gain == doctest::Approx(0.5));
    }

    TEST_CASE("audio_engine - set_paused renders zeros and holds cursors") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        null_backend &backend = *engine.null_backend_instance();
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), retained(true));
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 2);
        const u64 cursor = engine.voice(voice.value())->cursorFrames;
        engine.set_paused(true);
        CHECK(engine.is_paused());
        backend.set_capture_frames(1600);
        testing::step_frames(engine, 3);
        CHECK(testing::peak_of(backend.captured()) == doctest::Approx(0.0));
        CHECK(engine.voice(voice.value())->cursorFrames == cursor);
        CHECK(engine.is_playing(voice.value()));
        engine.set_paused(false);
        testing::step_frames(engine, 2);
        CHECK(engine.voice(voice.value())->cursorFrames > cursor);
    }

    /*-----------------------------------------------------------------------*/
    // Limits and stealing

    TEST_CASE("audio_engine - 256 simultaneous voices all play and the 257th steals the lowest priority") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const std::shared_ptr<const pcm_buffer> clip = testing::make_sine_pcm(440.0, 2.0, 0.01f);
        std::vector<voice_id> voices = {};
        for (u32 index = 0; index < MAX_VOICES; ++index) {
            CAPTURE(index);
            play_params params = retained(true);
            params.retain = false;
            params.priority = index == 17 ? static_cast<u8>(10) : static_cast<u8>(128);
            const core::result<voice_id> voice = engine.play(clip, params);
            CONJURE_REQUIRE_OK(voice);
            voices.push_back(voice.value());
        }
        testing::step_frames(engine, 1);
        CHECK(engine.stats().activeVoices == MAX_VOICES);
        CHECK(engine.active_voice_count() == MAX_VOICES);
        for (u32 index = 0; index < MAX_VOICES; ++index) {
            CAPTURE(index);
            CHECK(testing::slot_stats(engine, index).state == voice_state::PLAYING);
        }

        const core::result<voice_id> extra = engine.play(clip, retained(true));
        CONJURE_REQUIRE_OK(extra);
        CHECK(engine.is_playing(extra.value()));
        CHECK(engine.voice(extra.value())->state == voice_state::PRIMING);
        CHECK_FALSE(engine.is_playing(voices[17]));   // the victim is fading out
        testing::step_frames(engine, 1);
        CHECK_FALSE(engine.is_valid(voices[17]));
        CHECK(engine.voice(extra.value())->state == voice_state::PLAYING);
        testing::step_frames(engine, 1);
        CHECK(testing::slot_stats(engine, 17).state == voice_state::PLAYING);
        CHECK(engine.stats().activeVoices == MAX_VOICES);
        CHECK(engine.stats().voicesDropped == 0);
        CHECK(engine.stats().peakVoices == MAX_VOICES + 1);
    }

    TEST_CASE("audio_engine - VOICE_LIMIT_REACHED when every voice is a high-priority retained loop") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const std::shared_ptr<const pcm_buffer> clip = testing::make_sine_pcm(440.0, 2.0, 0.01f);
        play_params params = retained(true);
        params.priority = 255;
        for (u32 index = 0; index < MAX_VOICES; ++index) {
            CAPTURE(index);
            CONJURE_REQUIRE_OK(engine.play(clip, params));
        }
        testing::step_frames(engine, 1);
        play_params low = params;
        low.priority = 1;
        CONJURE_CHECK_ERROR(engine.play(clip, low), audio_error::VOICE_LIMIT_REACHED);
        CONJURE_CHECK_ERROR(engine.play(clip, params), audio_error::VOICE_LIMIT_REACHED);
        CHECK(engine.stats().voicesDropped == 2);
        CHECK(engine.active_voice_count() == MAX_VOICES);
    }

    TEST_CASE("audio_engine - setters on a stale voice_id are no-ops") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 0.05), play_params{});
        CONJURE_REQUIRE_OK(voice);
        testing::step_until_ended(engine, voice.value(), 20);
        const voice_id stale = voice.value();
        CHECK_FALSE(engine.is_valid(stale));
        engine.set_volume(stale, 0.5f);
        engine.set_pitch(stale, 2.0f);
        engine.set_pan(stale, 1.0f);
        engine.set_loop(stale, true);
        engine.set_spatial(stale, true);
        engine.set_priority(stale, 1);
        engine.set_group(stale, MASTER_GROUP);
        engine.set_position(stale, math::vec3(1.0f));
        engine.set_attenuation(stale, attenuation_params{});
        engine.apply_params(stale, play_params{});
        engine.seek(stale, 1.0);
        engine.pause(stale);
        engine.resume(stale);
        engine.stop(stale);
        engine.release(stale);
        CHECK_FALSE(engine.voice(stale).has_value());
        CHECK_FALSE(engine.is_playing(stale));
        CHECK_FALSE(engine.is_valid(voice_id{}));
        voice_id garbage = {};
        garbage.key.index = 100000;
        garbage.key.generation = 5;
        CHECK_FALSE(engine.is_valid(garbage));
        CHECK(logs.count(core::log_level::TRACE) >= 10);
        testing::step_frames(engine, 1);
    }

    TEST_CASE("audio_engine - non-finite setter arguments assert and are sanitised") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), retained(true));
        CONJURE_REQUIRE_OK(voice);
        const f32 nan = std::numeric_limits<f32>::quiet_NaN();
        CONJURE_CHECK_ASSERTS(engine.set_volume(voice.value(), nan));
        CONJURE_CHECK_ASSERTS(engine.set_pitch(voice.value(), nan));
        CONJURE_CHECK_ASSERTS(engine.set_pan(voice.value(), nan));
        CONJURE_CHECK_ASSERTS(engine.set_position(voice.value(), math::vec3(nan)));
        CONJURE_CHECK_ASSERTS(engine.set_group_gain(MASTER_GROUP, nan));
        testing::step_frames(engine, 2);
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(1.0));
        CHECK(testing::slot_stats(engine, 0).step == doctest::Approx(1.0));
        CHECK(engine.group(MASTER_GROUP)->gain == doctest::Approx(1.0));
    }

    TEST_CASE("audio_engine - 10000 set_volume calls in one frame are all applied and spill") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), retained(true));
        CONJURE_REQUIRE_OK(voice);
        for (u32 index = 0; index < 10000; ++index) {
            engine.set_volume(voice.value(), static_cast<f32>(index % 7) * 0.1f);
        }
        engine.set_volume(voice.value(), 0.3f);
        CHECK(engine.stats().commandsSpilled > 0);
        CHECK(engine.stats().commandsSent >= 10001);
        testing::step_frames(engine, 3);
        CHECK(testing::slot_stats(engine, 0).gain == doctest::Approx(0.3));
        CHECK(engine.stats().commandsSpilled < engine.stats().commandsSent);
    }

    /*-----------------------------------------------------------------------*/
    // Streaming

    TEST_CASE("audio_engine - a streamed clip with jobs == nullptr matches the PCM render sample for sample") {
        testing::log_capture logs = {};
        const audio_clip streamed = streamed_clip(0.3);
        const audio_clip pcm = pcm_clip_of(streamed);
        REQUIRE(streamed.is_streamed());
        REQUIRE(pcm.info.frameCount == streamed.info.frameCount);

        audio_engine streamEngine(testing::null_settings());
        const core::result<voice_id> streamVoice = streamEngine.play(streamed, retained());
        CONJURE_REQUIRE_OK(streamVoice);
        CHECK(streamVoice.value().is_valid());
        CHECK(streamEngine.voice(streamVoice.value())->streamed);
        CHECK(streamEngine.stats().streamingVoices == 1);
        const std::vector<f32> streamedRender = render_whole(streamEngine, streamVoice.value(), 60);
        CHECK_FALSE(streamEngine.is_playing(streamVoice.value()));
        CHECK(streamEngine.voice(streamVoice.value())->state == voice_state::FINISHED);
        CHECK(streamEngine.stats().underruns == 0);
        streamEngine.release(streamVoice.value());
        streamEngine.end_frame();
        CHECK(streamEngine.stats().streamingVoices == 0);

        audio_engine pcmEngine(testing::null_settings());
        const core::result<voice_id> pcmVoice = pcmEngine.play(pcm, retained());
        CONJURE_REQUIRE_OK(pcmVoice);
        const std::vector<f32> pcmRender = render_whole(pcmEngine, pcmVoice.value(), 60);

        REQUIRE(streamedRender.size() == pcmRender.size());
        CHECK(testing::peak_of(pcmRender) > 0.3f);
        usize mismatches = 0;
        for (usize index = 0; index < pcmRender.size(); ++index) {
            if (streamedRender[index] != pcmRender[index]) {
                ++mismatches;
            }
        }
        CHECK(mismatches == 0);
    }

    TEST_CASE("audio_engine - a streamed clip completes with a 2-worker job system") {
        testing::log_capture logs = {};
        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = 2;
        CONJURE_REQUIRE_OK(jobs.start(desc));
        {
            audio_settings settings = testing::null_settings();
            settings.jobs = &jobs;
            audio_engine engine(settings);
            const audio_clip clip = streamed_clip(0.5, 2);
            const core::result<voice_id> voice = engine.play(clip, retained());
            CONJURE_REQUIRE_OK(voice);
            CHECK(engine.is_playing(voice.value()));
            u32 frames = 0;
            while (engine.is_playing(voice.value()) && frames < 2000) {
                engine.update(testing::FRAME_SECONDS);
                engine.end_frame();
                ++frames;
            }
            CHECK(frames >= 30);
            CHECK(frames < 2000);
            CHECK(engine.voice(voice.value())->state == voice_state::FINISHED);
            CHECK(engine.voice(voice.value())->cursorFrames == 24000);
        }
        jobs.stop();
    }

    TEST_CASE("audio_engine - streamed voices loop, seek and stream slots are limited to 32") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const audio_clip clip = streamed_clip(0.1);
        const core::result<voice_id> looping = engine.play(clip, retained(true));
        CONJURE_REQUIRE_OK(looping);
        testing::step_frames(engine, 20);
        CHECK(engine.is_playing(looping.value()));
        CHECK(engine.stats().loops >= 2);
        CHECK(engine.voice(looping.value())->cursorFrames < 4800);

        engine.seek(looping.value(), 0.05);
        testing::step_frames(engine, 3);
        CHECK(engine.is_playing(looping.value()));
        CHECK(engine.voice(looping.value())->state == voice_state::PLAYING);
        engine.release(looping.value());
        testing::step_frames(engine, 2);

        std::vector<voice_id> voices = {};
        for (u32 index = 0; index < MAX_STREAMING_VOICES; ++index) {
            CAPTURE(index);
            const core::result<voice_id> voice = engine.play(clip, retained(true));
            CONJURE_REQUIRE_OK(voice);
            voices.push_back(voice.value());
        }
        CHECK(engine.stats().streamingVoices == MAX_STREAMING_VOICES);
        CONJURE_CHECK_ERROR(engine.play(clip, retained(true)), audio_error::STREAM_SLOT_LIMIT);
        CHECK(engine.stats().voicesDropped == 1);
        testing::step_frames(engine, 3);
        CHECK(engine.active_voice_count() == MAX_STREAMING_VOICES);
        engine.release(voices[0]);
        testing::step_frames(engine, 2);
        CONJURE_REQUIRE_OK(engine.play(clip, retained(true)));
    }

    /*-----------------------------------------------------------------------*/
    // Lifetime

    TEST_CASE("audio_engine - a voice keeps playing the old pcm_buffer after the clip is republished") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        std::shared_ptr<const pcm_buffer> original = testing::make_sine_pcm(440.0, 1.0);
        audio_clip clip = make_pcm_clip(original);
        const core::result<voice_id> voice = engine.play(clip, retained(true), 1);
        CONJURE_REQUIRE_OK(voice);
        CHECK(original.use_count() == 3);   // local, clip, voice

        clip = make_pcm_clip(testing::make_sine_pcm(880.0, 0.5));   // "hot reload" publishes a new buffer
        CHECK(original.use_count() == 2);
        testing::step_frames(engine, 3);
        CHECK(engine.is_playing(voice.value()));
        CHECK(engine.voice(voice.value())->clipVersion == 1);
        CHECK(engine.voice(voice.value())->clipFrames == 48000);

        engine.release(voice.value());
        testing::step_frames(engine, 2);
        CHECK(original.use_count() == 1);
    }

    TEST_CASE("audio_engine - destruction with 100 live voices releases everything") {
        testing::memory_guard guard = {};
        {
            // The capture lives inside the guarded scope: its stored messages are allocations too.
            testing::log_capture logs = {};
            audio_engine engine(testing::null_settings());
            const std::shared_ptr<const pcm_buffer> clip = testing::make_sine_pcm(440.0, 1.0, 0.1f, 2);
            const audio_clip streamed = streamed_clip(0.5);
            for (u32 index = 0; index < 90; ++index) {
                CAPTURE(index);
                CONJURE_REQUIRE_OK(engine.play(clip, retained(index % 2 == 0)));
            }
            for (u32 index = 0; index < 10; ++index) {
                CAPTURE(index);
                CONJURE_REQUIRE_OK(engine.play(streamed, retained(true)));
            }
            testing::step_frames(engine, 3);
            CHECK(engine.active_voice_count() == 100);
            CHECK(logs.count(core::log_level::ERR) == 0);
        }
        guard.check();
    }

    TEST_CASE("audio_engine - destruction logs the live retained voices") {
        testing::log_capture logs = {};
        {
            audio_engine engine(testing::null_settings());
            CONJURE_REQUIRE_OK(engine.play(testing::make_sine_pcm(440.0, 1.0), retained(true)));
            CONJURE_REQUIRE_OK(engine.play(testing::make_sine_pcm(440.0, 1.0), retained(true)));
            testing::step_frames(engine, 1);
        }
        CHECK(logs.contains(core::log_level::INFO, "2 live retained voice"));
    }

    TEST_CASE("audio_engine - update and end_frame tolerate zero and negative deltas") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), retained());
        CONJURE_REQUIRE_OK(voice);
        engine.update(0.0f);
        engine.update(-1.0f);
        engine.update(std::numeric_limits<f32>::quiet_NaN());
        engine.end_frame();
        CHECK(engine.voice(voice.value())->cursorFrames == 0);
        CHECK(engine.stats().framesRendered == 0);
        CHECK(engine.is_playing(voice.value()));
    }

    /*-----------------------------------------------------------------------*/
    // Groups through the voice API

    TEST_CASE("audio_engine - play with a stale or unknown group fails with INVALID_GROUP") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        group_id stale = {};
        stale.index = 5;
        stale.generation = 3;
        play_params params = retained();
        params.group = stale;
        CONJURE_CHECK_ERROR(engine.play(testing::make_sine_pcm(440.0, 1.0), params), audio_error::INVALID_GROUP);

        group_desc desc = {};
        desc.name = core::string_id("Removed");
        const core::result<group_id> removed = engine.create_group(desc);
        CONJURE_REQUIRE_OK(removed);
        CONJURE_REQUIRE_OK(engine.remove_group(removed.value()));
        params.group = removed.value();
        CONJURE_CHECK_ERROR(engine.play(testing::make_sine_pcm(440.0, 1.0), params), audio_error::INVALID_GROUP);
        // The group is checked before a stream slot is taken.
        CONJURE_CHECK_ERROR(engine.play(streamed_clip(0.1), params), audio_error::INVALID_GROUP);
        CHECK(engine.stats().streamingVoices == 0);
        CHECK(engine.active_voice_count() == 0);
        CHECK(logs.contains(core::log_level::TRACE, "invalid or stale group"));

        params.group = MASTER_GROUP;
        CONJURE_REQUIRE_OK(engine.play(testing::make_sine_pcm(440.0, 1.0), params));
    }

    TEST_CASE("audio_engine - set_group and apply_params move a voice to a live group") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        group_desc musicDesc = {};
        musicDesc.name = core::string_id("Music");
        musicDesc.gain = 0.5f;
        group_desc sfxDesc = {};
        sfxDesc.name = core::string_id("Sfx");
        sfxDesc.gain = 0.25f;
        const core::result<group_id> music = engine.create_group(musicDesc);
        const core::result<group_id> sfx = engine.create_group(sfxDesc);
        CONJURE_REQUIRE_OK(music);
        CONJURE_REQUIRE_OK(sfx);

        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), retained(true));
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 1);
        CHECK(testing::slot_stats(engine, 0).groupIndex == MASTER_GROUP.index);
        CHECK(engine.voice(voice.value())->effectiveGain == doctest::Approx(1.0));

        engine.set_group(voice.value(), music.value());
        CHECK(engine.voice(voice.value())->group == music.value());
        CHECK(engine.voice(voice.value())->effectiveGain == doctest::Approx(0.5));
        testing::step_frames(engine, 1);
        CHECK(testing::slot_stats(engine, 0).groupIndex == music.value().index);

        play_params moved = retained(true);
        moved.group = sfx.value();
        engine.apply_params(voice.value(), moved);
        testing::step_frames(engine, 1);
        CHECK(engine.voice(voice.value())->group == sfx.value());
        CHECK(engine.voice(voice.value())->effectiveGain == doctest::Approx(0.25));
        CHECK(testing::slot_stats(engine, 0).groupIndex == sfx.value().index);

        // apply_params with a stale group keeps the current one.
        moved.group.index = 9;
        moved.group.generation = 9;
        engine.apply_params(voice.value(), moved);
        testing::step_frames(engine, 1);
        CHECK(engine.voice(voice.value())->group == sfx.value());
        CHECK(testing::slot_stats(engine, 0).groupIndex == sfx.value().index);
    }

    /*-----------------------------------------------------------------------*/
    // Regressions from the module review

    TEST_CASE("regression - audio review 1 device loss keeps retrying after a failed restart and recovers") {
        testing::log_capture logs = {};
        audio_settings settings = testing::null_settings();
        settings.recoveryBackoffSeconds = 2.0f * testing::FRAME_SECONDS;
        settings.maxRecoveryAttempts = 5;
        audio_engine engine(settings);
        null_backend &backend = *engine.null_backend_instance();
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0), retained(true));
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 2);
        const u64 frozenCursor = engine.voice(voice.value())->cursorFrames;
        CHECK(frozenCursor == 1600);

        // The first two restarts fail, which leaves the backend STOPPED rather than LOST: the
        // engine must keep retrying from its own state (A14), not from the backend's.
        backend.fail_next_starts(2);
        backend.simulate_device_loss();
        testing::step_frames(engine, 1);
        CHECK(engine.stats().deviceState == device_state::LOST);
        CHECK(engine.is_playing(voice.value()));
        CHECK(engine.voice(voice.value())->cursorFrames == frozenCursor);
        CHECK(logs.contains(core::log_level::WARN, "audio device lost"));

        u32 frames = 1;
        while (engine.stats().deviceRestarts == 0 && frames < 20) {
            testing::step_frames(engine, 1);
            ++frames;
            if (engine.stats().deviceRestarts == 0) {
                CHECK(engine.stats().deviceState == device_state::LOST);
                CHECK(engine.voice(voice.value())->cursorFrames == frozenCursor);   // frozen while down
            }
        }
        CHECK(frames == 6);   // attempts after 2, 4 and 6 frames of backoff; the third succeeds
        CHECK(engine.stats().deviceRestarts == 1);
        CHECK_FALSE(engine.stats().fellBackToNull);
        CHECK(engine.backend_kind() == audio_backend_kind::NULL_BACKEND);
        CHECK(engine.null_backend_instance() == &backend);
        CHECK(engine.stats().deviceState == device_state::RUNNING);
        CHECK(logs.contains(core::log_level::INFO, "recovered after 3 attempt"));

        testing::step_frames(engine, 2);
        CHECK(engine.voice(voice.value())->cursorFrames > frozenCursor);
        CHECK(engine.is_playing(voice.value()));
    }

    TEST_CASE("regression - audio review 1 device loss falls back to the null backend after maxRecoveryAttempts") {
        testing::log_capture logs = {};
        audio_settings settings = testing::null_settings();
        settings.recoveryBackoffSeconds = 0.0f;
        settings.maxRecoveryAttempts = 3;
        audio_engine engine(settings);
        null_backend &backend = *engine.null_backend_instance();
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 0.2), retained());
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 2);

        backend.fail_next_starts(100);
        backend.simulate_device_loss();
        testing::step_frames(engine, 1);   // attempt 1
        CHECK(engine.stats().deviceState == device_state::LOST);
        CHECK_FALSE(engine.stats().fellBackToNull);
        testing::step_frames(engine, 1);   // attempt 2
        CHECK_FALSE(engine.stats().fellBackToNull);
        CHECK(engine.is_playing(voice.value()));
        testing::step_frames(engine, 1);   // attempt 3: give up, swap in a fresh null backend
        CHECK(engine.stats().fellBackToNull);
        CHECK(engine.backend_kind() == audio_backend_kind::NULL_BACKEND);
        REQUIRE(engine.null_backend_instance() != nullptr);
        CHECK(engine.stats().deviceState == device_state::RUNNING);
        REQUIRE(engine.last_backend_error() != nullptr);
        CHECK(engine.last_backend_error()->is(audio_error::DEVICE_START_FAILED));
        CHECK(logs.contains(core::log_level::WARN, "falling back to the null backend"));

        // The frozen voice finishes on the null backend instead of hanging whoever waits for it.
        const u32 frames = testing::step_until_ended(engine, voice.value(), 30);
        CHECK(frames < 30);
        CHECK_FALSE(engine.is_playing(voice.value()));
        CHECK(engine.voice(voice.value())->state == voice_state::FINISHED);
        CHECK(engine.stats().deviceRestarts == 0);
    }

    TEST_CASE("regression - audio review 2 a streamed clip that fails to open releases its stream slot") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const audio_clip garbage = undecodable_streamed_clip();
        REQUIRE(garbage.is_valid());
        for (u32 attempt = 0; attempt < MAX_STREAMING_VOICES; ++attempt) {
            CAPTURE(attempt);
            CONJURE_CHECK_ERROR(engine.play(garbage, retained()), audio_error::DECODE_FAILED);
        }
        CHECK(engine.stats().streamingVoices == 0);
        CHECK(engine.active_voice_count() == 0);
        CHECK(engine.stats().voicesDropped == MAX_STREAMING_VOICES);

        const audio_clip valid = streamed_clip(0.1);
        const core::result<voice_id> voice = engine.play(valid, retained());
        CONJURE_REQUIRE_OK(voice);
        CHECK(engine.stats().streamingVoices == 1);
        testing::step_until_ended(engine, voice.value(), 20);
        CHECK(engine.voice(voice.value())->state == voice_state::FINISHED);
    }

    TEST_CASE("regression - audio review 5 a streamed seek while a decode job is in flight lands at the target") {
        testing::log_capture logs = {};
        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = 2;
        CONJURE_REQUIRE_OK(jobs.start(desc));
        {
            audio_settings settings = testing::null_settings();
            settings.jobs = &jobs;
            audio_engine engine(settings);
            const audio_clip clip = streamed_clip(0.5);
            const core::result<voice_id> voice = engine.play(clip, retained());
            CONJURE_REQUIRE_OK(voice);
            engine.seek(voice.value(), 0.25);   // the first decode job may still be writing the ring
            u32 frames = 0;
            bool started = false;
            u64 firstCursor = 0;
            while (engine.is_playing(voice.value()) && frames < 2000) {
                engine.update(testing::FRAME_SECONDS);
                engine.end_frame();
                ++frames;
                if (!started && engine.voice(voice.value())->state == voice_state::PLAYING) {
                    started = true;
                    firstCursor = engine.voice(voice.value())->cursorFrames;
                }
            }
            CHECK(started);
            CHECK(firstCursor >= 12000);
            CHECK(firstCursor < 24000);
            CHECK(frames < 2000);
            CHECK(engine.voice(voice.value())->state == voice_state::FINISHED);
            CHECK(engine.voice(voice.value())->cursorFrames == 24000);
        }
        jobs.stop();
    }

    TEST_CASE("regression - audio review 5 pause and seek in the same frame reset the stream after the mixer paused") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const audio_clip clip = streamed_clip(0.5);
        const core::result<voice_id> voice = engine.play(clip, retained());
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 2);
        engine.pause(voice.value());
        engine.seek(voice.value(), 0.25);   // PAUSE is still travelling through the command ring
        testing::step_frames(engine, 2);
        CHECK(engine.voice(voice.value())->state == voice_state::PAUSED);
        CHECK(engine.voice(voice.value())->cursorFrames == 12000);
        testing::step_frames(engine, 2);
        CHECK(engine.voice(voice.value())->cursorFrames == 12000);

        engine.resume(voice.value());
        testing::step_frames(engine, 1);
        CHECK(engine.voice(voice.value())->state == voice_state::PLAYING);
        CHECK(engine.voice(voice.value())->cursorFrames == 12800);
        testing::step_until_ended(engine, voice.value(), 40);
        CHECK(engine.voice(voice.value())->cursorFrames == 24000);
        CHECK(engine.stats().underruns == 0);
    }

    TEST_CASE("regression - audio review 5 a streamed voice waiting for a stolen slot seeks before it starts") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        const std::shared_ptr<const pcm_buffer> filler = testing::make_sine_pcm(440.0, 2.0, 0.01f);
        play_params fillerParams = {};
        fillerParams.loop = true;   // one-shot loops are stealable
        for (u32 index = 0; index < MAX_VOICES; ++index) {
            CAPTURE(index);
            CONJURE_REQUIRE_OK(engine.play(filler, fillerParams));
        }
        testing::step_frames(engine, 1);

        play_params params = retained();
        params.priority = 200;
        const core::result<voice_id> voice = engine.play(streamed_clip(0.5), params);
        CONJURE_REQUIRE_OK(voice);
        CHECK(engine.voice(voice.value())->state == voice_state::PRIMING);
        engine.seek(voice.value(), 0.25);
        testing::step_frames(engine, 2);   // the victim fades out and the streamed voice takes its slot
        CHECK(engine.voice(voice.value())->state == voice_state::PLAYING);
        const u64 cursor = engine.voice(voice.value())->cursorFrames;
        CHECK(cursor >= 12000);
        CHECK(cursor <= 12000 + 2 * testing::FRAME_FRAMES);
        const u32 frames = testing::step_until_ended(engine, voice.value(), 40);
        CHECK(frames <= 16);
        CHECK(engine.voice(voice.value())->cursorFrames == 24000);
    }

    TEST_CASE("regression - audio review 8 stopping a paused voice stays silent and reports STOPPED") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        null_backend &backend = *engine.null_backend_instance();
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 1.0, 1.0f), retained(true));
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 2);
        engine.pause(voice.value());
        testing::step_frames(engine, 1);
        backend.set_capture_frames(2 * testing::FRAME_FRAMES);
        testing::step_frames(engine, 1);
        CHECK(testing::peak_of(backend.captured()) == doctest::Approx(0.0));

        engine.stop(voice.value());
        testing::step_frames(engine, 1);
        CHECK(testing::peak_of(backend.captured()) == doctest::Approx(0.0));
        CHECK(engine.voice(voice.value())->state == voice_state::STOPPED);
        CHECK(engine.finished_voices().size() == 1);
        CHECK(testing::slot_stats(engine, 0).state == voice_state::STOPPED);
    }

    TEST_CASE("regression - audio review 9 set_spatial on a stereo voice switches the downmix and pan law") {
        testing::log_capture logs = {};
        audio_engine engine(testing::null_settings());
        null_backend &backend = *engine.null_backend_instance();
        const core::result<voice_id> voice = engine.play(make_unbalanced_stereo(), retained(true));
        CONJURE_REQUIRE_OK(voice);
        testing::step_frames(engine, 2);
        backend.set_capture_frames(testing::FRAME_FRAMES);
        testing::step_frames(engine, 1);
        stereo_peaks flat = peaks_of(backend.captured());
        CHECK(flat.left == doctest::Approx(0.8).epsilon(0.01));
        CHECK(flat.right == doctest::Approx(0.2).epsilon(0.01));

        // At the listener position a spatial voice is centred: mono 0.5 through constant power.
        engine.set_spatial(voice.value(), true);
        testing::step_frames(engine, 3);
        backend.set_capture_frames(testing::FRAME_FRAMES);
        testing::step_frames(engine, 1);
        const f32 centred = 0.5f * std::cos(math::QUARTER_PI);
        const stereo_peaks spatial = peaks_of(backend.captured());
        CHECK(spatial.left == doctest::Approx(static_cast<f64>(centred)).epsilon(0.01));
        CHECK(spatial.right == doctest::Approx(static_cast<f64>(centred)).epsilon(0.01));
        CHECK(engine.voice(voice.value())->spatial);

        engine.set_spatial(voice.value(), false);
        testing::step_frames(engine, 3);
        backend.set_capture_frames(testing::FRAME_FRAMES);
        testing::step_frames(engine, 1);
        flat = peaks_of(backend.captured());
        CHECK(flat.left == doctest::Approx(0.8).epsilon(0.01));
        CHECK(flat.right == doctest::Approx(0.2).epsilon(0.01));
    }
}
