#include "conjure/testing/doctest.h"

#include "conjure/audio/audio_device.h"
#include "conjure/audio/null_backend.h"

#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::audio;

namespace {
    void silent_callback(void *user, f32 *output, u32 frameCount) noexcept {
        static_cast<void>(user);
        for (u32 sample = 0; sample < frameCount * MIX_CHANNELS; ++sample) {
            output[sample] = 0.0f;
        }
    }
}

TEST_SUITE("audio/audio_device") {

    TEST_CASE("backend_desc - defaults") {
        const backend_desc desc = {};
        CHECK(desc.format == audio_format{});
        CHECK(desc.format.sampleRate == 48000);
        CHECK(desc.format.channels == MIX_CHANNELS);
        CHECK(desc.periodFrames == 480);
        CHECK(desc.deviceName.empty());
        CHECK(desc.callback == nullptr);
        CHECK(desc.user == nullptr);
    }

    TEST_CASE("mix_callback - a plain function pointer fits the alias") {
        backend_desc desc = {};
        desc.callback = &silent_callback;
        std::vector<f32> buffer(MIX_CHANNELS * 4, 1.0f);
        desc.callback(nullptr, buffer.data(), 4);
        for (const f32 sample : buffer) {
            CHECK(sample == doctest::Approx(0.0));
        }
    }

    TEST_CASE("audio_device_info - defaults and equality") {
        const audio_device_info info = {};
        CHECK(info.name.empty());
        CHECK_FALSE(info.isDefault);
        CHECK(info.sampleRates.empty());
        CHECK(info.minChannels == 0);
        CHECK(info.maxChannels == 0);
        audio_device_info other = info;
        CHECK(other == info);
        other.name = "Speakers";
        CHECK(other != info);
    }

    TEST_CASE("audio_device_info - the null backend reports one default device named Null Output") {
        const null_backend backend = {};
        const core::result<std::vector<audio_device_info>> devices = backend.enumerate_devices();
        CONJURE_REQUIRE_OK(devices);
        REQUIRE(devices.value().size() == 1);
        const audio_device_info &device = devices.value()[0];
        CHECK(device.name == NULL_DEVICE_NAME);
        CHECK(device.name == "Null Output");
        CHECK(device.isDefault);
        CHECK(device.sampleRates == std::vector<u32>{48000});
        CHECK(device.minChannels == 2);
        CHECK(device.maxChannels == 2);
    }
}
