#include "conjure/core/math/math.h"

#include "core/math/look_rotation.h"
#include "conjure/core/assert.h"

#include <glm/ext/matrix_clip_space.hpp>

using namespace conjure;
using namespace conjure::math;

/*===========================================================================*/
/*===========================================================================*/
// Projection and view matrices

mat4 math::perspective(f32 fovYRadians, f32 aspect, f32 nearPlane, f32 farPlane) noexcept {
    CONJURE_ASSERT(fovYRadians > 0.0f && fovYRadians < PI, "perspective: field of view {} out of (0, PI)", fovYRadians);
    CONJURE_ASSERT(aspect > 0.0f, "perspective: aspect {} must be positive", aspect);
    CONJURE_ASSERT(nearPlane > 0.0f, "perspective: near plane {} must be positive", nearPlane);
    CONJURE_ASSERT(farPlane > nearPlane, "perspective: far plane {} must exceed near plane {}", farPlane, nearPlane);

    // Explicitly the right-handed, zero-to-one variant so the result does not depend on the glm
    // configuration macros of the including translation unit.
    return glm::perspectiveRH_ZO(fovYRadians, aspect, nearPlane, farPlane);
}

mat4 math::infinite_perspective(f32 fovYRadians, f32 aspect, f32 nearPlane) noexcept {
    CONJURE_ASSERT(
      fovYRadians > 0.0f && fovYRadians < PI,
      "infinite_perspective: field of view {} out of (0, PI)",
      fovYRadians
    );
    CONJURE_ASSERT(aspect > 0.0f, "infinite_perspective: aspect {} must be positive", aspect);
    CONJURE_ASSERT(nearPlane > 0.0f, "infinite_perspective: near plane {} must be positive", nearPlane);

    return glm::infinitePerspectiveRH_ZO(fovYRadians, aspect, nearPlane);
}

mat4 math::orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 nearPlane, f32 farPlane) noexcept {
    CONJURE_ASSERT(!nearly_equal(left, right), "orthographic: left and right ({}) must differ", left);
    CONJURE_ASSERT(!nearly_equal(bottom, top), "orthographic: bottom and top ({}) must differ", bottom);
    CONJURE_ASSERT(!nearly_equal(nearPlane, farPlane), "orthographic: near and far ({}) must differ", nearPlane);

    return glm::orthoRH_ZO(left, right, bottom, top, nearPlane, farPlane);
}

mat4 math::look_at(vec3 eye, vec3 target, vec3 up) noexcept {
    const vec3 offset = target - eye;
    if (nearly_zero(offset)) {
        return glm::translate(mat4(1.0f), -eye);
    }

    // The view matrix is the inverse of the camera's rigid transform: transpose the orthonormal
    // basis and translate by the eye expressed in that basis.
    const mat3 basis = detail::look_rotation_basis(glm::normalize(offset), up);
    const mat3 inverseBasis = glm::transpose(basis);
    mat4 result = mat4(inverseBasis);
    result[3] = vec4(-(inverseBasis * eye), 1.0f);
    return result;
}

std::optional<vec3> math::unproject(vec3 normalizedDeviceCoordinates, const mat4 &inverseViewProjection) noexcept {
    const vec4 world = inverseViewProjection * vec4(normalizedDeviceCoordinates, 1.0f);
    if (nearly_zero(world.w)) {
        return std::nullopt;
    }
    return vec3(world) / world.w;
}

std::optional<vec3> math::project(vec3 point, const mat4 &viewProjection) noexcept {
    const vec4 clip = viewProjection * vec4(point, 1.0f);
    if (nearly_zero(clip.w)) {
        return std::nullopt;
    }
    return vec3(clip) / clip.w;
}
