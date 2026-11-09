#pragma once

#include "conjure/audio/audio_types.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

// Listener and spatial maths (docs/design/12_audio.md 2.5). Pure functions, callable from any
// thread; the engine runs them on the main thread once per spatial voice per frame and sends the
// results to the mixer as commands.

namespace conjure::audio {

    /// <summary>
    /// The one listener. Defaults: origin, forward -Z, up +Y, no velocity, gain 1.
    /// </summary>
    struct listener_state {
        math::vec3 position = math::vec3(0.0f);
        math::vec3 forward = math::vec3(0.0f, 0.0f, -1.0f);
        math::vec3 up = math::vec3(0.0f, 1.0f, 0.0f);
        math::vec3 velocity = math::vec3(0.0f);
        f32 gain = 1.0f;                    // [0, 2]

        [[nodiscard]] bool operator==(const listener_state &other) const noexcept = default;
    };

    /// <summary>
    /// Distance attenuation of one voice.
    /// </summary>
    struct attenuation_params {
        attenuation_model model = attenuation_model::INVERSE;
        f32 minDistance = 1.0f;             // >= 0.01
        f32 maxDistance = 100.0f;           // >= minDistance
        f32 rolloff = 1.0f;                 // [0, 10]

        [[nodiscard]] bool operator==(const attenuation_params &other) const noexcept = default;
    };

    /// <summary>
    /// What the mixer needs for a spatial voice.
    /// </summary>
    struct spatial_result {
        f32 gain = 1.0f;
        f32 pan = 0.0f;
        f32 pitchRatio = 1.0f;

        [[nodiscard]] bool operator==(const spatial_result &other) const noexcept = default;
    };

    /// <summary>
    /// Clamp rules of A15: minDistance >= 0.01, maxDistance >= minDistance, rolloff [0, 10]; NaN
    /// becomes the default.
    /// </summary>
    [[nodiscard]] attenuation_params clamp_attenuation(const attenuation_params &params) noexcept;

    /// <summary>
    /// Sanitises a listener: non-finite components become the defaults, forward and up are
    /// normalised (and up made perpendicular to forward), gain clamped to [0, 2].
    /// </summary>
    [[nodiscard]] listener_state clamp_listener(const listener_state &listener) noexcept;

    /// <summary>
    /// Gain in [0, 1] for a distance, with d = clamp(distance, minDistance, maxDistance):
    /// INVERSE min / (min + rolloff * (d - min)), LINEAR 1 - rolloff * (d - min) / (max - min)
    /// clamped to [0, 1], EXPONENTIAL (d / min) ^ -rolloff, NONE 1.
    /// </summary>
    [[nodiscard]] f32 compute_attenuation(const attenuation_params &params, f32 distance) noexcept;

    /// <summary>
    /// Stereo pan in [-1, 1]: the listener-right component of the normalised direction to the
    /// source (+1 right, -1 left, 0 ahead, behind or coincident).
    /// </summary>
    [[nodiscard]] f32 compute_pan(const listener_state &listener, math::vec3 sourcePosition) noexcept;

    /// <summary>
    /// Doppler pitch ratio ((c - v_listener . dir) / (c - v_source . dir)) ^ dopplerFactor,
    /// clamped to [0.5, 2]; 1 when dopplerFactor is 0 or the source is at the listener.
    /// </summary>
    [[nodiscard]] f32 compute_doppler_ratio(
      const listener_state &listener,
      math::vec3 sourcePosition,
      math::vec3 sourceVelocity,
      f32 dopplerFactor,
      f32 speedOfSound
    ) noexcept;

    /// <summary>
    /// Attenuation, pan and Doppler ratio of a source relative to the listener in one call.
    /// </summary>
    [[nodiscard]] spatial_result compute_spatial(
      const listener_state &listener,
      math::vec3 sourcePosition,
      math::vec3 sourceVelocity,
      const attenuation_params &params,
      f32 dopplerFactor,
      f32 speedOfSound
    ) noexcept;
}
