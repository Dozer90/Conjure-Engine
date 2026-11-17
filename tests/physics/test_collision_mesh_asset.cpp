#include "conjure/testing/assert_capture.h"
#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "physics_test_fixture.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/assets_error.h"
#include "conjure/assets/cooked_blob.h"
#include "conjure/core/json/document.h"
#include "conjure/core/vfs/memory_file_system.h"
#include "conjure/physics/collision_mesh_asset.h"
#include "conjure/physics/components.h"
#include "conjure/physics/physics_error.h"
#include "conjure/physics/register.h"

#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace conjure;
using namespace physics_test;

namespace {

    [[nodiscard]] std::vector<std::byte> bytes_of(std::string_view text) {
        const std::byte *begin = reinterpret_cast<const std::byte *>(text.data());
        return std::vector<std::byte>(begin, begin + text.size());
    }

    void append_u32(std::vector<std::byte> &out, u32 value) {
        for (u32 shift = 0; shift < 32; shift += 8) {
            out.push_back(static_cast<std::byte>((value >> shift) & 0xFFu));
        }
    }

    void append_f32(std::vector<std::byte> &out, f32 value) {
        u32 bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        append_u32(out, bits);
    }

    /// <summary>
    /// A ramp: a 4 x 2 quad (two triangles) in the x-z plane. Positions are authored flat and the glTF
    /// node rotates them 30 degrees about z and lifts them, so importing bakes a node transform.
    /// </summary>
    struct ramp_source {
        std::vector<std::byte> glb = {};
        u32 vertexCount = 4;
        u32 indexCount = 6;
    };

    /// <summary>
    /// A run of the ramp's six stored indices that one triangle primitive reads.
    /// </summary>
    struct index_range {
        u32 first = 0;
        u32 count = 6;
    };

    /// <summary>
    /// What a test changes in the ramp document: the transform members of its node (empty for the
    /// default 30 degree rotation about z and 1 m lift), the POSITION count it declares (four are stored)
    /// and one triangle primitive per index range, all sharing the POSITION accessor.
    /// </summary>
    struct ramp_layout {
        std::string nodeTransform = {};
        u32 positionCount = 4;
        std::vector<index_range> primitives = {index_range{}};
    };

    /// <summary>
    /// Builds a minimal binary glTF (one mesh, one node with a rotation and translation) in memory.
    /// </summary>
    [[nodiscard]] ramp_source make_ramp_glb(const ramp_layout &layout = {}) {
        const std::array<math::vec3, 4> positions = {
            math::vec3(-2.0f, 0.0f, -1.0f),
            math::vec3(2.0f, 0.0f, -1.0f),
            math::vec3(2.0f, 0.0f, 1.0f),
            math::vec3(-2.0f, 0.0f, 1.0f),
        };
        const std::array<u16, 6> indices = {0, 2, 1, 0, 3, 2};

        std::vector<std::byte> bin = {};
        for (const math::vec3 &position : positions) {
            append_f32(bin, position.x);
            append_f32(bin, position.y);
            append_f32(bin, position.z);
        }
        const u32 indexOffset = static_cast<u32>(bin.size());
        for (const u16 index : indices) {
            bin.push_back(static_cast<std::byte>(index & 0xFFu));
            bin.push_back(static_cast<std::byte>(index >> 8));
        }
        while (bin.size() % 4 != 0) {
            bin.push_back(std::byte{0});
        }

        std::string nodeTransform = layout.nodeTransform;
        if (nodeTransform.empty()) {
            // 30 degrees about z: quaternion (x, y, z, w) = (0, 0, sin 15, cos 15), plus a lift of 1 m.
            const f32 half = math::radians(15.0f);
            const std::string rotation = std::to_string(std::sin(half)) + "," + std::to_string(std::cos(half));
            nodeTransform = R"("rotation":[0,0,)" + rotation + R"(],"translation":[0,1,0])";
        }
        std::string primitives = {};
        std::string indexAccessors = {};
        for (usize index = 0; index < layout.primitives.size(); ++index) {
            const index_range &range = layout.primitives[index];
            if (index > 0) {
                primitives += ",";
            }
            primitives += R"({"attributes":{"POSITION":0},"indices":)" + std::to_string(index + 1) + "}";
            indexAccessors += R"(,{"bufferView":1,"byteOffset":)" + std::to_string(range.first * 2)
              + R"(,"componentType":5123,"count":)" + std::to_string(range.count) + R"(,"type":"SCALAR"})";
        }
        const std::string json = std::string(R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)")
          + R"("nodes":[{"mesh":0,)" + nodeTransform + "}],"
          + R"("meshes":[{"primitives":[)" + primitives + "]}],"
          + R"("accessors":[{"bufferView":0,"componentType":5126,"count":)" + std::to_string(layout.positionCount)
          + R"(,"type":"VEC3","min":[-2,0,-1],"max":[2,0,1]})" + indexAccessors + "],"
          + "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":48},{\"buffer\":0,\"byteOffset\":"
          + std::to_string(indexOffset) + ",\"byteLength\":12}],"
          + "\"buffers\":[{\"byteLength\":" + std::to_string(bin.size()) + "}]}";
        std::vector<std::byte> jsonChunk = bytes_of(json);
        while (jsonChunk.size() % 4 != 0) {
            jsonChunk.push_back(std::byte{' '});
        }

        ramp_source source = {};
        std::vector<std::byte> &glb = source.glb;
        append_u32(glb, 0x46546C67u);   // "glTF"
        append_u32(glb, 2);
        append_u32(glb, static_cast<u32>(12 + 8 + jsonChunk.size() + 8 + bin.size()));
        append_u32(glb, static_cast<u32>(jsonChunk.size()));
        append_u32(glb, 0x4E4F534Au);   // "JSON"
        glb.insert(glb.end(), jsonChunk.begin(), jsonChunk.end());
        append_u32(glb, static_cast<u32>(bin.size()));
        append_u32(glb, 0x004E4942u);   // "BIN\0"
        glb.insert(glb.end(), bin.begin(), bin.end());
        return source;
    }

