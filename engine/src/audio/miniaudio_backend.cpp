#include "audio/miniaudio_backend.h"
#include "conjure/audio/audio_error.h"
#include "conjure/audio/log_category.h"

#include "conjure/core/error.h"

#include <miniaudio.h>

#include <algorithm>
#include <string>

// The second translation unit of the module that includes miniaudio.h.

using namespace conjure;
using namespace conjure::audio;

struct miniaudio_backend::impl {
    ma_context context = {};
    ma_device device = {};
    ma_device_id selectedId = {};
    bool contextInitialised = false;
    bool hasSelectedId = false;
};

/// <summary>
/// The two functions miniaudio calls; pUserData is the backend.
/// </summary>
struct conjure::audio::miniaudio_callbacks {
    static void data(ma_device *device, void *output, const void *input, ma_uint32 frameCount) {
        static_cast<void>(input);
        const miniaudio_backend *backend = static_cast<const miniaudio_backend *>(device->pUserData);
        if (backend != nullptr && backend->mDesc.callback != nullptr) {
            backend->mDesc.callback(backend->mDesc.user, static_cast<f32 *>(output), frameCount);
        }
    }

    static void notification(const ma_device_notification *notification) {
        miniaudio_backend *backend = static_cast<miniaudio_backend *>(notification->pDevice->pUserData);
        if (backend == nullptr) {
            return;
        }
        switch (notification->type) {
            case ma_device_notification_type_started:
            case ma_device_notification_type_rerouted:
            case ma_device_notification_type_interruption_ended:
            case ma_device_notification_type_unlocked:
                backend->mDeviceState.store(device_state::RUNNING, std::memory_order_release);
                return;
            case ma_device_notification_type_stopped:
            case ma_device_notification_type_interruption_began:
                // ma_device_stop() notifies too; stop() overwrites LOST with STOPPED right after.
                backend->mDeviceState.store(device_state::LOST, std::memory_order_release);
                return;
        }
    }
};

namespace {
    [[nodiscard]] core::error backend_error(audio_error code, const char *what, ma_result result) {
        return core::make_error(code, "{}: {}", what, ma_result_description(result));
    }

