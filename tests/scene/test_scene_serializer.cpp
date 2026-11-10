#include "conjure/testing/doctest.h"
#include "conjure/testing/approx.h"
#include "conjure/testing/log_capture.h"
#include "scene_test_fixture.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/vfs/memory_file_system.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/serialization.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_error.h"
#include "conjure/scene/scene_serializer.h"

#include <string>
#include <vector>

using namespace conjure;
using namespace scene_test;
using scene::scene_error;

namespace {

    /// <summary>
    /// A script-like component (R29): non-trivial (its own copy and destroy ops carry the text) with an
    /// entity reference the archive remaps. The ECS exposes no public route for custom serializer hooks
    /// yet, so the reflection-driven serializer stands in for script_component's own hooks.
    /// </summary>
    struct scripted {
        std::string text = {};
        ecs::entity target = {};

        [[nodiscard]] bool operator==(const scripted &) const = default;
    };

    [[nodiscard]] std::string save_text(const scene::scene &source) {
        core::json::writer out = {};
        REQUIRE(scene::scene_serializer::save_scene(source, out).has_value());
        return out.finish();
    }

    [[nodiscard]] core::result<scene::instantiate_report> load_text(
      scene_fixture &fixture,
      const std::string &text,
      scene::guid_policy guids = scene::guid_policy::KEEP
    ) {
        const core::result<core::json::document> parsed = core::json::document::parse(text);
        REQUIRE(parsed.has_value());
        CONJURE_TRY_ASSIGN(
          const scene::scene_document document,
          scene::scene_serializer::parse_document(parsed.value().root(), fixture.registry)
        );
        // What a REPLACE load does before instantiating: the file's name and settings become the scene's.
        fixture.active().set_name(document.name);
        fixture.active().set_settings(document.settings);
        return scene::scene_serializer::load_scene(fixture.active(), document, scene::SCENE_ID_AUTHORED, guids);
    }
}

CONJURE_REFLECT_STRUCT(scripted, "test.scripted", CONJURE_FIELD(text), CONJURE_FIELD(target));
CONJURE_COMPONENT(scripted, "test.scripted");

