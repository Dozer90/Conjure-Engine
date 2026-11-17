#pragma once

#include "conjure/renderer/mesh_asset.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <span>
#include <vector>

// Attribute generation for meshes without normals or tangents (docs/design/02_renderer.md 4.2, R20).
// Normals are flat per face (the glTF rule for missing normals), which needs one vertex per
// triangle corner; tangents come from the UV derivatives with Gram-Schmidt orthogonalisation and
// a handedness sign (Lengyel), or from the normal alone when the caller asks for that or the UVs
// give nothing.

namespace conjure::renderer::detail {

    /// <summary>
    /// Face normals, one per vertex, for an unwelded triangle list (vertex i belongs to triangle
    /// i / 3). Degenerate triangles get +Z.
    /// </summary>
    [[nodiscard]] std::vector<math::vec3> flat_normals(
      std::span<const math::vec3> positions,
      std::span<const u32> indices
    );

    /// <summary>
    /// Per-vertex tangents (xyz) with handedness in w, accumulated over the triangles that share a
    /// vertex. Without uv0 (empty span) the result is normal_tangents(normals); a vertex whose UVs
    /// give no direction (degenerate or cancelling triangles) gets the same tangent as there.
    /// </summary>
    [[nodiscard]] std::vector<math::vec4> generate_tangents(
      std::span<const math::vec3> positions,
      std::span<const math::vec3> normals,
      std::span<const math::vec2> uv0,
      std::span<const u32> indices
    );

    /// <summary>
    /// One tangent per normal, built from the normal alone: any_perpendicular(normal) with
    /// handedness +1. A valid frame for lighting that does not follow the texture, so a
    /// tangent-space normal map does not shade correctly with it.
    /// </summary>
    [[nodiscard]] std::vector<math::vec4> normal_tangents(std::span<const math::vec3> normals);

    /// <summary>
    /// The tangents of vertices that bring none, built as 'source' says: generate_tangents for
    /// FROM_UVS, normal_tangents for FROM_NORMAL, which reads the normals only.
    /// </summary>
    [[nodiscard]] std::vector<math::vec4> missing_tangents(
      std::span<const math::vec3> positions,
      std::span<const math::vec3> normals,
      std::span<const math::vec2> uv0,
      std::span<const u32> indices,
      tangent_generation source
    );

    /// <summary>
    /// A unit vector perpendicular to 'normal'.
    /// </summary>
    [[nodiscard]] math::vec3 any_perpendicular(const math::vec3 &normal) noexcept;
}
