#include "conjure/audio/null_backend.h"
#include "conjure/audio/audio_error.h"

#include "conjure/core/assert.h"
#include "conjure/core/error.h"

#include <algorithm>
#include <cmath>

using namespace conjure;
using namespace conjure::audio;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

null_backend::null_backend() :
    mRenderBuffer(static_cast<usize>(MIX_BLOCK_FRAMES) * MIX_CHANNELS, 0.0f) {}

null_backend::~null_backend() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

audio_backend_kind null_backend::kind() const noexcept {
    return audio_backend_kind::NULL_BACKEND;
}

core::result<void> null_backend::start(const backend_desc &desc) {
    if (desc.callback == nullptr) {
        return core::make_error(audio_error::INVALID_SETTINGS, "null backend: the mix callback is null");
    }
    if (desc.format.sampleRate < MIN_SAMPLE_RATE || desc.format.sampleRate > MAX_SAMPLE_RATE) {
        return core::make_error(
          audio_error::INVALID_SETTINGS,
          "null backend: sample rate {} outside [{}, {}]",
          desc.format.sampleRate,
          MIN_SAMPLE_RATE,
          MAX_SAMPLE_RATE
        );
    }

    stop();
    if (mFailingStarts > 0) {
        --mFailingStarts;
        return core::make_error(
          audio_error::DEVICE_START_FAILED,
          "null backend: start failure requested through fail_next_starts()"
        );
    }
    mDesc = desc;
    mDesc.format.channels = static_cast<u16>(MIX_CHANNELS);
    mDesc.format.format = sample_format::F32;
    mFractionalFrames = 0.0;
    mStats.virtualTimeSeconds = 0.0;
    mStats.framesRendered = 0;
    mStats.renderCalls = 0;
    mState.store(device_state::RUNNING, std::memory_order_release);
    return {};
}

void null_backend::stop() noexcept {
    mState.store(device_state::STOPPED, std::memory_order_release);
}

device_state null_backend::state() const noexcept {
    return mState.load(std::memory_order_acquire);
}

audio_format null_backend::output_format() const noexcept {
    return mDesc.format;
}

core::result<std::vector<audio_device_info>> null_backend::enumerate_devices() const {
    audio_device_info device = {};
    device.name = NULL_DEVICE_NAME;
    device.isDefault = true;
    device.sampleRates = {48000};
    device.minChannels = MIX_CHANNELS;
    device.maxChannels = MIX_CHANNELS;
    std::vector<audio_device_info> devices = {};
    devices.push_back(std::move(device));
    return devices;
}

void null_backend::update(f32 deltaSeconds) {
    if (state() != device_state::RUNNING || !(deltaSeconds > 0.0f)) {
        return;
    }
    const f64 exact = static_cast<f64>(deltaSeconds) * static_cast<f64>(mDesc.format.sampleRate) + mFractionalFrames;
    const f64 whole = std::round(exact);
    mFractionalFrames = exact - whole;
    if (whole <= 0.0) {
        return;
    }
    render_frames(static_cast<u32>(whole));
}

void null_backend::render_frames(u32 frameCount) {
    if (state() != device_state::RUNNING || mDesc.callback == nullptr) {
        return;
    }

    u32 remaining = frameCount;
    while (remaining > 0) {
        const u32 blockFrames = std::min(remaining, MIX_BLOCK_FRAMES);
        mDesc.callback(mDesc.user, mRenderBuffer.data(), blockFrames);
        ++mStats.renderCalls;
        remaining -= blockFrames;

        if (mCaptureFrames > 0) {
            const usize blockSamples = static_cast<usize>(blockFrames) * MIX_CHANNELS;
            const usize capacitySamples = static_cast<usize>(mCaptureFrames) * MIX_CHANNELS;
            const auto blockEnd = mRenderBuffer.begin() + static_cast<isize>(blockSamples);
            mCaptured.insert(mCaptured.end(), mRenderBuffer.begin(), blockEnd);
            if (mCaptured.size() > capacitySamples) {
                const auto excessEnd = mCaptured.begin() + static_cast<isize>(mCaptured.size() - capacitySamples);
                mCaptured.erase(mCaptured.begin(), excessEnd);
            }
        }
    }

    mStats.framesRendered += frameCount;
    mStats.virtualTimeSeconds = static_cast<f64>(mStats.framesRendered) / static_cast<f64>(mDesc.format.sampleRate);
}

void null_backend::set_capture_frames(u32 frames) {
    mCaptureFrames = frames;
    mCaptured.clear();
    mCaptured.reserve(static_cast<usize>(frames) * MIX_CHANNELS * 2);
}

void null_backend::simulate_device_loss() noexcept {
    mState.store(device_state::LOST, std::memory_order_release);
}

void null_backend::fail_next_starts(u32 count) noexcept {
    mFailingStarts = count;
}