TEST_SUITE("scene/scene_serializer") {

    TEST_CASE("scene_serializer - header keys in order") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        target.set_name("main");
        core::json::document settings = core::json::document::object();
        settings.set("skybox", "none");
        settings.set("ambient", 0.5);
        target.set_settings(settings);
        const ecs::entity root = fixture.create("Root");
        const std::string text = save_text(target);
        const std::string expectedStart = "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"main\","
          "\"settings\":{\"ambient\":0.5,\"skybox\":\"none\"},\"roots\":[\"" + target.guid_of(root).value().to_string()
          + "\"],\"archive\":{\"entities\":[";
        CHECK(text.substr(0, expectedStart.size()) == expectedStart);
        CHECK(text.back() == '}');
        // An empty scene writes an empty roots array and an empty entity list.
        target.clear();
        const std::string empty = save_text(target);
        CHECK(empty == "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"main\","
          "\"settings\":{\"ambient\":0.5,\"skybox\":\"none\"},\"roots\":[],\"archive\":{\"entities\":[]}}");
    }

    TEST_CASE("scene_serializer - bad format and newer version rejected before creating entities") {
        scene_fixture fixture = {};
        const std::string wrongFormat = "{\"format\":\"conjure.prefab\",\"version\":1,\"name\":\"x\",\"settings\":{},"
          "\"roots\":[],\"archive\":{\"entities\":[{\"components\":{\"conjure.scene.name\":{\"value\":\"a\"}}}]}}";
        CONJURE_CHECK_ERROR(load_text(fixture, wrongFormat), scene_error::BAD_FORMAT);
        const std::string noFormat = "{\"version\":1,\"archive\":{\"entities\":[]}}";
        CONJURE_CHECK_ERROR(load_text(fixture, noFormat), scene_error::BAD_FORMAT);
        const std::string newer = "{\"format\":\"conjure.scene\",\"version\":2,\"name\":\"x\",\"settings\":{},"
          "\"roots\":[],\"archive\":{\"entities\":[{\"components\":{\"conjure.scene.name\":{\"value\":\"a\"}}}]}}";
        CONJURE_CHECK_ERROR(load_text(fixture, newer), scene_error::UNSUPPORTED_VERSION);
        const std::string notObject = "[1, 2, 3]";
        CONJURE_CHECK_ERROR(load_text(fixture, notObject), scene_error::BAD_FORMAT);
        const std::string noArchive = "{\"format\":\"conjure.scene\",\"version\":1}";
        CONJURE_CHECK_ERROR(load_text(fixture, noArchive), scene_error::BAD_FORMAT);
        const std::string badRoots =
          "{\"format\":\"conjure.scene\",\"version\":1,\"roots\":[\"nope\"],\"archive\":{\"entities\":[]}}";
        CONJURE_CHECK_ERROR(load_text(fixture, badRoots), scene_error::BAD_FORMAT);
        const std::string badSettings =
          "{\"format\":\"conjure.scene\",\"version\":1,\"settings\":3,\"archive\":{\"entities\":[]}}";
        CONJURE_CHECK_ERROR(load_text(fixture, badSettings), scene_error::BAD_FORMAT);
        // Malformed component JSON fails the whole load through the ECS.
        const std::string malformed = "{\"format\":\"conjure.scene\",\"version\":1,\"archive\":{\"entities\":["
          "{\"components\":{\"conjure.scene.layer\":{\"index\":\"not a number\"}}}]}}";
        CONJURE_CHECK_ERROR(load_text(fixture, malformed), ecs::ecs_error::SERIALIZATION_FAILED);
        CHECK(fixture.world.entity_count() == 0);
        CHECK(fixture.active().roots().empty());
        CHECK(fixture.active().guids().size() == 0);
    }

    TEST_CASE("scene_serializer - an entity without components loads as a plain root") {
        scene_fixture fixture = {};
        const std::string emptyEntity = "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"plain\","
          "\"settings\":{},\"roots\":[],\"archive\":{\"entities\":[{\"components\":{}},"
          "{\"components\":{\"conjure.scene.name\":{\"value\":\"named\"}}}]}}";
        const core::result<scene::instantiate_report> loaded = load_text(fixture, emptyEntity);
        CONJURE_REQUIRE_OK(loaded);
        CHECK(loaded.value().entityCount == 2);
        REQUIRE(loaded.value().entities.size() == 2);
        const ecs::entity plain = loaded.value().entities[0];
        CHECK(fixture.world.is_alive(plain));
        // Linked as a root like any other entry (dirty tag, root list) but given nothing of its own.
        CHECK_FALSE(fixture.world.has<scene::parent>(plain));
        CHECK_FALSE(fixture.world.has<scene::name>(plain));
        CHECK_FALSE(fixture.world.has<scene::guid>(plain));
        CHECK_FALSE(fixture.world.has<scene::local_transform>(plain));
        CHECK(fixture.active().tree().depth_of(plain) == 0);
        REQUIRE(fixture.active().roots().size() == 2);
        CHECK(fixture.active().roots()[0].entity == plain);
        CHECK(fixture.active().find_path("named") == loaded.value().entities[1]);
        CONJURE_CHECK_OK(fixture.active().validate());
    }

    TEST_CASE("scene_serializer - parent loop in a file is HIERARCHY_CYCLE") {
        scene_fixture fixture = {};
        const ecs::entity keep = fixture.create("keep");
        // Entry 0 names entry 1 as its parent and entry 1 names entry 0: no entry is reachable from a root.
        const std::string loop = "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"loop\",\"settings\":{},"
          "\"roots\":[],\"archive\":{\"entities\":["
          "{\"components\":{\"conjure.scene.name\":{\"value\":\"a\"},"
          "\"conjure.scene.parent\":{\"value\":1,\"siblingIndex\":0}}},"
          "{\"components\":{\"conjure.scene.name\":{\"value\":\"b\"},"
          "\"conjure.scene.parent\":{\"value\":0,\"siblingIndex\":0}}}]}}";
        CONJURE_CHECK_ERROR(load_text(fixture, loop), scene_error::HIERARCHY_CYCLE);
        CHECK(fixture.world.entity_count() == 1);
        CHECK(fixture.active().contains(keep));
        CHECK(fixture.active().roots().size() == 1);
        // A chain below a root that reaches depth 32 is refused the same way, before anything exists.
        std::string deep = "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"deep\",\"settings\":{},"
          "\"roots\":[],\"archive\":{\"entities\":[{\"components\":{\"conjure.scene.name\":{\"value\":\"r\"}}}";
        for (u32 level = 1; level <= scene::MAX_HIERARCHY_DEPTH; ++level) {
            deep += ",{\"components\":{\"conjure.scene.parent\":{\"value\":" + std::to_string(level - 1)
              + ",\"siblingIndex\":0}}}";
        }
        deep += "]}}";
        CONJURE_CHECK_ERROR(load_text(fixture, deep), scene_error::HIERARCHY_TOO_DEEP);
        CHECK(fixture.world.entity_count() == 1);
        CHECK(fixture.active().guids().size() == 1);
    }

    TEST_CASE("scene_serializer - roots order from header") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity first = fixture.create("first");
        const ecs::entity second = fixture.create("second");
        const ecs::entity third = fixture.create("third");
        CONJURE_REQUIRE_OK(target.set_root_order(std::vector<ecs::entity>{third, first, second}));
        REQUIRE(target.roots().size() == 3);
        CHECK(target.roots()[0].entity == third);
        const std::string text = save_text(target);
        const core::uuid thirdGuid = target.guid_of(third).value();

        scene_fixture other = {};
        CONJURE_REQUIRE_OK(load_text(other, text));
        REQUIRE(other.active().roots().size() == 3);
        CHECK(other.active().name_of(other.active().roots()[0].entity).view() == "third");
        CHECK(other.active().name_of(other.active().roots()[1].entity).view() == "first");
        CHECK(other.active().name_of(other.active().roots()[2].entity).view() == "second");
        CHECK(other.active().guid_of(other.active().roots()[0].entity).value() == thirdGuid);
        // The header alone decides: a different header order is honoured over the archive order.
        const std::string reordered = [&]() {
            const std::string firstGuid = target.guid_of(first).value().to_string();
            const std::string secondGuid = target.guid_of(second).value().to_string();
            const std::string rootsKey = "\"roots\":[";
            const usize begin = text.find(rootsKey) + rootsKey.size();
            const usize end = text.find(']', begin);
            const std::string list = "\"" + secondGuid + "\",\"" + firstGuid + "\",\"" + thirdGuid.to_string() + "\"";
            return text.substr(0, begin) + list + text.substr(end);
        }();
        scene_fixture third_fixture = {};
        CONJURE_REQUIRE_OK(load_text(third_fixture, reordered));
        CHECK(third_fixture.active().name_of(third_fixture.active().roots()[0].entity).view() == "second");
        CHECK(third_fixture.active().name_of(third_fixture.active().roots()[1].entity).view() == "first");
        CHECK(third_fixture.active().name_of(third_fixture.active().roots()[2].entity).view() == "third");
    }

    TEST_CASE("scene_serializer - missing root listed appended with WARN") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity first = fixture.create("first");
        const ecs::entity second = fixture.create("second");
        const std::string text = save_text(target);
        // Drop the second root from the header and add a guid that no entity carries.
        const std::string secondGuid = target.guid_of(second).value().to_string();
        const std::string edited = [&]() {
            const std::string rootsKey = "\"roots\":[";
            const usize begin = text.find(rootsKey) + rootsKey.size();
            const usize end = text.find(']', begin);
            return text.substr(0, begin) + "\"" + target.guid_of(first).value().to_string() + "\",\""
              + test_uuid(99).to_string() + "\"" + text.substr(end);
        }();
        CHECK(edited.find(secondGuid) != std::string::npos);

        scene_fixture other = {};
        testing::log_capture logs = {};
        CONJURE_REQUIRE_OK(load_text(other, edited));
        REQUIRE(other.active().roots().size() == 2);
        CHECK(other.active().name_of(other.active().roots()[0].entity).view() == "first");
        CHECK(other.active().name_of(other.active().roots()[1].entity).view() == "second");
        CHECK(logs.count(core::log_level::WARN) == 2);
        CHECK(logs.contains(core::log_level::WARN, "skipped"));
        CHECK(logs.contains(core::log_level::WARN, "appended"));
    }

    TEST_CASE("scene_serializer - parent local index remapped") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        const ecs::entity grand = fixture.create("grand", child);
        CONJURE_REQUIRE_OK(target.set_local_transform(child, at(1.0f, 2.0f, 3.0f)));
        const std::string text = save_text(target);
        // Entities are written in hierarchy order: root at 0, child points at 0, grand at 1.
        CHECK(text.find("\"conjure.scene.parent\":{\"value\":0,\"siblingIndex\":0}") != std::string::npos);
        CHECK(text.find("\"conjure.scene.parent\":{\"value\":1,\"siblingIndex\":0}") != std::string::npos);
        CHECK(text.find("\"conjure.scene.children\"") == std::string::npos);
        CHECK(text.find("\"conjure.scene.world_transform\"") == std::string::npos);
        CHECK(text.find("\"conjure.scene.depth_01\"") == std::string::npos);
        CONJURE_UNUSED(grand);

        scene_fixture other = {};
        CONJURE_REQUIRE_OK(load_text(other, text));
        const scene::scene &loaded = other.active();
        REQUIRE(loaded.roots().size() == 1);
        const ecs::entity newRoot = loaded.roots()[0].entity;
        REQUIRE(loaded.tree().children_of(newRoot).size() == 1);
        const ecs::entity newChild = loaded.tree().children_of(newRoot)[0];
        REQUIRE(loaded.tree().children_of(newChild).size() == 1);
        const ecs::entity newGrand = loaded.tree().children_of(newChild)[0];
        CHECK(loaded.tree().parent_of(newChild) == newRoot);
        CHECK(loaded.tree().parent_of(newGrand) == newChild);
        CHECK(loaded.tree().depth_of(newGrand) == 2);
        CHECK(other.world.has<scene::children>(newRoot));
        CHECK(other.world.has<scene::transform_dirty>(newGrand));
        CHECK(other.world.has<scene::world_transform>(newGrand));
        CONJURE_CHECK_APPROX(loaded.local_transform_of(newChild).value().position, math::vec3(1.0f, 2.0f, 3.0f));
        other.step();
        CONJURE_CHECK_APPROX(translation_of(other.world_matrix(newGrand)), math::vec3(1.0f, 2.0f, 3.0f));
    }

    TEST_CASE("scene_serializer - asset_ref written as uuid and bound after load") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const assets::asset_id blobId = test_uuid(42);
        fixture.add_binary_blob(blobId, "bytes");
        const ecs::entity holder = fixture.create("holder");
        const test_asset_holder boundHolder{assets::asset_ref<assets::binary_asset>(blobId)};
        CONJURE_REQUIRE_OK(fixture.world.add<test_asset_holder>(holder, boundHolder));
        const ecs::entity unbound = fixture.create("unbound");
        CONJURE_REQUIRE_OK(fixture.world.add<test_asset_holder>(unbound));
        const std::string text = save_text(target);
        CHECK(text.find("\"test.asset_holder\":{\"asset\":\"" + blobId.to_string() + "\"}") != std::string::npos);
        CHECK(text.find("\"test.asset_holder\":{\"asset\":null}") != std::string::npos);

        scene_fixture other = {};
        other.add_binary_blob(blobId, "bytes");
        const core::result<scene::instantiate_report> report = load_text(other, text);
        CONJURE_REQUIRE_OK(report);
        REQUIRE(report.value().assetIds.size() == 1);
        CHECK(report.value().assetIds[0] == blobId);
        const ecs::entity loadedHolder = other.active().find_path("holder");
        REQUIRE(other.active().contains(loadedHolder));
        const test_asset_holder *bound = other.world.get<test_asset_holder>(loadedHolder);
        REQUIRE(bound != nullptr);
        CHECK(bound->asset.id() == blobId);
        CHECK(bound->asset.is_bound());
        CHECK(bound->asset.get() == nullptr);
        // Binding never loads; a later load publishes through the bound slot.
        const core::result<assets::asset_handle> loaded = other.assets.load_blocking(blobId);
        CONJURE_REQUIRE_OK(loaded);
        REQUIRE(bound->asset.get() != nullptr);
        CHECK(bound->asset.get()->bytes.size() == 5);
        const test_asset_holder *stillUnbound = other.world.get<test_asset_holder>(other.active().find_path("unbound"));
        REQUIRE(stillUnbound != nullptr);
        CHECK_FALSE(stillUnbound->asset.is_bound());

        // bindAssets = false and a scene without an asset manager both skip the pass.
        scene_fixture third = {};
        const core::result<core::json::document> parsed = core::json::document::parse(text);
        REQUIRE(parsed.has_value());
        const core::result<scene::scene_document> document =
          scene::scene_serializer::parse_document(parsed.value().root(), third.registry);
        CONJURE_REQUIRE_OK(document);
        scene::instantiate_options options = {};
        options.guids = scene::guid_policy::KEEP;
        options.bindAssets = false;
        ecs::entity root = {};
        const core::result<scene::instantiate_report> unboundReport =
          third.active().instantiate_archive(document.value().archive, 1, options, std::span<ecs::entity>(&root, 1));
        CONJURE_REQUIRE_OK(unboundReport);
        CHECK(unboundReport.value().assetIds.empty());
        CHECK_FALSE(third.world.get<test_asset_holder>(third.active().find_path("holder"))->asset.is_bound());
    }

    TEST_CASE("scene_serializer - bind_entity rebinds edited uuid") {
        scene_fixture fixture = {};
        const ecs::entity holder = fixture.create("holder");
        CONJURE_REQUIRE_OK(fixture.world.add<test_asset_holder>(holder));
        const core::result<core::json::document> edit =
          core::json::document::parse("{\"asset\":\"" + test_uuid(5).to_string() + "\"}");
        REQUIRE(edit.has_value());
        CONJURE_REQUIRE_OK(ecs::component_from_json(fixture.world, holder, fixture.registry.id_of<test_asset_holder>(),
          edit.value().root()));
        const test_asset_holder *before = fixture.world.get<test_asset_holder>(holder);
        REQUIRE(before != nullptr);
        CHECK(before->asset.id() == test_uuid(5));
        CHECK_FALSE(before->asset.is_bound());
        scene::scene_serializer::bind_entity(fixture.world, holder, fixture.assets);
        const test_asset_holder *after = fixture.world.get<test_asset_holder>(holder);
        REQUIRE(after != nullptr);
        CHECK(after->asset.is_bound());
        CHECK(after->asset.id() == test_uuid(5));
        CHECK(after->asset.slot()->id == test_uuid(5));
        // A dead entity and an unbound reference are left alone.
        scene::scene_serializer::bind_entity(fixture.world, ecs::entity{900, 1}, fixture.assets);
        std::vector<assets::asset_id> ids = {};
        const std::vector<ecs::entity> twice = {holder, holder};
        scene::scene_serializer::bind_asset_refs(fixture.world, twice, fixture.assets, &ids);
        CHECK(ids.size() == 2);
    }

    TEST_CASE("scene_serializer - save_subtree instantiate_subtree duplicate regenerates guids") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity branch = fixture.create("branch", root);
        const ecs::entity leaf = fixture.create("leaf", branch);
        CONJURE_REQUIRE_OK(target.set_local_transform(leaf, at(0.0f, 0.0f, 9.0f)));
        CONJURE_REQUIRE_OK(fixture.world.add<test_entity_holder>(leaf, test_entity_holder{root, 3}));
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(scene::scene_serializer::save_subtree(target, branch, out));
        const std::string text = out.finish();
        CHECK(text.find("\"format\":\"conjure.prefab\"") != std::string::npos);
        CHECK(text.find("\"name\":\"branch\"") != std::string::npos);
        // The outside reference became null; the root entry has no parent.
        CHECK(text.find("\"test.entity_holder\":{\"other\":null,\"weight\":3}") != std::string::npos);

        const core::result<core::json::document> parsed = core::json::document::parse(text);
        REQUIRE(parsed.has_value());
        const core::result<ecs::entity> copy =
          scene::scene_serializer::instantiate_subtree(target, parsed.value().root(), root, false, 0);
        CONJURE_REQUIRE_OK(copy);
        CHECK(target.tree().parent_of(copy.value()) == root);
        CHECK(target.tree().children_of(root)[0] == copy.value());
        CHECK(target.tree().children_of(root)[1] == branch);
        CHECK(target.guid_of(copy.value()).value() != target.guid_of(branch).value());
        REQUIRE(target.tree().children_of(copy.value()).size() == 1);
        const ecs::entity copiedLeaf = target.tree().children_of(copy.value())[0];
        CHECK(target.guid_of(copiedLeaf).value() != target.guid_of(leaf).value());
        CONJURE_CHECK_APPROX(target.local_transform_of(copiedLeaf).value().position, math::vec3(0.0f, 0.0f, 9.0f));
        CHECK(fixture.world.get<test_entity_holder>(copiedLeaf)->other.is_null());
        CHECK(target.guids().size() == 5);
        // Into the root level too.
        const core::result<ecs::entity> loose =
          scene::scene_serializer::instantiate_subtree(target, parsed.value().root(), ecs::NULL_ENTITY, false);
        CONJURE_REQUIRE_OK(loose);
        CHECK(target.root_owner(loose.value()).has_value());
        CONJURE_CHECK_ERROR(
          scene::scene_serializer::save_subtree(target, ecs::entity{900, 1}, out),
          scene_error::INVALID_ENTITY
        );
        fixture.step();
    }

    TEST_CASE("scene_serializer - keepGuids true collision is DUPLICATE_GUID") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity branch = fixture.create("branch", root);
        const ecs::entity leaf = fixture.create("leaf", branch);
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(scene::scene_serializer::save_subtree(target, branch, out));
        const core::result<core::json::document> parsed = core::json::document::parse(out.finish());
        REQUIRE(parsed.has_value());
        const u32 before = fixture.world.entity_count();
        CONJURE_CHECK_ERROR(
          scene::scene_serializer::instantiate_subtree(target, parsed.value().root(), root, true),
          scene_error::DUPLICATE_GUID
        );
        CHECK(fixture.world.entity_count() == before);
        CHECK(target.tree().children_of(root).size() == 1);
        // Once the originals are gone the guids are free again.
        CONJURE_REQUIRE_OK(target.destroy_now(branch));
        const core::result<ecs::entity> restored =
          scene::scene_serializer::instantiate_subtree(target, parsed.value().root(), root, true);
        CONJURE_REQUIRE_OK(restored);
        CHECK(target.find_guid(target.guid_of(restored.value()).value()) == restored.value());
        CONJURE_UNUSED(leaf);
    }

    TEST_CASE("scene_serializer - unknown component preserved verbatim with one WARN") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const std::string guidText = target.guid_of(root).value().to_string();
        const std::string text = "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"plugins\",\"settings\":{},"
          "\"roots\":[\"" + guidText + "\"],\"archive\":{\"entities\":[{\"components\":{"
          "\"com.studio.spinner\":{\"speed\":2.5},"
          "\"conjure.scene.guid\":{\"value\":\"" + guidText + "\"},"
          "\"conjure.scene.name\":{\"value\":\"Spinner\"},"
          "\"com.studio.other\":[1,2,3]}}]}}";
        target.clear();
        testing::log_capture logs = {};
        const core::result<scene::instantiate_report> report = load_text(fixture, text);
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().unknownComponents == 2);
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "2 unknown component(s)"));
        REQUIRE(target.roots().size() == 1);
        const ecs::entity loaded = target.roots()[0].entity;
        CHECK(fixture.world.has_raw(loaded, ecs::component_id{0}));
        const std::string saved = save_text(target);
        CHECK(saved.find("\"com.studio.spinner\":{\"speed\":2.5}") != std::string::npos);
        CHECK(saved.find("\"com.studio.other\":[1,2,3]") != std::string::npos);
        CHECK(saved.find("\"name\":\"plugins\"") != std::string::npos);
    }

    TEST_CASE("scene_serializer - script-like custom serializer component round trips") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        CONJURE_REQUIRE_OK(fixture.registry.register_component<scripted>());
        CHECK_FALSE(fixture.registry.info(fixture.registry.id_of<scripted>()).is_trivial());
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        CONJURE_REQUIRE_OK(fixture.world.add<scripted>(child, scripted{"spin fast", root}));
        const std::string text = save_text(target);
        CHECK(text.find("\"test.scripted\":{\"text\":\"spin fast\",\"target\":0}") != std::string::npos);

        // Loading deep-copies the text and rewires the reference to the new root.
        scene_fixture other = {};
        CONJURE_REQUIRE_OK(other.registry.register_component<scripted>());
        CONJURE_REQUIRE_OK(load_text(other, text));
        const ecs::entity newRoot = other.active().roots()[0].entity;
        const ecs::entity newChild = other.active().tree().children_of(newRoot)[0];
        const scripted *loaded = other.world.get<scripted>(newChild);
        REQUIRE(loaded != nullptr);
        CHECK(loaded->text == "spin fast");
        CHECK(loaded->target == newRoot);
        CHECK(save_text(other.active()) == text);

        // Duplicating inside one scene copies the script state as well.
        const core::result<ecs::entity> copy = target.duplicate_subtree(root);
        CONJURE_REQUIRE_OK(copy);
        const ecs::entity copiedChild = target.tree().children_of(copy.value())[0];
        const scripted *duplicated = fixture.world.get<scripted>(copiedChild);
        REQUIRE(duplicated != nullptr);
        CHECK(duplicated->text == "spin fast");
        CHECK(duplicated->target == copy.value());
    }
}
