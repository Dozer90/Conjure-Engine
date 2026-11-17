#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "renderer_test_support.h"

#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/renderer_error.h"

#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_payload.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

// mesh_asset.h (docs/design/02_renderer.md 2.6, 4.2, R8): attribute packing, geometry validation
// and attribute generation, the built-in shapes, the .cmesh blob and the loader.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    [[nodiscard]] mesh_geometry triangle_geometry() {
        mesh_geometry geometry = {};
        geometry.positions = {math::vec3(0.0f, 0.0f, 0.0f), math::vec3(1.0f, 0.0f, 0.0f), math::vec3(0.0f, 1.0f, 0.0f)};
        geometry.indices = {0, 1, 2};
        return geometry;
    }

    /// <summary>
    /// The triangle with U running along +Y and V along +X: its UV tangent is +Y with handedness -1,
    /// while its +Z normal alone gives +X with handedness +1, so the two ways of generating a tangent
    /// can be told apart.
    /// </summary>
    [[nodiscard]] mesh_geometry turned_uv_triangle() {
        mesh_geometry geometry = triangle_geometry();
        geometry.uv0 = {math::vec2(0.0f, 0.0f), math::vec2(0.0f, 1.0f), math::vec2(1.0f, 0.0f)};
        return geometry;
    }

    constexpr math::vec4 TURNED_UV_TANGENT = math::vec4(0.0f, 1.0f, 0.0f, -1.0f);
    constexpr math::vec4 Z_NORMAL_TANGENT = math::vec4(1.0f, 0.0f, 0.0f, 1.0f);

    [[nodiscard]] std::vector<std::byte> corrupt(std::vector<std::byte> blob, usize offset, std::byte value) {
        blob[offset] = value;
        return blob;
    }

    [[nodiscard]] std::vector<std::byte> with_float(std::vector<std::byte> blob, usize offset, f32 value) {
        std::memcpy(blob.data() + offset, &value, sizeof(value));
        return blob;
    }

    // End of the .cmesh header fields: magic, version, vertex, index and submesh counts.
    constexpr usize MESH_BLOB_COUNTS_END = 20;
}

