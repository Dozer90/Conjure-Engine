#include "core/math/look_rotation.h"

using namespace conjure;
using namespace conjure::math;

mat3 detail::look_rotation_basis(vec3 direction, vec3 up) noexcept {
    const vec3 back = -direction;

    vec3 right = glm::cross(up, back);
    if (nearly_zero(right)) {
        // The hint is useless (parallel to the view direction or zero): pick the world axis that
        // is least aligned with the direction as a replacement hint (ties prefer Y, then Z).
        const vec3 magnitudes = glm::abs(direction);
        vec3 fallbackUp = vec3(0.0f, 1.0f, 0.0f);
        if (magnitudes.y > magnitudes.x || magnitudes.y > magnitudes.z) {
            fallbackUp = magnitudes.z <= magnitudes.x ? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
        }
        right = glm::cross(fallbackUp, back);
    }

    right = glm::normalize(right);
    const vec3 orthogonalUp = glm::cross(back, right);
    return mat3(right, orthogonalUp, back);
}
