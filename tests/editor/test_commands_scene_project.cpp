#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/engine/project.h"

#include <filesystem>
#include <string>

// scene.* and project.* (docs/design/04_editor.md 4.4).

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/commands_scene_project") {

    TEST_CASE("scene.new / save / open - round trip through the project's assets") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        const std::string child = editor.create_entity("Child", "Kept");
        (void) editor.ok("component.add", core::json::object({{"entity", child}, {"type", "sprite"}}));
        const u64 hash = editor.world_hash();
        const core::json::document saved = editor.ok("scene.save");
        CHECK(saved.root()["path"].as_string() == "assets://scenes/test.scene.json");
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "assets" / "scenes" / "test.scene.json"));
        CHECK_FALSE(editor.context().scene_dirty());
        (void) editor.ok("scene.new", core::json::object({{"name", "empty"}}));
        CHECK(editor.scene().world().entity_count() == 0);
        CHECK(editor.context().undo().size() == 0);
        editor.clear_events();
        const core::json::document opened = editor.ok("scene.open", core::json::object({{"path", "scenes/test"}}));
        CHECK(opened.root()["entityCount"].as_i64() == 2);
        CHECK(editor.saw_event("scene.loaded"));
        CHECK(editor.world_hash() == hash);
        CHECK(editor.context().scene_path() == "assets://scenes/test.scene.json");
        (void) editor.ok("entity.get", core::json::object({{"entity", "Kept/Child"}}));
        CHECK(editor.fail(
          "scene.open",
          core::json::object({{"path", "scenes/missing"}})
        ) == protocol_error_code::ASSET_NOT_FOUND);
        const core::json::document scenes = editor.ok("project.list_scenes");
        CHECK(scenes.root()["count"].as_i64() == 1);
        CHECK(scenes.root()["scenes"][0].as_string() == "assets://scenes/test.scene.json");
    }

    TEST_CASE("scene.save_as / close - a new path, the dirty guard and discard") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("x");
        const core::json::document savedAs = editor.ok("scene.save_as", core::json::object({{"path", "scenes/other"}}));
        CHECK(savedAs.root()["path"].as_string() == "assets://scenes/other.scene.json");
        CHECK(editor.context().scene_path() == "assets://scenes/other.scene.json");
        (void) editor.create_entity("dirty");
        CHECK(editor.fail("scene.close") == protocol_error_code::INVALID_ARGS);
        CHECK(editor.context().has_scene());
        (void) editor.ok("scene.close", core::json::object({{"discard", true}}));
        CHECK_FALSE(editor.context().has_scene());
        CHECK(editor.scene().world().entity_count() == 0);
        CHECK(editor.fail("scene.save") == protocol_error_code::SCENE_NOT_OPEN);
    }

    TEST_CASE("scene.info - name, path, entity count and dirty flag") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("a");
        const core::json::document info = editor.ok("scene.info");
        CHECK(info.root()["name"].as_string() == "test_scene");
        CHECK(info.root()["path"].as_string() == "assets://scenes/test.scene.json");
        CHECK(info.root()["entityCount"].as_i64() == 1);
        CHECK(info.root()["dirty"].as_bool());
    }

    TEST_CASE("scene.query - with, without, fields, limit and offset in hierarchy order") {
        editor_test::headless_editor editor = {};
        const std::string a = editor.create_entity("A");
        const std::string b = editor.create_entity("B");
        const std::string c = editor.create_entity("C", a);
        (void) editor.ok("component.add", core::json::object({{"entity", a}, {"type", "sprite"}}));
        (void) editor.ok("component.add", core::json::object({{"entity", c}, {"type", "sprite"}}));
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", c}, {"position", core::json::array({1.0f, 2.0f, 3.0f})}})
        );
        const core::json::document sprites = editor.ok(
          "scene.query",
          core::json::object({{"with", core::json::array({"sprite"})}})
        );
        REQUIRE(sprites.root()["rows"].size() == 2);
        CHECK(sprites.root()["rows"][0]["guid"].as_string() == a);
        CHECK(sprites.root()["rows"][1]["guid"].as_string() == c);
        CHECK(sprites.root()["rows"][1]["path"].as_string() == "A/C");
        const core::json::document without = editor.ok(
          "scene.query",
          core::json::object({{"without", core::json::array({"sprite"})}})
        );
        REQUIRE(without.root()["rows"].size() == 1);
        CHECK(without.root()["rows"][0]["guid"].as_string() == b);
        const core::json::document fields = editor.ok(
          "scene.query",
          core::json::object({
            {"with", core::json::array({"sprite"})},
            {"fields", core::json::array({"transform.position"})},
            {"limit", 1},
            {"offset", 1},
          })
        );
        REQUIRE(fields.root()["rows"].size() == 1);
        CHECK(fields.root()["rows"][0]["guid"].as_string() == c);
        CHECK(fields.root()["rows"][0]["fields"]["transform.position"][2].as_f32() == doctest::Approx(3.0f));
        CHECK(fields.root()["total"].as_i64() == 2);
        CHECK(editor.fail(
          "scene.query",
          core::json::object({{"with", core::json::array({"nope"})}})
        ) == protocol_error_code::TYPE_NOT_FOUND);
    }

    TEST_CASE("scene.get_hierarchy - respects the depth limit and an explicit root") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Root");
        const std::string child = editor.create_entity("Child", root);
        (void) editor.create_entity("Grandchild", child);
        const core::json::document whole = editor.ok("scene.get_hierarchy");
        REQUIRE(whole.root()["roots"].size() == 1);
        CHECK(whole.root()["roots"][0]["name"].as_string() == "Root");
        CHECK(whole.root()["roots"][0]["children"][0]["children"][0]["name"].as_string() == "Grandchild");
        const core::json::document shallow = editor.ok("scene.get_hierarchy", core::json::object({{"depth", 1}}));
        CHECK(shallow.root()["roots"][0]["children"][0]["children"].size() == 0);
        const core::json::document fromChild = editor.ok("scene.get_hierarchy", core::json::object({{"root", child}}));
        REQUIRE(fromChild.root()["roots"].size() == 1);
        CHECK(fromChild.root()["roots"][0]["guid"].as_string() == child);
    }

    TEST_CASE("scene.set_settings - merges, replaces and undoes") {
        editor_test::headless_editor editor = {};
        (void) editor.ok(
          "scene.set_settings",
          core::json::object({{"settings", core::json::object({{"gravity", -9.8}, {"sky", "blue"}})}})
        );
        (void) editor.ok(
          "scene.set_settings",
          core::json::object({{"settings", core::json::object({{"sky", "red"}})}})
        );
        core::json::document info = editor.ok("scene.info");
        CHECK(info.root()["settings"]["sky"].as_string() == "red");
        CHECK(info.root()["settings"]["gravity"].as_f64() == doctest::Approx(-9.8));
        (void) editor.ok("history.undo");
        CHECK(editor.ok("scene.info").root()["settings"]["sky"].as_string() == "blue");
        (void) editor.ok(
          "scene.set_settings",
          core::json::object({{"settings", core::json::object({{"only", 1}})}, {"replace", true}})
        );
        info = editor.ok("scene.info");
        CHECK_FALSE(info.root()["settings"].contains("gravity"));
        CHECK(info.root()["settings"]["only"].as_i64() == 1);
        CHECK(editor.fail(
          "scene.set_settings",
          core::json::object({{"settings", 5}})
        ) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("project.create / info / set_settings - a project is written and its settings edited with undo") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path other = editor.project_directory() / "other_project";
        const core::json::document created = editor.ok(
          "project.create",
          core::json::object({{"path", other.generic_string()}, {"name", "Other"}})
        );
        CHECK(created.root()["name"].as_string() == "Other");
        CHECK(std::filesystem::is_regular_file(other / "project.json"));
        CHECK(std::filesystem::is_directory(other / "assets" / "scenes"));
        CHECK(std::filesystem::is_directory(other / "scripts"));
        CHECK(editor.fail(
          "project.create",
          core::json::object({{"path", other.generic_string()}, {"name", "Again"}})
        ) == protocol_error_code::INVALID_ARGS);
        const core::result<engine::project> loaded = engine::project::load(other);
        CONJURE_REQUIRE_OK(loaded);
        CHECK(loaded.value().description().name == "Other");

        const core::json::document info = editor.ok("project.info");
        CHECK(info.root()["name"].as_string() == "editor_test");
        CHECK(info.root()["path"].as_string() == editor.project_directory().generic_string());
        (void) editor.ok(
          "project.set_settings",
          core::json::object({{"settings", core::json::object({{"startup_scene", "scenes/test.scene.json"}})}})
        );
        CHECK(editor.ok("project.info").root()["startupScene"].as_string() == "scenes/test.scene.json");
        CHECK(editor.context().project()->description().startupScene.generic_string() == "scenes/test.scene.json");
        (void) editor.ok("history.undo");
        CHECK(editor.ok("project.info").root()["startupScene"].as_string().empty());
        CHECK(editor.fail(
          "project.set_settings",
          core::json::object({{"settings", core::json::object({{"schema_version", 99}})}})
        ) == protocol_error_code::INVALID_ARGS);
        (void) editor.ok("project.save");
    }

    TEST_CASE("regression - project.create serialises a name with quotes, backslashes and control characters") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path other = editor.project_directory() / "odd_name";
        const std::string name = "Say \"hi\" \\ tab\there";
        const core::json::document created = editor.ok(
          "project.create",
          core::json::object({{"path", other.generic_string()}, {"name", name}})
        );
        CHECK(created.root()["name"].as_string() == name);
        const core::result<engine::project> loaded = engine::project::load(other);
        CONJURE_REQUIRE_OK(loaded);
        CHECK(loaded.value().description().name == name);
    }

    TEST_CASE("project.open - restarts the engine over another project between frames") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path other = editor.project_directory() / "second";
        (void) editor.ok("project.create", core::json::object({{"path", other.generic_string()}, {"name", "Second"}}));
        (void) editor.create_entity("gone after the switch");
        const core::json::document opened = editor.ok(
          "project.open",
          core::json::object({{"path", other.generic_string()}})
        );
        CHECK(opened.root()["pending"].as_bool());
        CHECK(editor.application().has_pending_project_open());
        editor.pump(1);
        CHECK_FALSE(editor.application().has_pending_project_open());
        REQUIRE(editor.context().project() != nullptr);
        CHECK(editor.context().project()->description().name == "Second");
        CHECK(editor.context().undo().size() == 0);
        CHECK(editor.ok("project.info").root()["name"].as_string() == "Second");
        CHECK(editor.fail(
          "project.open",
          core::json::object({{"path", (other / "nowhere").generic_string()}})
        ) == protocol_error_code::ASSET_NOT_FOUND);
    }

    TEST_CASE("project.export - produces a runnable folder with the cooked content") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Exported");
        (void) editor.ok("scene.save");
        (void) editor.ok(
          "project.set_settings",
          core::json::object({{"settings", core::json::object({{"startup_scene", "scenes/test.scene.json"}})}})
        );
        const std::filesystem::path output = editor.project_directory() / "export";
        editor.clear_events();
        const core::json::document result = editor.ok(
          "project.export",
          core::json::object({
            {"directory", output.generic_string()},
            {"copyRuntime", false},
            {"publishManaged", false},
          })
        );
        CHECK(result.root()["assetCount"].as_i64() >= 1);
        CHECK(result.root()["steps"].size() >= 1);
        CHECK(std::filesystem::is_directory(output));
        CHECK(std::filesystem::is_regular_file(output / "project.json"));
        CHECK(editor.saw_event("progress"));
        CHECK(editor.fail(
          "project.export",
          core::json::object({{"directory", ""}})
        ) == protocol_error_code::INVALID_ARGS);
    }
}
