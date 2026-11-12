#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "conjure/testing/temp_directory.h"
#include "gltf_test_builder.h"
#include "renderer_test_support.h"

#include "conjure/renderer/gltf_importer.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/model_asset.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/assets_error.h"
#include "conjure/core/json/document.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/memory_file_system.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// gltf_importer.h (docs/design/02_renderer.md 4.2): .gltf / .glb to CPU meshes, materials,
// textures and the node tree, through cgltf. Every input is generated in-test.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;
using namespace conjure::testing::gltf_builder;

namespace {

    [[nodiscard]] std::vector<std::byte> bytes_of(std::string_view text) {
        std::vector<std::byte> bytes = {};
        for (const char character : text) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        return bytes;
    }

    [[nodiscard]] std::string text_of(std::span<const std::byte> bytes) {
        return std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    }

    /// <summary>
    /// A cube with a textured material, a plain quad, a two-level node tree and a spot light.
    /// </summary>
    [[nodiscard]] gltf_test_builder scene_builder() {
        gltf_test_builder builder = {};
        const u32 albedo = builder.add_solid_texture(4, 4, 200, 100, 50);
        const u32 normal = builder.add_solid_texture(2, 2, 128, 128, 255);
        material_input textured = {};
        textured.name = "painted";
        textured.baseColour = math::vec4(0.9f, 0.8f, 0.7f, 1.0f);
        textured.metallic = 0.0f;
        textured.roughness = 0.4f;
        textured.baseColourTexture = albedo;
        textured.normalTexture = normal;
        textured.emissive = math::vec3(0.1f, 0.2f, 0.3f);
        const u32 painted = builder.add_material(textured);
        material_input glass = {};
        glass.name = "glass";
        glass.alphaMode = "BLEND";
        glass.doubleSided = true;
        glass.baseColour = math::vec4(1.0f, 1.0f, 1.0f, 0.5f);
        const u32 glassIndex = builder.add_material(glass);
        const u32 cube = builder.add_mesh(gltf_test_builder::cube(0.5f, painted));
        const u32 quad = builder.add_mesh(gltf_test_builder::quad(1.0f, glassIndex));
        node_input root = {};
        root.name = "root";
        root.translation = math::vec3(1.0f, 2.0f, 3.0f);
        root.scale = math::vec3(2.0f);
        const u32 rootIndex = builder.add_node(root);
        node_input cubeNode = {};
        cubeNode.name = "cube";
        cubeNode.mesh = cube;
        cubeNode.parent = rootIndex;
        cubeNode.translation = math::vec3(0.0f, 1.0f, 0.0f);
        builder.add_node(cubeNode);
        node_input quadNode = {};
        quadNode.name = "pane";
        quadNode.mesh = quad;
        builder.add_node(quadNode);
        light_input spot = {};
        spot.type = "spot";
        spot.colour = math::vec3(1.0f, 0.5f, 0.25f);
        spot.intensity = 40.0f;
        spot.range = 12.0f;
        spot.innerCone = 0.2f;
        spot.outerCone = 0.5f;
        const u32 light = builder.add_light(spot);
        node_input lightNode = {};
        lightNode.name = "lamp";
        lightNode.light = light;
        lightNode.parent = rootIndex;
        builder.add_node(lightNode);
        return builder;
    }

    [[nodiscard]] gltf_import_result import_or_fail(
      const gltf_importer &importer,
      std::span<const std::byte> bytes,
      const gltf_import_options &options = {}
    ) {
        core::result<gltf_import_result> imported = importer.import_bytes(bytes, options, nullptr);
        CONJURE_REQUIRE_OK(imported);
        return std::move(imported).value();
    }

    /// <summary>
    /// A hand-written .gltf with one embedded buffer holding 'values' (little-endian floats); 'members'
    /// is spliced in as the further top-level members (bufferViews, accessors, meshes, ...).
    /// </summary>
    [[nodiscard]] std::string embedded_document(std::span<const f32> values, std::string_view members) {
        std::vector<std::byte> buffer(values.size() * sizeof(f32));
        std::memcpy(buffer.data(), values.data(), buffer.size());
        return std::string(R"({"asset": {"version": "2.0"}, "buffers": [{"byteLength": )") +
          std::to_string(buffer.size()) + R"(, "uri": "data:application/octet-stream;base64,)" +
          base64_encode(buffer) + R"("}], )" + std::string(members) + "}";
    }

    /// <summary>
    /// The three positions of a triangle in the XY plane.
    /// </summary>
    constexpr std::array<f32, 9> TRIANGLE_POSITIONS = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};

    /// <summary>
    /// A triangle whose COLOR_0 accessor has the given glTF type and component count.
    /// </summary>
    [[nodiscard]] std::string coloured_triangle(std::string_view colourType, usize colourComponents) {
        std::vector<f32> values(TRIANGLE_POSITIONS.begin(), TRIANGLE_POSITIONS.end());
        values.insert(values.end(), 3 * colourComponents, 0.5f);
        const std::string colourBytes = std::to_string(3 * colourComponents * sizeof(f32));
        return embedded_document(
          values,
          std::string(R"("bufferViews": [{"buffer": 0, "byteLength": 36}, )") +
            R"({"buffer": 0, "byteOffset": 36, "byteLength": )" + colourBytes + "}], " +
            R"("accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}, )" +
            R"({"bufferView": 1, "componentType": 5126, "count": 3, "type": ")" + std::string(colourType) + R"("}], )" +
            R"("meshes": [{"primitives": [{"attributes": {"POSITION": 0, "COLOR_0": 1}}]}], )" +
            R"("nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0)"
        );
    }

    /// <summary>
    /// The message of a failed import, or an empty string when it succeeded.
    /// </summary>
    [[nodiscard]] std::string failure_message(const core::result<gltf_import_result> &imported) {
        return imported.has_value() ? std::string() : std::string(imported.get_error().message());
    }
}

