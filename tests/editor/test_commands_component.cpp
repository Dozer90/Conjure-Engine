#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/core/string_id.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/scene/scene.h"

#include <array>
#include <optional>
#include <string>

// component.* (docs/design/04_editor.md 3.5, R40): values move through JSON only.

using namespace conjure;
using namespace conjure::editor;

namespace {
    // A dynamic component whose registered name starts with the "transform" alias. Namespace-scope ids are
    // constant-initialised and never interned; intern() keeps the field name visible.
    const std::array<ecs::dynamic_field_desc, 1> RIG_FIELDS = {
        ecs::dynamic_field_desc{core::string_id::intern("weight"), core::reflect::type_id_of<f32>(), 0},
    };
}

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

    TEST_CASE("regression - F524 component.set prefers a full registered type name whose first segment is an alias") {
        editor_test::headless_editor editor = {};
        ecs::component_registry *components = editor.context().engine_context().components;
        REQUIRE(components != nullptr);
        ecs::dynamic_component_desc desc = {};
        desc.name = core::string_id::intern("transform.rig");
        desc.fields = RIG_FIELDS;
        desc.size = sizeof(f32);
        desc.alignment = alignof(f32);
        CONJURE_REQUIRE_OK(components->register_dynamic(desc));
        const std::string guid = editor.create_entity("Rigged");
        (void) editor.ok("component.add", core::json::object({{"entity", guid}, {"type", "transform.rig"}}));
        // "transform" alone is the alias of the local transform, which has no "rig" field.
        (void) editor.ok(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "transform.rig.weight"}, {"value", 2}})
        );
        const core::json::document got = editor.ok(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "transform.rig.weight"}})
        );
        CHECK(got.root()["value"].as_f32() == doctest::Approx(2.0f));
        CHECK(got.root()["type"].as_string() == "transform.rig");
        // The short paths of the transform still resolve through the alias.
        const core::json::document position = core::json::array({1.0f, 2.0f, 3.0f});
        (void) editor.ok(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "transform.position"}, {"value", position}})
        );
        (void) editor.ok("history.undo");
        (void) editor.ok("history.undo");
        CHECK(editor.ok(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "transform.rig.weight"}})
        ).root()["value"].as_f32() == doctest::Approx(0.0f));
    }

    TEST_CASE("regression - F524 component.remove accepts the registered type name the inspector sends") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Target");
        (void) editor.ok("component.add", core::json::object({{"entity", guid}, {"type", "sprite"}}));
        const core::json::document size = core::json::array({2.0f, 2.0f});
        (void) editor.ok(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "conjure.renderer.sprite.size"}, {"value", size}})
        );
        (void) editor.ok(
          "component.remove",
          core::json::object({{"entity", guid}, {"type", "conjure.renderer.sprite"}})
        );
        CHECK(editor.fail(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "sprite"}})
        ) == protocol_error_code::COMPONENT_NOT_FOUND);
    }

    TEST_CASE("regression - F648 component.set on the name component refuses '/' and the empty name") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Named");
        CHECK(editor.fail(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "name.value"}, {"value", "a/b"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "name"}, {"value", core::json::object({{"value", ""}})}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "component.set_many",
          core::json::object({{"sets", core::json::array({core::json::object({
            {"entity", guid},
            {"path", "name.value"},
            {"value", "x/y"},
          })})}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.ok("entity.get", core::json::object({{"entity", guid}})).root()["name"].as_string() == "Named");
        (void) editor.ok(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "name.value"}, {"value", "Fine"}})
        );
        CHECK(editor.ok("entity.get", core::json::object({{"entity", guid}})).root()["name"].as_string() == "Fine");
    }

    TEST_CASE("regression - N4 component.set, component.add and entity.create refuse a layer index of 32 or more") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Layered");
        const std::optional<core::uuid> parsedGuid = core::uuid::parse(guid);
        REQUIRE(parsedGuid.has_value());
        const auto layerIndex = [&editor](const std::string &entity) {
            return editor.ok(
              "component.get",
              core::json::object({{"entity", entity}, {"path", "layer.index"}})
            ).root()["value"].as_i64();
        };
        (void) editor.ok(
          "component.add",
          core::json::object({{"entity", guid}, {"type", "layer"}, {"value", core::json::object({{"index", 5}})}})
        );
        CHECK(layerIndex(guid) == 5);
        CHECK(editor.fail(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "layer.index"}, {"value", 32}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "layer"}, {"value", core::json::object({{"index", 200}})}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(layerIndex(guid) == 5);
        (void) editor.ok(
          "component.set",
          core::json::object({{"entity", guid}, {"path", "layer.index"}, {"value", 31}})
        );
        CHECK(layerIndex(guid) == 31);
        CHECK(editor.scene().layer_of(editor.scene().find_guid(*parsedGuid)) == 31);
        // Every capture accepts what the write path let through.
        (void) editor.ok("scene.save");
        (void) editor.ok("play.start");
        (void) editor.ok("play.stop");

        const std::string other = editor.create_entity("Other");
        CHECK(editor.fail(
          "component.add",
          core::json::object({{"entity", other}, {"type", "layer"}, {"value", core::json::object({{"index", 40}})}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "component.get",
          core::json::object({{"entity", other}, {"path", "layer"}})
        ) == protocol_error_code::COMPONENT_NOT_FOUND);
        CHECK(editor.fail(
          "entity.create",
          core::json::object({
            {"name", "Refused"},
            {"components", core::json::object({{"layer", core::json::object({{"index", 99}})}})},
          })
        ) == protocol_error_code::INVALID_ARGS);
    }
}
