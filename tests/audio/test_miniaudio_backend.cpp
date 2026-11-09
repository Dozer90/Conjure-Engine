#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"

#include "wav_builder.h"

#include "conjure/audio/audio_backend.h"
#include "conjure/audio/audio_engine.h"

#include <atomic>
#include <memory>
#include <vector>

// Local-only exercise of the real-device backend ([audio_device]). Every case requires
// precondition::AUDIO_DEVICE (the probe in audio_test_support.cpp opens the default device once),
// so CI without a sound card skips them with exit 77 while a developer machine verifies the device
// path; once the precondition holds, a device that fails to start is a real failure. The waits are
// bounded spins on a condition the device thread publishes (no sleeps, clocks or threads of our
// own, CONVENTIONS 6); a device that never calls back fails the CHECK after the bound instead of
// hanging.

using namespace conjure;
using namespace conjure::audio;

namespace {
    std::atomic<u32> sCallbackCalls = 0;

    void silence_callback(void *user, f32 *output, u32 frameCount) noexcept {
        static_cast<void>(user);
        sCallbackCalls.fetch_add(1, std::memory_order_relaxed);
        for (u32 sample = 0; sample < frameCount * MIX_CHANNELS; ++sample) {
            output[sample] = 0.0f;
        }
    }

    // One relaxed load per iteration: a few hundred million of them span the seconds a slow
    // device can take to start, while a healthy one answers within a single 10 ms period.
    constexpr u32 CALLBACK_SPIN_LIMIT = 400000000;
    constexpr u32 ENGINE_UPDATE_LIMIT = 5000000;
}

TEST_SUITE("audio/miniaudio_backend") {

    TEST_CASE("miniaudio_backend - [audio_device] enumerate, start, stop and output format") {
        CONJURE_TEST_REQUIRE(testing::precondition::AUDIO_DEVICE);
        testing::log_capture logs = {};
        core::result<std::unique_ptr<audio_backend>> created = create_backend(audio_backend_kind::MINIAUDIO);
        CONJURE_REQUIRE_OK(created);
        audio_backend &backend = *created.value();
        CHECK(backend.kind() == audio_backend_kind::MINIAUDIO);
        CHECK(backend.state() == device_state::STOPPED);

        const core::result<std::vector<audio_device_info>> devices = backend.enumerate_devices();
        CONJURE_REQUIRE_OK(devices);
        REQUIRE(devices.value().size() >= 1);
        for (const audio_device_info &device : devices.value()) {
            CAPTURE(device.name);
            CHECK_FALSE(device.name.empty());
        }

        backend_desc desc = {};
        desc.callback = &silence_callback;
        desc.format.sampleRate = 48000;
        sCallbackCalls.store(0, std::memory_order_relaxed);
        const core::result<void> started = backend.start(desc);
        CONJURE_REQUIRE_OK(started);
        CHECK(backend.state() == device_state::RUNNING);
        CHECK(backend.output_format().sampleRate == 48000);
        CHECK(backend.output_format().channels == MIX_CHANNELS);

        u32 spins = 0;
        while (sCallbackCalls.load(std::memory_order_relaxed) == 0 && spins < CALLBACK_SPIN_LIMIT) {
            ++spins;
        }
        CHECK(sCallbackCalls.load(std::memory_order_relaxed) > 0);
        backend.stop();
        CHECK(backend.state() == device_state::STOPPED);
        backend.stop();
    }

    TEST_CASE("miniaudio_backend - [audio_device] an unknown device name falls back to the default with a WARN") {
        CONJURE_TEST_REQUIRE(testing::precondition::AUDIO_DEVICE);
        testing::log_capture logs = {};
        core::result<std::unique_ptr<audio_backend>> created = create_backend(audio_backend_kind::MINIAUDIO);
        CONJURE_REQUIRE_OK(created);
        backend_desc desc = {};
        desc.callback = &silence_callback;
        desc.deviceName = "No Such Device 1234";
        const core::result<void> started = created.value()->start(desc);
        CONJURE_REQUIRE_OK(started);
        CHECK(logs.contains(core::log_level::WARN, "not found"));
        created.value()->stop();
    }

    TEST_CASE("miniaudio_backend - [audio_device] the engine plays a 100 ms tone through the real device") {
        CONJURE_TEST_REQUIRE(testing::precondition::AUDIO_DEVICE);
        testing::log_capture logs = {};
        audio_settings settings = {};
        settings.backend = audio_backend_kind::MINIAUDIO;
        audio_engine engine(settings);
        REQUIRE(engine.backend_kind() == audio_backend_kind::MINIAUDIO);
        CHECK(engine.null_backend_instance() == nullptr);
        CHECK(engine.output_format().sampleRate == 48000);

        play_params params = {};
        params.volume = 0.2f;
        const core::result<voice_id> voice = engine.play(testing::make_sine_pcm(440.0, 0.1), params);
        CONJURE_REQUIRE_OK(voice);
        // The device thread advances the voice; the main thread only polls through update().
        u32 updates = 0;
        while (engine.is_playing(voice.value()) && updates < ENGINE_UPDATE_LIMIT) {
            engine.update(0.0f);
            engine.end_frame();
            ++updates;
        }
        CHECK(engine.stats().renderCalls > 0);
        CHECK(engine.stats().framesRendered >= 4800);
        CHECK_FALSE(engine.is_playing(voice.value()));
    }
}
