#pragma once

#include "conjure/audio/audio_backend.h"
#include "conjure/audio/audio_device.h"
#include "conjure/audio/audio_types.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <atomic>
#include <memory>
#include <vector>

// The real-device backend (docs/design/12_audio.md 3.8, 3.9): a miniaudio context and playback
// device. The data callback runs the mix callback on the device thread; the notification callback
// stores the device state into an atomic and nothing else. miniaudio converts the requested f32
// stereo format to the device's native one. This header keeps miniaudio out of the public tree:
// only miniaudio_backend.cpp includes miniaudio.h.

namespace conjure::audio {

    struct miniaudio_callbacks;

    class miniaudio_backend final : public audio_backend {
        private:
            friend struct miniaudio_callbacks;   // the miniaudio data and notification callbacks

            struct impl;

            std::unique_ptr<impl> mImpl = nullptr;     // ma_context, ma_device, selected device id
            backend_desc mDesc = {};
            std::atomic<device_state> mDeviceState = device_state::STOPPED;
            bool mDeviceInitialised = false;

        public:
            miniaudio_backend();
            ~miniaudio_backend() override;

        public:
            /// <summary>
            /// True when the library was built with miniaudio (CONJURE_WITH_AUDIO).
            /// </summary>
            [[nodiscard]] static bool is_compiled() noexcept;

            [[nodiscard]] audio_backend_kind kind() const noexcept override;
            [[nodiscard]] core::result<void> start(const backend_desc &desc) override;
            void stop() noexcept override;
            [[nodiscard]] device_state state() const noexcept override;
            [[nodiscard]] audio_format output_format() const noexcept override;
            [[nodiscard]] core::result<std::vector<audio_device_info>> enumerate_devices() const override;

            /// <summary>
            /// Nothing to poll: the device thread drives the mixer and the notification callback
            /// publishes state changes.
            /// </summary>
            void update(f32 deltaSeconds) override;

        private:
            // const: the context lives behind mImpl (a unique_ptr), so creating it lazily from
            // enumerate_devices() const mutates no member of this object.
            [[nodiscard]] core::result<void> ensure_context() const;
            void uninit_device() noexcept;
    };
}
