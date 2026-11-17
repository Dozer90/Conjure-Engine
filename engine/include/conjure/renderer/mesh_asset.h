#pragma once

#include "conjure/renderer/limits.h"

#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/api.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// The CPU mesh asset (docs/design/02_renderer.md 2.6, 4.2, R8). Geometry is stored in the two GPU
// streams: stream 0 holds positions (RGB32_FLOAT), stream 1 the packed attributes (32 bytes: normal
// and tangent as RGBA16_SNORM with the tangent sign in the normal's w, uv0 as RG32_FLOAT, uv1 as
// RG16_FLOAT, colour as RGBA8_UNORM). Indices are R32_UINT. Shadow and depth passes bind stream 0
// only. Positions and indices stay resident for the editor's CPU picking (cpu_geometry, R11).

namespace conjure::renderer {

    inline constexpr std::string_view MESH_ASSET_TYPE_NAME = "conjure.renderer.mesh";
    inline constexpr u32 MESH_BLOB_VERSION = 1;

    /*-----------------------------------------------------------------------*/
    // Attribute packing

    /// <summary>
    /// Packs a value in [-1, 1] into a 16-bit signed normalised integer (D3D/Vulkan SNORM rules).
    /// </summary>
    [[nodiscard]] CONJURE_API i16 pack_snorm16(f32 value) noexcept;
    [[nodiscard]] CONJURE_API f32 unpack_snorm16(i16 value) noexcept;

    /// <summary>
    /// IEEE 754 half precision conversion (round to nearest even; out-of-range values saturate to
    /// infinity, NaN stays NaN).
    /// </summary>
    [[nodiscard]] CONJURE_API u16 pack_half(f32 value) noexcept;
    [[nodiscard]] CONJURE_API f32 unpack_half(u16 value) noexcept;

    /// <summary>
    /// Packs a colour with components in [0, 1] into RGBA8 (red in the low byte).
    /// </summary>
    [[nodiscard]] CONJURE_API u32 pack_rgba8(const math::vec4 &colour) noexcept;
    [[nodiscard]] CONJURE_API math::vec4 unpack_rgba8(u32 packed) noexcept;

    /// <summary>
    /// Stream 1 of a vertex (32 bytes).
    /// </summary>
    struct packed_vertex_attributes {
        std::array<i16, 4> normal = {0, 0, 0, 0};       // RGBA16_SNORM; w = tangent sign (+1 / -1)
        std::array<i16, 4> tangent = {0, 0, 0, 0};      // RGBA16_SNORM; w unused
        math::vec2 uv0 = math::vec2(0.0f);              // RG32_FLOAT
        std::array<u16, 2> uv1 = {0, 0};                // RG16_FLOAT
        u32 colour = 0xFFFFFFFFu;                       // RGBA8_UNORM

        [[nodiscard]] bool operator==(const packed_vertex_attributes &other) const noexcept = default;
    };

    static_assert(sizeof(packed_vertex_attributes) == 32, "stream 1 is 32 bytes per vertex");
    static_assert(std::is_trivially_copyable_v<packed_vertex_attributes>, "stream 1 is uploaded verbatim");

    /// <summary>
    /// Packs one vertex's attributes. The normal and tangent are normalised first (a zero vector
    /// packs to +Z / +X); 'tangentSign' is the handedness stored in the normal's w.
    /// </summary>
    [[nodiscard]] CONJURE_API packed_vertex_attributes pack_vertex_attributes(
      const math::vec3 &normal,
      const math::vec3 &tangent,
      f32 tangentSign,
      const math::vec2 &uv0,
      const math::vec2 &uv1,
      const math::vec4 &colour
    ) noexcept;

    /*-----------------------------------------------------------------------*/
    // Geometry

    /// <summary>
    /// How mesh_asset::from_geometry builds the tangents of a geometry that carries none
    /// (docs/design/02_renderer.md R20). FROM_UVS takes them from the uv0 derivatives of the
    /// triangles around each vertex, made perpendicular to the normal, with the handedness of the
    /// UV winding: the frame a tangent-space normal map needs. A geometry without uv0, and a vertex
    /// whose UVs give no direction (degenerate or cancelling triangles), gets the FROM_NORMAL
    /// tangent. FROM_NORMAL reads no UV: the tangent is a unit vector perpendicular to the normal,
    /// handedness +1. Lighting works with it; a tangent-space normal map does not shade correctly,
    /// because the tangent does not follow the texture.
    /// </summary>
    enum class tangent_generation : u8 {
        FROM_UVS = 0,
        FROM_NORMAL
    };

    /// <summary>
    /// Unpacked geometry as importers produce it. Optional attributes are either empty or one per
    /// position; missing normals are computed flat per face, missing tangents as the
    /// tangent_generation given to from_geometry says (from the UV derivatives by default),
    /// missing uv1 is zero and missing colours are white.
    /// </summary>
    struct mesh_geometry {
        std::vector<math::vec3> positions = {};
        std::vector<math::vec3> normals = {};
        std::vector<math::vec4> tangents = {};          // xyz + handedness in w
        std::vector<math::vec2> uv0 = {};
        std::vector<math::vec2> uv1 = {};
        std::vector<math::vec4> colours = {};
        std::vector<u32> indices = {};                  // triangle list
        std::vector<u32> submeshIndexCounts = {};       // partition of 'indices'; empty = one submesh
        std::vector<u32> submeshMaterialSlots = {};     // one per submesh; empty = slot i for submesh i
    };

