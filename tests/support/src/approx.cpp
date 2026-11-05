#include "conjure/testing/approx.h"

#include <cmath>

using namespace conjure::testing;
using namespace conjure::math;

/*===========================================================================*/
/*===========================================================================*/
// Scalars and vectors

bool conjure::testing::approx_equal(f32 left, f32 right, f32 epsilon) noexcept {
    return std::fabs(left - right) <= epsilon;
}

bool conjure::testing::approx_equal(const vec2 &left, const vec2 &right, f32 epsilon) noexcept {
    return approx_equal(left.x, right.x, epsilon) && approx_equal(left.y, right.y, epsilon);
}

bool conjure::testing::approx_equal(const vec3 &left, const vec3 &right, f32 epsilon) noexcept {
    return approx_equal(left.x, right.x, epsilon) && approx_equal(left.y, right.y, epsilon)
      && approx_equal(left.z, right.z, epsilon);
}

bool conjure::testing::approx_equal(const vec4 &left, const vec4 &right, f32 epsilon) noexcept {
    return approx_equal(left.x, right.x, epsilon) && approx_equal(left.y, right.y, epsilon)
      && approx_equal(left.z, right.z, epsilon) && approx_equal(left.w, right.w, epsilon);
}


/*===========================================================================*/
/*===========================================================================*/
// Rotations and matrices

bool conjure::testing::approx_equal(const quat &left, const quat &right, f32 epsilon) noexcept {
    // q and -q are the same rotation: compare against whichever sign is closer.
    const bool sameSign = approx_equal(left.x, right.x, epsilon) && approx_equal(left.y, right.y, epsilon)
      && approx_equal(left.z, right.z, epsilon) && approx_equal(left.w, right.w, epsilon);
    if (sameSign) {
        return true;
    }
    return approx_equal(left.x, -right.x, epsilon) && approx_equal(left.y, -right.y, epsilon)
      && approx_equal(left.z, -right.z, epsilon) && approx_equal(left.w, -right.w, epsilon);
}

bool conjure::testing::approx_equal(const mat3 &left, const mat3 &right, f32 epsilon) noexcept {
    for (glm::length_t column = 0; column < 3; ++column) {
        for (glm::length_t row = 0; row < 3; ++row) {
            if (!approx_equal(left[column][row], right[column][row], epsilon)) {
                return false;
            }
        }
    }
    return true;
}

bool conjure::testing::approx_equal(const mat4 &left, const mat4 &right, f32 epsilon) noexcept {
    for (glm::length_t column = 0; column < 4; ++column) {
        for (glm::length_t row = 0; row < 4; ++row) {
            if (!approx_equal(left[column][row], right[column][row], epsilon)) {
                return false;
            }
        }
    }
    return true;
}


/*===========================================================================*/
/*===========================================================================*/
// Geometry

bool conjure::testing::approx_equal(const aabb &left, const aabb &right, f32 epsilon) noexcept {
    return approx_equal(left.min, right.min, epsilon) && approx_equal(left.max, right.max, epsilon);
}

bool conjure::testing::approx_equal(const transform &left, const transform &right, f32 epsilon) noexcept {
    return approx_equal(left.position, right.position, epsilon) && approx_equal(left.rotation, right.rotation, epsilon)
      && approx_equal(left.scale, right.scale, epsilon);
}
