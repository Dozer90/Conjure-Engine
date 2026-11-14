#pragma once

#include "conjure/assets/asset_types.h"
#include "conjure/core/api.h"
#include "conjure/core/math/aabb.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

// The conjure.physics.collision_mesh asset (docs/design/14_physics.md 2.7, P14, R44): positions and
// triangle indices cooked from .obj, .gltf and .glb sources by collision_mesh_importer, loaded by a CPU-only
// loader that builds the Jolt mesh shape on the load job. The importer claims .obj by extension; a .gltf or
// .glb is a render model by extension and becomes collision geometry when its .meta names
// COLLISION_MESH_IMPORTER_ID (the editor's asset.import / asset.reimport take an "importer"). Cooked payload
// (little-endian, sections aligned to 16 bytes): { u32 vertexCount; u32 indexCount; u32 reserved[2];
// f32 position[vertexCount][3]; (padding) u32 index[indexCount]; }.

namespace conjure::physics {

    namespace detail {
        /// <summary>
        /// The Jolt shapes built for a loaded mesh. Opaque here: Jolt never reaches a public header.
        /// </summary>
        struct collision_mesh_shapes;
    }

    /// <summary>
    /// Registered asset type name.
    /// </summary>
    inline constexpr std::string_view COLLISION_MESH_ASSET_TYPE = "conjure.physics.collision_mesh";

    /// <summary>
    /// Importer id recorded in .meta files.
    /// </summary>
    inline constexpr std::string_view COLLISION_MESH_IMPORTER_ID = "conjure.physics.collision_mesh_importer";

    /// <summary>
    /// Byte alignment of every section of the cooked payload.
    /// </summary>
    inline constexpr usize COLLISION_MESH_SECTION_ALIGNMENT = 16;

    /// <summary>
    /// Triangle geometry for mesh and convex colliders. Values are validated on construction: every
    /// index is in range, the index count is a multiple of three and degenerate triangles are dropped.
    /// </summary>
    class CONJURE_API collision_mesh_asset {
        private:
            std::vector<math::vec3> mPositions = {};
            std::vector<u32> mIndices = {};
            math::aabb mBounds = {};
            std::shared_ptr<const detail::collision_mesh_shapes> mShapes = nullptr;   // set by the loader

        public:
            collision_mesh_asset();
            ~collision_mesh_asset();

            collision_mesh_asset(const collision_mesh_asset &) = default;
            collision_mesh_asset &operator=(const collision_mesh_asset &) = default;
            collision_mesh_asset(collision_mesh_asset &&) noexcept = default;
            collision_mesh_asset &operator=(collision_mesh_asset &&) noexcept = default;

        public:
            /// <summary>
            /// Builds a mesh from positions and triangle indices. GEOMETRY_INVALID when the index count
            /// is not a multiple of three, an index is out of range, a position is not finite or no
            /// non-degenerate triangle remains. Degenerate triangles (repeated vertex or zero area) are
            /// dropped; unreferenced vertices are kept.
            /// </summary>
            [[nodiscard]] static core::result<collision_mesh_asset> from_geometry(
              std::span<const math::vec3> positions,
              std::span<const u32> indices
            );

            /// <summary>
            /// Parses a cooked payload. GEOMETRY_INVALID for a short or inconsistent buffer.
            /// </summary>
            [[nodiscard]] static core::result<collision_mesh_asset> from_payload(std::span<const std::byte> payload);

            /// <summary>
            /// The cooked payload of this mesh (deterministic bytes).
            /// </summary>
            [[nodiscard]] std::vector<std::byte> to_payload() const;

            [[nodiscard]] std::span<const math::vec3> positions() const noexcept {
                return mPositions;
            }

            [[nodiscard]] std::span<const u32> indices() const noexcept {
                return mIndices;
            }

            /// <summary>
            /// Bounds of every position (invalid for an empty mesh).
            /// </summary>
            [[nodiscard]] const math::aabb &bounds() const noexcept {
                return mBounds;
            }

            [[nodiscard]] u32 vertex_count() const noexcept {
                return static_cast<u32>(mPositions.size());
            }

            [[nodiscard]] u32 triangle_count() const noexcept {
                return static_cast<u32>(mIndices.size() / 3);
            }

            /// <summary>
            /// True once the loader built the Jolt shapes (false for a mesh made by from_geometry).
            /// </summary>
            [[nodiscard]] bool has_shapes() const noexcept {
                return mShapes != nullptr;
            }

            /// <summary>
            /// The Jolt shapes, or nullptr. For the physics module's own use.
            /// </summary>
            [[nodiscard]] const detail::collision_mesh_shapes *shapes() const noexcept {
                return mShapes.get();
            }

            /// <summary>
            /// Installs the Jolt shapes. For the physics module's own use.
            /// </summary>
            void set_shapes(std::shared_ptr<const detail::collision_mesh_shapes> shapes) noexcept;
    };
}

CONJURE_DECLARE_ASSET_TYPE(conjure::physics::collision_mesh_asset, "conjure.physics.collision_mesh");