TEST_SUITE("renderer/mesh_asset") {

    TEST_CASE("pack_snorm16 - follows the D3D SNORM rules and round-trips the end points") {
        CHECK(pack_snorm16(1.0f) == 32767);
        CHECK(pack_snorm16(-1.0f) == -32767);
        CHECK(pack_snorm16(0.0f) == 0);
        CHECK(pack_snorm16(2.0f) == 32767);
        CHECK(pack_snorm16(-5.0f) == -32767);
        CHECK(unpack_snorm16(32767) == doctest::Approx(1.0f));
        CHECK(unpack_snorm16(-32767) == doctest::Approx(-1.0f));
        CHECK(unpack_snorm16(-32768) == doctest::Approx(-1.0f));
        CHECK(unpack_snorm16(0) == doctest::Approx(0.0f));
        for (f32 value = -1.0f; value <= 1.0f; value += 0.125f) {
            CHECK(unpack_snorm16(pack_snorm16(value)) == doctest::Approx(value).epsilon(1.0e-4));
        }
    }

    TEST_CASE("pack_half - IEEE half conversion with round to nearest even and saturation") {
        CHECK(pack_half(0.0f) == 0x0000);
        CHECK(pack_half(1.0f) == 0x3C00);
        CHECK(pack_half(-2.0f) == 0xC000);
        CHECK(pack_half(0.5f) == 0x3800);
        CHECK(pack_half(65504.0f) == 0x7BFF);
        CHECK(pack_half(70000.0f) == 0x7C00);   // saturates to +inf
        CHECK(pack_half(-70000.0f) == 0xFC00);
        CHECK(pack_half(std::numeric_limits<f32>::infinity()) == 0x7C00);
        CHECK((pack_half(std::numeric_limits<f32>::quiet_NaN()) & 0x7C00) == 0x7C00);
        CHECK((pack_half(std::numeric_limits<f32>::quiet_NaN()) & 0x03FF) != 0);
        CHECK(pack_half(1.0e-8f) == 0x0000);     // below the smallest subnormal
        CHECK(pack_half(6.0e-8f) == 0x0001);     // the smallest subnormal
        CHECK(unpack_half(0x3C00) == doctest::Approx(1.0f));
        CHECK(unpack_half(0xC000) == doctest::Approx(-2.0f));
        CHECK(unpack_half(0x0001) == doctest::Approx(5.960464e-8f));
        CHECK(std::isinf(unpack_half(0x7C00)));
        CHECK(std::isnan(unpack_half(0x7E00)));
        // 1 + 2^-11 is exactly halfway between two halves: rounds to even (1.0).
        CHECK(pack_half(1.00048828125f) == 0x3C00);
        // 1 + 3 * 2^-11 is halfway too and rounds to the even neighbour above.
        CHECK(pack_half(1.00146484375f) == 0x3C02);
        for (const f32 value : {0.1f, 3.14159f, 1234.5f, -0.001f, 0.333f}) {
            CAPTURE(value);
            CHECK(unpack_half(pack_half(value)) == doctest::Approx(value).epsilon(1.0e-3));
        }
    }

    TEST_CASE("pack_rgba8 - packs clamped components with red in the low byte") {
        CHECK(pack_rgba8(math::vec4(1.0f, 0.0f, 0.0f, 1.0f)) == 0xFF0000FFu);
        CHECK(pack_rgba8(math::vec4(0.0f, 1.0f, 0.0f, 0.0f)) == 0x0000FF00u);
        CHECK(pack_rgba8(math::vec4(3.0f, -1.0f, 0.5f, 1.0f)) == 0xFF8000FFu);
        CONJURE_CHECK_APPROX(unpack_rgba8(0xFF8000FFu), math::vec4(1.0f, 0.0f, 128.0f / 255.0f, 1.0f));
        CHECK(pack_rgba8(unpack_rgba8(0x12345678u)) == 0x12345678u);
    }

    TEST_CASE("pack_vertex_attributes - normalises the vectors, stores the handedness and packs 32 bytes") {
        const packed_vertex_attributes packed = pack_vertex_attributes(
          math::vec3(0.0f, 2.0f, 0.0f),
          math::vec3(3.0f, 0.0f, 0.0f),
          -1.0f,
          math::vec2(0.25f, 0.75f),
          math::vec2(0.5f, 1.0f),
          math::vec4(1.0f, 0.5f, 0.0f, 1.0f)
        );
        CHECK(packed.normal[0] == 0);
        CHECK(packed.normal[1] == 32767);
        CHECK(packed.normal[2] == 0);
        CHECK(packed.normal[3] == -32767);
        CHECK(packed.tangent[0] == 32767);
        CHECK(packed.tangent[1] == 0);
        CHECK(packed.tangent[2] == 0);
        CONJURE_CHECK_APPROX(packed.uv0, math::vec2(0.25f, 0.75f));
        CHECK(unpack_half(packed.uv1[0]) == doctest::Approx(0.5f));
        CHECK(unpack_half(packed.uv1[1]) == doctest::Approx(1.0f));
        CHECK(packed.colour == pack_rgba8(math::vec4(1.0f, 0.5f, 0.0f, 1.0f)));
        // Zero vectors fall back to +Z and +X.
        const packed_vertex_attributes zero = pack_vertex_attributes(
          math::vec3(0.0f),
          math::vec3(0.0f),
          1.0f,
          math::vec2(0.0f),
          math::vec2(0.0f),
          math::vec4(1.0f)
        );
        CHECK(zero.normal[2] == 32767);
        CHECK(zero.normal[3] == 32767);
        CHECK(zero.tangent[0] == 32767);
        static_assert(sizeof(packed_vertex_attributes) == 32, "stream 1 is 32 bytes");
    }

    TEST_CASE("mesh_asset - from_geometry packs the streams, computes bounds and keeps the CPU geometry") {
        mesh_geometry geometry = triangle_geometry();
        geometry.uv0 = {math::vec2(0.0f, 0.0f), math::vec2(1.0f, 0.0f), math::vec2(0.0f, 1.0f)};
        geometry.colours = {
          math::vec4(1.0f, 0.0f, 0.0f, 1.0f), math::vec4(0.0f, 1.0f, 0.0f, 1.0f), math::vec4(0.0f, 0.0f, 1.0f, 1.0f)
        };
        core::result<mesh_asset> created = mesh_asset::from_geometry(geometry, "tri");
        CONJURE_REQUIRE_OK(created);
        const mesh_asset &mesh = created.value();
        CHECK(mesh.name() == "tri");
        CHECK(mesh.vertex_count() == 3);
        CHECK(mesh.index_count() == 3);
        CHECK(mesh.triangle_count() == 1);
        REQUIRE(mesh.submeshes().size() == 1);
        CHECK(mesh.submeshes()[0].firstIndex == 0);
        CHECK(mesh.submeshes()[0].indexCount == 3);
        CHECK(mesh.submeshes()[0].materialSlot == 0);
        CONJURE_CHECK_APPROX(mesh.bounds().min, math::vec3(0.0f));
        CONJURE_CHECK_APPROX(mesh.bounds().max, math::vec3(1.0f, 1.0f, 0.0f));
        CHECK(mesh.submeshes()[0].bounds == mesh.bounds());
        CHECK(mesh.streams().positions.size() == 3);
        CHECK(mesh.streams().attributes.size() == 3);
        CHECK(mesh.streams().indices.size() == 3);
        // The flat normal of a counter-clockwise XY triangle faces +Z; the tangent follows +U (+X).
        const packed_vertex_attributes &attributes = mesh.streams().attributes[0];
        CHECK(unpack_snorm16(attributes.normal[2]) == doctest::Approx(1.0f));
        CHECK(unpack_snorm16(attributes.tangent[0]) == doctest::Approx(1.0f).epsilon(1.0e-3));
        CHECK(attributes.colour == 0xFF0000FFu);
        CHECK(mesh.streams().attributes[1].colour == 0xFF00FF00u);
        CHECK(mesh.cpu_geometry().positions.size() == 3);
        CHECK(mesh.cpu_geometry().indices.size() == 3);
        CHECK(mesh.cpu_geometry().positions.data() == mesh.streams().positions.data());
        CHECK(mesh.gpu_byte_size() == 3 * 12 + 3 * 32 + 3 * 4);
        CHECK(mesh == mesh_asset(mesh));
        CHECK_FALSE(mesh == mesh_asset());
    }

    TEST_CASE("mesh_asset - supplied normals, tangents and uv1 are kept and missing colours are white") {
        mesh_geometry geometry = triangle_geometry();
        geometry.normals.assign(3, math::vec3(0.0f, -1.0f, 0.0f));
        geometry.tangents.assign(3, math::vec4(0.0f, 0.0f, 1.0f, -1.0f));
        geometry.uv1 = {math::vec2(0.5f, 0.5f), math::vec2(0.5f, 0.5f), math::vec2(0.5f, 0.5f)};
        core::result<mesh_asset> created = mesh_asset::from_geometry(geometry);
        CONJURE_REQUIRE_OK(created);
        const packed_vertex_attributes &attributes = created.value().streams().attributes[2];
        CHECK(unpack_snorm16(attributes.normal[1]) == doctest::Approx(-1.0f));
        CHECK(unpack_snorm16(attributes.normal[3]) == doctest::Approx(-1.0f));
        CHECK(unpack_snorm16(attributes.tangent[2]) == doctest::Approx(1.0f));
        CHECK(unpack_half(attributes.uv1[0]) == doctest::Approx(0.5f));
        CHECK(attributes.colour == 0xFFFFFFFFu);
        CONJURE_CHECK_APPROX(attributes.uv0, math::vec2(0.0f));
    }

    TEST_CASE("mesh_asset - tangents a geometry lacks come from the UVs or, when asked, from the normal alone") {
        const mesh_geometry geometry = turned_uv_triangle();

        SUBCASE("FROM_UVS is the default and follows the texture") {
            core::result<mesh_asset> byDefault = mesh_asset::from_geometry(geometry, "tri");
            core::result<mesh_asset> fromUvs = mesh_asset::from_geometry(
              geometry,
              "tri",
              tangent_generation::FROM_UVS
            );
            CONJURE_REQUIRE_OK(byDefault);
            CONJURE_REQUIRE_OK(fromUvs);
            CHECK(byDefault.value() == fromUvs.value());
            const std::span<const packed_vertex_attributes> attributes = fromUvs.value().streams().attributes;
            REQUIRE(attributes.size() == 3);
            for (usize vertex = 0; vertex < attributes.size(); ++vertex) {
                CAPTURE(vertex);
                CONJURE_CHECK_APPROX(renderer_support::unpacked_tangent(attributes[vertex]), TURNED_UV_TANGENT);
            }
        }
        SUBCASE("FROM_NORMAL reads no UV and changes nothing but the tangent") {
            core::result<mesh_asset> fromUvs = mesh_asset::from_geometry(geometry, "tri");
            core::result<mesh_asset> fromNormal = mesh_asset::from_geometry(
              geometry,
              "tri",
              tangent_generation::FROM_NORMAL
            );
            CONJURE_REQUIRE_OK(fromUvs);
            CONJURE_REQUIRE_OK(fromNormal);
            const mesh_streams &streams = fromNormal.value().streams();
            REQUIRE(streams.attributes.size() == 3);
            for (usize vertex = 0; vertex < streams.attributes.size(); ++vertex) {
                CAPTURE(vertex);
                const packed_vertex_attributes &attributes = streams.attributes[vertex];
                const packed_vertex_attributes &textured = fromUvs.value().streams().attributes[vertex];
                CONJURE_CHECK_APPROX(renderer_support::unpacked_tangent(attributes), Z_NORMAL_TANGENT);
                CONJURE_CHECK_APPROX(
                  renderer_support::unpacked_normal(attributes),
                  renderer_support::unpacked_normal(textured)
                );
                CONJURE_CHECK_APPROX(attributes.uv0, geometry.uv0[vertex]);
                CHECK(attributes.uv1 == textured.uv1);
                CHECK(attributes.colour == textured.colour);
            }
            CHECK(streams.positions == fromUvs.value().streams().positions);
            CHECK(streams.indices == fromUvs.value().streams().indices);
            CHECK(fromNormal.value().bounds() == fromUvs.value().bounds());
            CHECK_FALSE(fromNormal.value() == fromUvs.value());
        }
        SUBCASE("a geometry without uv0 gets the FROM_NORMAL tangent under either value") {
            mesh_geometry bare = geometry;
            bare.uv0.clear();
            core::result<mesh_asset> fromUvs = mesh_asset::from_geometry(bare, "tri", tangent_generation::FROM_UVS);
            core::result<mesh_asset> fromNormal = mesh_asset::from_geometry(
              bare,
              "tri",
              tangent_generation::FROM_NORMAL
            );
            CONJURE_REQUIRE_OK(fromUvs);
            CONJURE_REQUIRE_OK(fromNormal);
            CHECK(fromUvs.value() == fromNormal.value());
            CONJURE_CHECK_APPROX(
              renderer_support::unpacked_tangent(fromUvs.value().streams().attributes[0]),
              Z_NORMAL_TANGENT
            );
        }
        SUBCASE("tangents the geometry carries are kept under either value") {
            mesh_geometry authored = geometry;
            authored.normals.assign(3, math::vec3(0.0f, 0.0f, 1.0f));
            authored.tangents.assign(3, math::vec4(0.0f, -1.0f, 0.0f, 1.0f));
            core::result<mesh_asset> fromUvs = mesh_asset::from_geometry(authored, "tri");
            core::result<mesh_asset> fromNormal = mesh_asset::from_geometry(
              authored,
              "tri",
              tangent_generation::FROM_NORMAL
            );
            CONJURE_REQUIRE_OK(fromUvs);
            CONJURE_REQUIRE_OK(fromNormal);
            CHECK(fromUvs.value() == fromNormal.value());
            CONJURE_CHECK_APPROX(
              renderer_support::unpacked_tangent(fromNormal.value().streams().attributes[1]),
              math::vec4(0.0f, -1.0f, 0.0f, 1.0f)
            );
        }
        SUBCASE("every FROM_NORMAL tangent is a unit vector perpendicular to its normal, handedness +1") {
            // Both sides of the 0.99 threshold on |y| where the helper axis changes, a normal that is
            // not unit length and a zero one (packed as +Z).
            const std::array<math::vec3, 9> normals = {
              math::vec3(1.0f, 0.0f, 0.0f),   math::vec3(0.0f, 1.0f, 0.0f),  math::vec3(0.0f, -1.0f, 0.0f),
              math::vec3(0.0f, 0.0f, -1.0f),  math::vec3(0.3f, -0.5f, 0.8f), math::vec3(0.1f, 0.995f, 0.0f),
              math::vec3(0.15f, 0.98f, 0.1f), math::vec3(-4.0f, 2.0f, 4.0f), math::vec3(0.0f),
            };
            mesh_geometry varied = {};
            for (usize index = 0; index < normals.size(); ++index) {
                varied.positions.emplace_back(static_cast<f32>(index), static_cast<f32>(index % 3), 0.0f);
                varied.normals.push_back(normals[index]);
                varied.uv0.emplace_back(static_cast<f32>(index), 0.5f);
                varied.indices.push_back(static_cast<u32>(index));
            }
            core::result<mesh_asset> created = mesh_asset::from_geometry(
              varied,
              "varied",
              tangent_generation::FROM_NORMAL
            );
            CONJURE_REQUIRE_OK(created);
            REQUIRE(created.value().vertex_count() == normals.size());
            for (usize index = 0; index < normals.size(); ++index) {
                CAPTURE(index);
                const packed_vertex_attributes &attributes = created.value().streams().attributes[index];
                const math::vec3 normal = renderer_support::unpacked_normal(attributes);
                const math::vec4 tangent = renderer_support::unpacked_tangent(attributes);
                CHECK(glm::length(math::vec3(tangent)) == doctest::Approx(1.0f).epsilon(1.0e-3));
                CHECK(std::abs(glm::dot(normal, math::vec3(tangent))) < 1.0e-3f);
                CHECK(tangent.w == doctest::Approx(1.0f));
            }
        }
        SUBCASE("FROM_NORMAL uses the flat normals generated for a geometry without normals") {
            // Two faces sharing an edge: the XY triangle faces +Z, the one in the XZ plane faces -Y.
            mesh_geometry folded = {};
            folded.positions = {
              math::vec3(0.0f, 0.0f, 0.0f), math::vec3(1.0f, 0.0f, 0.0f), math::vec3(0.0f, 1.0f, 0.0f),
              math::vec3(0.0f, 0.0f, -1.0f),
            };
            folded.indices = {0, 1, 2, 0, 3, 1};
            core::result<mesh_asset> created = mesh_asset::from_geometry(
              folded,
              "folded",
              tangent_generation::FROM_NORMAL
            );
            CONJURE_REQUIRE_OK(created);
            const mesh_streams &streams = created.value().streams();
            REQUIRE(streams.attributes.size() == 6);   // one vertex per corner
            for (usize vertex = 0; vertex < streams.attributes.size(); ++vertex) {
                CAPTURE(vertex);
                const bool upper = vertex < 3;
                const math::vec3 normal = upper ? math::vec3(0.0f, 0.0f, 1.0f) : math::vec3(0.0f, -1.0f, 0.0f);
                const math::vec4 tangent = upper ? Z_NORMAL_TANGENT : math::vec4(0.0f, 0.0f, -1.0f, 1.0f);
                CONJURE_CHECK_APPROX(renderer_support::unpacked_normal(streams.attributes[vertex]), normal);
                CONJURE_CHECK_APPROX(renderer_support::unpacked_tangent(streams.attributes[vertex]), tangent);
            }
        }
    }

    TEST_CASE("mesh_asset - submesh counts partition the index list and carry their material slots") {
        mesh_geometry geometry = {};
        geometry.positions = {
          math::vec3(0.0f), math::vec3(1.0f, 0.0f, 0.0f), math::vec3(0.0f, 1.0f, 0.0f),
          math::vec3(5.0f, 5.0f, 5.0f), math::vec3(6.0f, 5.0f, 5.0f), math::vec3(5.0f, 6.0f, 5.0f),
        };
        geometry.indices = {0, 1, 2, 3, 4, 5};
        geometry.submeshIndexCounts = {3, 3};
        geometry.submeshMaterialSlots = {2, 0};
        core::result<mesh_asset> created = mesh_asset::from_geometry(geometry);
        CONJURE_REQUIRE_OK(created);
        const std::span<const submesh> submeshes = created.value().submeshes();
        REQUIRE(submeshes.size() == 2);
        CHECK(submeshes[0].firstIndex == 0);
        CHECK(submeshes[0].indexCount == 3);
        CHECK(submeshes[0].materialSlot == 2);
        CHECK(submeshes[1].firstIndex == 3);
        CHECK(submeshes[1].indexCount == 3);
        CHECK(submeshes[1].materialSlot == 0);
        CONJURE_CHECK_APPROX(submeshes[1].bounds.min, math::vec3(5.0f));
        CONJURE_CHECK_APPROX(submeshes[1].bounds.max, math::vec3(6.0f, 6.0f, 5.0f));
        CONJURE_CHECK_APPROX(created.value().bounds().max, math::vec3(6.0f, 6.0f, 5.0f));
        // Without material slots submesh i uses slot i.
        geometry.submeshMaterialSlots.clear();
        core::result<mesh_asset> defaults = mesh_asset::from_geometry(geometry);
        CONJURE_REQUIRE_OK(defaults);
        CHECK(defaults.value().submeshes()[1].materialSlot == 1);
    }

    TEST_CASE("mesh_asset - from_geometry rejects every malformed input with INVALID_ARGUMENT") {
        mesh_geometry empty = {};
        CONJURE_CHECK_ERROR(mesh_asset::from_geometry(empty), core::error_code::INVALID_ARGUMENT);

        mesh_geometry badCount = triangle_geometry();
        badCount.normals.assign(2, math::vec3(0.0f, 0.0f, 1.0f));
        CONJURE_CHECK_ERROR(mesh_asset::from_geometry(badCount), core::error_code::INVALID_ARGUMENT);
        badCount = triangle_geometry();
        badCount.uv0.assign(4, math::vec2(0.0f));
        CONJURE_CHECK_ERROR(mesh_asset::from_geometry(badCount), core::error_code::INVALID_ARGUMENT);
        badCount = triangle_geometry();
        badCount.colours.assign(1, math::vec4(1.0f));
        CONJURE_CHECK_ERROR(mesh_asset::from_geometry(badCount), core::error_code::INVALID_ARGUMENT);

        mesh_geometry notTriangles = triangle_geometry();
        notTriangles.indices = {0, 1};
        CONJURE_CHECK_ERROR(mesh_asset::from_geometry(notTriangles), core::error_code::INVALID_ARGUMENT);
        notTriangles.indices.clear();
        CONJURE_CHECK_ERROR(mesh_asset::from_geometry(notTriangles), core::error_code::INVALID_ARGUMENT);

        mesh_geometry outOfRange = triangle_geometry();
        outOfRange.indices = {0, 1, 3};
        CONJURE_CHECK_ERROR(mesh_asset::from_geometry(outOfRange), core::error_code::INVALID_ARGUMENT);

        mesh_geometry badPartition = triangle_geometry();
        badPartition.submeshIndexCounts = {3, 3};
        CONJURE_CHECK_ERROR(mesh_asset::from_geometry(badPartition), core::error_code::INVALID_ARGUMENT);
        badPartition.submeshIndexCounts = {2, 1};
        CONJURE_CHECK_ERROR(mesh_asset::from_geometry(badPartition), core::error_code::INVALID_ARGUMENT);

        mesh_geometry badSlots = triangle_geometry();
        badSlots.submeshIndexCounts = {3};
        badSlots.submeshMaterialSlots = {1, 2};
        CONJURE_CHECK_ERROR(mesh_asset::from_geometry(badSlots), core::error_code::INVALID_ARGUMENT);
        badSlots.submeshMaterialSlots = {MAX_MATERIAL_SLOTS};
        CONJURE_CHECK_ERROR(mesh_asset::from_geometry(badSlots), core::error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("mesh_asset - the built-in shapes have the expected topology and bounds") {
        const mesh_asset cube = mesh_asset::unit_cube();
        CHECK(cube.vertex_count() == 24);
        CHECK(cube.index_count() == 36);
        CHECK(cube.triangle_count() == 12);
        CONJURE_CHECK_APPROX(cube.bounds().min, math::vec3(-0.5f));
        CONJURE_CHECK_APPROX(cube.bounds().max, math::vec3(0.5f));
        CHECK(cube.submeshes().size() == 1);
        // Every face normal is axis aligned and points away from the centre.
        for (usize vertex = 0; vertex < cube.vertex_count(); ++vertex) {
            const packed_vertex_attributes &attributes = cube.streams().attributes[vertex];
            const math::vec3 normal(
              unpack_snorm16(attributes.normal[0]),
              unpack_snorm16(attributes.normal[1]),
              unpack_snorm16(attributes.normal[2])
            );
            CHECK(glm::length(normal) == doctest::Approx(1.0f).epsilon(1.0e-3));
            CHECK(glm::dot(normal, cube.streams().positions[vertex]) > 0.0f);
        }

        const mesh_asset sphere = mesh_asset::uv_sphere(2.0f, 8, 16);
        CHECK(sphere.vertex_count() > 0);
        // Counter-clockwise from outside: every non-degenerate triangle's winding normal points away from the centre.
        for (const mesh_asset *shape : {&cube, &sphere}) {
            const std::span<const math::vec3> positions = shape->streams().positions;
            const std::span<const u32> indices = shape->streams().indices;
            for (usize triangle = 0; triangle + 2 < indices.size(); triangle += 3) {
                const math::vec3 &a = positions[indices[triangle]];
                const math::vec3 &b = positions[indices[triangle + 1]];
                const math::vec3 &c = positions[indices[triangle + 2]];
                const math::vec3 winding = glm::cross(b - a, c - a);
                if (glm::length(winding) > 1.0e-6f) {
                    CHECK(glm::dot(winding, (a + b + c) / 3.0f) > 0.0f);
                }
            }
        }
        CHECK(sphere.index_count() % 3 == 0);
        CONJURE_CHECK_APPROX(sphere.bounds().min, math::vec3(-2.0f));
        CONJURE_CHECK_APPROX(sphere.bounds().max, math::vec3(2.0f));
        for (const math::vec3 &position : sphere.streams().positions) {
            CHECK(glm::length(position) == doctest::Approx(2.0f).epsilon(1.0e-3));
        }
        const mesh_asset coarse = mesh_asset::uv_sphere(1.0f, 1, 1);
        CHECK(coarse.index_count() > 0);

        const mesh_asset quad = mesh_asset::quad(2.0f);
        CHECK(quad.vertex_count() == 4);
        CHECK(quad.index_count() == 6);
        CONJURE_CHECK_APPROX(quad.bounds().min, math::vec3(-2.0f, -2.0f, 0.0f));
        CONJURE_CHECK_APPROX(quad.bounds().max, math::vec3(2.0f, 2.0f, 0.0f));
        CHECK(unpack_snorm16(quad.streams().attributes[0].normal[2]) == doctest::Approx(1.0f));
    }

    TEST_CASE("mesh_asset - write_blob and parse_blob round-trip deterministically") {
        const mesh_asset cube = mesh_asset::unit_cube();
        const std::vector<std::byte> blob = cube.write_blob();
        CHECK_FALSE(blob.empty());
        CHECK(blob == cube.write_blob());
        core::result<mesh_asset> parsed = mesh_asset::parse_blob(blob);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value() == cube);
        CHECK(parsed.value().name() == cube.name());
        CHECK(parsed.value().submeshes().size() == 1);
        CHECK(parsed.value().bounds() == cube.bounds());

        mesh_geometry geometry = triangle_geometry();
        geometry.submeshIndexCounts = {3};
        geometry.submeshMaterialSlots = {4};
        core::result<mesh_asset> created = mesh_asset::from_geometry(geometry, "named");
        CONJURE_REQUIRE_OK(created);
        core::result<mesh_asset> again = mesh_asset::parse_blob(created.value().write_blob());
        CONJURE_REQUIRE_OK(again);
        CHECK(again.value() == created.value());
        CHECK(again.value().name() == "named");
        CHECK(again.value().submeshes()[0].materialSlot == 4);
    }

    TEST_CASE("mesh_asset - corrupt blobs fail with INVALID_ASSET instead of reading out of bounds") {
        const mesh_asset original = mesh_asset::unit_cube();
        const std::vector<std::byte> blob = original.write_blob();
        CONJURE_CHECK_ERROR(mesh_asset::parse_blob({}), renderer_error::INVALID_ASSET);
        CONJURE_CHECK_ERROR(
          mesh_asset::parse_blob(std::span<const std::byte>(blob).subspan(0, 3)),
          renderer_error::INVALID_ASSET
        );
        CONJURE_CHECK_ERROR(mesh_asset::parse_blob(corrupt(blob, 0, std::byte{'X'})), renderer_error::INVALID_ASSET);
        // version
        CONJURE_CHECK_ERROR(mesh_asset::parse_blob(corrupt(blob, 4, std::byte{0x7F})), renderer_error::INVALID_ASSET);
        for (const usize length : {usize{8}, usize{16}, usize{32}, blob.size() / 2, blob.size() - 1}) {
            CAPTURE(length);
            CONJURE_CHECK_ERROR(
              mesh_asset::parse_blob(std::span<const std::byte>(blob).subspan(0, length)),
              renderer_error::INVALID_ASSET
            );
        }
        std::vector<std::byte> trailing = blob;
        trailing.push_back(std::byte{0});
        CONJURE_CHECK_ERROR(mesh_asset::parse_blob(trailing), renderer_error::INVALID_ASSET);
        // A corrupted header field (version and the three counts) must fail; corrupted name or position
        // bytes may parse but never change the sizes.
        for (usize offset = 4; offset < 64 && offset < blob.size(); ++offset) {
            CAPTURE(offset);
            const core::result<mesh_asset> parsed = mesh_asset::parse_blob(corrupt(blob, offset, std::byte{0xFF}));
            if (offset < MESH_BLOB_COUNTS_END) {
                CHECK_FALSE(parsed.has_value());
            }
            if (!parsed.has_value()) {
                CHECK(parsed.get_error().is(renderer_error::INVALID_ASSET));
                continue;
            }
            CHECK(parsed.value().vertex_count() == original.vertex_count());
            CHECK(parsed.value().index_count() == original.index_count());
            CHECK(parsed.value().submeshes().size() == original.submeshes().size());
        }
    }

    TEST_CASE("mesh_asset - parse_blob rejects non-finite positions and non-finite or inverted bounds") {
        const mesh_asset quad = mesh_asset::quad(1.0f);
        const std::vector<std::byte> blob = quad.write_blob();
        CONJURE_REQUIRE_OK(mesh_asset::parse_blob(blob));
        // Layout: magic, version, three counts, the name, positions, attributes, indices, then one
        // submesh {firstIndex, indexCount, materialSlot, bounds} and the mesh bounds.
        const usize vertexBytes = sizeof(math::vec3) + sizeof(packed_vertex_attributes);
        const usize positions = MESH_BLOB_COUNTS_END + 4 + quad.name().size();
        const usize submesh = positions + quad.vertex_count() * vertexBytes + quad.index_count() * sizeof(u32);
        const usize submeshBoundsMin = submesh + 3 * sizeof(u32);
        const usize meshBoundsMax = submeshBoundsMin + 2 * sizeof(math::vec3) + sizeof(math::vec3);
        REQUIRE(meshBoundsMax + sizeof(math::vec3) == blob.size());
        const f32 notANumber = std::numeric_limits<f32>::quiet_NaN();
        const f32 infinity = std::numeric_limits<f32>::infinity();

        SUBCASE("a NaN position") {
            const std::vector<std::byte> patched = with_float(blob, positions, notANumber);
            CONJURE_CHECK_ERROR(mesh_asset::parse_blob(patched), renderer_error::INVALID_ASSET);
        }
        SUBCASE("submesh bounds with min.x above max.x") {
            const f32 maxX = quad.submeshes()[0].bounds.max.x;
            const std::vector<std::byte> patched = with_float(blob, submeshBoundsMin, maxX + 1.0f);
            CONJURE_CHECK_ERROR(mesh_asset::parse_blob(patched), renderer_error::INVALID_ASSET);
        }
        SUBCASE("an infinite mesh bound") {
            const std::vector<std::byte> patched = with_float(blob, meshBoundsMax, infinity);
            CONJURE_CHECK_ERROR(mesh_asset::parse_blob(patched), renderer_error::INVALID_ASSET);
        }
        SUBCASE("from_geometry with a NaN or infinite position") {
            mesh_geometry geometry = triangle_geometry();
            geometry.positions[1].y = notANumber;
            CONJURE_CHECK_ERROR(mesh_asset::from_geometry(geometry), core::error_code::INVALID_ARGUMENT);
            geometry.positions[1].y = infinity;
            CONJURE_CHECK_ERROR(mesh_asset::from_geometry(geometry), core::error_code::INVALID_ARGUMENT);
        }
    }

    TEST_CASE("mesh_loader - loads a blob, finalises trivially and falls back to the unit cube") {
        const mesh_loader loader = {};
        CHECK(loader.type() == assets::asset_type_traits<mesh_asset>::id());
        const mesh_asset sphere = mesh_asset::uv_sphere(1.0f, 4, 8);
        const std::vector<std::byte> blob = sphere.write_blob();
        assets::load_context context = {};
        context.id = renderer_support::test_asset_id(1);
        context.type = loader.type();
        core::result<assets::asset_payload> loaded = loader.load_cpu(context, blob);
        CONJURE_REQUIRE_OK(loaded);
        REQUIRE(assets::payload_as<mesh_asset>(loaded.value()) != nullptr);
        CHECK(*assets::payload_as<mesh_asset>(loaded.value()) == sphere);
        assets::finalise_context finaliseContext = {};
        finaliseContext.id = context.id;
        core::result<assets::asset_payload> finalised = loader.finalise(finaliseContext, std::move(loaded).value());
        CONJURE_REQUIRE_OK(finalised);
        REQUIRE(assets::payload_as<mesh_asset>(finalised.value()) != nullptr);
        CHECK(*assets::payload_as<mesh_asset>(finalised.value()) == sphere);

        CONJURE_CHECK_ERROR(
          loader.load_cpu(context, std::span<const std::byte>(blob).subspan(0, 5)),
          renderer_error::INVALID_ASSET
        );

        core::result<assets::asset_payload> fallback = loader.create_fallback();
        CONJURE_REQUIRE_OK(fallback);
        REQUIRE(assets::payload_as<mesh_asset>(fallback.value()) != nullptr);
        CHECK(*assets::payload_as<mesh_asset>(fallback.value()) == mesh_asset::unit_cube());
        CHECK(MESH_ASSET_TYPE_NAME == "conjure.renderer.mesh");
        CHECK(assets::asset_type_traits<mesh_asset>::NAME == MESH_ASSET_TYPE_NAME);
    }
}
