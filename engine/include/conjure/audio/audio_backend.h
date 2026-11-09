#pragma once

#include "conjure/audio/audio_device.h"
#include "conjure/audio/audio_types.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <memory>
#include <vector>

// The backend interface (docs/design/12_audio.md 4.3): the piece that owns a device (or a virtual
// clock) and drives the mix callback. Implementations: miniaudio (real devices, device thread)
// and null (headless, main thread). Every method is main-thread only except state().

namespace conjure::audio {

    class audio_backend {
        public:
            audio_backend();
            virtual ~audio_backend();

            audio_backend(const audio_backend &) = delete;
            audio_backend &operator=(const audio_backend &) = delete;

        public:
            /// <summary>
            /// Which implementation this is (never AUTO).
            /// </summary>
            [[nodiscard]] virtual audio_backend_kind kind() const noexcept = 0;

            /// <summary>
            /// Opens the device and starts driving desc.callback. Fails with INVALID_SETTINGS for a
            /// null callback or an unsupported sample rate, BACKEND_INIT_FAILED when the device
            /// cannot be opened and DEVICE_START_FAILED when it cannot be started; an unknown
            /// desc.deviceName is not an error (WARN, default device). A running backend is stopped
            /// first, and a failed start leaves the backend STOPPED.
            /// </summary>
            [[nodiscard]] virtual core::result<void> start(const backend_desc &desc) = 0;

            /// <summary>
            /// Stops the device; the callback is not running when this returns. Safe to call twice.
            /// </summary>
            virtual void stop() noexcept = 0;

            /// <summary>
            /// Atomic device state; readable from any thread.
            /// </summary>
            [[nodiscard]] virtual device_state state() const noexcept = 0;

            /// <summary>
            /// The format the callback is driven at after start() (the requested one; miniaudio
            /// converts to the device's native format internally).
            /// </summary>
            [[nodiscard]] virtual audio_format output_format() const noexcept = 0;

            /// <summary>
            /// Playback devices this backend can open.
            /// </summary>
            [[nodiscard]] virtual core::result<std::vector<audio_device_info>> enumerate_devices() const = 0;

            /// <summary>
            /// Main thread, once per frame. null: advance the clock and render; miniaudio: poll.
            /// </summary>
            virtual void update(f32 deltaSeconds) = 0;
    };

    /// <summary>
    /// True when a backend of this kind is compiled into the library (AUTO and NULL_BACKEND always).
    /// </summary>
    [[nodiscard]] bool is_backend_compiled(audio_backend_kind kind) noexcept;

    /// <summary>
    /// Creates a stopped backend. AUTO = MINIAUDIO when a device backend is compiled in, else
    /// NULL_BACKEND. Fails with UNSUPPORTED (core) when the requested kind is not compiled in.
    /// </summary>
    [[nodiscard]] core::result<std::unique_ptr<audio_backend>> create_backend(audio_backend_kind kind);
}
