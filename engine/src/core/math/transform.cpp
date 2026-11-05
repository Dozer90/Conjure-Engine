#include "conjure/core/math/transform.h"

#include "core/math/look_rotation.h"

#include <glm/gtc/quaternion.hpp>

using namespace conjure;
using namespace conjure::math;

/*===========================================================================*/
/*===========================================================================*/
// Local helpers

namespace {
    // 1 / scale per component; a (nearly) zero component becomes 0 so the result never holds
    // infinities, which would poison every later multiplication with NaN.
    vec3 safe_reciprocal(vec3 scale) noexcept {
        vec3 result = vec3(0.0f);
        for (glm::length_t axis = 0; axis < 3; ++axis) {
            if (!nearly_zero(scale[axis])) {
                result[axis] = 1.0f / scale[axis];
            }
        }
        return result;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Public methods

mat4 transform::to_matrix() const noexcept {
    // translate * rotate * scale, written out: each rotation column scaled by its axis, then the
    // translation in the fourth column. Cheaper than three glm matrix products.
    const mat3 rotationMatrix = glm::mat3_cast(rotation);
    mat4 result = mat4(1.0f);
    result[0] = vec4(rotationMatrix[0] * scale.x, 0.0f);
    result[1] = vec4(rotationMatrix[1] * scale.y, 0.0f);
    result[2] = vec4(rotationMatrix[2] * scale.z, 0.0f);
    result[3] = vec4(position, 1.0f);
    return result;
}

transform transform::from_matrix(const mat4 &matrix) noexcept {
    transform result = {};
    result.position = vec3(matrix[3]);

    mat3 basis = mat3(matrix);
    result.scale = vec3(glm::length(basis[0]), glm::length(basis[1]), glm::length(basis[2]));

    // A collapsed axis makes the determinant exactly zero, which would hide a mirror on the other
    // axes; repair the zero columns first so the sign test below sees a non-degenerate basis.
    for (glm::length_t axis = 0; axis < 3; ++axis) {
        if (nearly_zero(result.scale[axis])) {
            result.scale[axis] = 0.0f;
            basis[axis] = vec3(0.0f);
            basis[axis][axis] = 1.0f;
            continue;
        }
        basis[axis] /= result.scale[axis];
    }

    // A mirrored basis (negative determinant) cannot be a rotation; express the mirror as a
    // negative x scale so that the remaining basis is a proper rotation.
    if (glm::determinant(basis) < 0.0f) {
        result.scale.x = -result.scale.x;
        basis[0] = -basis[0];
    }

    result.rotation = glm::normalize(glm::quat_cast(basis));
    return result;
}

transform transform::operator*(const transform &child) const noexcept {
    transform result = {};
    result.position = transform_point(child.position);
    result.rotation = glm::normalize(rotation * child.rotation);
    result.scale = scale * child.scale;
    return result;
}

transform transform::inverse() const noexcept {
    transform result = {};
    result.rotation = glm::conjugate(rotation);
    result.scale = safe_reciprocal(scale);
    result.position = -(result.rotation * (result.scale * position));
    return result;
}

vec3 transform::inverse_transform_point(vec3 point) const noexcept {
    const vec3 local = glm::conjugate(rotation) * (point - position);
    return safe_reciprocal(scale) * local;
}

transform transform::look_at(vec3 eye, vec3 target, vec3 upHint) noexcept {
    transform result = {};
    result.position = eye;

    const vec3 offset = target - eye;
    if (nearly_zero(offset)) {
        return result;
    }

    result.rotation = glm::normalize(glm::quat_cast(detail::look_rotation_basis(glm::normalize(offset), upHint)));
    return result;
}

transform transform::lerp(const transform &a, const transform &b, f32 t) noexcept {
    // t is deliberately not clamped (like math::lerp) so callers can extrapolate for prediction
    // and rewind; glm::slerp follows the great circle outside [0, 1] just as well.
    transform result = {};
    result.position = glm::mix(a.position, b.position, t);
    result.rotation = glm::normalize(glm::slerp(a.rotation, b.rotation, t));
    result.scale = glm::mix(a.scale, b.scale, t);
    return result;
}
