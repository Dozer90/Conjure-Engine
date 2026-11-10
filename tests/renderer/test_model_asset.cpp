#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "conjure/testing/assert_capture.h"
#include "renderer_test_support.h"

#include "conjure/renderer/limits.h"
#include "conjure/renderer/model_asset.h"

#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_payload.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// model_asset.h (docs/design/02_renderer.md 4.2): the glTF node tree with mesh / material indices
// into the id lists, its canonical JSON, hierarchy queries and the loader without a fallback.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    [[nodiscard]] model_asset sample_model() {
        model_asset model = {};
        model.set_name("robot");
        const u32 body = model.add_mesh(core::uuid(1, 1));
        const u32 head = model.add_mesh(core::uuid(1, 2));
        const u32 skin = model.add_material(core::uuid(2, 1));
        model_node root = {};
        root.name = "root";
        root.localTransform.position = math::vec3(1.0f, 0.0f, 0.0f);
        root.localTransform.scale = math::vec3(2.0f);
        CHECK(model.add_node(root) == 0);
        model_node bodyNode = {};
        bodyNode.name = "body";
        bodyNode.parent = 0;
        bodyNode.mesh = body;
        bodyNode.materials[0] = skin;
        bodyNode.localTransform.position = math::vec3(0.0f, 1.0f, 0.0f);
        CHECK(model.add_node(bodyNode) == 1);
        model_node headNode = {};
        headNode.name = "head";
        headNode.parent = 1;
        headNode.mesh = head;
        headNode.localTransform.position = math::vec3(0.0f, 0.5f, 0.0f);
        headNode.localTransform.rotation = glm::angleAxis(math::HALF_PI, math::vec3(0.0f, 1.0f, 0.0f));
        CHECK(model.add_node(headNode) == 2);
        model_node lamp = {};
        lamp.name = "lamp";
        lamp.parent = 0;
        lamp.hasLight = true;
        lamp.lightData.type = light_type::SPOT;
        lamp.lightData.intensity = 300.0f;
        lamp.lightData.castsShadows = true;
        lamp.hasCamera = true;
        lamp.cameraData.projection = projection_type::ORTHOGRAPHIC;
        lamp.cameraData.orthographicHeight = 4.0f;
        CHECK(model.add_node(lamp) == 3);
        model_node loose = {};
        loose.name = "loose";
        CHECK(model.add_node(loose) == 4);
        return model;
    }

    [[nodiscard]] std::vector<std::byte> bytes_of(std::string_view text) {
        std::vector<std::byte> bytes = {};
        for (const char character : text) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        return bytes;
    }
}

