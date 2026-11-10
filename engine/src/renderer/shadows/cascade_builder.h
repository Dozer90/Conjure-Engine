#pragma once

#include "conjure/renderer/camera.h"
#include "conjure/renderer/limits.h"
#include "conjure/renderer/render_settings.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <array>
#include <span>

// Cascaded shadow maps for a directional light (docs/design/02_renderer.md R9, 3.4): practical
// split scheme lerp(uniform, logarithmic, lambda) over [near, shadows.distance]; each slice's eight
// frustum corners give a bounding sphere whose radius is rounded up to a centimetre, the light's
// orthographic projection is sized to the sphere and its translation snapped to whole texels, and the
// depth range is padded toward the light so off-screen casters stay inside. Reverse-Z projections.

namespace conjure::renderer {

    struct cascade {
        math::mat4 viewFromWorld = math::mat4(1.0f);    // light view
        math::mat4 clipFromWorld = math::mat4(1.0f);    // reverse-Z orthographic light projection * view
        math::vec3 centre = math::vec3(0.0f);           // world-space bounding sphere centre (snapped)
        f32 radius = 0.0f;
        f32 nearDistance = 0.0f;                        // camera-space split start
        f32 farDistance = 0.0f;                         // camera-space split end
        f32 texelWorldSize = 0.0f;                      // metres per shadow texel
    };

    struct cascade_set {
        std::array<cascade, MAX_SHADOW_CASCADES> cascades = {};
        u32 count = 0;
    };

    /// <summary>
    /// The split distances: count + 1 values from the near plane to the shadow distance.
    /// </summary>
    [[nodiscard]] std::array<f32, MAX_SHADOW_CASCADES + 1> compute_cascade_splits(
      f32 nearPlane,
      f32 shadowDistance,
      u32 count,
      f32 lambda
    ) noexcept;

    /// <summary>
    /// Builds the cascades of a view for a light direction (pointing from the light).
    /// </summary>
    [[nodiscard]] cascade_set build_cascades(
      const camera &cameraData,
      const math::mat4 &worldFromView,
      f32 aspectRatio,
      const math::vec3 &lightDirection,
      const shadow_settings &settings,
      u32 resolution
    ) noexcept;

    /// <summary>
    /// The eight world-space corners of a camera slice between two view distances.
    /// </summary>
    [[nodiscard]] std::array<math::vec3, 8> slice_corners(
      const camera &cameraData,
      const math::mat4 &worldFromView,
      f32 aspectRatio,
      f32 nearDistance,
      f32 farDistance
    ) noexcept;
}