    [[nodiscard]] audio_device_info describe(const ma_device_info &info) {
        audio_device_info described = {};
        described.name = info.name;
        described.isDefault = info.isDefault != 0;
        for (ma_uint32 index = 0; index < info.nativeDataFormatCount; ++index) {
            const ma_uint32 sampleRate = info.nativeDataFormats[index].sampleRate;
            const ma_uint32 channels = info.nativeDataFormats[index].channels;
            if (sampleRate != 0
              && std::find(described.sampleRates.begin(), described.sampleRates.end(), sampleRate)
                == described.sampleRates.end()) {
                described.sampleRates.push_back(sampleRate);
            }
            if (channels != 0) {
                described.minChannels =
                  described.minChannels == 0 ? channels : std::min(described.minChannels, channels);
                described.maxChannels = std::max(described.maxChannels, channels);
            }
        }
        std::sort(described.sampleRates.begin(), described.sampleRates.end());
        return described;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

miniaudio_backend::miniaudio_backend() :
    mImpl(std::make_unique<impl>()) {}

miniaudio_backend::~miniaudio_backend() {
    stop();
    if (mImpl->contextInitialised) {
        ma_context_uninit(&mImpl->context);
        mImpl->contextInitialised = false;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

bool miniaudio_backend::is_compiled() noexcept {
    return true;
}

audio_backend_kind miniaudio_backend::kind() const noexcept {
    return audio_backend_kind::MINIAUDIO;
}

core::result<void> miniaudio_backend::start(const backend_desc &desc) {
    if (desc.callback == nullptr) {
        return core::make_error(audio_error::INVALID_SETTINGS, "miniaudio backend: the mix callback is null");
    }
    if (desc.format.sampleRate < MIN_SAMPLE_RATE || desc.format.sampleRate > MAX_SAMPLE_RATE) {
        return core::make_error(
          audio_error::INVALID_SETTINGS,
          "miniaudio backend: sample rate {} outside [{}, {}]",
          desc.format.sampleRate,
          MIN_SAMPLE_RATE,
          MAX_SAMPLE_RATE
        );
    }

    stop();
    CONJURE_TRY(ensure_context());

    mDesc = desc;
    mDesc.format.channels = static_cast<u16>(MIX_CHANNELS);
    mDesc.format.format = sample_format::F32;

    // Resolve the requested device by name; an unknown name falls back to the default with a WARN.
    mImpl->hasSelectedId = false;
    if (!desc.deviceName.empty()) {
        ma_device_info *playbackInfos = nullptr;
        ma_uint32 playbackCount = 0;
        const ma_result enumerated =
          ma_context_get_devices(&mImpl->context, &playbackInfos, &playbackCount, nullptr, nullptr);
        if (enumerated == MA_SUCCESS) {
            for (ma_uint32 index = 0; index < playbackCount; ++index) {
                if (desc.deviceName == playbackInfos[index].name) {
                    mImpl->selectedId = playbackInfos[index].id;
                    mImpl->hasSelectedId = true;
                    break;
                }
            }
        }
        if (!mImpl->hasSelectedId) {
            CONJURE_LOG_WARN(LOG_AUDIO, "audio device '{}' not found; using the default device", desc.deviceName);
        }
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = MIX_CHANNELS;
    config.playback.pDeviceID = mImpl->hasSelectedId ? &mImpl->selectedId : nullptr;
    config.sampleRate = mDesc.format.sampleRate;
    config.periodSizeInFrames = mDesc.periodFrames;
    config.dataCallback = &miniaudio_callbacks::data;
    config.notificationCallback = &miniaudio_callbacks::notification;
    config.pUserData = this;

    mDeviceState.store(device_state::STARTING, std::memory_order_release);
    const ma_result initialised = ma_device_init(&mImpl->context, &config, &mImpl->device);
    if (initialised != MA_SUCCESS) {
        mDeviceState.store(device_state::STOPPED, std::memory_order_release);
        return backend_error(audio_error::BACKEND_INIT_FAILED, "ma_device_init", initialised);
    }
    mDeviceInitialised = true;

    const ma_result started = ma_device_start(&mImpl->device);
    if (started != MA_SUCCESS) {
        uninit_device();
        mDeviceState.store(device_state::STOPPED, std::memory_order_release);
        return backend_error(audio_error::DEVICE_START_FAILED, "ma_device_start", started);
    }

    mDeviceState.store(device_state::RUNNING, std::memory_order_release);
    CONJURE_LOG_INFO(
      LOG_AUDIO,
      "audio device '{}' started at {} Hz, {} channels, period {} frames",
      mImpl->device.playback.name,
      mDesc.format.sampleRate,
      MIX_CHANNELS,
      mImpl->device.playback.internalPeriodSizeInFrames
    );
    return {};
}

void miniaudio_backend::stop() noexcept {
    uninit_device();
    mDeviceState.store(device_state::STOPPED, std::memory_order_release);
}

device_state miniaudio_backend::state() const noexcept {
    return mDeviceState.load(std::memory_order_acquire);
}

audio_format miniaudio_backend::output_format() const noexcept {
    return mDesc.format;
}

core::result<std::vector<audio_device_info>> miniaudio_backend::enumerate_devices() const {
    // Enumeration needs the context, which is created lazily behind mImpl.
    CONJURE_TRY(ensure_context());

    ma_device_info *playbackInfos = nullptr;
    ma_uint32 playbackCount = 0;
    const ma_result enumerated =
      ma_context_get_devices(&mImpl->context, &playbackInfos, &playbackCount, nullptr, nullptr);
    if (enumerated != MA_SUCCESS) {
        return backend_error(audio_error::DEVICE_NOT_FOUND, "ma_context_get_devices", enumerated);
    }

    std::vector<audio_device_info> devices = {};
    devices.reserve(playbackCount);
    for (ma_uint32 index = 0; index < playbackCount; ++index) {
        devices.push_back(describe(playbackInfos[index]));
    }
    return devices;
}

void miniaudio_backend::update(f32 deltaSeconds) {
    static_cast<void>(deltaSeconds);
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

core::result<void> miniaudio_backend::ensure_context() const {
    if (mImpl->contextInitialised) {
        return {};
    }
    ma_context_config config = ma_context_config_init();
    const ma_result initialised = ma_context_init(nullptr, 0, &config, &mImpl->context);
    if (initialised != MA_SUCCESS) {
        return backend_error(audio_error::BACKEND_INIT_FAILED, "ma_context_init", initialised);
    }
    mImpl->contextInitialised = true;
    return {};
}

void miniaudio_backend::uninit_device() noexcept {
    if (!mDeviceInitialised) {
        return;
    }
    // ma_device_uninit stops the device and joins the device thread, so the callback is not running
    // when this returns.
    ma_device_uninit(&mImpl->device);
    mDeviceInitialised = false;
}
