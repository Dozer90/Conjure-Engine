#include "conjure/audio/voice.h"
#include "conjure/audio/mix_groups.h"
#include "conjure/audio/spatialisation.h"

#include <algorithm>
#include <cmath>

using namespace conjure;
using namespace conjure::audio;

namespace {
    constexpr f32 MIN_VOLUME = 0.0f;
    constexpr f32 MAX_VOLUME = 2.0f;
    constexpr f32 MIN_PITCH = 0.25f;
    constexpr f32 MAX_PITCH = 4.0f;

    [[nodiscard]] f32 clamp_or_default(f32 value, f32 low, f32 high, f32 fallback) noexcept {
        if (!std::isfinite(value)) {
            return fallback;
        }
        return std::clamp(value, low, high);
    }
}

f32 conjure::audio::clamp_volume(f32 volume) noexcept {
    return clamp_or_default(volume, MIN_VOLUME, MAX_VOLUME, 1.0f);
}

f32 conjure::audio::clamp_pitch(f32 pitch) noexcept {
    return clamp_or_default(pitch, MIN_PITCH, MAX_PITCH, 1.0f);
}

f32 conjure::audio::clamp_pan(f32 pan) noexcept {
    return clamp_or_default(pan, -1.0f, 1.0f, 0.0f);
}

play_params conjure::audio::clamp_play_params(const play_params &params) noexcept {
    play_params clamped = params;
    clamped.volume = clamp_volume(params.volume);
    clamped.pitch = clamp_pitch(params.pitch);
    clamped.pan = clamp_pan(params.pan);
    if (!std::isfinite(params.startSeconds) || params.startSeconds < 0.0) {
        clamped.startSeconds = 0.0;
    }

    attenuation_params attenuation = {};
    attenuation.model = params.attenuation;
    attenuation.minDistance = params.minDistance;
    attenuation.maxDistance = params.maxDistance;
    attenuation.rolloff = params.rolloff;
    attenuation = clamp_attenuation(attenuation);
    clamped.minDistance = attenuation.minDistance;
    clamped.maxDistance = attenuation.maxDistance;
    clamped.rolloff = attenuation.rolloff;
    return clamped;
}