    /// <summary>
    /// A unit cube as OBJ text with quads, comments and relative indices.
    /// </summary>
    constexpr std::string_view HULL_OBJ = R"(# cube
v -0.5 -0.5 -0.5
v 0.5 -0.5 -0.5
v 0.5 0.5 -0.5
v -0.5 0.5 -0.5
v -0.5 -0.5 0.5
v 0.5 -0.5 0.5
v 0.5 0.5 0.5
v -0.5 0.5 0.5
vn 0 0 -1
f 1/1/1 2/2/1 3/3/1 4/4/1
f 5 8 7 6
f 1 5 6 2
f 2 6 7 3
f 3 7 8 4
f -4 -8 -5 -1
)";

    /// <summary>
    /// Runs the importer on bytes at 'assetPath' and returns the primary cooked payload.
    /// </summary>
    [[nodiscard]] core::result<assets::import_output> import_bytes(
      std::string_view assetPath,
      std::span<const std::byte> source,
      core::vfs::memory_file_system &fileSystem,
      const core::json::value &settings = {}
    ) {
        assets::asset_type_registry types = {};
        assets::importer_registry importers = {};
        CONJURE_REQUIRE_OK(physics::register_asset_types(types, importers));
        const assets::asset_importer *importer = importers.find(physics::COLLISION_MESH_IMPORTER_ID);
        REQUIRE(importer != nullptr);
        assets::sub_asset_table subAssets = {};
        const std::string filePath(assetPath.substr(assetPath.find("://") + 3));
        assets::import_context context(
          fileSystem,
          std::string(assetPath),
          "assets/" + filePath,
          source,
          settings,
          "windows",
          core::uuid(1, 1),
          subAssets,
          {}
        );
        return importer->import(context);
    }

    /// <summary>
    /// Publishes a mesh through a memory source and an asset manager, binding a ref to it.
    /// </summary>
    class loaded_mesh {
        public:
            core::jobs::job_system jobs = {};
            assets::memory_asset_source *source = nullptr;
            std::unique_ptr<assets::asset_manager> manager = nullptr;
            assets::asset_handle handle = {};
            core::uuid id = core::uuid(0x5EEDu, 0x7u);

        public:
            explicit loaded_mesh(const physics::collision_mesh_asset &mesh) {
                assets::asset_manager_desc desc = {};
                desc.jobs = &jobs;
                std::unique_ptr<assets::memory_asset_source> owned = std::make_unique<assets::memory_asset_source>();
                source = owned.get();
                desc.source = std::move(owned);
                manager = std::make_unique<assets::asset_manager>(std::move(desc));
                assets::importer_registry importers = {};
                CONJURE_REQUIRE_OK(physics::register_asset_types(manager->types(), importers));

                assets::cooked_blob_header header = {};
                header.type = assets::asset_type_traits<physics::collision_mesh_asset>::id();
                header.typeVersion = 1;
                header.importerVersion = 1;
                header.id = id;
                source->add(assets::make_cooked_blob(header, {}, mesh.to_payload()));
                const core::result<assets::asset_handle> loaded = manager->load_blocking(id);
                CONJURE_REQUIRE_OK(loaded);
                handle = loaded.value();
                REQUIRE(handle.state() == assets::asset_state::LOADED);
            }

            ~loaded_mesh() {
                handle.reset();
                manager->shutdown();
            }

            [[nodiscard]] assets::asset_ref<physics::collision_mesh_asset> ref() const {
                return handle.ref<physics::collision_mesh_asset>();
            }
    };

    [[nodiscard]] physics::collision_mesh_asset box_mesh(const math::vec3 &halfExtents) {
        std::vector<math::vec3> positions = {};
        for (u32 corner = 0; corner < 8; ++corner) {
            positions.push_back(math::vec3(
              (corner & 1) != 0 ? halfExtents.x : -halfExtents.x,
              (corner & 2) != 0 ? halfExtents.y : -halfExtents.y,
              (corner & 4) != 0 ? halfExtents.z : -halfExtents.z
            ));
        }
        const std::array<u32, 36> indices = {
            0, 2, 1, 1, 2, 3,  4, 5, 6, 5, 7, 6,  0, 1, 5, 0, 5, 4,
            2, 6, 7, 2, 7, 3,  0, 4, 6, 0, 6, 2,  1, 3, 7, 1, 7, 5,
        };
        const core::result<physics::collision_mesh_asset> mesh = physics::collision_mesh_asset::from_geometry(
          positions,
          indices
        );
        REQUIRE(mesh.has_value());
        return mesh.value();
    }
}

