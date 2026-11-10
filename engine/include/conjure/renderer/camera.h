#pragma once

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <string_view>

// Cameras and projection helpers (docs/design/02_renderer.md 4.2). Every projection is reverse-Z:
// the near plane maps to depth 1 and the far plane to depth 0, for perspective and orthographic
// cameras alike, so a single GreaterOrEqual depth test serves every pass (R1).

namespace conjure::renderer {

    enum class projection_type : u8 {
        PERSPECTIVE = 0,
        ORTHOGRAPHIC
    };

    struct camera {
        projection_type projection = projection_type::PERSPECTIVE;
        f32 verticalFovRadians = 1.0472f;       // 60 degrees
        f32 orthographicHeight = 10.0f;         // world units visible vertically
        f32 nearPlane = 0.1f;
        f32 farPlane = 1000.0f;

        [[nodiscard]] bool operator==(const camera &other) const noexcept = default;
    };

    /// <summary>
    /// Reverse-Z projection: near maps to depth 1, far to depth 0, for both projection types. A
    /// non-positive aspect ratio, near plane or field of view, or a far plane at or before the near
    /// plane, asserts and yields the identity.
    /// </summary>
    [[nodiscard]] math::mat4 clip_from_view(const camera &camera, f32 aspectRatio) noexcept;

    /// <summary>
    /// The inward-facing frustum planes of a clip-from-world matrix (reverse-Z aware: the near plane
    /// is the one at depth 1).
    /// </summary>
    [[nodiscard]] math::frustum frustum_from_clip(const math::mat4 &clipFromWorld) noexcept;

    /// <summary>
    /// The world-space ray through a viewport position given in [0, 1] x [0, 1] (origin top-left, y
    /// down). The ray starts on the near plane; for a perspective camera it passes through the eye.
    /// The aspect ratio is the viewport's width over height (the design's signature omitted it, but a
    /// non-square viewport cannot be unprojected without it).
    /// </summary>
    [[nodiscard]] math::ray ray_from_viewport(
      const camera &camera,
      const math::mat4 &worldFromView,
      math::vec2 viewportUv,
      f32 aspectRatio
    ) noexcept;

    /// <summary>
    /// The enumerator name of a projection type ("PERSPECTIVE").
    /// </summary>
    [[nodiscard]] std::string_view to_string(projection_type type) noexcept;
}
