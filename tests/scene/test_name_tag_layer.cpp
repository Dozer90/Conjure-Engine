#include "conjure/testing/doctest.h"
#include "scene_test_fixture.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/serialization.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_error.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/scene_serializer.h"
#include "conjure/scene/scene_snapshot.h"

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace scene_test;
using scene::scene_error;

TEST_SUITE("scene/name_tag_layer") {

    TEST_CASE("name - names survive round trip as strings") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity entity = fixture.create("Lamp Post 7");
        CONJURE_REQUIRE_OK(target.set_tag(entity, core::string_id("Decoration")));
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(scene::scene_serializer::save_scene(target, out));
        const std::string text = out.finish();
        CHECK(text.find("\"conjure.scene.name\":{\"value\":\"Lamp Post 7\"}") != std::string::npos);
        CHECK(text.find("\"conjure.scene.tag\":{\"value\":\"Decoration\"}") != std::string::npos);

        // Loading into a fresh scene resolves the same interned ids.
        scene_fixture other = {};
        const core::result<core::json::document> parsed = core::json::document::parse(text);
        CONJURE_REQUIRE_OK(parsed);
        const core::result<scene::scene_document> document =
          scene::scene_serializer::parse_document(parsed.value().root(), other.registry);
        CONJURE_REQUIRE_OK(document);
        CONJURE_REQUIRE_OK(scene::scene_serializer::load_scene(
          other.active(),
          document.value(),
          scene::SCENE_ID_AUTHORED,
          scene::guid_policy::KEEP
        ));
        REQUIRE(other.active().roots().size() == 1);
        const ecs::entity loaded = other.active().roots()[0].entity;
        CHECK(other.active().name_of(loaded) == core::string_id("Lamp Post 7"));
        CHECK(other.active().name_of(loaded).view() == "Lamp Post 7");
        CHECK(other.active().tag_of(loaded).view() == "Decoration");
    }

    TEST_CASE("tag - query") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const core::string_id coin("Coin");
        std::vector<ecs::entity> coins = {};
        for (u32 index = 0; index < 5; ++index) {
            coins.push_back(fixture.create("c"));
            CONJURE_REQUIRE_OK(target.set_tag(coins.back(), coin));
        }
        const ecs::entity other = fixture.create("other");
        CONJURE_REQUIRE_OK(target.set_tag(other, core::string_id("Other")));
        std::vector<ecs::entity> found = {};
        target.find_by_tag(coin, found);
        CHECK(found.size() == 5);
        // Retagging moves the entity between queries.
        CONJURE_REQUIRE_OK(target.set_tag(coins[0], core::string_id("Other")));
        found.clear();
        target.find_by_tag(coin, found);
        CHECK(found.size() == 4);
        found.clear();
        target.find_by_tag(core::string_id("Other"), found);
        CHECK(found.size() == 2);
        CONJURE_CHECK_ERROR(target.set_tag(ecs::entity{900, 1}, coin), scene_error::INVALID_ENTITY);
    }

    TEST_CASE("layer - index 31 max") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity entity = fixture.create("layered");
        CHECK(target.layer_of(entity) == 0);
        CONJURE_REQUIRE_OK(target.set_layer(entity, 31));
        CHECK(target.layer_of(entity) == 31);
        CONJURE_CHECK_ERROR(target.set_layer(entity, 32), core::error_code::OUT_OF_RANGE);
        CONJURE_CHECK_ERROR(target.set_layer(entity, 255), core::error_code::OUT_OF_RANGE);
        CHECK(target.layer_of(entity) == 31);
        CONJURE_REQUIRE_OK(target.set_layer(entity, 0));
        CHECK(target.layer_of(entity) == 0);
        CHECK(fixture.world.has<scene::layer>(entity));
        CONJURE_CHECK_ERROR(target.set_layer(ecs::entity{900, 1}, 1), scene_error::INVALID_ENTITY);
        std::vector<ecs::entity> found = {};
        target.find_by_layer(0, found);
        CHECK(found.size() == 1);
    }

    TEST_CASE("layer - find_by_layer lists only entities carrying a layer") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity a = fixture.create("a");
        const ecs::entity b = fixture.create("b");
        CONJURE_REQUIRE_OK(target.set_layer(a, 0));
        std::vector<ecs::entity> found = {};
        target.find_by_layer(0, found);
        CHECK(found == std::vector<ecs::entity>{a});
        // b reads as layer 0 but carries no layer component, so the column scan does not see it.
        CHECK(target.layer_of(b) == 0);
        CHECK_FALSE(fixture.world.has<scene::layer>(b));
    }

    TEST_CASE("regression - code sweep F151 an archive layer of 32 or more fails before anything is created") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();

        SUBCASE("a scene file") {
            const std::string text = "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"layers\",\"settings\":{},"
              "\"roots\":[],\"archive\":{\"entities\":["
              "{\"components\":{\"conjure.scene.layer\":{\"index\":31}}},"
              "{\"components\":{\"conjure.scene.layer\":{\"index\":200}}}]}}";
            const core::result<core::json::document> parsed = core::json::document::parse(text);
            CONJURE_REQUIRE_OK(parsed);
            const core::result<scene::scene_document> document =
              scene::scene_serializer::parse_document(parsed.value().root(), fixture.registry);
            CONJURE_REQUIRE_OK(document);
            const core::result<scene::instantiate_report> loaded = scene::scene_serializer::load_scene(
              target,
              document.value(),
              scene::SCENE_ID_AUTHORED,
              scene::guid_policy::KEEP
            );
            CONJURE_CHECK_ERROR(loaded, scene_error::BAD_FORMAT);
            REQUIRE_FALSE(loaded.has_value());
            CHECK(loaded.get_error().message().find("entry 1 has layer 200") != std::string::npos);
            CHECK(fixture.world.entity_count() == 0);
        }
        SUBCASE("a snapshot") {
            // Built from JSON: capture itself refuses such a layer (the next test case).
            const core::result<core::json::document> parsed =
              core::json::document::parse("{\"entities\":[{\"components\":{\"conjure.scene.layer\":{\"index\":32}}}]}");
            CONJURE_REQUIRE_OK(parsed);
            core::result<ecs::entity_archive> archive = ecs::read_archive_json(parsed.value().root(), fixture.registry);
            CONJURE_REQUIRE_OK(archive);
            scene::scene_snapshot snapshot(fixture.registry);
            snapshot.archive = std::move(archive).value();
            snapshot.rootEntries.push_back(0);
            snapshot.rootOwners.push_back(scene::SCENE_ID_AUTHORED);
            const ecs::entity current = fixture.create("current");
            REQUIRE(target.contains(current));
            CONJURE_CHECK_ERROR(scene::restore(target, snapshot), scene_error::BAD_FORMAT);
            // restore clears the scene before it instantiates, so like any failed restore it leaves it empty.
            CHECK(fixture.world.entity_count() == 0);
        }
        SUBCASE("a prefab") {
            const std::string text = "{\"format\":\"conjure.prefab\",\"version\":1,\"name\":\"bad\",\"archive\":{"
              "\"entities\":[{\"components\":{\"conjure.scene.name\":{\"value\":\"root\"}}},"
              "{\"components\":{\"conjure.scene.parent\":{\"value\":0,\"siblingIndex\":0},"
              "\"conjure.scene.layer\":{\"index\":255}}}]}}";
            const core::result<core::json::document> parsed = core::json::document::parse(text);
            CONJURE_REQUIRE_OK(parsed);
            const core::result<scene::prefab_document> document =
              scene::scene_serializer::parse_prefab(parsed.value().root(), fixture.registry);
            CONJURE_REQUIRE_OK(document);
            scene::prefab_asset asset = {};
            asset.archive = document.value().archive;
            asset.name = document.value().name;
            asset.id = test_uuid(77);
            const core::result<ecs::entity> instantiated = scene::prefab::instantiate(target, asset);
            CONJURE_CHECK_ERROR(instantiated, scene_error::BAD_FORMAT);
            REQUIRE_FALSE(instantiated.has_value());
            CHECK(instantiated.get_error().message().find("entry 1 has layer 255") != std::string::npos);
            CHECK(fixture.world.entity_count() == 0);
        }
        CONJURE_CHECK_OK(target.validate());
    }

    TEST_CASE("regression - code sweep F151 a capture refuses a layer of 32 or more, naming the entity") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        CONJURE_REQUIRE_OK(target.set_layer(child, 3));
        // A raw write skips set_layer's range check, as the editor's component.set does.
        fixture.world.get<scene::layer>(child)->index = 40;

        // Each capture refuses while the content still exists, so no later restore or load can lose it.
        const core::result<scene::scene_snapshot> snapshot = scene::capture(target);
        CONJURE_CHECK_ERROR(snapshot, scene_error::BAD_FORMAT);
        REQUIRE_FALSE(snapshot.has_value());
        CHECK(snapshot.get_error().message().find(std::format("entity {} has layer 40", child)) != std::string::npos);
        core::json::writer out = {};
        CONJURE_CHECK_ERROR(scene::scene_serializer::save_scene(target, out), scene_error::BAD_FORMAT);
        CONJURE_CHECK_ERROR(scene::scene_serializer::capture_subtree(target, root), scene_error::BAD_FORMAT);
        CONJURE_CHECK_ERROR(scene::prefab::build_document(target, root, "bad"), scene_error::BAD_FORMAT);
        CONJURE_CHECK_ERROR(target.duplicate_subtree(root), scene_error::BAD_FORMAT);
        CHECK(fixture.world.entity_count() == 2);

        // Index 31 is the last valid layer: it captures, copies and comes back from a snapshot.
        fixture.world.get<scene::layer>(child)->index = 31;
        const core::result<ecs::entity> copy = target.duplicate_subtree(root);
        CONJURE_REQUIRE_OK(copy);
        REQUIRE(target.tree().children_of(copy.value()).size() == 1);
        CHECK(target.layer_of(target.tree().children_of(copy.value())[0]) == 31);
        const core::result<scene::scene_snapshot> valid = scene::capture(target);
        CONJURE_REQUIRE_OK(valid);
        CONJURE_REQUIRE_OK(scene::restore(target, valid.value()));
        CHECK(fixture.world.entity_count() == 4);
        CONJURE_CHECK_OK(target.validate());
    }

    TEST_CASE("layer_table - lookup") {
        scene::layer_table table = {};
        for (u32 index = 0; index < scene::layer_table::LAYER_COUNT; ++index) {
            CHECK(table.name(static_cast<u8>(index)).is_null());
        }
        CHECK_FALSE(table.find(core::string_id("Default")).has_value());
        CHECK_FALSE(table.find(core::string_id()).has_value());
        table.set_name(0, core::string_id("Default"));
        table.set_name(31, core::string_id("UI"));
        CHECK(table.name(0).view() == "Default");
        CHECK(table.name(31).view() == "UI");
        CHECK(table.find(core::string_id("Default")) == std::optional<u8>(u8{0}));
        CHECK(table.find(core::string_id("UI")) == std::optional<u8>(u8{31}));
        CHECK_FALSE(table.find(core::string_id("World")).has_value());
        CHECK(table.name(32).is_null());
        CHECK(table.names().size() == 32);

        scene_fixture fixture = {};
        CHECK(fixture.manager.layers().name(0).is_null());
        fixture.manager.layers().set_name(3, core::string_id("Props"));
        CHECK(fixture.manager.layers().find(core::string_id("Props")) == std::optional<u8>(u8{3}));
    }
}
