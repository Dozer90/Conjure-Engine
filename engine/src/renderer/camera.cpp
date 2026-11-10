#include "conjure/renderer/camera.h"

#include "conjure/core/assert.h"

#include <cmath>
#include <utility>

// Reverse-Z projections (docs/design/02_renderer.md R1, 4.2). Both matrices are built by hand so the
// near / far roles are explicit: depth is 1 on the near plane and 0 on the far plane.

using namespace conjure;
using namespace conjure::renderer;

namespace {
    [[nodiscard]] bool valid_camera(const camera &value, f32 aspectRatio) noexcept {
        if (!(aspectRatio > 0.0f) || !(value.nearPlane > 0.0f) || !(value.farPlane > value.nearPlane)) {
            return false;
        }
        if (value.projection == projection_type::PERSPECTIVE) {
            return value.verticalFovRadians > 0.0f && value.verticalFovRadians < math::PI;
        }
        return value.orthographicHeight > 0.0f;
    }
}

math::mat4 conjure::renderer::clip_from_view(const camera &camera, f32 aspectRatio) noexcept {
    CONJURE_ASSERT(
      valid_camera(camera, aspectRatio),
      "clip_from_view: invalid camera (aspect {}, near {}, far {}, fov {}, height {})",
      aspectRatio,
      camera.nearPlane,
      camera.farPlane,
      camera.verticalFovRadians,
      camera.orthographicHeight
    );
    if (!valid_camera(camera, aspectRatio)) {
        return math::mat4(1.0f);
    }

    const f32 nearPlane = camera.nearPlane;
    const f32 farPlane = camera.farPlane;
    const f32 depthRange = farPlane - nearPlane;
    math::mat4 result(0.0f);

    if (camera.projection == projection_type::PERSPECTIVE) {
        const f32 focal = 1.0f / std::tan(camera.verticalFovRadians * 0.5f);
        result[0][0] = focal / aspectRatio;
        result[1][1] = focal;
        // depth = (A z + B) / -z with depth(-near) = 1 and depth(-far) = 0.
        result[2][2] = nearPlane / depthRange;
        result[3][2] = (farPlane * nearPlane) / depthRange;
        result[2][3] = -1.0f;
        return result;
    }

    const f32 halfHeight = camera.orthographicHeight * 0.5f;
    const f32 halfWidth = halfHeight * aspectRatio;
    result[0][0] = 1.0f / halfWidth;
    result[1][1] = 1.0f / halfHeight;
    // depth = A z + B with depth(-near) = 1 and depth(-far) = 0.
    result[2][2] = 1.0f / depthRange;
    result[3][2] = farPlane / depthRange;
    result[3][3] = 1.0f;
    return result;
}

math::frustum conjure::renderer::frustum_from_clip(const math::mat4 &clipFromWorld) noexcept {
    // Gribb-Hartmann yields the planes z >= 0 and w - z >= 0; under reverse-Z the first is the
    // geometric far plane and the second the near plane, so the two labels are swapped here.
    math::frustum result = math::frustum::from_view_projection(clipFromWorld);
    std::swap(
      result.planes[static_cast<usize>(math::frustum_plane::NEAR_PLANE)],
      result.planes[static_cast<usize>(math::frustum_plane::FAR_PLANE)]
    );
    return result;
}

math::ray conjure::renderer::ray_from_viewport(
  const camera &camera,
  const math::mat4 &worldFromView,
  math::vec2 viewportUv,
  f32 aspectRatio
) noexcept {
    const math::mat4 clipFromView = clip_from_view(camera, aspectRatio);
    const math::mat4 worldFromClip = worldFromView * glm::inverse(clipFromView);

    const f32 ndcX = viewportUv.x * 2.0f - 1.0f;
    const f32 ndcY = 1.0f - viewportUv.y * 2.0f;

    // Near is depth 1, far is depth 0 under reverse-Z.
    const math::vec4 nearClip = worldFromClip * math::vec4(ndcX, ndcY, 1.0f, 1.0f);
    const math::vec4 farClip = worldFromClip * math::vec4(ndcX, ndcY, 0.0f, 1.0f);

    math::ray result = {};
    if (math::absolute(nearClip.w) <= math::EPSILON || math::absolute(farClip.w) <= math::EPSILON) {
        result.origin = math::vec3(worldFromView[3]);
        result.direction = math::safe_normalize(-math::vec3(worldFromView[2]));
        return result;
    }
    const math::vec3 nearPoint = math::vec3(nearClip) / nearClip.w;
    const math::vec3 farPoint = math::vec3(farClip) / farClip.w;
    result.origin = nearPoint;
    result.direction = math::safe_normalize(farPoint - nearPoint, math::safe_normalize(-math::vec3(worldFromView[2])));
    return result;
}

std::string_view conjure::renderer::to_string(projection_type type) noexcept {
    switch (type) {
        case projection_type::PERSPECTIVE: {
            return "PERSPECTIVE";
        }
        case projection_type::ORTHOGRAPHIC: {
            return "ORTHOGRAPHIC";
        }
    }
    return "PERSPECTIVE";
}
