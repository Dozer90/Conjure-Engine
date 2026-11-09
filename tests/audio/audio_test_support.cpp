#include "conjure/testing/doctest.h"

#include "conjure/testing/test_environment.h"

#include "conjure/audio/audio_backend.h"
#include "conjure/audio/audio_device.h"
#include "conjure/audio/audio_types.h"

#include "conjure/core/log.h"

#include <memory>
#include <vector>

// Installs the probe behind precondition::AUDIO_DEVICE (tests/support): the [audio_device] cases
// skip with exit 77 under CTest on a machine without a playback device instead of passing vacuously.

using namespace conjure;
using namespace conjure::audio;
using namespace conjure::testing;

namespace {

    void probe_callback(void *user, f32 *output, u32 frameCount) noexcept {
        static_cast<void>(user);
        for (u32 sample = 0; sample < frameCount * MIX_CHANNELS; ++sample) {
            output[sample] = 0.0f;
        }
    }

    /// <summary>
    /// Opens the default device once per process: the miniaudio backend must be compiled in, list
    /// at least one playback device and start. A failed attempt is final.
    /// </summary>
    bool audio_device_available() noexcept {
        static const bool available = []() noexcept {
            if (!is_backend_compiled(audio_backend_kind::MINIAUDIO)) {
                return false;
            }
            // The probe may run outside any log_capture; keep the console quiet while it works.
            const bool consoleWasEnabled = core::log::console_enabled();
            core::log::set_console_enabled(false);
            bool opened = false;
            core::result<std::unique_ptr<audio_backend>> created = create_backend(audio_backend_kind::MINIAUDIO);
            if (created.has_value()) {
                audio_backend &backend = *created.value();
                const core::result<std::vector<audio_device_info>> devices = backend.enumerate_devices();
                if (devices.has_value() && !devices.value().empty()) {
                    backend_desc desc = {};
                    desc.callback = &probe_callback;
                    opened = backend.start(desc).has_value();
                    backend.stop();
                }
            }
            core::log::set_console_enabled(consoleWasEnabled);
            return opened;
        }();
        return available;
    }

    struct probe_installer {
        probe_installer() {
            set_precondition_probe(precondition::AUDIO_DEVICE, &audio_device_available);
        }
    };

    const probe_installer sProbeInstaller = {};
}
