#pragma once

#include "conjure/physics/collision_mesh_asset.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

// The .gltf / .glb / .obj importer of conjure.physics.collision_mesh (docs/design/14_physics.md 2.7):
// node transforms baked, primitives merged, vertices welded within weldDistance (meta setting
// "weld_distance", default 1e-4 m), everything deterministic. External glTF buffers are read as side
// files of the source.

namespace conjure::physics::detail {

    /// <summary>
    /// Positions and triangle indices gathered from a source before welding.
    /// </summary>
    struct raw_geometry {
        std::vector<math::vec3> positions = {};
        std::vector<u32> indices = {};
    };

    /// <summary>
    /// Parses Wavefront OBJ text ("v" and "f" lines, polygons triangulated as fans, negative indices
    /// relative to the end). COLLISION_MESH_PARSE_FAILED names the line.
    /// </summary>
    [[nodiscard]] core::result<raw_geometry> parse_obj(std::string_view text);

    /// <summary>
    /// Merges vertices closer than weldDistance (first occurrence wins) and rewrites the indices.
    /// Deterministic: a sequential pass over a grid of cells.
    /// </summary>
    [[nodiscard]] raw_geometry weld_vertices(const raw_geometry &source, f32 weldDistance);

    class collision_mesh_importer final : public assets::asset_importer {
        public:
            // 2: glTF sources are validated (accessors, buffer lengths, per-primitive triangle counts),
            // mirrored nodes keep their winding, and welding finds every vertex of a shared grid cell.
            static constexpr u32 VERSION = 2;
            static constexpr f32 DEFAULT_WELD_DISTANCE = 1.0e-4f;

        public:
            collision_mesh_importer();
            ~collision_mesh_importer() override;

        public:
            [[nodiscard]] std::string_view id() const noexcept override;
            [[nodiscard]] u32 version() const noexcept override;
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override;
            [[nodiscard]] core::json::document default_settings() const override;
            [[nodiscard]] core::result<assets::import_output> import(assets::import_context &context) const override;
    };
}
