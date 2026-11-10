#include "renderer/shadows/cascade_builder.h"

#include <algorithm>
#include <cmath>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    constexpr f32 RADIUS_ROUNDING = 0.01f;   // one centimetre

    /// <summary>
    /// Reverse-Z orthographic projection: depth 1 at -nearPlane, 0 at -farPlane (view space -Z).
    /// </summary>
    [[nodiscard]] math::mat4 reverse_orthographic(f32 halfExtent, f32 nearPlane, f32 farPlane) noexcept {
        math::mat4 result(0.0f);
        const f32 depthRange = farPlane - nearPlane;
        result[0][0] = 1.0f / halfExtent;
        result[1][1] = 1.0f / halfExtent;
        result[2][2] = 1.0f / depthRange;
        result[3][2] = farPlane / depthRange;
        result[3][3] = 1.0f;
        return result;
    }
}

std::array<f32, MAX_SHADOW_CASCADES + 1> conjure::renderer::compute_cascade_splits(
  f32 nearPlane,
  f32 shadowDistance,
  u32 count,
  f32 lambda
) noexcept {
    std::array<f32, MAX_SHADOW_CASCADES + 1> splits = {};
    const u32 cascadeCount = std::clamp<u32>(count, 1, MAX_SHADOW_CASCADES);
    const f32 near = std::max(nearPlane, 1.0e-3f);
    const f32 far = std::max(shadowDistance, near * 1.001f);
    const f32 weight = math::saturate(lambda);
    splits[0] = near;
    for (u32 index = 1; index <= cascadeCount; ++index) {
        const f32 fraction = static_cast<f32>(index) / static_cast<f32>(cascadeCount);
        const f32 uniform = near + (far - near) * fraction;
        const f32 logarithmic = near * std::pow(far / near, fraction);
        splits[index] = math::lerp(uniform, logarithmic, weight);
    }
    for (u32 index = cascadeCount + 1; index <= MAX_SHADOW_CASCADES; ++index) {
        splits[index] = far;
    }
    return splits;
}

std::array<math::vec3, 8> conjure::renderer::slice_corners(
  const camera &cameraData,
  const math::mat4 &worldFromView,
  f32 aspectRatio,
  f32 nearDistance,
  f32 farDistance
) noexcept {
    std::array<math::vec3, 8> corners = {};
    f32 nearHalfHeight = 0.0f;
    f32 farHalfHeight = 0.0f;
    if (cameraData.projection == projection_type::PERSPECTIVE) {
        const f32 tangent = std::tan(cameraData.verticalFovRadians * 0.5f);
        nearHalfHeight = nearDistance * tangent;
        farHalfHeight = farDistance * tangent;
    } else {
        nearHalfHeight = cameraData.orthographicHeight * 0.5f;
        farHalfHeight = nearHalfHeight;
    }
    const f32 nearHalfWidth = nearHalfHeight * aspectRatio;
    const f32 farHalfWidth = farHalfHeight * aspectRatio;
    // Near: bottom-left, bottom-right, top-right, top-left; then the far plane.
    const std::array<math::vec3, 8> view = {
      math::vec3(
        -nearHalfWidth,
        -nearHalfHeight,
        -nearDistance
      ), math::vec3(nearHalfWidth, -nearHalfHeight, -nearDistance),
      math::vec3(
        nearHalfWidth,
        nearHalfHeight,
        -nearDistance
      ),   math::vec3(-nearHalfWidth, nearHalfHeight, -nearDistance),
      math::vec3(
        -farHalfWidth,
        -farHalfHeight,
        -farDistance
      ),    math::vec3(farHalfWidth, -farHalfHeight, -farDistance),
      math::vec3(
        farHalfWidth,
        farHalfHeight,
        -farDistance
      ),      math::vec3(-farHalfWidth, farHalfHeight, -farDistance),
    };
    for (usize index = 0; index < 8; ++index) {
        corners[index] = math::vec3(worldFromView * math::vec4(view[index], 1.0f));
    }
    return corners;
}

cascade_set conjure::renderer::build_cascades(
  const camera &cameraData,
  const math::mat4 &worldFromView,
  f32 aspectRatio,
  const math::vec3 &lightDirection,
  const shadow_settings &settings,
  u32 resolution
) noexcept {
    cascade_set set = {};
    set.count = std::clamp<u32>(settings.cascadeCount, 1, MAX_SHADOW_CASCADES);
    const f32 texels = static_cast<f32>(std::max<u32>(resolution, 1));
    const std::array<f32, MAX_SHADOW_CASCADES + 1> splits =
      compute_cascade_splits(cameraData.nearPlane, settings.distance, set.count, settings.splitLambda);
    const math::vec3 direction = math::safe_normalize(lightDirection, math::vec3(0.0f, -1.0f, 0.0f));
    const math::vec3 up = math::absolute(
      direction.y
    ) < 0.99f ? math::vec3(0.0f, 1.0f, 0.0f) : math::vec3(1.0f, 0.0f, 0.0f);

    for (u32 index = 0; index < set.count; ++index) {
        cascade &slice = set.cascades[index];
        slice.nearDistance = splits[index];
        slice.farDistance = splits[index + 1];
        const std::array<math::vec3, 8> corners = slice_corners(
          cameraData,
          worldFromView,
          aspectRatio,
          slice.nearDistance,
          slice.farDistance
        );

        math::vec3 centre(0.0f);
        for (const math::vec3 &corner : corners) {
            centre += corner;
        }
        centre /= 8.0f;
        f32 radius = 0.0f;
        for (const math::vec3 &corner : corners) {
            radius = std::max(radius, glm::length(corner - centre));
        }
        radius = std::ceil(radius / RADIUS_ROUNDING) * RADIUS_ROUNDING;
        radius = std::max(radius, RADIUS_ROUNDING);
        slice.radius = radius;
        slice.texelWorldSize = (2.0f * radius) / texels;

        // Snap the centre to whole texels in light space so the map does not swim as the camera moves.
        // The snapping frame is anchored at the world origin: a frame centred on the (moving) slice
        // centre would snap relative to itself and never hold still.
        const math::mat4 lightView = math::look_at(-direction, math::vec3(0.0f), up);
        math::vec4 lightCentre = lightView * math::vec4(centre, 1.0f);
        const f32 texelSize = slice.texelWorldSize;
        lightCentre.x = std::floor(lightCentre.x / texelSize) * texelSize;
        lightCentre.y = std::floor(lightCentre.y / texelSize) * texelSize;
        const math::vec3 snappedCentre = math::vec3(glm::inverse(lightView) * lightCentre);
        slice.centre = snappedCentre;

        const f32 padding = std::max(settings.casterPadding, 0.0f);
        const math::vec3 eye = snappedCentre - direction * (radius + padding);
        slice.viewFromWorld = math::look_at(eye, snappedCentre, up);
        const math::mat4 projection = reverse_orthographic(radius, 0.0f, 2.0f * radius + padding);
        slice.clipFromWorld = projection * slice.viewFromWorld;
    }
    return set;
}