TEST_SUITE("renderer/model_asset") {

    TEST_CASE("model_node - the default node is a root without mesh, materials, light or camera") {
        const model_node node = {};
        CHECK(node.name.empty());
        CHECK(node.parent == INVALID_INDEX);
        CHECK(node.mesh == INVALID_INDEX);
        for (const u32 material : node.materials) {
            CHECK(material == INVALID_INDEX);
        }
        CHECK_FALSE(node.hasLight);
        CHECK_FALSE(node.hasCamera);
        CHECK(node.localTransform == math::transform());
        CHECK(node == model_node());
    }

    TEST_CASE("model_asset - add_node, add_mesh and add_material append and the queries walk the tree") {
        const model_asset model = sample_model();
        CHECK(model.name() == "robot");
        CHECK(model.nodes().size() == 5);
        CHECK(model.meshes().size() == 2);
        CHECK(model.materials().size() == 1);
        CHECK(model.meshes()[1] == core::uuid(1, 2));
        CHECK(model.materials()[0] == core::uuid(2, 1));
        CHECK(model.root_nodes() == std::vector<u32>{0, 4});
        CHECK(model.children_of(0) == std::vector<u32>{1, 3});
        CHECK(model.children_of(1) == std::vector<u32>{2});
        CHECK(model.children_of(2).empty());
        CHECK(model.children_of(99).empty());
        // World transforms compose parent first: root scale 2 doubles the body offset.
        const math::transform body = model.world_transform(1);
        CONJURE_CHECK_APPROX(body.position, math::vec3(1.0f, 2.0f, 0.0f));
        CONJURE_CHECK_APPROX(body.scale, math::vec3(2.0f));
        const math::transform head = model.world_transform(2);
        CONJURE_CHECK_APPROX(head.position, math::vec3(1.0f, 3.0f, 0.0f));
        CHECK(math::absolute(head.rotation.y) == doctest::Approx(std::sin(math::HALF_PI * 0.5f)).epsilon(1.0e-3));
        CHECK(model.world_transform(4) == math::transform());
        CHECK(model.world_transform(99) == math::transform());
    }

    TEST_CASE("model_asset - add_node refuses a parent that does not exist yet") {
        model_asset model = {};
        model_node orphan = {};
        orphan.parent = 3;
        u32 index = 0;
        CONJURE_CHECK_ASSERTS(index = model.add_node(orphan));
        CHECK(index == INVALID_INDEX);
        CHECK(model.nodes().empty());
        model_node self = {};
        self.parent = 0;
        CONJURE_CHECK_ASSERTS(index = model.add_node(self));
        CHECK(index == INVALID_INDEX);
    }

    TEST_CASE("model_asset - to_json is canonical and parse_json restores the tree") {
        const model_asset model = sample_model();
        const std::string json = model.to_json();
        CHECK(json == model.to_json());
        CHECK(json.find("\"version\"") != std::string::npos);
        CHECK(json.find("\"spot\"") != std::string::npos);
        CHECK(json.find("\"orthographic\"") != std::string::npos);
        CHECK(json.find(core::uuid(1, 2).to_string()) != std::string::npos);
        core::result<model_asset> parsed = model_asset::parse_json(json);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value() == model);
        CHECK(parsed.value().name() == "robot");
        CHECK(parsed.value().nodes()[3].hasLight);
        CHECK(parsed.value().nodes()[3].lightData.type == light_type::SPOT);
        CHECK(parsed.value().nodes()[3].lightData.intensity == doctest::Approx(300.0f));
        CHECK(parsed.value().nodes()[3].lightData.castsShadows);
        CHECK(parsed.value().nodes()[3].hasCamera);
        CHECK(parsed.value().nodes()[3].cameraData.projection == projection_type::ORTHOGRAPHIC);
        CHECK(parsed.value().nodes()[1].materials[0] == 0);
        CHECK(parsed.value().nodes()[1].materials[1] == INVALID_INDEX);
        CHECK(parsed.value().nodes()[2].parent == 1);
        CHECK(approx_equal(parsed.value().nodes()[2].localTransform, model.nodes()[2].localTransform, 1.0e-4f));
        core::result<model_asset> fromBlob = model_asset::parse_blob(model.write_blob());
        CONJURE_REQUIRE_OK(fromBlob);
        CHECK(fromBlob.value() == model);
        CHECK(model.write_blob().size() == json.size());
    }

    TEST_CASE("model_asset - parse_json accepts a minimal document and rejects malformed ones") {
        core::result<model_asset> empty = model_asset::parse_json("{}");
        CONJURE_REQUIRE_OK(empty);
        CHECK(empty.value().nodes().empty());
        core::result<model_asset> minimal = model_asset::parse_json(
          R"({"nodes": [{"name": "a"}, {"name": "b", "parent": 0}]})"
        );
        CONJURE_REQUIRE_OK(minimal);
        CHECK(minimal.value().nodes().size() == 2);
        CHECK(minimal.value().nodes()[1].parent == 0);
        CHECK(minimal.value().root_nodes() == std::vector<u32>{0});

        CONJURE_CHECK_ERROR(model_asset::parse_json("{"), core::error_code::PARSE_FAILURE);
        CONJURE_CHECK_ERROR(model_asset::parse_json("7"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(model_asset::parse_json(R"({"version": 99})"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(model_asset::parse_json(R"({"nodes": {}})"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(model_asset::parse_json(R"({"nodes": [5]})"), core::error_code::INVALID_ARGUMENT);
        // The list is parent-first: a parent index at or after the node fails.
        CONJURE_CHECK_ERROR(
          model_asset::parse_json(R"({"nodes": [{"parent": 0}]})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          model_asset::parse_json(R"({"nodes": [{"parent": 1}, {}]})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          model_asset::parse_json(R"({"nodes": [{"parent": "root"}]})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(model_asset::parse_json(R"({"nodes": [{"mesh": 0}]})"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(
          model_asset::parse_json(R"({"materials": [], "nodes": [{"materials": [0]}]})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          model_asset::parse_json(R"({"nodes": [{"materials": "none"}]})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          model_asset::parse_json(R"({"nodes": [{"transform": 3}]})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(model_asset::parse_json(R"({"meshes": "x"})"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(
          model_asset::parse_json(R"({"meshes": ["not-a-uuid"]})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          model_asset::parse_json(R"({"nodes": [{"light": {"type": "laser"}}]})"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          model_asset::parse_json(R"({"nodes": [{"camera": {"projection": "fisheye"}}]})"),
          core::error_code::INVALID_ARGUMENT
        );
    }

    TEST_CASE("model_loader - loads the JSON payload and has no fallback") {
        const model_loader loader = {};
        CHECK(loader.type() == assets::asset_type_traits<model_asset>::id());
        const model_asset source = sample_model();
        const std::vector<std::byte> blob = source.write_blob();
        assets::load_context context = {};
        context.id = renderer_support::test_asset_id(11);
        context.type = loader.type();
        core::result<assets::asset_payload> loaded = loader.load_cpu(context, blob);
        CONJURE_REQUIRE_OK(loaded);
        REQUIRE(assets::payload_as<model_asset>(loaded.value()) != nullptr);
        CHECK(*assets::payload_as<model_asset>(loaded.value()) == source);
        core::result<assets::asset_payload> finalised = loader.finalise(
          assets::finalise_context{},
          std::move(loaded).value()
        );
        CONJURE_REQUIRE_OK(finalised);
        CHECK(*assets::payload_as<model_asset>(finalised.value()) == source);
        CONJURE_CHECK_ERROR(loader.load_cpu(context, bytes_of("{")), core::error_code::PARSE_FAILURE);
        core::result<assets::asset_payload> fallback = loader.create_fallback();
        CONJURE_REQUIRE_OK(fallback);
        CHECK(fallback.value().get() == nullptr);
        CHECK(MODEL_ASSET_TYPE_NAME == "conjure.renderer.model");
        CHECK(MODEL_FORMAT_VERSION == 1);
    }
}