TEST_SUITE("renderer/gltf_importer") {

    TEST_CASE("gltf_importer - imports meshes, materials, textures and the node tree from an embedded .gltf") {
        const gltf_test_builder builder = scene_builder();
        const std::vector<std::byte> bytes = bytes_of(builder.build_embedded());
        const gltf_importer importer = {};
        const gltf_import_result result = import_or_fail(importer, bytes);

        REQUIRE(result.meshes.size() == 2);
        CHECK(result.meshes[0].name() == "cube");
        CHECK(result.meshes[0].vertex_count() == 24);
        CHECK(result.meshes[0].index_count() == 36);
        CONJURE_CHECK_APPROX(result.meshes[0].bounds().min, math::vec3(-0.5f));
        CONJURE_CHECK_APPROX(result.meshes[0].bounds().max, math::vec3(0.5f));
        CHECK(result.meshes[1].name() == "quad");
        CHECK(result.meshes[1].vertex_count() == 4);
        CHECK(result.meshes[1].index_count() == 6);
        // Normals and uvs came from the file; tangents were generated along +U.
        const packed_vertex_attributes &quadVertex = result.meshes[1].streams().attributes[1];
        CHECK(unpack_snorm16(quadVertex.normal[2]) == doctest::Approx(1.0f));
        CONJURE_CHECK_APPROX(quadVertex.uv0, math::vec2(1.0f, 1.0f));
        CHECK(unpack_snorm16(quadVertex.tangent[0]) == doctest::Approx(1.0f).epsilon(1.0e-3));

        REQUIRE(result.materials.size() == 2);
        REQUIRE(result.materialTextures.size() == 2);
        const material_desc &painted = result.materials[0].desc();
        CHECK(result.materials[0].name() == "painted");
        CONJURE_CHECK_APPROX(painted.baseColourFactor, math::vec4(0.9f, 0.8f, 0.7f, 1.0f));
        CHECK(painted.metallicFactor == doctest::Approx(0.0f));
        CHECK(painted.roughnessFactor == doctest::Approx(0.4f));
        CHECK(painted.normalScale == doctest::Approx(0.75f));
        CONJURE_CHECK_APPROX(painted.emissiveFactor, math::vec3(0.1f, 0.2f, 0.3f));
        CHECK(painted.alphaMode == alpha_mode::SOLID);
        CHECK_FALSE(painted.doubleSided);
        CHECK(result.materialTextures[0][static_cast<usize>(material_texture::BASE_COLOUR)] == 0);
        CHECK(result.materialTextures[0][static_cast<usize>(material_texture::NORMAL)] == 1);
        CHECK(result.materialTextures[0][static_cast<usize>(material_texture::EMISSIVE)] == INVALID_INDEX);
        const material_desc &glass = result.materials[1].desc();
        CHECK(glass.alphaMode == alpha_mode::BLEND);
        CHECK(glass.doubleSided);
        CHECK(glass.baseColourFactor.w == doctest::Approx(0.5f));
        CHECK(std::ranges::all_of(result.materialTextures[1], [](u32 index) { return index == INVALID_INDEX; }));

        REQUIRE(result.textures.size() == 2);
        CHECK(result.textures[0].width() == 4);
        CHECK(result.textures[0].colour_space() == texture_colour_space::SRGB);
        CHECK(result.textures[0].pixel(0, 0) == core::rgba8{200, 100, 50, 255});
        CHECK(result.textures[0].mip_count() == 3);
        CHECK(result.textures[1].colour_space() == texture_colour_space::LINEAR);
        CHECK(result.textures[1].wrap() == texture_wrap::REPEAT);
        CHECK(result.textures[1].filter() == sampler_filter::LINEAR);

        const model_asset &model = result.model;
        REQUIRE(model.nodes().size() == 4);
        CHECK(model.meshes().size() == 2);
        CHECK(model.materials().size() == 2);
        CHECK(model.nodes()[0].name == "root");
        CHECK(model.nodes()[0].parent == INVALID_INDEX);
        CONJURE_CHECK_APPROX(model.nodes()[0].localTransform.position, math::vec3(1.0f, 2.0f, 3.0f));
        CONJURE_CHECK_APPROX(model.nodes()[0].localTransform.scale, math::vec3(2.0f));
        CHECK(model.nodes()[1].name == "cube");
        CHECK(model.nodes()[1].parent == 0);
        CHECK(model.nodes()[1].mesh == 0);
        CHECK(model.nodes()[1].materials[0] == 0);
        CHECK(model.nodes()[1].materials[1] == INVALID_INDEX);
        CHECK(model.nodes()[2].name == "lamp");
        CHECK(model.nodes()[2].parent == 0);
        CHECK(model.nodes()[2].hasLight);
        CHECK(model.nodes()[2].lightData.type == light_type::SPOT);
        CONJURE_CHECK_APPROX(model.nodes()[2].lightData.colour, math::vec3(1.0f, 0.5f, 0.25f));
        CHECK(model.nodes()[2].lightData.intensity == doctest::Approx(40.0f));
        CHECK(model.nodes()[2].lightData.range == doctest::Approx(12.0f));
        CHECK(model.nodes()[2].lightData.innerConeRadians == doctest::Approx(0.2f));
        CHECK(model.nodes()[2].lightData.outerConeRadians == doctest::Approx(0.5f));
        CHECK(model.nodes()[3].name == "pane");
        CHECK(model.nodes()[3].mesh == 1);
        CHECK(model.nodes()[3].materials[0] == 1);
        CHECK(model.root_nodes() == std::vector<u32>{0, 3});
        CHECK(result.warnings.empty());
    }

    TEST_CASE("gltf_importer - options control lights, cameras, the root scale and the tangent warning") {
        gltf_test_builder builder = scene_builder();
        gltf_import_options options = {};
        options.importLights = false;
        options.scale = 0.5f;
        options.generateTangents = false;
        const gltf_importer importer = {};
        const gltf_import_result result = import_or_fail(importer, bytes_of(builder.build_embedded()), options);
        CHECK_FALSE(result.model.nodes()[2].hasLight);
        CONJURE_CHECK_APPROX(result.model.nodes()[0].localTransform.position, math::vec3(0.5f, 1.0f, 1.5f));
        CONJURE_CHECK_APPROX(result.model.nodes()[0].localTransform.scale, math::vec3(1.0f));
        // children unscaled
        CONJURE_CHECK_APPROX(result.model.nodes()[1].localTransform.position, math::vec3(0.0f, 1.0f, 0.0f));
        REQUIRE(result.warnings.size() == 1);
        CHECK(result.warnings[0].find("generateTangents") != std::string::npos);
    }

    TEST_CASE("gltf_importer - unsupported features warn and are skipped") {
        gltf_test_builder builder = scene_builder();
        builder.use_extension("KHR_materials_volume");
        builder.add_skin_and_animation();
        material_input transformed = {};
        transformed.name = "shifted";
        transformed.baseColourTexture = 0;
        transformed.textureTransform = true;
        static_cast<void>(builder.add_material(transformed));
        const gltf_importer importer = {};
        const gltf_import_result result = import_or_fail(importer, bytes_of(builder.build_embedded()));
        CHECK(result.materials.size() == 3);
        const auto hasWarning = [&result](std::string_view fragment) {
            return std::ranges::any_of(result.warnings, [fragment](const std::string &warning) {
                return warning.find(fragment) != std::string::npos;
            });
        };
        CHECK(hasWarning("KHR_materials_volume"));
        CHECK(hasWarning("skins"));
        CHECK(hasWarning("animations"));
        CHECK(hasWarning("KHR_texture_transform"));
        CHECK(result.materials[2].desc().alphaMode == alpha_mode::SOLID);
    }

    TEST_CASE("regression - image data URIs decode completely with and without base64 padding") {
        // The decoded size is derived from the text; exporters omit the '=' padding, which used to
        // cut one or two bytes off the PNG. Three image sizes cover every length remainder.
        for (const bool padded : {true, false}) {
            for (const u32 edge : {1u, 2u, 3u}) {
                CAPTURE(padded);
                CAPTURE(edge);
                gltf_test_builder builder = {};
                const u32 texture = builder.add_solid_texture(edge, edge, 10, 20, 30);
                material_input material = {};
                material.baseColourTexture = texture;
                const u32 index = builder.add_material(material);
                builder.add_mesh(gltf_test_builder::quad(1.0f, index));
                node_input node = {};
                node.mesh = 0;
                builder.add_node(node);
                builder.use_image_data_uris(padded);
                const std::string text = builder.build_embedded();
                CHECK(text.find("data:image/png;base64,") != std::string::npos);
                const gltf_importer importer = {};
                const gltf_import_result result = import_or_fail(importer, bytes_of(text));
                REQUIRE(result.textures.size() == 1);
                CHECK(result.textures[0].width() == edge);
                CHECK(result.textures[0].height() == edge);
                CHECK(result.textures[0].pixel(0, 0) == core::rgba8{10, 20, 30, 255});
                CHECK(result.warnings.empty());
            }
        }
    }

    TEST_CASE("gltf_importer - a texture used as colour and as data is decoded once and warned about") {
        gltf_test_builder builder = {};
        const u32 shared = builder.add_solid_texture(2, 2, 10, 20, 30);
        material_input material = {};
        material.baseColourTexture = shared;
        material.metallicRoughnessTexture = shared;
        const u32 index = builder.add_material(material);
        builder.add_mesh(gltf_test_builder::quad(1.0f, index));
        node_input node = {};
        node.mesh = 0;
        builder.add_node(node);
        const gltf_importer importer = {};
        const gltf_import_result result = import_or_fail(importer, bytes_of(builder.build_embedded()));
        REQUIRE(result.textures.size() == 1);
        CHECK(result.textures[0].colour_space() == texture_colour_space::SRGB);   // first use wins
        REQUIRE(result.warnings.size() == 1);
        CHECK(result.warnings[0].find("both as colour and as data") != std::string::npos);
        CHECK(result.materialTextures[0][static_cast<usize>(material_texture::METALLIC_ROUGHNESS)] == 0);
    }

    TEST_CASE("gltf_importer - unlit, masked and vertex-coloured primitives import their flags and colours") {
        gltf_test_builder builder = {};
        material_input unlit = {};
        unlit.unlit = true;
        unlit.alphaMode = "MASK";
        unlit.alphaCutoff = 0.25f;
        const u32 material = builder.add_material(unlit);
        mesh_input coloured = gltf_test_builder::quad(1.0f, material);
        coloured.colours.assign(4, math::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        coloured.tangents.assign(4, math::vec4(0.0f, 1.0f, 0.0f, -1.0f));
        builder.add_mesh(coloured);
        node_input node = {};
        node.mesh = 0;
        builder.add_node(node);
        const gltf_importer importer = {};
        const gltf_import_result result = import_or_fail(importer, bytes_of(builder.build_embedded()));
        REQUIRE(result.materials.size() == 1);
        CHECK(result.materials[0].desc().unlit);
        CHECK(result.materials[0].desc().alphaMode == alpha_mode::MASK);
        CHECK(result.materials[0].desc().alphaCutoff == doctest::Approx(0.25f));
        REQUIRE(result.meshes.size() == 1);
        const packed_vertex_attributes &vertex = result.meshes[0].streams().attributes[0];
        CHECK(vertex.colour == 0xFF0000FFu);
        CHECK(unpack_snorm16(vertex.tangent[1]) == doctest::Approx(1.0f));
        CHECK(unpack_snorm16(vertex.normal[3]) == doctest::Approx(-1.0f));
    }

    TEST_CASE("gltf_importer - a mesh without normals or indices still imports with flat normals") {
        gltf_test_builder builder = {};
        mesh_input bare = {};
        bare.name = "bare";
        bare.positions = {math::vec3(0.0f), math::vec3(1.0f, 0.0f, 0.0f), math::vec3(0.0f, 1.0f, 0.0f)};
        builder.add_mesh(bare);
        node_input node = {};
        node.mesh = 0;
        builder.add_node(node);
        const gltf_importer importer = {};
        const gltf_import_result result = import_or_fail(importer, bytes_of(builder.build_embedded()));
        REQUIRE(result.meshes.size() == 1);
        CHECK(result.meshes[0].vertex_count() == 3);
        CHECK(result.meshes[0].index_count() == 3);
        CHECK(unpack_snorm16(result.meshes[0].streams().attributes[0].normal[2]) == doctest::Approx(1.0f));
        CHECK(result.materials.empty());
        CHECK(result.model.nodes()[0].materials[0] == INVALID_INDEX);
    }

    TEST_CASE("gltf_importer - .glb and external .bin files import the same scene as the embedded form") {
        const gltf_test_builder builder = scene_builder();
        const gltf_importer importer = {};
        const gltf_import_result embedded = import_or_fail(importer, bytes_of(builder.build_embedded()));
        const gltf_import_result glb = import_or_fail(importer, builder.build_glb());
        CHECK(glb.meshes.size() == embedded.meshes.size());
        CHECK(glb.meshes[0] == embedded.meshes[0]);
        CHECK(glb.meshes[1] == embedded.meshes[1]);
        CHECK(glb.materials[0] == embedded.materials[0]);
        CHECK(glb.textures[0] == embedded.textures[0]);
        CHECK(glb.model == embedded.model);

        temp_directory directory("gltf");
        const std::vector<std::byte> binary = builder.binary();
        static_cast<void>(directory.write_file("scene.bin", text_of(binary)));
        const std::filesystem::path path = directory.write_file("scene.gltf", builder.build_external("scene.bin"));
        core::result<gltf_import_result> fromFile = importer.import(path, {});
        CONJURE_REQUIRE_OK(fromFile);
        CHECK(fromFile.value().meshes[0] == embedded.meshes[0]);
        CHECK(fromFile.value().textures[1] == embedded.textures[1]);
        CHECK(fromFile.value().model.name() == "scene");
        CHECK(std::ranges::equal(fromFile.value().model.nodes(), embedded.model.nodes()));

        // The reader callback serves external files for import_bytes.
        const std::vector<std::byte> externalText = bytes_of(builder.build_external("scene.bin"));
        std::vector<std::string> requested = {};
        const gltf_file_reader reader = [&](std::string_view relativePath) -> core::result<std::vector<std::byte>> {
            requested.emplace_back(relativePath);
            if (relativePath == "scene.bin") {
                return binary;
            }
            return core::make_error(core::error_code::NOT_FOUND, "missing");
        };
        core::result<gltf_import_result> viaReader = importer.import_bytes(externalText, {}, reader);
        CONJURE_REQUIRE_OK(viaReader);
        CHECK(requested == std::vector<std::string>{"scene.bin"});
        CHECK(viaReader.value().meshes[1] == embedded.meshes[1]);
    }

    TEST_CASE(
      "gltf_importer - malformed input, missing buffers and unsupported topology fail with the documented codes"
    ) {
        const gltf_importer importer = {};
        CONJURE_CHECK_ERROR(importer.import_bytes({}, {}, nullptr), core::error_code::PARSE_FAILURE);
        CONJURE_CHECK_ERROR(
          importer.import_bytes(bytes_of("{ not json"), {}, nullptr),
          core::error_code::PARSE_FAILURE
        );
        CONJURE_CHECK_ERROR(
          importer.import_bytes(bytes_of(R"({"asset": {"version": "1.0"}})"), {}, nullptr),
          core::error_code::PARSE_FAILURE
        );

        const gltf_test_builder builder = scene_builder();
        // External buffer without a reader: NOT_FOUND.
        const std::vector<std::byte> external = bytes_of(builder.build_external("scene.bin"));
        CONJURE_CHECK_ERROR(importer.import_bytes(external, {}, nullptr), core::error_code::NOT_FOUND);
        const gltf_file_reader failing = [](std::string_view) -> core::result<std::vector<std::byte>> {
            return core::make_error(core::error_code::IO_FAILURE, "disk on fire");
        };
        CONJURE_CHECK_ERROR(importer.import_bytes(external, {}, failing), core::error_code::NOT_FOUND);
        temp_directory directory("gltf_fail");
        CONJURE_CHECK_ERROR(importer.import(directory.file("missing.gltf"), {}), core::error_code::NOT_FOUND);
        const std::filesystem::path orphan = directory.write_file("orphan.gltf", builder.build_external("absent.bin"));
        CONJURE_CHECK_ERROR(importer.import(orphan, {}), core::error_code::NOT_FOUND);

        // Lines are not supported.
        gltf_test_builder lines = {};
        mesh_input line = gltf_test_builder::quad();
        line.mode = 1;
        lines.add_mesh(line);
        node_input node = {};
        node.mesh = 0;
        lines.add_node(node);
        CONJURE_CHECK_ERROR(
          importer.import_bytes(bytes_of(lines.build_embedded()), {}, nullptr),
          core::error_code::UNSUPPORTED
        );

        // A truncated .glb is a parse failure.
        std::vector<std::byte> glb = builder.build_glb();
        glb.resize(glb.size() / 2);
        CONJURE_CHECK_ERROR(importer.import_bytes(glb, {}, nullptr), core::error_code::PARSE_FAILURE);
    }

    TEST_CASE("gltf_importer - malformed documents fail with PARSE_FAILURE instead of reading out of bounds") {
        const gltf_importer importer = {};
        const std::string triangleView = R"("bufferViews": [{"buffer": 0, "byteLength": 36}, )";
        const std::string meshAndScene = R"("meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}], )"
                                         R"("nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0)";

        SUBCASE("an image buffer view running past its buffer") {
            const std::string text = embedded_document(
              TRIANGLE_POSITIONS,
              triangleView + R"({"buffer": 0, "byteLength": 4096}], )" +
                R"("accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}], )" +
                R"("images": [{"bufferView": 1, "mimeType": "image/png"}], "textures": [{"source": 0}], )" +
                meshAndScene
            );
            const core::result<gltf_import_result> imported = importer.import_bytes(bytes_of(text), {}, nullptr);
            CONJURE_CHECK_ERROR(imported, core::error_code::PARSE_FAILURE);
            CHECK(failure_message(imported).find("invalid document") != std::string::npos);
        }
        SUBCASE("an accessor whose elements run past its buffer view") {
            const std::string text = embedded_document(
              TRIANGLE_POSITIONS,
              R"("bufferViews": [{"buffer": 0, "byteLength": 36}], )"
              R"("accessors": [{"bufferView": 0, "componentType": 5126, "count": 30, "type": "VEC3"}], )" +
                meshAndScene
            );
            const core::result<gltf_import_result> imported = importer.import_bytes(bytes_of(text), {}, nullptr);
            CONJURE_CHECK_ERROR(imported, core::error_code::PARSE_FAILURE);
            CHECK(failure_message(imported).find("invalid document") != std::string::npos);
        }
        SUBCASE("a byteStride large enough to wrap the accessor range check") {
            // 2^62 * 4 wraps to 0, so cgltf_validate alone sees a 12-byte accessor and the importer
            // then reads element i at 2^62 * i bytes into the view.
            const std::string text = embedded_document(
              TRIANGLE_POSITIONS,
              R"("bufferViews": [{"buffer": 0, "byteLength": 36, "byteStride": 4611686018427387904}], )"
              R"("accessors": [{"bufferView": 0, "componentType": 5126, "count": 5, "type": "VEC3"}], )" +
                meshAndScene
            );
            const core::result<gltf_import_result> imported = importer.import_bytes(bytes_of(text), {}, nullptr);
            CONJURE_CHECK_ERROR(imported, core::error_code::PARSE_FAILURE);
            CHECK(failure_message(imported).find("byteStride above 252") != std::string::npos);
        }
        SUBCASE("an index count large enough to wrap the accessor range check") {
            // 4 * (2^62 + 1) wraps to 4, so cgltf_validate alone accepts the accessor and then scans
            // 2^62 indices for their bound.
            std::vector<f32> values(TRIANGLE_POSITIONS.begin(), TRIANGLE_POSITIONS.end());
            values.resize(values.size() + 3, 0.0f);
            const std::string text = embedded_document(
              values,
              triangleView + R"({"buffer": 0, "byteOffset": 36, "byteLength": 12}], )" +
                R"("accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}, )" +
                R"({"bufferView": 1, "componentType": 5125, "count": 4611686018427387905, "type": "SCALAR"}], )" +
                R"("meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}], )" +
                R"("nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0)"
            );
            const core::result<gltf_import_result> imported = importer.import_bytes(bytes_of(text), {}, nullptr);
            CONJURE_CHECK_ERROR(imported, core::error_code::PARSE_FAILURE);
            CHECK(failure_message(imported).find("accessor 1 has more elements") != std::string::npos);
        }
        SUBCASE("a sparse count large enough to wrap the sparse range checks") {
            // The sparse indices wrap the same way, and cgltf_validate scans them for their bound.
            const std::string sparse = R"("sparse": {"count": 4611686018427387905, )"
                                       R"("indices": {"bufferView": 1, "componentType": 5125}, )"
                                       R"("values": {"bufferView": 2}})";
            const std::string text = embedded_document(
              TRIANGLE_POSITIONS,
              triangleView + R"({"buffer": 0, "byteLength": 12}, )" +
                R"({"buffer": 0, "byteOffset": 12, "byteLength": 24}], )" +
                R"("accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", )" + sparse +
                "}], " + meshAndScene
            );
            const core::result<gltf_import_result> imported = importer.import_bytes(bytes_of(text), {}, nullptr);
            CONJURE_CHECK_ERROR(imported, core::error_code::PARSE_FAILURE);
            CHECK(failure_message(imported).find("accessor 0 has more elements") != std::string::npos);
        }
        SUBCASE("two nodes listing each other as children") {
            // Without a scene every parentless node is a root: the cycle has none, so only the
            // validation sees it.
            const std::string nodes = R"({"asset": {"version": "2.0"}, )"
                                      R"("nodes": [{"children": [1]}, {"children": [0]}])";
            const std::string cycle = nodes + "}";
            const core::result<gltf_import_result> imported = importer.import_bytes(bytes_of(cycle), {}, nullptr);
            CONJURE_CHECK_ERROR(imported, core::error_code::PARSE_FAILURE);
            CHECK(failure_message(imported).find("invalid document") != std::string::npos);
            const std::string listed = nodes + R"(, "scenes": [{"nodes": [0]}], "scene": 0})";
            CONJURE_CHECK_ERROR(importer.import_bytes(bytes_of(listed), {}, nullptr), core::error_code::PARSE_FAILURE);
        }
        // The documents the builder writes are valid and still import.
        const gltf_test_builder builder = scene_builder();
        CONJURE_CHECK_OK(importer.import_bytes(bytes_of(builder.build_embedded()), {}, nullptr));
        CONJURE_CHECK_OK(importer.import_bytes(builder.build_glb(), {}, nullptr));
    }

    TEST_CASE("regression - a node hierarchy thousands of levels deep imports without exhausting the stack") {
        // Every node is the only child of the previous one; the old recursive walk used one call
        // frame per level and overflowed the stack long before the end of the chain.
        constexpr u32 DEPTH = 10000;
        std::string nodes = {};
        for (u32 index = 0; index + 1 < DEPTH; ++index) {
            nodes += R"({"children": [)" + std::to_string(index + 1) + "]}, ";
        }
        nodes += "{}";
        const std::string text = R"({"asset": {"version": "2.0"}, "nodes": [)" + nodes +
          R"(], "scenes": [{"nodes": [0]}], "scene": 0})";
        const gltf_importer importer = {};
        const gltf_import_result result = import_or_fail(importer, bytes_of(text));
        REQUIRE(result.model.nodes().size() == DEPTH);
        CHECK(result.model.root_nodes() == std::vector<u32>{0});
        u32 misplaced = 0;
        for (u32 index = 1; index < DEPTH; ++index) {
            if (result.model.nodes()[index].parent != index - 1) {
                ++misplaced;
            }
        }
        CHECK(misplaced == 0);
    }

    TEST_CASE(
      "gltf_importer - a COLOR_0 accessor that is not VEC3 or VEC4 fails the import instead of reading out of bounds"
    ) {
        const gltf_importer importer = {};
        const gltf_import_result valid = import_or_fail(importer, bytes_of(coloured_triangle("VEC3", 3)));
        REQUIRE(valid.meshes.size() == 1);
        CHECK(valid.meshes[0].streams().attributes[0].colour == pack_rgba8(math::vec4(0.5f, 0.5f, 0.5f, 1.0f)));
        const std::array<std::pair<std::string_view, usize>, 2> narrow = {
          std::pair<std::string_view, usize>{"SCALAR", 1},
          std::pair<std::string_view, usize>{"VEC2", 2},
        };
        for (const auto &[type, components] : narrow) {
            CAPTURE(type);
            const core::result<gltf_import_result> imported = importer.import_bytes(
              bytes_of(coloured_triangle(type, components)),
              {},
              nullptr
            );
            CONJURE_CHECK_ERROR(imported, core::error_code::PARSE_FAILURE);
            CHECK(failure_message(imported).find("COLOR_0") != std::string::npos);
        }
    }

    TEST_CASE("gltf_importer - import refuses buffer URIs outside the model's directory") {
        const gltf_test_builder builder = scene_builder();
        const std::string binary = text_of(builder.binary());
        temp_directory directory("gltf_escape");
        const std::filesystem::path secret = directory.write_file("secret.bin", binary);
        const gltf_importer importer = {};

        const std::filesystem::path climbing = directory.write_file(
          "sub/climbing.gltf",
          builder.build_external("../secret.bin")
        );
        const core::result<gltf_import_result> climbed = importer.import(climbing, {});
        CONJURE_CHECK_ERROR(climbed, core::error_code::NOT_FOUND);
        CHECK(failure_message(climbed).find("outside the model's directory") != std::string::npos);

        const std::u8string absoluteUri = secret.generic_u8string();
        const std::filesystem::path absolute = directory.write_file(
          "sub/absolute.gltf",
          builder.build_external(std::string(reinterpret_cast<const char *>(absoluteUri.data()), absoluteUri.size()))
        );
        const core::result<gltf_import_result> rooted = importer.import(absolute, {});
        CONJURE_CHECK_ERROR(rooted, core::error_code::NOT_FOUND);
        CHECK(failure_message(rooted).find("outside the model's directory") != std::string::npos);

        // A '..' that stays inside the directory and a sub-directory are fine.
        static_cast<void>(directory.write_file("sub/bin/data.bin", binary));
        const std::filesystem::path nested = directory.write_file(
          "sub/nested.gltf",
          builder.build_external("bin/../bin/data.bin")
        );
        core::result<gltf_import_result> imported = importer.import(nested, {});
        CONJURE_REQUIRE_OK(imported);
        CHECK(imported.value().meshes.size() == 2);
        CHECK(imported.value().model.name() == "nested");
    }

    TEST_CASE("gltf_import_options - JSON round-trips, defaults apply and wrong types fail") {
        gltf_import_options options = {};
        options.generateTangents = false;
        options.scale = 0.01f;
        options.importCameras = true;
        options.importLights = false;
        const core::json::document document = gltf_import_options_to_document(options);
        const core::json::value root = document.root();
        CHECK_FALSE(root["generateTangents"].as_bool(true));
        CHECK(root["scale"].as_f64() == doctest::Approx(0.01));
        CHECK(root["importCameras"].as_bool());
        CHECK_FALSE(root["importLights"].as_bool(true));
        core::result<gltf_import_options> restored = gltf_import_options_from_json(root);
        CONJURE_REQUIRE_OK(restored);
        CHECK(restored.value() == options);
        core::result<gltf_import_options> defaults = gltf_import_options_from_json(
          core::json::document::object().root()
        );
        CONJURE_REQUIRE_OK(defaults);
        CHECK(defaults.value() == gltf_import_options());
        core::result<gltf_import_options> absent = gltf_import_options_from_json(core::json::value());
        CONJURE_REQUIRE_OK(absent);
        CHECK(absent.value() == gltf_import_options());
        CONJURE_CHECK_ERROR(
          gltf_import_options_from_json(core::json::object({{"scale", "big"}}).root()),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          gltf_import_options_from_json(core::json::object({{"scale", -1}}).root()),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          gltf_import_options_from_json(core::json::object({{"importLights", 1}}).root()),
          core::error_code::INVALID_ARGUMENT
        );
        const core::result<core::json::document> array = core::json::document::parse("[]");
        CONJURE_REQUIRE_OK(array);
        CONJURE_CHECK_ERROR(gltf_import_options_from_json(array.value().root()), core::error_code::INVALID_ARGUMENT);
    }

    TEST_CASE(
      "gltf_asset_importer - the model is the primary asset and meshes, materials and textures are sub-assets"
    ) {
        const gltf_asset_importer importer = {};
        CHECK(importer.id() == GLTF_IMPORTER_ID);
        CHECK(importer.version() == GLTF_IMPORTER_VERSION);
        REQUIRE(importer.extensions().size() == 2);
        CHECK(importer.extensions()[0] == ".gltf");
        CHECK(importer.extensions()[1] == ".glb");
        CHECK(importer.default_settings().root()["generateTangents"].as_bool());

        const gltf_test_builder builder = scene_builder();
        core::vfs::memory_file_system fileSystem = {};
        const std::vector<std::byte> bytes = bytes_of(builder.build_embedded());
        assets::sub_asset_table subAssets = {};
        assets::import_context context(
          fileSystem,
          "assets://models/scene.gltf",
          "assets/models/scene.gltf",
          bytes,
          {},
          "windows",
          renderer_support::test_asset_id(20),
          subAssets,
          {}
        );
        core::result<assets::import_output> imported = importer.import(context);
        CONJURE_REQUIRE_OK(imported);
        const std::vector<assets::imported_asset> &assets = imported.value().assets;
        REQUIRE(assets.size() == 1 + 2 + 2 + 2);
        const assets::imported_asset *primary = nullptr;
        std::vector<std::string> subPaths = {};
        for (const assets::imported_asset &asset : assets) {
            if (asset.subPath.empty()) {
                primary = &asset;
            } else {
                subPaths.push_back(asset.subPath);
            }
        }
        REQUIRE(primary != nullptr);
        CHECK(primary->type == assets::asset_type_traits<model_asset>::id());
        CHECK(primary->displayName == "scene");
        std::ranges::sort(subPaths);
        CHECK(
          subPaths == std::vector<std::string>{"material/0", "material/1", "mesh/0", "mesh/1", "texture/0", "texture/1"}
        );

        // Sub-asset ids are stable: a second import hands out the same ids.
        core::result<model_asset> model = model_asset::parse_blob(primary->payload);
        CONJURE_REQUIRE_OK(model);
        REQUIRE(model.value().meshes().size() == 2);
        REQUIRE(model.value().materials().size() == 2);
        CHECK(model.value().meshes()[0] == subAssets.id_for("mesh/0"));
        CHECK(model.value().materials()[1] == subAssets.id_for("material/1"));
        CHECK(primary->dependencies.size() == 4);
        CHECK(std::ranges::is_sorted(primary->dependencies));
        CHECK(std::ranges::find(primary->dependencies, subAssets.id_for("mesh/1")) != primary->dependencies.end());

        // The painted material depends on its two textures; the glass one on none.
        for (const assets::imported_asset &asset : assets) {
            if (asset.subPath == "material/0") {
                CHECK(asset.type == assets::asset_type_traits<material_asset>::id());
                CHECK(asset.displayName == "painted");
                REQUIRE(asset.dependencies.size() == 2);
                CHECK(std::ranges::find(asset.dependencies, subAssets.id_for("texture/0")) != asset.dependencies.end());
                core::result<material_asset> material = material_asset::parse_blob(asset.payload);
                CONJURE_REQUIRE_OK(material);
                CHECK(material.value().desc().textures[0].id() == subAssets.id_for("texture/0"));
                CHECK(material.value().desc().textures[2].id() == subAssets.id_for("texture/1"));
            } else if (asset.subPath == "material/1") {
                CHECK(asset.dependencies.empty());
            } else if (asset.subPath == "mesh/0") {
                CHECK(asset.type == assets::asset_type_traits<mesh_asset>::id());
                CHECK(asset.displayName == "cube");
                core::result<mesh_asset> mesh = mesh_asset::parse_blob(asset.payload);
                CONJURE_REQUIRE_OK(mesh);
                CHECK(mesh.value().vertex_count() == 24);
            } else if (asset.subPath == "texture/1") {
                CHECK(asset.type == assets::asset_type_traits<texture_asset>::id());
                core::result<texture_asset> texture = texture_asset::parse_blob(asset.payload);
                CONJURE_REQUIRE_OK(texture);
                CHECK(texture.value().colour_space() == texture_colour_space::LINEAR);
            }
        }

        core::result<assets::import_output> again = importer.import(context);
        CONJURE_REQUIRE_OK(again);
        CHECK(again.value().assets.size() == assets.size());
        CHECK(again.value().assets[0].payload == assets[0].payload);

        // Warnings reach the context; bad settings and bad files fail.
        gltf_test_builder warned = scene_builder();
        warned.add_skin_and_animation();
        const std::vector<std::byte> warnedBytes = bytes_of(warned.build_embedded());
        assets::import_context warnedContext(
          fileSystem,
          "assets://models/w.gltf",
          "assets/models/w.gltf",
          warnedBytes,
          {},
          "windows",
          renderer_support::test_asset_id(21),
          subAssets,
          {}
        );
        CONJURE_REQUIRE_OK(importer.import(warnedContext));
        CHECK(warnedContext.warnings().size() == 2);
        const core::json::document badSettings = core::json::object({{"scale", "x"}});
        assets::import_context badSettingsContext(
          fileSystem,
          "assets://models/s.gltf",
          "assets/models/s.gltf",
          bytes,
          badSettings.root(),
          "windows",
          renderer_support::test_asset_id(22),
          subAssets,
          {}
        );
        CONJURE_CHECK_ERROR(importer.import(badSettingsContext), assets::asset_error_code::IMPORT_FAILED);
        const std::vector<std::byte> garbage = bytes_of("{");
        assets::import_context garbageContext(
          fileSystem,
          "assets://models/g.gltf",
          "assets/models/g.gltf",
          garbage,
          {},
          "windows",
          renderer_support::test_asset_id(23),
          subAssets,
          {}
        );
        CONJURE_CHECK_ERROR(importer.import(garbageContext), assets::asset_error_code::IMPORT_FAILED);

        // External buffers are read as side files next to the source.
        CONJURE_REQUIRE_OK(fileSystem.write_file("assets/models/ext.bin", builder.binary()));
        const std::vector<std::byte> externalBytes = bytes_of(builder.build_external("ext.bin"));
        assets::import_context externalContext(
          fileSystem,
          "assets://models/ext.gltf",
          "assets/models/ext.gltf",
          externalBytes,
          {},
          "windows",
          renderer_support::test_asset_id(24),
          subAssets,
          {}
        );
        CONJURE_REQUIRE_OK(importer.import(externalContext));
        REQUIRE(externalContext.side_files().size() == 1);
        CHECK(externalContext.side_files()[0] == "assets://models/ext.bin");
    }
}