TEST_SUITE("physics/collision_mesh_asset") {

    TEST_CASE("collision_mesh_asset - from_geometry rejects bad indices and keeps only proper triangles") {
        const std::array<math::vec3, 4> positions = {
            math::vec3(0.0f), math::vec3(1.0f, 0.0f, 0.0f), math::vec3(0.0f, 0.0f, 1.0f), math::vec3(2.0f, 0.0f, 0.0f)
        };
        const std::array<u32, 3> outOfRange = {0, 1, 9};
        CONJURE_CHECK_ERROR(
          physics::collision_mesh_asset::from_geometry(positions, outOfRange),
          physics::physics_error::GEOMETRY_INVALID
        );
        const std::array<u32, 4> notTriangles = {0, 1, 2, 3};
        CONJURE_CHECK_ERROR(
          physics::collision_mesh_asset::from_geometry(positions, notTriangles),
          physics::physics_error::GEOMETRY_INVALID
        );
        const std::array<u32, 6> degenerate = {0, 0, 1, 0, 1, 3};   // repeated vertex, then collinear
        CONJURE_CHECK_ERROR(
          physics::collision_mesh_asset::from_geometry(positions, degenerate),
          physics::physics_error::GEOMETRY_INVALID
        );
        CONJURE_CHECK_ERROR(
          physics::collision_mesh_asset::from_geometry(positions, std::span<const u32>()),
          physics::physics_error::GEOMETRY_INVALID
        );
        const std::array<math::vec3, 3> notFinite = {
            math::vec3(0.0f), math::vec3(std::numeric_limits<f32>::quiet_NaN()), math::vec3(1.0f)
        };
        const std::array<u32, 3> one = {0, 1, 2};
        CONJURE_CHECK_ERROR(
          physics::collision_mesh_asset::from_geometry(notFinite, one),
          physics::physics_error::GEOMETRY_INVALID
        );

        const std::array<u32, 9> mixed = {0, 1, 2, 0, 0, 1, 1, 3, 2};
        const core::result<physics::collision_mesh_asset> mesh = physics::collision_mesh_asset::from_geometry(
          positions,
          mixed
        );
        CONJURE_REQUIRE_OK(mesh);
        CHECK(mesh.value().triangle_count() == 2);
        CHECK(mesh.value().vertex_count() == 4);
        CHECK(mesh.value().indices().size() == 6);
        CHECK(mesh.value().bounds().min == math::vec3(0.0f));
        CHECK(mesh.value().bounds().max == math::vec3(2.0f, 0.0f, 1.0f));
        CHECK_FALSE(mesh.value().has_shapes());
        CHECK(mesh.value().shapes() == nullptr);
        const physics::collision_mesh_asset empty = {};
        CHECK(empty.triangle_count() == 0);
        CHECK_FALSE(empty.bounds().is_valid());
    }

    TEST_CASE("collision_mesh_asset - the payload round trips with 16-byte aligned sections") {
        const physics::collision_mesh_asset mesh = box_mesh(math::vec3(1.0f, 2.0f, 3.0f));
        const std::vector<std::byte> payload = mesh.to_payload();
        CHECK(payload.size() % physics::COLLISION_MESH_SECTION_ALIGNMENT == 0);
        // 16-byte header, 8 vertices * 12 bytes = 96 (ends at 112), indices at 112.
        CHECK(payload.size() == 112 + 36 * 4);
        const core::result<physics::collision_mesh_asset> parsed = physics::collision_mesh_asset::from_payload(payload);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().vertex_count() == 8);
        CHECK(parsed.value().triangle_count() == 12);
        CHECK(parsed.value().to_payload() == payload);
        CHECK(parsed.value().bounds() == mesh.bounds());

        std::vector<std::byte> truncated(payload.begin(), payload.begin() + 60);
        CONJURE_CHECK_ERROR(
          physics::collision_mesh_asset::from_payload(truncated),
          physics::physics_error::GEOMETRY_INVALID
        );
        CONJURE_CHECK_ERROR(
          physics::collision_mesh_asset::from_payload(std::span<const std::byte>()),
          physics::physics_error::GEOMETRY_INVALID
        );
    }

    TEST_CASE("collision_mesh_importer - ramp.glb imports with the node transform baked and is byte-identical twice") {
        const ramp_source ramp = make_ramp_glb();
        core::vfs::memory_file_system fileSystem = {};
        const core::result<assets::import_output> first = import_bytes(
          "assets://physics/ramp.glb",
          ramp.glb,
          fileSystem
        );
        CONJURE_REQUIRE_OK(first);
        REQUIRE(first.value().assets.size() == 1);
        const assets::imported_asset &primary = first.value().assets[0];
        CHECK(primary.subPath.empty());
        CHECK(primary.displayName == "ramp");
        CHECK(primary.type == assets::asset_type_traits<physics::collision_mesh_asset>::id());
        const core::result<physics::collision_mesh_asset> mesh =
          physics::collision_mesh_asset::from_payload(primary.payload);
        CONJURE_REQUIRE_OK(mesh);
        CHECK(mesh.value().vertex_count() == ramp.vertexCount);
        CHECK(mesh.value().indices().size() == ramp.indexCount);
        // The node rotates the flat quad by 30 degrees about z and lifts it one metre: the corner at
        // x = 2 ends up at y = 1 + 2 sin(30) = 2.
        f32 highest = -1.0e9f;
        for (const math::vec3 &position : mesh.value().positions()) {
            highest = std::max(highest, position.y);
        }
        CHECK(highest == doctest::Approx(2.0f).epsilon(0.001));
        CHECK(mesh.value().bounds().min.y == doctest::Approx(0.0f).epsilon(0.01));

        const core::result<assets::import_output> second = import_bytes(
          "assets://physics/ramp.glb",
          ramp.glb,
          fileSystem
        );
        CONJURE_REQUIRE_OK(second);
        CHECK(second.value().assets[0].payload == primary.payload);
    }

    TEST_CASE("collision_mesh_importer - hull.obj parses quads and relative indices") {
        core::vfs::memory_file_system fileSystem = {};
        const std::vector<std::byte> obj = bytes_of(HULL_OBJ);
        const core::result<assets::import_output> imported = import_bytes("assets://physics/hull.obj", obj, fileSystem);
        CONJURE_REQUIRE_OK(imported);
        const core::result<physics::collision_mesh_asset> mesh =
          physics::collision_mesh_asset::from_payload(imported.value().assets[0].payload);
        CONJURE_REQUIRE_OK(mesh);
        CHECK(mesh.value().vertex_count() == 8);
        CHECK(mesh.value().triangle_count() == 12);
        CHECK(mesh.value().bounds().min == math::vec3(-0.5f));
        CHECK(mesh.value().bounds().max == math::vec3(0.5f));

        const std::vector<std::byte> broken = bytes_of("v 0 0 0\nv 1 0 0\nf 1 2 9\n");
        const core::result<assets::import_output> failed = import_bytes(
          "assets://physics/broken.obj",
          broken,
          fileSystem
        );
        CONJURE_CHECK_ERROR(failed, assets::asset_error_code::IMPORT_FAILED);
        if (!failed.has_value()) {
            CHECK(failed.get_error().message().find("line 3") != std::string::npos);
        }
        const std::vector<std::byte> noTriangles = bytes_of("v 0 0 0\nv 1 0 0\nv 2 0 0\nf 1 2 3\n");
        CONJURE_CHECK_ERROR(
          import_bytes("assets://physics/flat.obj", noTriangles, fileSystem),
          assets::asset_error_code::IMPORT_FAILED
        );
        const std::vector<std::byte> garbage = bytes_of("not a gltf file at all, long enough to pass the size check");
        CONJURE_CHECK_ERROR(
          import_bytes("assets://physics/garbage.gltf", garbage, fileSystem),
          assets::asset_error_code::IMPORT_FAILED
        );
    }

    TEST_CASE("regression - review 17 an OBJ face with more than 63 vertices is a parse error not a truncation") {
        core::vfs::memory_file_system fileSystem = {};
        // A 70-gon fan: 70 vertices on a circle plus one face naming them all.
        std::string obj = {};
        for (u32 index = 0; index < 70; ++index) {
            const f32 angle = static_cast<f32>(index) * 6.2831853f / 70.0f;
            obj += "v " + std::to_string(std::cos(angle)) + " 0 " + std::to_string(std::sin(angle)) + "\n";
        }
        obj += "f";
        for (u32 index = 1; index <= 70; ++index) {
            obj += " " + std::to_string(index);
        }
        obj += "\n";
        const core::result<assets::import_output> failed = import_bytes(
          "assets://physics/wide.obj",
          bytes_of(obj),
          fileSystem
        );
        CONJURE_CHECK_ERROR(failed, assets::asset_error_code::IMPORT_FAILED);
        if (!failed.has_value()) {
            CHECK(failed.get_error().message().find("line 71") != std::string::npos);
            CHECK(failed.get_error().message().find("too many") != std::string::npos);
        }
        // Exactly 63 corners (64 tokens) still fit.
        std::string wide = obj.substr(0, obj.find("f"));
        wide += "f";
        for (u32 index = 1; index <= 63; ++index) {
            wide += " " + std::to_string(index);
        }
        wide += "\n";
        const core::result<assets::import_output> imported = import_bytes(
          "assets://physics/fits.obj",
          bytes_of(wide),
          fileSystem
        );
        CONJURE_REQUIRE_OK(imported);
        const core::result<physics::collision_mesh_asset> mesh =
          physics::collision_mesh_asset::from_payload(imported.value().assets[0].payload);
        CONJURE_REQUIRE_OK(mesh);
        CHECK(mesh.value().triangle_count() == 61);
    }

    TEST_CASE("collision_mesh_importer - welding merges vertices within weld_distance") {
        core::vfs::memory_file_system fileSystem = {};
        const std::vector<std::byte> obj = bytes_of("v 0 0 0\nv 1 0 0\nv 0 0 1\nv 1.00001 0 0\nf 1 2 3\nf 3 4 1\n");
        const core::result<assets::import_output> welded = import_bytes("assets://physics/weld.obj", obj, fileSystem);
        CONJURE_REQUIRE_OK(welded);
        const core::result<physics::collision_mesh_asset> mesh =
          physics::collision_mesh_asset::from_payload(welded.value().assets[0].payload);
        CONJURE_REQUIRE_OK(mesh);
        CHECK(mesh.value().vertex_count() == 3);
        CHECK(mesh.value().triangle_count() == 2);

        const core::json::document settings = core::json::object({{"weld_distance", 0.0}});
        const core::result<assets::import_output> unwelded = import_bytes(
          "assets://physics/weld.obj",
          obj,
          fileSystem,
          settings.root()
        );
        CONJURE_REQUIRE_OK(unwelded);
        const core::result<physics::collision_mesh_asset> raw =
          physics::collision_mesh_asset::from_payload(unwelded.value().assets[0].payload);
        CONJURE_REQUIRE_OK(raw);
        CHECK(raw.value().vertex_count() == 4);
    }

    TEST_CASE("collision_mesh_importer - welding merges every duplicate of a vertex that shares a cell with another") {
        // With a 1 m weld distance the first two vertices share a grid cell but are too far apart to weld; the
        // third duplicates the second and must still find it although the cell already has a vertex.
        core::vfs::memory_file_system fileSystem = {};
        const std::vector<std::byte> obj = bytes_of(
          "v 0.05 0.05 0.05\nv 0.95 0.95 0.95\nv 0.95 0.95 0.95\nv 5 0 0\nv 0 5 0\nv 0 0 5\n"
          "f 1 4 5\nf 2 5 6\nf 3 6 4\n"
        );
        const core::json::document settings = core::json::object({{"weld_distance", 1.0}});
        const core::result<assets::import_output> imported = import_bytes(
          "assets://physics/cell.obj",
          obj,
          fileSystem,
          settings.root()
        );
        CONJURE_REQUIRE_OK(imported);
        const core::result<physics::collision_mesh_asset> mesh =
          physics::collision_mesh_asset::from_payload(imported.value().assets[0].payload);
        CONJURE_REQUIRE_OK(mesh);
        CHECK(mesh.value().vertex_count() == 5);
        CHECK(mesh.value().triangle_count() == 3);
    }

    TEST_CASE("collision_mesh_importer - a glTF accessor larger than its buffer view is a parse failure not a crash") {
        core::vfs::memory_file_system fileSystem = {};
        ramp_layout layout = {};
        layout.positionCount = 3000000;
        const ramp_source oversized = make_ramp_glb(layout);
        CONJURE_CHECK_ERROR(
          import_bytes("assets://physics/oversized.glb", oversized.glb, fileSystem),
          assets::asset_error_code::IMPORT_FAILED
        );
    }

    TEST_CASE(
      "collision_mesh_importer - a glTF whose external buffer file is shorter than its declared byteLength is a "
      "parse failure not a crash"
    ) {
        // The accessor fits the declared buffer, so only the length of the file actually read can catch it.
        core::vfs::memory_file_system fileSystem = {};
        CONJURE_REQUIRE_OK(fileSystem.write_file("assets/physics/short.bin", std::vector<std::byte>(48)));
        const std::string json = std::string(R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)")
          + R"("nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],)"
          + R"("accessors":[{"bufferView":0,"componentType":5126,"count":3000000,"type":"VEC3",)"
          + R"("min":[0,0,0],"max":[1,1,1]}],)"
          + R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36000000}],)"
          + R"("buffers":[{"uri":"short.bin","byteLength":36000000}]})";
        const core::result<assets::import_output> imported = import_bytes(
          "assets://physics/short.gltf",
          bytes_of(json),
          fileSystem
        );
        CONJURE_CHECK_ERROR(imported, assets::asset_error_code::IMPORT_FAILED);
        if (!imported.has_value()) {
            CHECK(imported.get_error().message().find("short.bin") != std::string::npos);
        }
    }

    TEST_CASE(
      "collision_mesh_importer - a glTF primitive whose index count is not a multiple of three is a parse failure"
    ) {
        // Four indices plus five: the total is a multiple of three, so only a per-primitive check catches it.
        core::vfs::memory_file_system fileSystem = {};
        ramp_layout layout = {};
        layout.primitives = {index_range{0, 4}, index_range{1, 5}};
        const ramp_source ragged = make_ramp_glb(layout);
        const core::result<assets::import_output> imported = import_bytes(
          "assets://physics/ragged.glb",
          ragged.glb,
          fileSystem
        );
        CONJURE_CHECK_ERROR(imported, assets::asset_error_code::IMPORT_FAILED);
        if (!imported.has_value()) {
            CHECK(imported.get_error().message().find("multiple of three") != std::string::npos);
        }
    }

    TEST_CASE("collision_mesh_importer - a mirrored glTF node keeps its triangles facing the same way") {
        core::vfs::memory_file_system fileSystem = {};
        ramp_layout layout = {};
        layout.nodeTransform = R"("scale":[-1,1,1])";
        const ramp_source mirrored = make_ramp_glb(layout);
        const core::result<assets::import_output> imported = import_bytes(
          "assets://physics/mirrored.glb",
          mirrored.glb,
          fileSystem
        );
        CONJURE_REQUIRE_OK(imported);
        const core::result<physics::collision_mesh_asset> mesh =
          physics::collision_mesh_asset::from_payload(imported.value().assets[0].payload);
        CONJURE_REQUIRE_OK(mesh);
        REQUIRE(mesh.value().triangle_count() == 2);
        const std::span<const math::vec3> positions = mesh.value().positions();
        const std::span<const u32> indices = mesh.value().indices();
        for (u32 triangle = 0; triangle < mesh.value().triangle_count(); ++triangle) {
            CAPTURE(triangle);
            const math::vec3 &a = positions[indices[triangle * 3]];
            const math::vec3 &b = positions[indices[triangle * 3 + 1]];
            const math::vec3 &c = positions[indices[triangle * 3 + 2]];
            // The authored winding is counter-clockwise seen from above (+y), the face Jolt collides with.
            CHECK(glm::cross(b - a, c - a).y > 0.0f);
        }
    }

    TEST_CASE("collision_mesh_asset - the loader builds the Jolt shape on load") {
        const loaded_mesh loaded(box_mesh(math::vec3(0.5f)));
        const physics::collision_mesh_asset *mesh = loaded.ref().get();
        REQUIRE(mesh != nullptr);
        CHECK(mesh->has_shapes());
        CHECK(mesh->triangle_count() == 12);
    }

    TEST_CASE("collision_mesh_asset - a sphere dropped on a ramp mesh slides and lands at the analytic x") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            // A ramp rising 30 degrees towards -x, from x = 0 (y = 0) up to x = -8 (y = 8 tan 30).
            const f32 slope = std::tan(math::radians(30.0f));
            const std::array<math::vec3, 4> positions = {
                math::vec3(-8.0f, 8.0f * slope, -4.0f),
                math::vec3(0.0f, 0.0f, -4.0f),
                math::vec3(0.0f, 0.0f, 4.0f),
                math::vec3(-8.0f, 8.0f * slope, 4.0f),
            };
            // Counter-clockwise seen from above: Jolt simulates against the front face of a mesh triangle.
            const std::array<u32, 6> indices = {0, 2, 1, 0, 3, 2};
            const core::result<physics::collision_mesh_asset> geometry = physics::collision_mesh_asset::from_geometry(
              positions,
              indices
            );
            CONJURE_REQUIRE_OK(geometry);
            const loaded_mesh loaded(geometry.value());

            physics::physics_settings settings = test_settings();
            settings.allowSleeping = false;
            physics_fixture fixture(jobs, settings);
            const ecs::entity ramp = fixture.create_at(math::vec3(0.0f));
            physics::mesh_collider collider = {};
            collider.mesh = loaded.ref();
            CONJURE_REQUIRE_OK(fixture.world.add<physics::mesh_collider>(ramp, collider));
            (void)fixture.spawn_ground();
            const ecs::entity ball = fixture.create_at(math::vec3(-4.0f, 4.0f * slope + 2.0f, 0.0f));
            physics::rigid_body body = {};
            body.linearDamping = 0.0f;
            body.flags = physics::rigid_body_flags::NONE;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(ball, body));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::sphere_collider>(ball));
            physics::physics_material slippery = {};
            slippery.friction = 0.0f;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::physics_material>(ball, slippery));
            fixture.run_steps(2);
            REQUIRE(fixture.physics.has_body(ramp));
            REQUIRE(fixture.physics.has_body(ball));

            // Frictionless slide: the ball reaches the foot of the ramp (x = 0) and rolls on along +x
            // on the ground; after 240 steps it is past the ramp and resting at radius height.
            fixture.run_steps(240);
            const math::vec3 position = fixture.position_of(ball);
            CHECK(position.x > 0.5f);
            CHECK(position.y == doctest::Approx(0.5f).epsilon(0.05));
            // The ramp's bottom edge is a hit for a ray from above (-4, 10, 0): the surface is there.
            math::ray ray = {};
            ray.origin = math::vec3(-4.0f, 10.0f, 0.0f);
            ray.direction = math::vec3(0.0f, -1.0f, 0.0f);
            const std::optional<physics::raycast_hit> hit = fixture.physics.raycast(ray, 20.0f);
            REQUIRE(hit.has_value());
            CHECK(hit->entity == ramp);
            CHECK(hit->point.y == doctest::Approx(4.0f * slope).epsilon(0.01));
        });
    }

    TEST_CASE("collision_mesh_asset - a convex collider stacks on a box") {
        core::jobs::job_system jobs = {};
        const loaded_mesh loaded(box_mesh(math::vec3(0.5f)));
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        const ecs::entity base = fixture.spawn_box(math::vec3(0.0f, 0.5f, 0.0f), physics::motion_type::STATIC);
        const ecs::entity prop = fixture.create_at(math::vec3(0.0f, 2.5f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(prop));
        physics::convex_collider convex = {};
        convex.mesh = loaded.ref();
        convex.convexRadius = 0.02f;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::convex_collider>(prop, convex));
        fixture.run_steps(180);
        REQUIRE(fixture.physics.has_body(prop));
        CHECK(fixture.position_of(prop).y == doctest::Approx(1.5f).epsilon(0.02));
        CHECK(fixture.position_of(base).y == doctest::Approx(0.5f));
        CHECK(fixture.physics.stats().shapeCacheEntries >= 2);

        // Reloading the asset marks the body for a shape rebuild; the body survives.
        fixture.physics.on_asset_reloaded(loaded.id);
        fixture.run_steps(2);
        CHECK(fixture.physics.has_body(prop));
        CHECK(fixture.position_of(prop).y == doctest::Approx(1.5f).epsilon(0.02));
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("collision_mesh_asset - a collider on an unloaded asset keeps the body pending") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity prop = fixture.create_at(math::vec3(0.0f, 2.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(prop));
        physics::convex_collider convex = {};
        convex.mesh = assets::asset_ref<physics::collision_mesh_asset>(core::uuid(3, 4));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::convex_collider>(prop, convex));
        fixture.run_steps(3);
        CHECK_FALSE(fixture.physics.has_body(prop));
        CHECK(fixture.physics.stats().bodiesWithoutShape == 1);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE(
      "collision_mesh_asset - a compound with one unloaded asset stays pending instead of building without it"
    ) {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        physics::convex_collider unloaded = {};
        unloaded.mesh = assets::asset_ref<physics::collision_mesh_asset>(core::uuid(3, 4));

        SUBCASE("a new body waits for the asset") {
            const ecs::entity prop = fixture.create_at(math::vec3(0.0f, 2.0f, 0.0f));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(prop));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(prop));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::convex_collider>(prop, unloaded));
            fixture.run_steps(3);
            CHECK_FALSE(fixture.physics.has_body(prop));
            CHECK(fixture.physics.stats().bodiesWithoutShape == 1);
        }
        SUBCASE("an existing body keeps its shape until the asset loads") {
            const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 2.0f, 0.0f));
            fixture.run_step();
            REQUIRE(fixture.physics.has_body(box));
            unloaded.offset = math::vec3(2.0f, 0.0f, 0.0f);
            CONJURE_REQUIRE_OK(fixture.world.add<physics::convex_collider>(box, unloaded));
            fixture.run_steps(3);
            CHECK(fixture.physics.has_body(box));
            std::array<ecs::entity, 2> out = {};
            CHECK(fixture.physics.overlap_sphere(math::vec3(0.0f, 2.0f, 0.0f), 0.1f, out) == 1);
            CHECK(out[0] == box);
        }
        SUBCASE("a body waiting for an asset logs no collider warning") {
            testing::log_capture logs = {};
            const ecs::entity prop = fixture.create_at(math::vec3(0.0f, 2.0f, 0.0f));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(prop));
            physics::box_collider trigger = {};
            trigger.isTrigger = true;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(prop, trigger));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::convex_collider>(prop, unloaded));
            fixture.run_steps(5);
            CHECK_FALSE(logs.contains(core::log_level::WARN, "MIXED_TRIGGER_FLAGS"));
            CHECK_FALSE(fixture.physics.has_body(prop));
        }
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE(
      "collision_mesh_asset - a rigid_body with mass 0 on a mesh_collider creates its body without a mass assert"
    ) {
        // A mesh has no volume to derive a mass from, and Jolt builds motion properties for every body a
        // rigid_body authors (it may change motion type later), static ones included.
        const loaded_mesh loaded(box_mesh(math::vec3(2.0f, 0.5f, 2.0f)));
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        testing::log_capture logs = {};
        logs.allow_errors();
        testing::expect_assert guard = {};
        std::array<ecs::entity, 2> bodies = {};
        const std::array<physics::motion_type, 2> motions = {
            physics::motion_type::STATIC,
            physics::motion_type::KINEMATIC
        };
        for (usize index = 0; index < bodies.size(); ++index) {
            bodies[index] = fixture.create_at(math::vec3(static_cast<f32>(index) * 10.0f, 0.0f, 0.0f));
            physics::rigid_body body = {};
            body.motionType = motions[index];
            body.mass = 0.0f;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(bodies[index], body));
            physics::mesh_collider collider = {};
            collider.mesh = loaded.ref();
            CONJURE_REQUIRE_OK(fixture.world.add<physics::mesh_collider>(bodies[index], collider));
        }
        fixture.run_steps(2);
        CHECK(fixture.physics.has_body(bodies[0]));
        CHECK(fixture.physics.has_body(bodies[1]));

        // The kinematic one goes static and back: the motion properties it was created with stay usable.
        physics::rigid_body *kinematic = fixture.world.get<physics::rigid_body>(bodies[1]);
        REQUIRE(kinematic != nullptr);
        kinematic->motionType = physics::motion_type::STATIC;
        fixture.run_steps(2);
        kinematic = fixture.world.get<physics::rigid_body>(bodies[1]);
        REQUIRE(kinematic != nullptr);
        kinematic->motionType = physics::motion_type::KINEMATIC;
        fixture.run_steps(2);
        CHECK(fixture.physics.has_body(bodies[1]));
        CHECK(guard.count() == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("collision_mesh_asset - a static trigger made of a mesh reports the body that falls through it") {
        // A static trigger is a kinematic body to Jolt (code sweep F408), which needs a mass the mesh cannot
        // give it. With a mesh_collider alone the default mass of the implicit static body (1 kg) covers it;
        // next to a rigid_body without a mass the world falls back to 1 kg itself.
        const loaded_mesh loaded(box_mesh(math::vec3(2.0f, 0.5f, 2.0f)));
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.allowSleeping = false;
        physics_fixture fixture(jobs, settings);
        testing::log_capture logs = {};
        logs.allow_errors();
        testing::expect_assert guard = {};
        const ecs::entity trigger = fixture.create_at(math::vec3(0.0f, 3.0f, 0.0f));
        SUBCASE("a mesh_collider alone") {}
        SUBCASE("a mesh_collider next to a static rigid_body without a mass") {
            physics::rigid_body body = {};
            body.motionType = physics::motion_type::STATIC;
            body.mass = 0.0f;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(trigger, body));
        }
        physics::mesh_collider collider = {};
        collider.mesh = loaded.ref();
        collider.isTrigger = true;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::mesh_collider>(trigger, collider));
        const ecs::entity ball = fixture.spawn_sphere(math::vec3(0.0f, 6.0f, 0.0f), 0.25f);

        // A mesh is a surface: the ball enters at the top face of the box and again at the bottom face.
        trigger_tally tally = {};
        tally.trigger = trigger;
        tally.other = ball;
        for (u32 step = 0; step < 120; ++step) {
            CAPTURE(step);
            tally.run(fixture, 1);
            // Nothing else is reported: no pair but this one, and no solid contact.
            for (const physics::contact_event &event : fixture.physics.contact_events()) {
                CHECK(involves(event, trigger, ball));
                const bool isTriggerKind = event.kind == physics::contact_kind::TRIGGER_ENTER
                  || event.kind == physics::contact_kind::TRIGGER_EXIT;
                CHECK(isTriggerKind);
            }
        }
        CHECK(tally.enters >= 1);
        CHECK(tally.exits == tally.enters);
        // No response: the ball is far below the trigger.
        CHECK(fixture.position_of(ball).y < 0.0f);
        CHECK(fixture.position_of(trigger) == math::vec3(0.0f, 3.0f, 0.0f));
        CHECK(guard.count() == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }
}
