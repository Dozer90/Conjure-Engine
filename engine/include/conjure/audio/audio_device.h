#pragma once

#include "conjure/audio/audio_types.h"

#include "conjure/core/types.h"

#include <string>
#include <vector>

// Device descriptions shared by every backend (docs/design/12_audio.md 3.9, 4.3).

namespace conjure::audio {

    /// <summary>
    /// The function a backend drives to pull mixed audio: interleaved f32 frames of MIX_CHANNELS
    /// channels at the format negotiated by start(). Called on the device thread (miniaudio) or on
    /// the main thread (null); it must not allocate, lock or log.
    /// </summary>
    using mix_callback = void (*)(void *user, f32 *output, u32 frameCount) noexcept;

    /// <summary>
    /// One playback device as reported by enumerate_devices().
    /// </summary>
    struct audio_device_info {
        std::string name = {};
        bool isDefault = false;
        std::vector<u32> sampleRates = {};  // Native rates, ascending; empty when the backend converts freely
        u32 minChannels = 0;
        u32 maxChannels = 0;

        [[nodiscard]] bool operator==(const audio_device_info &other) const = default;
    };

    /// <summary>
    /// What audio_backend::start() needs: the requested mix format (channels fixed at
    /// MIX_CHANNELS), the period size (480 = 10 ms at 48 kHz; miniaudio may round), the device
    /// name (empty = default) and the callback the backend drives.
    /// </summary>
    struct backend_desc {
        audio_format format = {};
        u32 periodFrames = 480;
        std::string deviceName = {};
        mix_callback callback = nullptr;
        void *user = nullptr;
    };

    /// <summary>
    /// Name the null backend reports for its single virtual device.
    /// </summary>
    inline constexpr const char *NULL_DEVICE_NAME = "Null Output";
}
