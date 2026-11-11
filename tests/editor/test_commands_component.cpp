#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/uuid.h"
#include "conjure/editor/protocol/protocol_message.h"

#include <string>

// component.* (docs/design/04_editor.md 3.5, R40): values move through JSON only.

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/commands_component") {

    TEST_CASE("component.add / remove / set - each is undoable and restores the serialised scene") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Target");
        const editor_test::editor_snapshot bare = editor_test::capture_snapshot(editor);
        const core::json::document added = editor.ok(
          "component.add",
          core::json::object({{"entity", guid}, {"type", "sprite"}})
        );
        CHECK(added.root()["type"].as_string() == "conjure.renderer.sprite");
        const editor_test::editor_snapshot withSprite = editor_test::capture_snapshot(editor);
        const core::json::document set = editor.ok(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "sprite.size"}, {"value", core::json::array({3.0f, 4.0f})}})
        );
        CHECK(set.root()["previous"][0].as_f32() == doctest::Approx(1.0f));
        const core::json::document got = editor.ok(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "sprite.size"}})
        );
        CHECK(got.root()["value"][1].as_f32() == doctest::Approx(4.0f));
        CHECK(got.root()["kind"].as_string() == "VEC2");
        CHECK(got.root()["type"].as_string() == "conjure.renderer.sprite");
        (void) editor.ok("component.remove", core::json::object({{"entity", guid}, {"type", "sprite"}}));
        CHECK(editor.fail(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "sprite.size"}})
        ) == protocol_error_code::COMPONENT_NOT_FOUND);
        (void) editor.ok("history.undo");
        CHECK(editor.ok(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "sprite.size"}})
        ).root()["value"][1].as_f32() == doctest::Approx(4.0f));
        (void) editor.ok("history.undo");
        const editor_test::editor_snapshot afterUndoSet = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(withSprite == afterUndoSet, editor_test::describe_difference(withSprite, afterUndoSet));
        (void) editor.ok("history.undo");
        const editor_test::editor_snapshot afterUndoAdd = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(bare == afterUndoAdd, editor_test::describe_difference(bare, afterUndoAdd));
    }

    TEST_CASE("component.set_many - one undo entry, all or nothing") {
        editor_test::headless_editor editor = {};
        const std::string a = editor.create_entity("A");
        const std::string b = editor.create_entity("B");
        const core::json::document sets = core::json::array({
          core::json::object({{"entity", a}, {"path", "transform.position.x"}, {"value", 5.0f}}),
          core::json::object({{"entity", b}, {"path", "transform.position.y"}, {"value", 6.0f}}),
        });
        const core::json::document result = editor.ok("component.set_many", core::json::object({{"sets", sets}}));
        CHECK(result.root()["applied"].as_i64() == 2);
        CHECK(editor.context().undo().size() == 3);
        CHECK(editor.ok(
          "component.get",
          core::json::object({{"entity", b}, {"path", "transform.position.y"}})
        ).root()["value"].as_f32() == doctest::Approx(6.0f));
        const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
        const core::json::document failing = core::json::array({
          core::json::object({{"entity", a}, {"path", "transform.position.x"}, {"value", 50.0f}}),
          core::json::object({{"entity", b}, {"path", "transform.nothing"}, {"value", 1.0f}}),
        });
        CHECK(editor.fail(
          "component.set_many",
          core::json::object({{"sets", failing}})
        ) == protocol_error_code::PATH_NOT_FOUND);
        const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(before == after, editor_test::describe_difference(before, after));
        (void) editor.ok("history.undo");
        CHECK(editor.ok(
          "component.get",
          core::json::object({{"entity", a}, {"path", "transform.position.x"}})
        ).root()["value"].as_f32() == doctest::Approx(0.0f));
        CHECK(editor.fail(
          "component.set_many",
          core::json::object({{"sets", core::json::document::array()}})
        ) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("component.set - a bare type name replaces the whole component") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Whole");
        (void) editor.ok("component.add", core::json::object({{"entity", guid}, {"type", "sprite"}}));
        const core::json::document whole = core::json::object({
          {"size", core::json::array({7.0f, 8.0f})},
          {"layer", 3},
        });
        (void) editor.ok("component.set", core::json::object({{"entity", guid}, {"path", "sprite"}, {"value", whole}}));
        const core::json::document got = editor.ok(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "sprite"}})
        );
        CHECK(got.root()["value"]["size"][0].as_f32() == doctest::Approx(7.0f));
        CHECK(got.root()["value"]["layer"].as_i64() == 3);
        CHECK(got.root()["kind"].as_string() == "STRUCT");
        CHECK(editor.fail(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "sprite"}, {"value", 1}})
        ) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("component.set / get - a type name written in full is accepted as the path") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Qualified");
        (void) editor.ok("component.add", core::json::object({{"entity", guid}, {"type", "sprite"}}));
        const core::json::document set = editor.ok(
          "component.set",
          core::json::object({
            {"entity", guid},
            {"path", "conjure.renderer.sprite.size"},
            {"value", core::json::array({3.0f, 4.0f})},
          })
        );
        CHECK(set.root()["path"].as_string() == "conjure.renderer.sprite.size");
        const core::json::document got = editor.ok(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "sprite.size"}})
        );
        CHECK(got.root()["value"][0].as_f64() == doctest::Approx(3.0));
        const core::json::document whole = editor.ok(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "conjure.renderer.sprite"}})
        );
        CHECK(whole.root()["value"]["size"][1].as_f64() == doctest::Approx(4.0));
        CHECK(editor.fail(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "conjure.renderer.nothing.size"}})
        ) == protocol_error_code::TYPE_NOT_FOUND);
        (void) editor.ok("history.undo");
        const core::json::document restored = editor.ok(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "sprite.size"}})
        );
        CHECK(restored.root()["value"][0].as_f64() == doctest::Approx(1.0));
    }

    TEST_CASE("component.set - a missing path is PATH_NOT_FOUND and a type mismatch INVALID_ARGS") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Paths");
        CHECK(editor.fail(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "transform.wibble"}, {"value", 1}})
        ) == protocol_error_code::PATH_NOT_FOUND);
        CHECK(editor.fail(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "transform.position.x"}, {"value", "one"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "no_such_type.x"}, {"value", 1}})
        ) == protocol_error_code::TYPE_NOT_FOUND);
        CHECK(editor.fail(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "transform..x"}, {"value", 1}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "component.set",
          core::json::object({{"entity", "Nobody"}, {"path", "transform.position.x"}, {"value", 1}})
        ) == protocol_error_code::ENTITY_NOT_FOUND);
    }

    TEST_CASE("regression - an entity reference written through the protocol must be a guid string or null") {
        editor_test::headless_editor editor = {};
        const std::string parent = editor.create_entity("Parent");
        const std::string child = editor.create_entity("Child", parent);
        // The archive encoding {"external": "index:generation"} carries raw ecs ids and is refused (D5).
        CHECK(editor.fail(
          "component.set",
          core::json::object({
            {"entity", child},
            {"path", "parent.value"},
            {"value", core::json::object({{"external", "0:1"}})},
          })
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "component.set",
          core::json::object({{"entity", child}, {"path", "parent.value"}, {"value", 7}})
        ) == protocol_error_code::INVALID_ARGS);
        const std::string unknown = core::uuid::generate().to_string();
        CHECK(editor.fail(
          "component.set",
          core::json::object({{"entity", child}, {"path", "parent.value"}, {"value", unknown}})
        ) == protocol_error_code::ENTITY_NOT_FOUND);
        // The parent link still names the parent by guid when read back.
        const core::json::document got = editor.ok(
          "component.get",
          core::json::object({{"entity", child}, {"path", "parent.value"}})
        );
        CHECK(got.root()["value"].as_string() == parent);
    }

    TEST_CASE("component.add - adding twice and removing the transform are INVALID_ARGS") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Dup");
        (void) editor.ok("component.add", core::json::object({{"entity", guid}, {"type", "camera"}}));
        CHECK(editor.fail(
          "component.add",
          core::json::object({{"entity", guid}, {"type", "camera"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "component.add",
          core::json::object({{"entity", guid}, {"type", "no_such_type"}})
        ) == protocol_error_code::TYPE_NOT_FOUND);
        CHECK(editor.fail(
          "component.remove",
          core::json::object({{"entity", guid}, {"type", "transform"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "component.remove",
          core::json::object({{"entity", guid}, {"type", "sprite"}})
        ) == protocol_error_code::COMPONENT_NOT_FOUND);
    }

    TEST_CASE("component.list - names the editable components of an entity") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Listed");
        (void) editor.ok("component.add", core::json::object({{"entity", guid}, {"type", "sprite"}}));
        const core::json::document list = editor.ok("component.list", core::json::object({{"entity", guid}}));
        bool sprite = false;
        bool transform = false;
        for (const core::json::value name : list.root()["types"]) {
            sprite = sprite || name.as_string() == "conjure.renderer.sprite";
            transform = transform || name.as_string() == "conjure.scene.local_transform";
        }
        CHECK(sprite);
        CHECK(transform);
        CHECK(list.root()["count"].as_u64() == list.root()["types"].size());
    }

    TEST_CASE("component.set - the script component's fields are reachable by path") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Scripted");
        (void) editor.ok(
          "script.attach",
          core::json::object({
            {"entity", guid},
            {"className", "Game.Mover"},
            {"fields", core::json::object({{"speed", 1.5f}})},
          })
        );
        const core::json::document got = editor.ok(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "script.scripts[0].fields.speed"}})
        );
        CHECK(got.root()["value"].as_f32() == doctest::Approx(1.5f));
        (void) editor.ok(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "script.scripts[0].fields.speed"}, {"value", 9.0f}})
        );
        CHECK(editor.ok(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "script.scripts[0].fields.speed"}})
        ).root()["value"].as_f32() == doctest::Approx(9.0f));
        (void) editor.ok("history.undo");
        CHECK(editor.ok(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "script.scripts[0].fields.speed"}})
        ).root()["value"].as_f32() == doctest::Approx(1.5f));
        (void) editor.ok("history.undo");
        CHECK(editor.fail(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "script"}})
        ) == protocol_error_code::COMPONENT_NOT_FOUND);
    }

    TEST_CASE("component.set - entity references travel as guid strings") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Camera");
        (void) editor.ok("component.add", core::json::object({{"entity", guid}, {"type", "camera"}}));
        const core::json::document got = editor.ok(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "camera.cameraData.projection"}})
        );
        CHECK(got.root()["value"].as_string() == "PERSPECTIVE");
        (void) editor.ok(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "camera.cameraData.projection"}, {"value", "ORTHOGRAPHIC"}})
        );
        CHECK(editor.ok(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "camera.cameraData.projection"}})
        ).root()["value"].as_string() == "ORTHOGRAPHIC");
        CHECK(editor.fail(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "camera.cameraData.projection"}, {"value", "SIDEWAYS"}})
        ) == protocol_error_code::INVALID_ARGS);
    }
}
