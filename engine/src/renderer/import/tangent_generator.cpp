#include "renderer/import/tangent_generator.h"

#include <cmath>

using namespace conjure;
using namespace conjure::renderer;

math::vec3 conjure::renderer::detail::any_perpendicular(const math::vec3 &normal) noexcept {
    const math::vec3 unit = math::safe_normalize(normal, math::vec3(0.0f, 0.0f, 1.0f));
    const math::vec3 helper = math::absolute(
      unit.y
    ) < 0.99f ? math::vec3(0.0f, 1.0f, 0.0f) : math::vec3(1.0f, 0.0f, 0.0f);
    return math::safe_normalize(glm::cross(helper, unit), math::vec3(1.0f, 0.0f, 0.0f));
}

std::vector<math::vec3> conjure::renderer::detail::flat_normals(
  std::span<const math::vec3> positions,
  std::span<const u32> indices
) {
    std::vector<math::vec3> normals(positions.size(), math::vec3(0.0f, 0.0f, 1.0f));
    for (usize triangle = 0; triangle + 2 < indices.size(); triangle += 3) {
        const u32 i0 = indices[triangle];
        const u32 i1 = indices[triangle + 1];
        const u32 i2 = indices[triangle + 2];
        if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size()) {
            continue;
        }
        const math::vec3 normal = math::safe_normalize(
          glm::cross(positions[i1] - positions[i0], positions[i2] - positions[i0]),
          math::vec3(0.0f, 0.0f, 1.0f)
        );
        normals[i0] = normal;
        normals[i1] = normal;
        normals[i2] = normal;
    }
    return normals;
}

std::vector<math::vec4> conjure::renderer::detail::generate_tangents(
  std::span<const math::vec3> positions,
  std::span<const math::vec3> normals,
  std::span<const math::vec2> uv0,
  std::span<const u32> indices
) {
    const usize vertexCount = positions.size();
    std::vector<math::vec4> tangents(vertexCount, math::vec4(0.0f));
    if (uv0.size() != vertexCount || normals.size() != vertexCount) {
        for (usize index = 0; index < vertexCount; ++index) {
            const math::vec3 normal = index < normals.size() ? normals[index] : math::vec3(0.0f, 0.0f, 1.0f);
            tangents[index] = math::vec4(any_perpendicular(normal), 1.0f);
        }
        return tangents;
    }

    std::vector<math::vec3> tangentSum(vertexCount, math::vec3(0.0f));
    std::vector<math::vec3> bitangentSum(vertexCount, math::vec3(0.0f));

    for (usize triangle = 0; triangle + 2 < indices.size(); triangle += 3) {
        const u32 i0 = indices[triangle];
        const u32 i1 = indices[triangle + 1];
        const u32 i2 = indices[triangle + 2];
        if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) {
            continue;
        }
        const math::vec3 edge1 = positions[i1] - positions[i0];
        const math::vec3 edge2 = positions[i2] - positions[i0];
        const math::vec2 delta1 = uv0[i1] - uv0[i0];
        const math::vec2 delta2 = uv0[i2] - uv0[i0];
        const f32 determinant = delta1.x * delta2.y - delta2.x * delta1.y;
        if (math::absolute(determinant) <= 1.0e-12f) {
            continue;
        }
        const f32 inverse = 1.0f / determinant;
        const math::vec3 tangent = (edge1 * delta2.y - edge2 * delta1.y) * inverse;
        const math::vec3 bitangent = (edge2 * delta1.x - edge1 * delta2.x) * inverse;
        tangentSum[i0] += tangent;
        tangentSum[i1] += tangent;
        tangentSum[i2] += tangent;
        bitangentSum[i0] += bitangent;
        bitangentSum[i1] += bitangent;
        bitangentSum[i2] += bitangent;
    }

    for (usize index = 0; index < vertexCount; ++index) {
        const math::vec3 normal = math::safe_normalize(normals[index], math::vec3(0.0f, 0.0f, 1.0f));
        const math::vec3 accumulated = tangentSum[index];
        // Gram-Schmidt: remove the normal component, fall back to any perpendicular when the
        // UVs gave nothing usable.
        math::vec3 orthogonal = accumulated - normal * glm::dot(normal, accumulated);
        if (glm::dot(orthogonal, orthogonal) <= 1.0e-12f) {
            tangents[index] = math::vec4(any_perpendicular(normal), 1.0f);
            continue;
        }
        orthogonal = glm::normalize(orthogonal);
        const f32 handedness = glm::dot(glm::cross(normal, orthogonal), bitangentSum[index]) < 0.0f ? -1.0f : 1.0f;
        tangents[index] = math::vec4(orthogonal, handedness);
    }
    return tangents;
}
