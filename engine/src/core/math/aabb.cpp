#include "conjure/core/math/aabb.h"

using namespace conjure;
using namespace conjure::math;

/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::array<vec3, 8> aabb::corners() const noexcept {
    std::array<vec3, 8> result = {};
    for (usize index = 0; index < result.size(); ++index) {
        result[index] = vec3(
          (index & 1u) != 0 ? max.x : min.x,
          (index & 2u) != 0 ? max.y : min.y,
          (index & 4u) != 0 ? max.z : min.z
        );
    }
    return result;
}

aabb aabb::transformed(const mat4 &matrix) const noexcept {
    if (!is_valid()) {
        return *this;
    }

    aabb result = {};
    for (const vec3 &corner : corners()) {
        result.expand(vec3(matrix * vec4(corner, 1.0f)));
    }
    return result;
}
