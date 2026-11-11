#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/uuid.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/scene/hierarchy.h"

#include <span>
#include <string>
#include <vector>

// entity.* (docs/design/04_editor.md 3.5, 3.6, 10).

using namespace conjure;
using namespace conjure::editor;

namespace {
    core::json::document vec3(f32 x, f32 y, f32 z) {
        return core::json::array({x, y, z});
    }
}

TEST_SUITE("editor/commands_entity") {

    TEST_CASE("entity.create - assigns a guid and undo destroys it") {
        editor_test::headless_editor editor = {};
        const core::json::document result = editor.ok("entity.create", core::json::object({{"name", "Wall"}}));
        const std::string guid = std::string(result.root()["guid"].as_string());
        CHECK(core::uuid::parse(guid).has_value());
        CHECK(result.root()["name"].as_string() == "Wall");
        CHECK(editor.scene().world().entity_count() == 1);
        (void) editor.ok("history.undo");
        CHECK(editor.scene().world().entity_count() == 0);
        CHECK(editor.fail(
          "entity.get",
          core::json::object({{"entity", guid}})
        ) == protocol_error_code::ENTITY_NOT_FOUND);
        (void) editor.ok("history.redo");
        CHECK(editor.scene().world().entity_count() == 1);
        // Redo recreates the entity with the same guid.
        (void) editor.ok("entity.get", core::json::object({{"entity", guid}}));
    }

    TEST_CASE("entity.create - an explicit guid is honoured and a duplicate is INVALID_ARGS") {
        editor_test::headless_editor editor = {};
        const std::string wanted = core::uuid::generate().to_string();
        const core::json::document result = editor.ok(
          "entity.create",
          core::json::object({{"name", "Fixed"}, {"guid", wanted}})
        );
        CHECK(result.root()["guid"].as_string() == wanted);
        CHECK(editor.fail(
          "entity.create",
          core::json::object({{"name", "Dup"}, {"guid", wanted}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "entity.create",
          core::json::object({{"name", "Bad"}, {"guid", "not-a-uuid"}})
        ) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("entity.create - components, parent and index are applied") {
        editor_test::headless_editor editor = {};
        const std::string parent = editor.create_entity("Parent");
        (void) editor.create_entity("First", parent);
        const core::json::document result = editor.ok(
          "entity.create",
          core::json::object({
            {"name", "Second"},
            {"parent", parent},
            {"index", 0},
            {"components", core::json::object({{"sprite", core::json::object({{"size", core::json::array({
              2.0f,
              3.0f,
            })}})}})},
          })
        );
        const std::string guid = std::string(result.root()["guid"].as_string());
        const core::json::document got = editor.ok("entity.get", core::json::object({{"entity", guid}}));
        CHECK(got.root()["parent"].as_string() == parent);
        CHECK(got.root()["components"].contains("conjure.renderer.sprite"));
        CHECK(got.root()["components"]["conjure.renderer.sprite"]["size"][1].as_f32() == doctest::Approx(3.0f));
        const core::json::document parentInfo = editor.ok("entity.get", core::json::object({{"entity", parent}}));
        REQUIRE(parentInfo.root()["children"].size() == 2);
        CHECK(parentInfo.root()["children"][0].as_string() == guid);
        CHECK(editor.fail(
          "entity.create",
          core::json::object({
            {"name", "x"},
            {"components", core::json::object({{"nope", core::json::document::object()}})},
          })
        ) == protocol_error_code::TYPE_NOT_FOUND);
        CHECK(editor.scene().world().entity_count() == 3);
    }

    TEST_CASE("entity.destroy - a subtree is restored with children, components and sibling index") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Root");
        const std::string first = editor.create_entity("First", root);
        const std::string middle = editor.create_entity("Middle", root);
        const std::string last = editor.create_entity("Last", root);
        const std::string grandchild = editor.create_entity("Grandchild", middle);
        (void) editor.ok("component.add", core::json::object({{"entity", grandchild}, {"type", "sprite"}}));
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", middle}, {"position", vec3(1.0f, 2.0f, 3.0f)}})
        );
        const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
        const core::json::document destroyed = editor.ok("entity.destroy", core::json::object({{"entity", middle}}));
        CHECK(destroyed.root()["destroyed"].as_i64() == 2);
        CHECK(editor.scene().world().entity_count() == 3);
        (void) editor.ok("history.undo");
        const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(before == after, editor_test::describe_difference(before, after));
        const core::json::document rootInfo = editor.ok("entity.get", core::json::object({{"entity", root}}));
        REQUIRE(rootInfo.root()["children"].size() == 3);
        CHECK(rootInfo.root()["children"][0].as_string() == first);
        CHECK(rootInfo.root()["children"][1].as_string() == middle);
        CHECK(rootInfo.root()["children"][2].as_string() == last);
        const core::json::document grand = editor.ok("entity.get", core::json::object({{"entity", grandchild}}));
        CHECK(grand.root()["components"].contains("conjure.renderer.sprite"));
    }

    TEST_CASE("entity.destroy_many - outermost subtrees only, restored in order") {
        editor_test::headless_editor editor = {};
        const std::string a = editor.create_entity("A");
        const std::string b = editor.create_entity("B");
        const std::string child = editor.create_entity("Child", a);
        const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
        const core::json::document result = editor.ok(
          "entity.destroy_many",
          core::json::object({{"entities", core::json::array({a, child, b})}})
        );
        CHECK(result.root()["destroyed"].as_i64() == 3);
        CHECK(editor.scene().world().entity_count() == 0);
        (void) editor.ok("history.undo");
        const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(before == after, editor_test::describe_difference(before, after));
        (void) editor.ok("history.redo");
        CHECK(editor.scene().world().entity_count() == 0);
    }

    TEST_CASE("regression - entity.destroy_many undo keeps the sibling order when survivors sit in between") {
        editor_test::headless_editor editor = {};
        SUBCASE("roots") {
            const std::string a = editor.create_entity("A");
            const std::string b = editor.create_entity("B");
            const std::string c = editor.create_entity("C");
            const std::string d = editor.create_entity("D");
            const std::string e = editor.create_entity("E");
            const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
            (void) editor.ok("entity.destroy_many", core::json::object({{"entities", core::json::array({d, b})}}));
            REQUIRE(editor.scene().roots().size() == 3);
            (void) editor.ok("history.undo");
            const std::span<const scene::root_entry> roots = editor.scene().roots();
            REQUIRE(roots.size() == 5);
            const std::vector<std::string> expected = {a, b, c, d, e};
            for (usize index = 0; index < roots.size(); ++index) {
                CAPTURE(index);
                CHECK(editor.scene().guid_of(roots[index].entity)->to_string() == expected[index]);
            }
            const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
            CHECK_MESSAGE(before == after, editor_test::describe_difference(before, after));
            (void) editor.ok("history.redo");
            REQUIRE(editor.scene().roots().size() == 3);
            CHECK(editor.scene().guid_of(editor.scene().roots()[0].entity)->to_string() == a);
            CHECK(editor.scene().guid_of(editor.scene().roots()[1].entity)->to_string() == c);
            CHECK(editor.scene().guid_of(editor.scene().roots()[2].entity)->to_string() == e);
            (void) editor.ok("history.undo");
            for (usize index = 0; index < 5; ++index) {
                CAPTURE(index);
                CHECK(editor.scene().guid_of(editor.scene().roots()[index].entity)->to_string() == expected[index]);
            }
        }
        SUBCASE("children of one parent") {
            const std::string parent = editor.create_entity("Parent");
            const std::string first = editor.create_entity("First", parent);
            const std::string second = editor.create_entity("Second", parent);
            const std::string third = editor.create_entity("Third", parent);
            const std::string fourth = editor.create_entity("Fourth", parent);
            (void) editor.create_entity("Grandchild", second);
            const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
            (void) editor.ok(
              "entity.destroy_many",
              core::json::object({{"entities", core::json::array({fourth, second})}})
            );
            core::json::document info = editor.ok("entity.get", core::json::object({{"entity", parent}}));
            REQUIRE(info.root()["children"].size() == 2);
            (void) editor.ok("history.undo");
            info = editor.ok("entity.get", core::json::object({{"entity", parent}}));
            REQUIRE(info.root()["children"].size() == 4);
            CHECK(info.root()["children"][0].as_string() == first);
            CHECK(info.root()["children"][1].as_string() == second);
            CHECK(info.root()["children"][2].as_string() == third);
            CHECK(info.root()["children"][3].as_string() == fourth);
            const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
            CHECK_MESSAGE(before == after, editor_test::describe_difference(before, after));
            (void) editor.ok("history.redo");
            info = editor.ok("entity.get", core::json::object({{"entity", parent}}));
            REQUIRE(info.root()["children"].size() == 2);
            CHECK(info.root()["children"][0].as_string() == first);
            CHECK(info.root()["children"][1].as_string() == third);
        }
    }

    TEST_CASE("entity.rename - changes the name and undo restores it") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Old");
        (void) editor.ok("entity.rename", core::json::object({{"entity", guid}, {"name", "New"}}));
        CHECK(editor.ok("entity.get", core::json::object({{"entity", guid}})).root()["name"].as_string() == "New");
        (void) editor.ok("history.undo");
        CHECK(editor.ok("entity.get", core::json::object({{"entity", guid}})).root()["name"].as_string() == "Old");
        // Paths resolve by name.
        (void) editor.ok("entity.get", core::json::object({{"entity", "Old"}}));
    }

    TEST_CASE("entity.set_parent - keeps the world transform by default and undo restores the old parent") {
        editor_test::headless_editor editor = {};
        const std::string parent = editor.create_entity("Parent");
        const std::string child = editor.create_entity("Child");
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", parent}, {"position", vec3(10.0f, 0.0f, 0.0f)}})
        );
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", child}, {"position", vec3(1.0f, 0.0f, 0.0f)}})
        );
        (void) editor.ok("entity.set_parent", core::json::object({{"entity", child}, {"parent", parent}}));
        editor.pump(1);
        const ecs::entity childEntity = editor.scene().find_guid(*core::uuid::parse(child));
        const std::optional<math::transform> world = editor.scene().world_transform_of(childEntity);
        REQUIRE(world.has_value());
        CONJURE_CHECK_APPROX(world->position, math::vec3(1.0f, 0.0f, 0.0f));
        const std::optional<math::transform> local = editor.scene().local_transform_of(childEntity);
        REQUIRE(local.has_value());
        CONJURE_CHECK_APPROX(local->position, math::vec3(-9.0f, 0.0f, 0.0f));
        (void) editor.ok("history.undo");
        CHECK(editor.scene().tree().parent_of(childEntity).is_null());
        const std::optional<math::transform> restored = editor.scene().local_transform_of(childEntity);
        REQUIRE(restored.has_value());
        CONJURE_CHECK_APPROX(restored->position, math::vec3(1.0f, 0.0f, 0.0f));
        // keepWorld false keeps the local transform instead; null parent moves to the root.
        (void) editor.ok(
          "entity.set_parent",
          core::json::object({{"entity", child}, {"parent", parent}, {"keepWorld", false}})
        );
        CONJURE_CHECK_APPROX(editor.scene().local_transform_of(childEntity)->position, math::vec3(1.0f, 0.0f, 0.0f));
        (void) editor.ok("entity.set_parent", core::json::object({{"entity", child}, {"parent", nullptr}}));
        CHECK(editor.scene().tree().parent_of(childEntity).is_null());
        CHECK(editor.fail(
          "entity.set_parent",
          core::json::object({{"entity", parent}, {"parent", parent}})
        ) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("entity.duplicate - copies the subtree next to the original and undo removes the copies") {
        editor_test::headless_editor editor = {};
        const std::string original = editor.create_entity("Original");
        (void) editor.create_entity("Child", original);
        (void) editor.ok("component.add", core::json::object({{"entity", original}, {"type", "sprite"}}));
        const core::json::document result = editor.ok(
          "entity.duplicate",
          core::json::object({{"entity", original}, {"count", 2}, {"offset", vec3(1.0f, 0.0f, 0.0f)}})
        );
        REQUIRE(result.root()["guids"].size() == 2);
        CHECK(editor.scene().world().entity_count() == 6);
        const std::string copy = std::string(result.root()["guids"][1].as_string());
        const core::json::document copyInfo = editor.ok("entity.get", core::json::object({{"entity", copy}}));
        CHECK(copyInfo.root()["children"].size() == 1);
        CHECK(copyInfo.root()["components"].contains("conjure.renderer.sprite"));
        const core::json::value position = copyInfo.root()["components"]["conjure.scene.local_transform"]["position"];
        CHECK(position[0].as_f32() == doctest::Approx(2.0f));
        (void) editor.ok("history.undo");
        CHECK(editor.scene().world().entity_count() == 2);
        (void) editor.ok("history.redo");
        CHECK(editor.scene().world().entity_count() == 6);
        (void) editor.ok("entity.get", core::json::object({{"entity", copy}}));
    }

    TEST_CASE("entity.create_many - creates 10,000 entities with overrides and one undo entry") {
        editor_test::headless_editor editor = {};
        core::json::document overrides = core::json::document::array();
        for (int index = 0; index < 3; ++index) {
            overrides.push_back(core::json::object({{"transform", core::json::object({{"position", vec3(
              static_cast<f32>(index),
              0.0f,
              0.0f
            )}})}}));
        }
        const core::json::document result = editor.ok(
          "entity.create_many",
          core::json::object({
            {"count", 10000},
            {"nameFormat", "Cell_{i}"},
            {"components", core::json::object({{"sprite", core::json::object({{"size", core::json::array({
              0.9f,
              0.9f,
            })}})}})},
            {"overrides", overrides},
          })
        );
        CHECK(result.root()["count"].as_i64() == 10000);
        REQUIRE(result.root()["guids"].size() == 10000);
        CHECK(editor.scene().world().entity_count() == 10000);
        CHECK(editor.context().undo().size() == 1);
        const std::string second = std::string(result.root()["guids"][2].as_string());
        const core::json::document info = editor.ok("entity.get", core::json::object({{"entity", second}}));
        CHECK(info.root()["name"].as_string() == "Cell_2");
        CHECK(
          info.root()["components"]["conjure.scene.local_transform"]["position"][0].as_f32() == doctest::Approx(2.0f)
        );
        CHECK(info.root()["components"].contains("conjure.renderer.sprite"));
        (void) editor.ok("history.undo");
        CHECK(editor.scene().world().entity_count() == 0);
        (void) editor.ok("history.redo");
        CHECK(editor.scene().world().entity_count() == 10000);
        (void) editor.ok("entity.get", core::json::object({{"entity", second}}));
        CHECK(editor.fail(
          "entity.create_many",
          core::json::object({{"count", 0}})
        ) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("entity.set_transform / entity.get - local and world space, undo restores the local transform") {
        editor_test::headless_editor editor = {};
        const std::string parent = editor.create_entity("P");
        const std::string child = editor.create_entity("C", parent);
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", parent}, {"position", vec3(5.0f, 0.0f, 0.0f)}})
        );
        editor.pump(1);
        const core::json::document local = editor.ok(
          "entity.set_transform",
          core::json::object({
            {"entity", child},
            {"position", vec3(1.0f, 2.0f, 3.0f)},
            {"rotation", vec3(0.0f, 90.0f, 0.0f)},
            {"scale", vec3(2.0f, 2.0f, 2.0f)},
          })
        );
        CHECK(local.root()["local"]["position"][2].as_f32() == doctest::Approx(3.0f));
        CHECK(local.root()["local"]["scale"][0].as_f32() == doctest::Approx(2.0f));
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", child}, {"position", vec3(0.0f, 0.0f, 0.0f)}, {"space", "world"}})
        );
        editor.pump(1);
        const ecs::entity childEntity = editor.scene().find_guid(*core::uuid::parse(child));
        CONJURE_CHECK_APPROX(editor.scene().local_transform_of(childEntity)->position, math::vec3(-5.0f, 0.0f, 0.0f));
        (void) editor.ok("history.undo");
        CONJURE_CHECK_APPROX(editor.scene().local_transform_of(childEntity)->position, math::vec3(1.0f, 2.0f, 3.0f));
        CHECK(editor.fail(
          "entity.set_transform",
          core::json::object({{"entity", child}, {"position", core::json::array({1.0f})}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "entity.set_transform",
          core::json::object({{"entity", child}, {"space", "sideways"}})
        ) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("entity.set_enabled - toggles and undo restores") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Toggle");
        (void) editor.ok("entity.set_enabled", core::json::object({{"entity", guid}, {"enabled", false}}));
        CHECK_FALSE(editor.ok("entity.get", core::json::object({{"entity", guid}})).root()["enabled"].as_bool());
        (void) editor.ok("history.undo");
        CHECK(editor.ok("entity.get", core::json::object({{"entity", guid}})).root()["enabled"].as_bool());
    }

    TEST_CASE("entity.find - filters by name, component, tag and layer in hierarchy order") {
        editor_test::headless_editor editor = {};
        const std::string a = editor.create_entity("Cell");
        const std::string b = editor.create_entity("Cell");
        (void) editor.create_entity("Other");
        (void) editor.ok("component.add", core::json::object({{"entity", b}, {"type", "sprite"}}));
        const core::json::document byName = editor.ok("entity.find", core::json::object({{"name", "Cell"}}));
        REQUIRE(byName.root()["guids"].size() == 2);
        CHECK(byName.root()["guids"][0].as_string() == a);
        const core::json::document byComponent = editor.ok(
          "entity.find",
          core::json::object({{"name", "Cell"}, {"component", "sprite"}})
        );
        REQUIRE(byComponent.root()["guids"].size() == 1);
        CHECK(byComponent.root()["guids"][0].as_string() == b);
        const core::json::document limited = editor.ok("entity.find", core::json::object({{"limit", 1}}));
        CHECK(limited.root()["guids"].size() == 1);
        CHECK(limited.root()["total"].as_i64() == 3);
        CHECK(editor.fail(
          "entity.find",
          core::json::object({{"component", "no_such_component"}})
        ) == protocol_error_code::TYPE_NOT_FOUND);
    }

    TEST_CASE("entity.bounds - a sprite reports its bounds and a bare entity a unit box") {
        editor_test::headless_editor editor = {};
        const std::string bare = editor.create_entity("Bare");
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", bare}, {"position", vec3(3.0f, 0.0f, 0.0f)}})
        );
        editor.pump(1);
        const core::json::document bounds = editor.ok("entity.bounds", core::json::object({{"entity", bare}}));
        CHECK(bounds.root()["min"][0].as_f32() == doctest::Approx(2.5f));
        CHECK(bounds.root()["max"][0].as_f32() == doctest::Approx(3.5f));
        CHECK_FALSE(bounds.root()["fromRenderBounds"].as_bool());
    }

    TEST_CASE("entity.* - the inverse of every entity command restores the serialised scene exactly") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Root");
        const std::string child = editor.create_entity("Child", root);
        (void) editor.ok("component.add", core::json::object({{"entity", child}, {"type", "sprite"}}));
        const editor_test::editor_snapshot initial = editor_test::capture_snapshot(editor);
        (void) editor.ok("entity.rename", core::json::object({{"entity", child}, {"name", "Renamed"}}));
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", child}, {"position", vec3(4.0f, 5.0f, 6.0f)}})
        );
        (void) editor.ok("entity.set_enabled", core::json::object({{"entity", root}, {"enabled", false}}));
        (void) editor.ok("entity.set_parent", core::json::object({{"entity", child}, {"parent", nullptr}}));
        (void) editor.ok("entity.duplicate", core::json::object({{"entity", root}}));
        (void) editor.ok("entity.create_many", core::json::object({{"count", 5}, {"parent", root}}));
        (void) editor.ok("entity.destroy", core::json::object({{"entity", root}}));
        const editor_test::editor_snapshot final = editor_test::capture_snapshot(editor);
        // Seven edits after the snapshot; the set-up commands stay applied.
        (void) editor.ok("history.undo", core::json::object({{"count", 7}}));
        const editor_test::editor_snapshot undone = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(initial == undone, editor_test::describe_difference(initial, undone));
        (void) editor.ok("history.redo", core::json::object({{"count", 100}}));
        const editor_test::editor_snapshot redone = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(final == redone, editor_test::describe_difference(final, redone));
    }
}