    struct mesh_streams {
        std::vector<math::vec3> positions = {};
        std::vector<packed_vertex_attributes> attributes = {};
        std::vector<u32> indices = {};
    };

    struct submesh {
        u32 firstIndex = 0;
        u32 indexCount = 0;
        u32 materialSlot = 0;
        math::aabb bounds = {};

        [[nodiscard]] bool operator==(const submesh &other) const noexcept = default;
    };

    /// <summary>
    /// Positions and indices for CPU picking.
    /// </summary>
    struct cpu_geometry_view {
        std::span<const math::vec3> positions = {};
        std::span<const u32> indices = {};
    };

    class CONJURE_API mesh_asset {
        private:
            mesh_streams mStreams = {};
            std::vector<submesh> mSubmeshes = {};
            math::aabb mBounds = {};
            std::string mName = {};

        public:
            mesh_asset();
            ~mesh_asset();

            mesh_asset(const mesh_asset &) = default;
            mesh_asset &operator=(const mesh_asset &) = default;
            mesh_asset(mesh_asset &&) noexcept = default;
            mesh_asset &operator=(mesh_asset &&) noexcept = default;

        public:
            /// <summary>
            /// Packs geometry into streams. Fails with INVALID_ARGUMENT for no positions, an
            /// attribute array whose size differs from the position count, an index count that is
            /// not a multiple of three, an index out of range, a non-finite position, or submesh
            /// counts that do not partition the index list. Missing attributes are generated as
            /// described above; 'missingTangents' chooses how and is not read when the geometry
            /// has tangents.
            /// </summary>
            [[nodiscard]] static core::result<mesh_asset> from_geometry(
              const mesh_geometry &geometry,
              std::string name = {},
              tangent_generation missingTangents = tangent_generation::FROM_UVS
            );

            /// <summary>
            /// A unit cube centred on the origin with flat normals: the fallback mesh.
            /// </summary>
            [[nodiscard]] static mesh_asset unit_cube();

            /// <summary>
            /// A UV sphere of the given radius.
            /// </summary>
            [[nodiscard]] static mesh_asset uv_sphere(f32 radius, u32 rings = 16, u32 segments = 32);

            /// <summary>
            /// A quad in the XY plane facing +Z with the given half size.
            /// </summary>
            [[nodiscard]] static mesh_asset quad(f32 halfSize = 0.5f);

            /// <summary>
            /// Parses a .cmesh blob. Fails with renderer_error::INVALID_ASSET naming the problem.
            /// </summary>
            [[nodiscard]] static core::result<mesh_asset> parse_blob(std::span<const std::byte> bytes);

            /// <summary>
            /// Serialises the mesh as a .cmesh blob (little-endian, deterministic).
            /// </summary>
            [[nodiscard]] std::vector<std::byte> write_blob() const;

            [[nodiscard]] const mesh_streams &streams() const noexcept {
                return mStreams;
            }

            [[nodiscard]] std::span<const submesh> submeshes() const noexcept {
                return mSubmeshes;
            }

            [[nodiscard]] const math::aabb &bounds() const noexcept {
                return mBounds;
            }

            [[nodiscard]] std::string_view name() const noexcept {
                return mName;
            }

            [[nodiscard]] u32 vertex_count() const noexcept {
                return static_cast<u32>(mStreams.positions.size());
            }

            [[nodiscard]] u32 index_count() const noexcept {
                return static_cast<u32>(mStreams.indices.size());
            }

            [[nodiscard]] u32 triangle_count() const noexcept {
                return index_count() / 3;
            }

            /// <summary>
            /// Positions and indices for the editor's CPU picking (R11).
            /// </summary>
            [[nodiscard]] cpu_geometry_view cpu_geometry() const noexcept;

            /// <summary>
            /// Bytes of stream 0, stream 1 and the index buffer together.
            /// </summary>
            [[nodiscard]] u64 gpu_byte_size() const noexcept;

            [[nodiscard]] bool operator==(const mesh_asset &other) const noexcept;
    };

}

CONJURE_DECLARE_ASSET_TYPE(conjure::renderer::mesh_asset, "conjure.renderer.mesh");

namespace conjure::renderer {

    /// <summary>
    /// The conjure.renderer.mesh loader: parses the blob on the load job, finalise is trivial, the
    /// fallback is the unit cube.
    /// </summary>
    class CONJURE_API mesh_loader final : public assets::typed_asset_loader<mesh_asset, mesh_asset> {
        public:
            mesh_loader();
            ~mesh_loader() override;

        protected:
            [[nodiscard]] core::result<mesh_asset> load_cpu_typed(
              const assets::load_context &context,
              std::span<const std::byte> payloadBytes
            ) const override;

            [[nodiscard]] core::result<mesh_asset> finalise_typed(
              const assets::finalise_context &context,
              mesh_asset &&intermediate
            ) const override;

            [[nodiscard]] std::optional<mesh_asset> create_fallback_typed() const override;
    };
}
