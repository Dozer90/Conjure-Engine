#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"

#include "wav_builder.h"

#include "audio/commands.h"
#include "audio/mixer.h"
#include "conjure/audio/clip.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

// Private-header test of the hot path (docs/design/12_audio.md 9 step 4 allows it for the mixer).

using namespace conjure;
using namespace conjure::audio;

namespace {
    voice_command play_command(const pcm_buffer &pcm, u32 slot, u32 generation, f32 gain = 1.0f, f32 pan = 0.0f) {
        voice_command command = {};
        command.kind = command_kind::PLAY;
        command.voiceIndex = slot;
        command.generation = generation;
        command.payload.play.pcm = &pcm;
        command.payload.play.gain = gain;
        command.payload.play.pan = pan;
        command.payload.play.step = 1.0;
        command.payload.play.channels = pcm.format().channels;
        return command;
    }

    voice_command simple_command(command_kind kind, u32 slot, u32 generation) {
        voice_command command = {};
        command.kind = kind;
        command.voiceIndex = slot;
        command.generation = generation;
        return command;
    }

    std::vector<f32> render(mixer &mix, u32 frames) {
        std::vector<f32> out(static_cast<usize>(frames) * MIX_CHANNELS, 0.0f);
        mix.render(out.data(), frames);
        return out;
    }

    std::vector<voice_event> drain(mixer &mix) {
        std::vector<voice_event> events = {};
        voice_event event = {};
        while (mix.events().try_pop(event)) {
            events.push_back(event);
        }
        return events;
    }

    usize count_events(const std::vector<voice_event> &events, event_kind kind) {
        return static_cast<usize>(std::count_if(events.begin(), events.end(), [kind](const voice_event &event) {
            return event.kind == kind;
        }));
    }
}

