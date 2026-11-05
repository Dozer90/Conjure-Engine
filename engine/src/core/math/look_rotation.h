#pragma once

#include "conjure/core/math/glm_types.h"

// Shared by transform::look_at and the look_at view matrix so both agree on the handling of
// degenerate inputs.

namespace conjure::math::detail {

    /// <summary>
    /// The right-handed camera basis (columns: right, up, back) for a unit view direction and an up
    /// hint: -Z of the basis is the direction. An up hint parallel to the direction (or zero) is
    /// replaced by the world axis least aligned with the direction, so the result is always
    /// orthonormal.
    /// </summary>
    [[nodiscard]] mat3 look_rotation_basis(vec3 direction, vec3 up) noexcept;
}
