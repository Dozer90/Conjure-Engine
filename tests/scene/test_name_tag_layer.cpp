#include "conjure/testing/doctest.h"
#include "scene_test_fixture.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/string_id.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_error.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/scene_serializer.h"

#include <string>
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
