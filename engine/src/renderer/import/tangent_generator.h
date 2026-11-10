#pragma once

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <span>
#include <vector>

// Attribute generation for meshes without normals or tangents (docs/design/02_renderer.md 4.2).
// Normals are flat per face (the glTF rule for missing normals), which needs one vertex per
// triangle corner; tangents come from the UV derivatives with Gram-Schmidt orthogonalisation and
// a handedness sign (Lengyel).

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
    /// vertex. Without uv0 (empty span) every tangent is an arbitrary vector perpendicular to the
    /// normal with handedness +1.
    /// </summary>
    [[nodiscard]] std::vector<math::vec4> generate_tangents(
      std::span<const math::vec3> positions,
      std::span<const math::vec3> normals,
      std::span<const math::vec2> uv0,
      std::span<const u32> indices
    );

    /// <summary>
    /// A unit vector perpendicular to 'normal'.
    /// </summary>
    [[nodiscard]] math::vec3 any_perpendicular(const math::vec3 &normal) noexcept;
}
