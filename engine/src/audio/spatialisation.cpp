#include "conjure/audio/spatialisation.h"

#include <algorithm>
#include <cmath>

using namespace conjure;
using namespace conjure::audio;

namespace {
    constexpr f32 MIN_MIN_DISTANCE = 0.01f;
    constexpr f32 MAX_ROLLOFF = 10.0f;
    constexpr f32 MIN_DOPPLER_RATIO = 0.5f;
    constexpr f32 MAX_DOPPLER_RATIO = 2.0f;
    constexpr f32 MAX_DOPPLER_FACTOR = 10.0f;

    [[nodiscard]] bool is_finite(const math::vec3 &value) noexcept {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    [[nodiscard]] math::vec3 right_of(const listener_state &listener) noexcept {
        return math::safe_normalize(glm::cross(listener.forward, listener.up), math::vec3(1.0f, 0.0f, 0.0f));
    }
}

attenuation_params conjure::audio::clamp_attenuation(const attenuation_params &params) noexcept {
    attenuation_params clamped = params;
    const attenuation_params defaults = {};

    if (!std::isfinite(params.minDistance)) {
        clamped.minDistance = defaults.minDistance;
    }
    clamped.minDistance = std::max(clamped.minDistance, MIN_MIN_DISTANCE);

    if (!std::isfinite(params.maxDistance)) {
        clamped.maxDistance = defaults.maxDistance;
    }
    clamped.maxDistance = std::max(clamped.maxDistance, clamped.minDistance);

    if (!std::isfinite(params.rolloff)) {
        clamped.rolloff = defaults.rolloff;
    }
    clamped.rolloff = std::clamp(clamped.rolloff, 0.0f, MAX_ROLLOFF);
    return clamped;
}

listener_state conjure::audio::clamp_listener(const listener_state &listener) noexcept {
    const listener_state defaults = {};
    listener_state clamped = listener;

    if (!is_finite(listener.position)) {
        clamped.position = defaults.position;
    }
    if (!is_finite(listener.velocity)) {
        clamped.velocity = defaults.velocity;
    }
    if (!is_finite(listener.forward)) {
        clamped.forward = defaults.forward;
    }
    if (!is_finite(listener.up)) {
        clamped.up = defaults.up;
    }
    clamped.forward = math::safe_normalize(clamped.forward, defaults.forward);
    clamped.up = math::safe_normalize(clamped.up, defaults.up);

    // Make up perpendicular to forward; a parallel pair falls back to the default basis.
    const math::vec3 right = glm::cross(clamped.forward, clamped.up);
    if (math::nearly_zero(right)) {
        const math::vec3 alternative = std::abs(clamped.forward.y) < 0.9f ? defaults.up : math::vec3(1.0f, 0.0f, 0.0f);
        const math::vec3 rebuiltRight = glm::cross(clamped.forward, alternative);
        clamped.up = math::safe_normalize(glm::cross(rebuiltRight, clamped.forward), defaults.up);
    } else {
        clamped.up = math::safe_normalize(glm::cross(right, clamped.forward), defaults.up);
    }

    if (!std::isfinite(listener.gain)) {
        clamped.gain = defaults.gain;
    }
    clamped.gain = std::clamp(clamped.gain, 0.0f, 2.0f);
    return clamped;
}

f32 conjure::audio::compute_attenuation(const attenuation_params &params, f32 distance) noexcept {
    const attenuation_params safe = clamp_attenuation(params);
    if (!std::isfinite(distance)) {
        distance = safe.maxDistance;
    }
    const f32 clamped = std::clamp(distance, safe.minDistance, safe.maxDistance);

    switch (safe.model) {
        case attenuation_model::NONE:
            return 1.0f;
        case attenuation_model::INVERSE: {
            const f32 denominator = safe.minDistance + safe.rolloff * (clamped - safe.minDistance);
            if (denominator <= 0.0f) {
                return 1.0f;
            }
            return std::clamp(safe.minDistance / denominator, 0.0f, 1.0f);
        }
        case attenuation_model::LINEAR: {
            const f32 range = safe.maxDistance - safe.minDistance;
            if (range <= 0.0f) {
                return 1.0f;
            }
            return std::clamp(1.0f - safe.rolloff * (clamped - safe.minDistance) / range, 0.0f, 1.0f);
        }
        case attenuation_model::EXPONENTIAL:
            return std::clamp(std::pow(clamped / safe.minDistance, -safe.rolloff), 0.0f, 1.0f);
    }
    return 1.0f;
}

f32 conjure::audio::compute_pan(const listener_state &listener, math::vec3 sourcePosition) noexcept {
    if (!is_finite(sourcePosition)) {
        return 0.0f;
    }
    const math::vec3 offset = sourcePosition - listener.position;
    if (math::nearly_zero(offset)) {
        return 0.0f;
    }
    const math::vec3 direction = math::safe_normalize(offset);
    return std::clamp(glm::dot(direction, right_of(listener)), -1.0f, 1.0f);
}

f32 conjure::audio::compute_doppler_ratio(
  const listener_state &listener,
  math::vec3 sourcePosition,
  math::vec3 sourceVelocity,
  f32 dopplerFactor,
  f32 speedOfSound
) noexcept {
    if (!std::isfinite(dopplerFactor) || dopplerFactor <= 0.0f) {
        return 1.0f;
    }
    if (!std::isfinite(speedOfSound) || speedOfSound <= 0.0f) {
        return 1.0f;
    }
    if (!is_finite(sourcePosition) || !is_finite(sourceVelocity)) {
        return 1.0f;
    }
    const math::vec3 offset = sourcePosition - listener.position;
    if (math::nearly_zero(offset)) {
        return 1.0f;
    }
    const math::vec3 direction = math::safe_normalize(offset);
    const f32 factor = std::min(dopplerFactor, MAX_DOPPLER_FACTOR);

    // 'direction' points from the listener to the source, so a positive source speed recedes and a
    // positive listener speed approaches: f' = f (c + v_listener) / (c + v_source). The speed of
    // sound bounds both so the ratio stays finite.
    const f32 limit = speedOfSound * 0.99f;
    const f32 listenerSpeed = std::clamp(glm::dot(listener.velocity, direction), -limit, limit);
    const f32 sourceSpeed = std::clamp(glm::dot(sourceVelocity, direction), -limit, limit);
    const f32 ratio = (speedOfSound + listenerSpeed) / (speedOfSound + sourceSpeed);
    if (!std::isfinite(ratio) || ratio <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(std::pow(ratio, factor), MIN_DOPPLER_RATIO, MAX_DOPPLER_RATIO);
}

spatial_result conjure::audio::compute_spatial(
  const listener_state &listener,
  math::vec3 sourcePosition,
  math::vec3 sourceVelocity,
  const attenuation_params &params,
  f32 dopplerFactor,
  f32 speedOfSound
) noexcept {
    spatial_result result = {};
    if (!is_finite(sourcePosition)) {
        sourcePosition = listener.position;
    }
    const f32 distance = glm::length(sourcePosition - listener.position);
    result.gain = compute_attenuation(params, distance);
    result.pan = compute_pan(listener, sourcePosition);
    result.pitchRatio = compute_doppler_ratio(listener, sourcePosition, sourceVelocity, dopplerFactor, speedOfSound);
    return result;
}