TEST_SUITE("audio/mixer") {

    TEST_CASE("mixer - a fresh mixer renders silence and counts calls") {
        mixer mix(48000);
        const std::vector<f32> out = render(mix, 1000);
        CHECK(testing::peak_of(out) == doctest::Approx(0.0));
        CHECK(mix.render_calls() == 1);
        CHECK(mix.frames_rendered() == 1000);
        CHECK(mix.active_voices() == 0);
        CHECK(mix.slot_state(0) == voice_state::FREE);
        CHECK(mix.group_gain(0) == doctest::Approx(1.0));
        CHECK_FALSE(mix.master_paused());
    }

    TEST_CASE("mixer - gain ramps are bounded: no step exceeds the ramp slope") {
        mixer mix(48000);
        const std::shared_ptr<const pcm_buffer> pcm = testing::make_pcm([] {
            testing::wav_spec spec = {};
            spec.shape = testing::wave_shape::SQUARE;
            spec.frequency = 1.0;   // Constant +amplitude for the first half second.
            spec.amplitude = 1.0f;
            spec.channels = 2;
            return spec;
        }());
        REQUIRE(mix.commands().try_push(play_command(*pcm, 0, 1, 1.0f)));
        static_cast<void>(render(mix, 256));

        voice_command setGain = simple_command(command_kind::SET_GAIN, 0, 1);
        setGain.payload.scalar = 0.0f;
        setGain.fadeFrames = 240;
        REQUIRE(mix.commands().try_push(setGain));
        const std::vector<f32> out = render(mix, 512);
        CHECK(testing::largest_step(out, 2, 0) <= 1.0f / 240.0f + 1.0e-5f);
        CHECK(out[0] == doctest::Approx(1.0 - 1.0 / 240.0).epsilon(1.0e-4));
        CHECK(out[2 * 300] == doctest::Approx(0.0));
        CHECK(mix.slot(0).gainCurrent == doctest::Approx(0.0));
        CHECK(mix.slot_state(0) == voice_state::PLAYING);
    }

    TEST_CASE("mixer - linear interpolation at fractional cursors matches a reference") {
        mixer mix(48000);
        audio_format format = {};
        format.channels = 1;
        std::vector<f32> ramp(1000, 0.0f);
        for (usize index = 0; index < ramp.size(); ++index) {
            ramp[index] = static_cast<f32>(index) * 0.001f;
        }
        const pcm_buffer pcm(format, ramp);
        voice_command play = play_command(pcm, 3, 7, 1.0f, 0.0f);
        play.payload.play.step = 0.25;
        REQUIRE(mix.commands().try_push(play));
        const std::vector<f32> out = render(mix, 100);
        const f32 centre = std::cos(0.25f * 3.14159265f);   // constant power at pan 0
        for (u32 frame = 0; frame < 100; ++frame) {
            CAPTURE(frame);
            const f32 expected = static_cast<f32>(frame) * 0.25f * 0.001f * centre;
            CHECK(out[frame * 2] == doctest::Approx(static_cast<f64>(expected)).epsilon(1.0e-4));
            CHECK(out[frame * 2 + 1] == doctest::Approx(static_cast<f64>(expected)).epsilon(1.0e-4));
        }
        CHECK(mix.slot_cursor(3) == 25);
    }

    TEST_CASE("mixer - a stereo voice played flat keeps unity at centre and a spatial one downmixes") {
        mixer mix(48000);
        audio_format format = {};
        format.channels = 2;
        std::vector<f32> samples(2000, 0.0f);
        for (usize frame = 0; frame < 1000; ++frame) {
            samples[frame * 2] = 0.8f;
            samples[frame * 2 + 1] = 0.2f;
        }
        const pcm_buffer pcm(format, samples);
        // Gains of 0.25 keep the sum of the three voices below the output clamp.
        REQUIRE(mix.commands().try_push(play_command(pcm, 0, 1, 0.25f)));
        voice_command downmixed = play_command(pcm, 1, 1, 0.25f);
        downmixed.payload.play.downmix = true;
        REQUIRE(mix.commands().try_push(downmixed));
        voice_command balanced = play_command(pcm, 2, 1, 0.25f, 0.5f);
        REQUIRE(mix.commands().try_push(balanced));

        const std::vector<f32> out = render(mix, 10);
        const f32 centre = std::cos(0.25f * 3.14159265f);
        // Slot 0: 0.8 / 0.2 untouched; slot 1: mono 0.5 * constant power; slot 2: left 0.8 * 0.5, right 0.2.
        const f32 expectedLeft = 0.25f * (0.8f + 0.5f * centre + 0.4f);
        const f32 expectedRight = 0.25f * (0.2f + 0.5f * centre + 0.2f);
        CHECK(out[0] == doctest::Approx(static_cast<f64>(expectedLeft)).epsilon(1.0e-4));
        CHECK(out[1] == doctest::Approx(static_cast<f64>(expectedRight)).epsilon(1.0e-4));
        CHECK(mix.active_voices() == 3);
    }

    TEST_CASE("mixer - the output is hard-clamped to [-1, 1]") {
        mixer mix(48000);
        const std::shared_ptr<const pcm_buffer> loud = testing::make_pcm([] {
            testing::wav_spec spec = {};
            spec.shape = testing::wave_shape::SQUARE;
            spec.frequency = 1.0;
            spec.amplitude = 1.0f;
            spec.channels = 2;
            return spec;
        }());
        for (u32 slot = 0; slot < 4; ++slot) {
            REQUIRE(mix.commands().try_push(play_command(*loud, slot, 1, 2.0f)));
        }
        const std::vector<f32> out = render(mix, 64);
        for (const f32 sample : out) {
            CHECK(sample <= 1.0f);
            CHECK(sample >= -1.0f);
        }
        CHECK(testing::peak_of(out) == doctest::Approx(1.0));
    }

    TEST_CASE("mixer - a generation mismatch drops a stale command") {
        mixer mix(48000);
        const std::shared_ptr<const pcm_buffer> pcm = testing::make_sine_pcm(440.0, 1.0);
        REQUIRE(mix.commands().try_push(play_command(*pcm, 0, 2)));
        static_cast<void>(render(mix, 16));

        voice_command stalePause = simple_command(command_kind::PAUSE, 0, 1);
        REQUIRE(mix.commands().try_push(stalePause));
        static_cast<void>(render(mix, 16));
        CHECK(mix.slot_state(0) == voice_state::PLAYING);
        CHECK(mix.commands_dropped() == 1);

        voice_command pause = simple_command(command_kind::PAUSE, 0, 2);
        REQUIRE(mix.commands().try_push(pause));
        static_cast<void>(render(mix, 16));
        CHECK(mix.slot_state(0) == voice_state::PAUSED);
        const std::vector<voice_event> events = drain(mix);
        CHECK(count_events(events, event_kind::STARTED) == 1);
        CHECK(count_events(events, event_kind::PAUSED) == 1);
        CHECK(events.back().generation == 2);

        voice_command outOfRange = simple_command(command_kind::PAUSE, MAX_VOICES + 5, 1);
        REQUIRE(mix.commands().try_push(outOfRange));
        static_cast<void>(render(mix, 16));
        CHECK(mix.commands_dropped() == 2);
    }

    TEST_CASE("mixer - event ring overflow sets eventsDropped without losing the FINISHED state") {
        mixer mix(48000);
        audio_format format = {};
        format.channels = 1;
        const pcm_buffer tiny(format, std::vector<f32>(4, 0.1f));
        voice_command looping = play_command(tiny, 0, 1);
        looping.payload.play.loop = true;
        REQUIRE(mix.commands().try_push(looping));
        // Every 4 frames one LOOPED event: 48000 frames produce far more than the ring holds.
        static_cast<void>(render(mix, 48000));
        CHECK(mix.events_dropped() > 0);
        CHECK(mix.loops() == 12000);

        const pcm_buffer finishing(format, std::vector<f32>(100, 0.2f));
        REQUIRE(mix.commands().try_push(play_command(finishing, 1, 1)));
        static_cast<void>(render(mix, 256));
        CHECK(mix.slot_state(1) == voice_state::FINISHED);
        CHECK(mix.slot_cursor(1) == 100);
        CHECK(mix.slot_generation(1) == 1);
    }

    TEST_CASE("mixer - STOP fades to silence then reports STOPPED") {
        mixer mix(48000);
        const std::shared_ptr<const pcm_buffer> pcm = testing::make_sine_pcm(440.0, 1.0, 1.0f);
        REQUIRE(mix.commands().try_push(play_command(*pcm, 0, 1)));
        static_cast<void>(render(mix, 256));
        voice_command stop = simple_command(command_kind::STOP, 0, 1);
        stop.fadeFrames = 96;
        REQUIRE(mix.commands().try_push(stop));
        static_cast<void>(render(mix, 256));
        CHECK(mix.slot_state(0) == voice_state::STOPPED);
        const std::vector<voice_event> events = drain(mix);
        CHECK(count_events(events, event_kind::STOPPED) == 1);
        CHECK(count_events(events, event_kind::FINISHED) == 0);
    }

    TEST_CASE("mixer - group gain, group pause and master pause apply at the top of the block") {
        mixer mix(48000);
        const std::shared_ptr<const pcm_buffer> pcm = testing::make_pcm([] {
            testing::wav_spec spec = {};
            spec.shape = testing::wave_shape::SQUARE;
            spec.frequency = 1.0;
            spec.amplitude = 1.0f;
            spec.channels = 2;
            return spec;
        }());
        voice_command play = play_command(*pcm, 0, 1);
        play.groupIndex = 3;
        REQUIRE(mix.commands().try_push(play));
        voice_command groupGain = {};
        groupGain.kind = command_kind::SET_GROUP_GAIN;
        groupGain.groupIndex = 3;
        groupGain.payload.scalar = 0.25f;
        REQUIRE(mix.commands().try_push(groupGain));
        std::vector<f32> out = render(mix, 16);
        CHECK(out[0] == doctest::Approx(0.25));
        CHECK(mix.group_gain(3) == doctest::Approx(0.25));

        voice_command groupPause = {};
        groupPause.kind = command_kind::SET_GROUP_PAUSED;
        groupPause.groupIndex = 3;
        groupPause.payload.flag = true;
        REQUIRE(mix.commands().try_push(groupPause));
        const u64 cursorBefore = mix.slot_cursor(0);
        out = render(mix, 16);
        CHECK(out[0] == doctest::Approx(0.0));
        CHECK(mix.slot_cursor(0) == cursorBefore);
        CHECK(mix.group_paused(3));
        groupPause.payload.flag = false;
        REQUIRE(mix.commands().try_push(groupPause));

        voice_command masterPause = {};
        masterPause.kind = command_kind::SET_MASTER_PAUSED;
        masterPause.payload.flag = true;
        REQUIRE(mix.commands().try_push(masterPause));
        out = render(mix, 16);
        CHECK(out[0] == doctest::Approx(0.0));
        CHECK(mix.master_paused());
        CHECK(mix.slot_cursor(0) == cursorBefore);
    }

    TEST_CASE("regression - audio review 8 STOP on a PAUSED voice produces no audio and reports STOPPED") {
        mixer mix(48000);
        const std::shared_ptr<const pcm_buffer> pcm = testing::make_sine_pcm(440.0, 1.0, 1.0f);
        REQUIRE(mix.commands().try_push(play_command(*pcm, 0, 1)));
        static_cast<void>(render(mix, 256));
        REQUIRE(mix.commands().try_push(simple_command(command_kind::PAUSE, 0, 1)));
        static_cast<void>(render(mix, 256));
        CHECK(mix.slot_state(0) == voice_state::PAUSED);
        static_cast<void>(drain(mix));

        voice_command stop = simple_command(command_kind::STOP, 0, 1);
        stop.fadeFrames = 240;
        REQUIRE(mix.commands().try_push(stop));
        const std::vector<f32> out = render(mix, 256);
        CHECK(testing::peak_of(out) == doctest::Approx(0.0));
        CHECK(mix.slot_state(0) == voice_state::STOPPED);
        CHECK(mix.active_voices() == 0);
        const std::vector<voice_event> events = drain(mix);
        CHECK(count_events(events, event_kind::STOPPED) == 1);
        CHECK(count_events(events, event_kind::RESUMED) == 0);
    }

    TEST_CASE("regression - code sweep F022 STOP under a paused group or master ends the voice at once") {
        voice_command freeze = {};
        freeze.payload.flag = true;
        SUBCASE("a paused group") {
            freeze.kind = command_kind::SET_GROUP_PAUSED;
            freeze.groupIndex = 3;
        }
        SUBCASE("the master pause") {
            freeze.kind = command_kind::SET_MASTER_PAUSED;
        }

        mixer mix(48000);
        const std::shared_ptr<const pcm_buffer> pcm = testing::make_sine_pcm(440.0, 1.0, 1.0f);
        voice_command play = play_command(*pcm, 0, 1);
        play.groupIndex = 3;
        REQUIRE(mix.commands().try_push(play));
        static_cast<void>(render(mix, 256));
        REQUIRE(mix.commands().try_push(freeze));
        static_cast<void>(render(mix, 256));
        CHECK(mix.slot_state(0) == voice_state::PLAYING);
        static_cast<void>(drain(mix));

        voice_command stop = simple_command(command_kind::STOP, 0, 1);
        stop.fadeFrames = 240;
        REQUIRE(mix.commands().try_push(stop));
        const std::vector<f32> out = render(mix, 256);
        CHECK(testing::peak_of(out) == doctest::Approx(0.0));
        CHECK(mix.slot_state(0) == voice_state::STOPPED);
        CHECK(mix.active_voices() == 0);
        const std::vector<voice_event> events = drain(mix);
        CHECK(count_events(events, event_kind::STOPPED) == 1);
    }

    TEST_CASE("regression - code sweep F022 a group pause during a STOP fade ends the voice at once") {
        mixer mix(48000);
        const std::shared_ptr<const pcm_buffer> pcm = testing::make_sine_pcm(440.0, 1.0, 1.0f);
        voice_command play = play_command(*pcm, 0, 1);
        play.groupIndex = 3;
        REQUIRE(mix.commands().try_push(play));
        static_cast<void>(render(mix, 256));

        voice_command stop = simple_command(command_kind::STOP, 0, 1);
        stop.fadeFrames = 4800;
        REQUIRE(mix.commands().try_push(stop));
        static_cast<void>(render(mix, 256));
        CHECK(mix.slot_state(0) == voice_state::STOPPING);
        static_cast<void>(drain(mix));

        voice_command groupPause = {};
        groupPause.kind = command_kind::SET_GROUP_PAUSED;
        groupPause.groupIndex = 3;
        groupPause.payload.flag = true;
        REQUIRE(mix.commands().try_push(groupPause));
        const std::vector<f32> out = render(mix, 256);
        CHECK(testing::peak_of(out) == doctest::Approx(0.0));
        CHECK(mix.slot_state(0) == voice_state::STOPPED);
        CHECK(mix.active_voices() == 0);
        CHECK(count_events(drain(mix), event_kind::STOPPED) == 1);

        // Unpausing the group brings nothing back: the slot is free for the next PLAY.
        groupPause.payload.flag = false;
        REQUIRE(mix.commands().try_push(groupPause));
        CHECK(testing::peak_of(render(mix, 256)) == doctest::Approx(0.0));
        CHECK(mix.slot_state(0) == voice_state::STOPPED);
    }

    TEST_CASE("regression - audio review 9 SET_DOWNMIX switches a stereo voice between the two pan laws") {
        mixer mix(48000);
        audio_format format = {};
        format.channels = 2;
        std::vector<f32> samples(2000, 0.0f);
        for (usize frame = 0; frame < 1000; ++frame) {
            samples[frame * 2] = 0.8f;
            samples[frame * 2 + 1] = 0.2f;
        }
        const pcm_buffer pcm(format, samples);
        REQUIRE(mix.commands().try_push(play_command(pcm, 0, 1)));
        std::vector<f32> out = render(mix, 10);
        CHECK(out[0] == doctest::Approx(0.8));
        CHECK(out[1] == doctest::Approx(0.2));

        voice_command downmix = simple_command(command_kind::SET_DOWNMIX, 0, 1);
        downmix.payload.flag = true;
        REQUIRE(mix.commands().try_push(downmix));
        out = render(mix, 10);
        const f32 centre = 0.5f * std::cos(0.25f * 3.14159265f);
        CHECK(out[0] == doctest::Approx(static_cast<f64>(centre)).epsilon(1.0e-4));
        CHECK(out[1] == doctest::Approx(static_cast<f64>(centre)).epsilon(1.0e-4));
        CHECK(mix.slot(0).downmix);

        downmix.payload.flag = false;
        REQUIRE(mix.commands().try_push(downmix));
        out = render(mix, 10);
        CHECK(out[0] == doctest::Approx(0.8));
        CHECK(out[1] == doctest::Approx(0.2));

        // A mono voice never downmixes, whatever the flag says.
        const std::shared_ptr<const pcm_buffer> mono = testing::make_sine_pcm(440.0, 1.0);
        REQUIRE(mix.commands().try_push(play_command(*mono, 1, 1)));
        voice_command monoDownmix = simple_command(command_kind::SET_DOWNMIX, 1, 1);
        monoDownmix.payload.flag = true;
        REQUIRE(mix.commands().try_push(monoDownmix));
        static_cast<void>(render(mix, 10));
        CHECK_FALSE(mix.slot(1).downmix);
    }

    TEST_CASE("regression - audio review 4 PLAY publishes the new occupant's state with its generation") {
        // The two stores cannot interleave with a reader in a single-threaded test; this pins the
        // observable half of the contract documented on mixer_voice: once slot_generation() reports
        // the new occupant, slot_state() and slot_cursor() are the new occupant's, never the
        // previous one's FINISHED.
        mixer mix(48000);
        audio_format format = {};
        format.channels = 1;
        const pcm_buffer tiny(format, std::vector<f32>(100, 0.2f));
        REQUIRE(mix.commands().try_push(play_command(tiny, 0, 1)));
        static_cast<void>(render(mix, 256));
        CHECK(mix.slot_state(0) == voice_state::FINISHED);
        CHECK(mix.slot_generation(0) == 1);
        static_cast<void>(drain(mix));

        const std::shared_ptr<const pcm_buffer> longer = testing::make_sine_pcm(440.0, 1.0);
        voice_command replay = play_command(*longer, 0, 2);
        replay.payload.play.startFrame = 1000;
        REQUIRE(mix.commands().try_push(replay));
        static_cast<void>(render(mix, 16));
        CHECK(mix.slot_generation(0) == 2);
        CHECK(mix.slot_state(0) == voice_state::PLAYING);
        CHECK(mix.slot_cursor(0) == 1016);
        const std::vector<voice_event> events = drain(mix);
        REQUIRE(events.size() == 1);
        CHECK(events[0].kind == event_kind::STARTED);
        CHECK(events[0].generation == 2);
        CHECK(events[0].frame == 1000);
    }

    TEST_CASE("mixer - render allocates nothing") {
        mixer mix(48000);
        const std::shared_ptr<const pcm_buffer> pcm = testing::make_sine_pcm(440.0, 1.0, 0.1f, 2);
        for (u32 slot = 0; slot < MAX_VOICES; ++slot) {
            voice_command play = play_command(*pcm, slot, 1, 0.01f);
            play.payload.play.loop = true;
            REQUIRE(mix.commands().try_push(play));
        }
        std::vector<f32> out(480 * MIX_CHANNELS, 0.0f);
        mix.render(out.data(), 480);
        testing::allocation_scope scope = {};
        mix.render(out.data(), 480);
        mix.render(out.data(), 480);
        const u64 allocations = scope.allocation_count();
        CHECK(allocations == 0);
        CHECK(mix.active_voices() == MAX_VOICES);
    }
}
