#include "renderer/gpu_types.h"

#include <bit>

using namespace conjure;
using namespace conjure::renderer;

gpu::instance conjure::renderer::gpu::make_instance(
  const math::mat4 &worldFromLocal,
  u32 entityId,
  u32 flags
) noexcept {
    instance record = {};
    // Row r of the 3x4 matrix is (m[0][r], m[1][r], m[2][r], m[3][r]) in glm's column-major storage.
    for (glm::length_t row = 0; row < 3; ++row) {
        const math::vec4 values(
          worldFromLocal[0][row],
          worldFromLocal[1][row],
          worldFromLocal[2][row],
          worldFromLocal[3][row]
        );
        if (row == 0) {
            record.world0 = values;
        } else if (row == 1) {
            record.world1 = values;
        } else {
            record.world2 = values;
        }
    }

    // The normal matrix is the inverse transpose of the upper 3x3; a singular matrix (zero scale)
    // falls back to the upper 3x3 itself so nothing becomes NaN.
    const math::mat3 upper(worldFromLocal);
    const f32 determinant = glm::determinant(upper);
    const math::mat3 normalMatrix = math::absolute(
      determinant
    ) > 1.0e-12f ? glm::transpose(glm::inverse(upper)) : upper;
    record.normal0 = math::vec4(normalMatrix[0][0], normalMatrix[1][0], normalMatrix[2][0], bits_as_float(entityId));
    record.normal1 = math::vec4(normalMatrix[0][1], normalMatrix[1][1], normalMatrix[2][1], bits_as_float(flags));
    record.normal2 = math::vec4(normalMatrix[0][2], normalMatrix[1][2], normalMatrix[2][2], 0.0f);
    return record;
}

f32 conjure::renderer::gpu::bits_as_float(u32 bits) noexcept {
    return std::bit_cast<f32>(bits);
}

u32 conjure::renderer::gpu::float_as_bits(f32 value) noexcept {
    return std::bit_cast<u32>(value);
}
