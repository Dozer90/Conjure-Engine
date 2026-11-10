#include "conjure/testing/doctest.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/string_id.h"
#include "conjure/physics/physics_error.h"
#include "conjure/physics/physics_layers.h"

#include <string>

using namespace conjure;
using physics::physics_error;
using physics::physics_layer_mask;
using physics::physics_layer_table;

namespace {

    constexpr std::string_view SAMPLE = R"({
        "names": ["default", "player", "enemy", "projectile", "trigger"],
        "ignore": [["projectile", "projectile"], ["player", "player"], ["trigger", "trigger"]]
    })";

    [[nodiscard]] core::string_id id(std::string_view name) {
        return core::string_id(name);
    }
}

TEST_SUITE("physics/physics_layers") {

    TEST_CASE("physics_layer_mask - all none only includes with without") {
        static_assert(physics_layer_mask::all().bits == 0xFFFFFFFFu);
        static_assert(physics_layer_mask::none().bits == 0u);
        static_assert(physics_layer_mask::only(3).bits == 8u);
        static_assert(physics_layer_mask::only(32).bits == 0u);
        static_assert(physics_layer_mask::none().with(31).includes(31));
        static_assert(!physics_layer_mask::none().with(31).includes(30));
        static_assert(!physics_layer_mask::all().without(5).includes(5));
        static_assert(physics_layer_mask::all().without(5).includes(6));
        static_assert(!physics_layer_mask::all().includes(40));
        const physics_layer_mask mask = {};
        CHECK(mask == physics_layer_mask::all());
    }

    TEST_CASE("physics_layer_table - the default table has one layer default colliding with itself") {
        const physics_layer_table table = {};
        CHECK(table.count() == 1);
        CHECK(table.name(0) == id("default"));
        CHECK(table.name(0).view() == "default");
        CHECK(table.find(id("default")) == 0);
        CHECK_FALSE(table.find(id("missing")).has_value());
        CHECK(table.collides(0, 0));
        CHECK_FALSE(table.collides(0, 1));
        CHECK(table.collision_mask(0) == physics_layer_mask::only(0));
        CHECK(table.collision_mask(7) == physics_layer_mask::none());
        CHECK(table.name(9).is_null());
        CONJURE_CHECK_OK(table.validate());
    }

    TEST_CASE("physics_layer_table - from_json parses the design sample") {
        const core::result<core::json::document> parsed = core::json::document::parse(SAMPLE);
        CONJURE_REQUIRE_OK(parsed);
        const core::result<physics_layer_table> built = physics_layer_table::from_json(parsed.value().root());
        CONJURE_REQUIRE_OK(built);
        const physics_layer_table &table = built.value();
        CHECK(table.count() == 5);
        CHECK(table.find(id("player")) == 1);
        CHECK(table.find(id("trigger")) == 4);
        CHECK(table.collides(0, 1));
        CHECK(table.collides(1, 2));
        CHECK_FALSE(table.collides(1, 1));
        CHECK_FALSE(table.collides(3, 3));
        CHECK_FALSE(table.collides(4, 4));
        CHECK(table.collides(3, 2));
        CHECK(table.collision_mask(1) == physics_layer_mask::all().without(1).without(5).without(6).without(7)
          .without(8).without(9).without(10).without(11).without(12).without(13).without(14).without(15)
          .without(16).without(17).without(18).without(19).without(20).without(21).without(22).without(23)
          .without(24).without(25).without(26).without(27).without(28).without(29).without(30).without(31));
        CONJURE_CHECK_OK(table.validate());
    }

    TEST_CASE("physics_layer_table - set_collides is symmetric") {
        physics_layer_table table = {};
        CONJURE_REQUIRE_OK(table.add_layer(id("a")));
        CONJURE_REQUIRE_OK(table.add_layer(id("b")));
        CHECK(table.collides(1, 2));
        table.set_collides(1, 2, false);
        CHECK_FALSE(table.collides(1, 2));
        CHECK_FALSE(table.collides(2, 1));
        table.set_collides(2, 1, true);
        CHECK(table.collides(1, 2));
        // Indices outside the table are ignored.
        table.set_collides(2, 9, false);
        CHECK_FALSE(table.collides(2, 9));
        CONJURE_CHECK_OK(table.validate());
    }

    TEST_CASE("physics_layer_table - 33 layers hit LAYER_LIMIT_REACHED") {
        physics_layer_table table = {};
        for (u32 index = 1; index < physics::MAX_PHYSICS_LAYERS; ++index) {
            CAPTURE(index);
            const core::result<u8> added = table.add_layer(id("layer_" + std::to_string(index)));
            CONJURE_REQUIRE_OK(added);
            CHECK(added.value() == index);
        }
        CHECK(table.count() == physics::MAX_PHYSICS_LAYERS);
        CONJURE_CHECK_ERROR(table.add_layer(id("one_too_many")), physics_error::LAYER_LIMIT_REACHED);
        CHECK(table.collides(0, 31));
        CHECK(table.collision_mask(31) == physics_layer_mask::all());
        CONJURE_CHECK_OK(table.validate());

        core::json::document names = core::json::document::array();
        for (u32 index = 0; index <= physics::MAX_PHYSICS_LAYERS; ++index) {
            names.push_back("n" + std::to_string(index));
        }
        core::json::document section = core::json::document::object();
        section.set("names", names);
        CONJURE_CHECK_ERROR(physics_layer_table::from_json(section.root()), physics_error::LAYER_LIMIT_REACHED);
    }

    TEST_CASE("physics_layer_table - duplicate names are DUPLICATE_LAYER") {
        physics_layer_table table = {};
        CONJURE_CHECK_ERROR(table.add_layer(id("default")), physics_error::DUPLICATE_LAYER);
        CONJURE_CHECK_ERROR(table.add_layer(core::string_id()), physics_error::DUPLICATE_LAYER);
        const core::result<core::json::document> parsed = core::json::document::parse(R"({"names": ["a", "b", "a"]})");
        CONJURE_REQUIRE_OK(parsed);
        CONJURE_CHECK_ERROR(physics_layer_table::from_json(parsed.value().root()), physics_error::DUPLICATE_LAYER);
    }

    TEST_CASE("physics_layer_table - unknown names in ignore are UNKNOWN_LAYER") {
        const core::result<core::json::document> parsed =
          core::json::document::parse(R"({"names": ["a", "b"], "ignore": [["a", "ghost"]]})");
        CONJURE_REQUIRE_OK(parsed);
        CONJURE_CHECK_ERROR(physics_layer_table::from_json(parsed.value().root()), physics_error::UNKNOWN_LAYER);
    }

    TEST_CASE("physics_layer_table - malformed sections are LAYER_CONFIG_INVALID") {
        const char *texts[] = {
            R"({})",
            R"({"names": []})",
            R"({"names": "a"})",
            R"({"names": [1, 2]})",
            R"({"names": ["a"], "ignore": "x"})",
            R"({"names": ["a"], "ignore": [["a"]]})",
            R"({"names": ["a"], "ignore": [[1, 2]]})",
            R"([1])",
        };
        for (const char *text : texts) {
            CAPTURE(text);
            const core::result<core::json::document> parsed = core::json::document::parse(text);
            CONJURE_REQUIRE_OK(parsed);
            CONJURE_CHECK_ERROR(
              physics_layer_table::from_json(parsed.value().root()),
              physics_error::LAYER_CONFIG_INVALID
            );
        }
        CONJURE_CHECK_ERROR(physics_layer_table::from_json(core::json::value()), physics_error::LAYER_CONFIG_INVALID);
    }

    TEST_CASE("physics_layer_table - to_json round trips canonically") {
        const core::result<core::json::document> parsed = core::json::document::parse(SAMPLE);
        CONJURE_REQUIRE_OK(parsed);
        const core::result<physics_layer_table> built = physics_layer_table::from_json(parsed.value().root());
        CONJURE_REQUIRE_OK(built);

        core::json::writer out = {};
        CONJURE_REQUIRE_OK(built.value().to_json(out));
        const std::string text = out.finish();
        const core::result<core::json::document> reparsed = core::json::document::parse(text);
        CONJURE_REQUIRE_OK(reparsed);
        CHECK(reparsed.value().to_canonical_string() == text);

        const core::result<physics_layer_table> again = physics_layer_table::from_json(reparsed.value().root());
        CONJURE_REQUIRE_OK(again);
        CHECK(again.value() == built.value());
        const std::string expected = std::string(R"({"ignore":[["player","player"],["projectile","projectile"],)")
          + R"(["trigger","trigger"]],"names":["default","player","enemy","projectile","trigger"]})";
        CHECK(text == expected);
    }

    TEST_CASE("physics_layer_table - to_json refuses a name that was never interned") {
        physics_layer_table table = {};
        CONJURE_REQUIRE_OK(table.add_layer(core::string_id::from_hash(0x1234567887654321ull)));
        core::json::writer out = {};
        CONJURE_CHECK_ERROR(table.to_json(out), physics_error::LAYER_CONFIG_INVALID);
    }
}
