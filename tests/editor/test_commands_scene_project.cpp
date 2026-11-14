#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "editor_test_support.h"

#include "conjure/core/dynamic_library.h"
#include "conjure/core/json/document.h"
#include "conjure/editor/editor_application.h"
#include "conjure/editor/editor_options.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/undo_stack.h"
#include "conjure/engine/application.h"
#include "conjure/engine/project.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <system_error>

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

    TEST_CASE("scene.open - a malformed scene file answers IO_ERROR and keeps the open scene") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        (void) editor.ok("scene.save");
        const std::filesystem::path broken = editor.project_directory() / "assets" / "scenes" / "broken.scene.json";
        {
            std::ofstream output(broken, std::ios::binary | std::ios::trunc);
            REQUIRE(output.is_open());
            output << "{ not a scene";
        }
        const std::string pathBefore = editor.context().scene_path();
        const u64 entitiesBefore = editor.scene().world().entity_count();
        editor.log().allow_errors();
        const protocol_response response = editor.send("scene.open", core::json::object({{"path", "scenes/broken"}}));
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorCode == protocol_error_code::IO_ERROR);
        CHECK(response.errorMessage.find("broken") != std::string::npos);
        CHECK(editor.context().scene_path() == pathBefore);
        CHECK(editor.scene().world().entity_count() == entitiesBefore);
        (void) editor.ok("entity.get", core::json::object({{"entity", "Kept"}}));
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

    TEST_CASE("regression - N2 a play session never crosses a project restart") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path other = editor.project_directory() / "second";
        (void) editor.ok("project.create", core::json::object({{"path", other.generic_string()}, {"name", "Second"}}));
        (void) editor.create_entity("Old project");
        (void) editor.ok("play.start");
        CHECK(editor.fail(
          "project.open",
          core::json::object({{"path", other.generic_string()}})
        ) == protocol_error_code::WRONG_PLAY_STATE);
        // A host request does not go through project.open's play-state check: the restart itself leaves play.
        CONJURE_REQUIRE_OK(editor.application().request_open_project(other));
        editor.pump(1);
        CHECK_FALSE(editor.application().has_pending_project_open());
        REQUIRE(editor.context().project() != nullptr);
        CHECK(editor.context().project()->description().name == "Second");
        CHECK(editor.context().play().is_editing());
        CHECK_FALSE(editor.context().undo().frozen());
        CHECK(editor.context().engine().current_simulation_mode() == engine::simulation_mode::PAUSED);
        CHECK(editor.ok("play.state").root()["state"].as_string() == "editing");
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

    TEST_CASE("project.export - runtimePath is null when the runtime was not copied and names the copied executable "
      "otherwise") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Exported");
        (void) editor.ok("scene.save");
        const std::filesystem::path root = editor.project_directory();
        const core::json::document plain = editor.ok(
          "project.export",
          core::json::object({
            {"directory", (root / "export_plain").generic_string()},
            {"copyRuntime", false},
            {"publishManaged", false},
          })
        );
        CHECK(plain.root()["runtimePath"].is_null());

        // A stand-in distribution directory: both spellings of the runtime executable (the pipeline copies the
        // one of this platform) and the shared library.
        const std::filesystem::path distribution = root / "distribution";
        std::error_code ignored = {};
        std::filesystem::create_directories(distribution, ignored);
        const std::array<std::string, 3> names = {
            "conjure_runtime", "conjure_runtime.exe", core::dynamic_library::decorate_name("conjure_shared"),
        };
        for (const std::string &name : names) {
            std::ofstream stream(distribution / name, std::ios::binary | std::ios::trunc);
            REQUIRE(stream.is_open());
            stream << "stand-in";
        }
        const std::filesystem::path output = root / "export_runtime";
        const core::json::document copied = editor.ok(
          "project.export",
          core::json::object({
            {"directory", output.generic_string()},
            {"distributionDirectory", distribution.generic_string()},
            {"publishManaged", false},
          })
        );
        REQUIRE(copied.root()["runtimePath"].is_string());
        const std::filesystem::path runtime = std::filesystem::path(
          std::string(copied.root()["runtimePath"].as_string())
        );
        CHECK(runtime.parent_path() == output);
        CHECK(runtime.stem().generic_string() == "conjure_runtime");
        CHECK(std::filesystem::is_regular_file(runtime));
    }

    TEST_CASE("regression - F029 project.export without a distribution directory warns that the runtime was not "
      "copied") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Exported");
        (void) editor.ok("scene.save");
        const std::filesystem::path root = editor.project_directory();
        const auto runtimeWarnings = [](const core::json::document &result) {
            usize count = 0;
            for (const core::json::value warning : result.root()["warnings"]) {
                if (warning.as_string().find("the runtime executable") != std::string_view::npos) {
                    ++count;
                }
            }
            return count;
        };
        // What conjure_editor --export sends without --distribution.
        const core::json::document plain = editor.ok(
          "project.export",
          core::json::object({{"directory", (root / "export_plain").generic_string()}, {"publishManaged", false}})
        );
        CHECK(runtimeWarnings(plain) == 1);
        CHECK(plain.root()["runtimePath"].is_null());
        // A client that turned the copy off on purpose is not told about it.
        const core::json::document chosen = editor.ok(
          "project.export",
          core::json::object({
            {"directory", (root / "export_chosen").generic_string()},
            {"copyRuntime", false},
            {"publishManaged", false},
          })
        );
        CHECK(runtimeWarnings(chosen) == 0);
    }

    TEST_CASE("regression - F029 conjure_editor --export forwards --distribution so the runtime is copied") {
        testing::log_capture logs = {};
        testing::temp_directory directory = {};
        const std::filesystem::path project = directory.path() / "project";
        std::error_code ignored = {};
        std::filesystem::create_directories(project / "assets", ignored);
        {
            std::ofstream stream(project / "project.json", std::ios::binary | std::ios::trunc);
            REQUIRE(stream.is_open());
            stream << R"({"schema_version": 1, "name": "cli_export", "assets_root": "assets"})";
        }
        const std::filesystem::path distribution = directory.path() / "distribution";
        std::filesystem::create_directories(distribution, ignored);
        const std::array<std::string, 3> names = {
            "conjure_runtime", "conjure_runtime.exe", core::dynamic_library::decorate_name("conjure_shared"),
        };
        for (const std::string &name : names) {
            std::ofstream stream(distribution / name, std::ios::binary | std::ios::trunc);
            REQUIRE(stream.is_open());
            stream << "stand-in";
        }
        // What --project <dir> --export <out> [--distribution <dir>] parse into.
        const auto exportWith = [&project](const std::filesystem::path &output, const std::filesystem::path &from) {
            editor_options options = {};
            options.headless = true;
            options.aiPort = u16{0};
            options.projectPath = project;
            options.exportDirectory = output;
            options.exportDistribution = from;
            options.engineArguments = {"--log-level", "warn"};
            editor_application application = {};
            CONJURE_REQUIRE_OK(application.initialise(options, {}));
            const int code = application.run();
            application.shutdown();
            return code;
        };
        const auto hasRuntime = [](const std::filesystem::path &output) {
            std::error_code error = {};
            for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(output, error)) {
                if (entry.is_regular_file() && entry.path().stem().generic_string() == "conjure_runtime") {
                    return true;
                }
            }
            return false;
        };
        const std::filesystem::path copied = directory.path() / "out";
        CHECK(exportWith(copied, distribution) == 0);
        CHECK(hasRuntime(copied));
        const std::filesystem::path plain = directory.path() / "plain";
        CHECK(exportWith(plain, {}) == 0);
        CHECK(std::filesystem::is_regular_file(plain / "project.json"));
        CHECK_FALSE(hasRuntime(plain));
    }

    TEST_CASE("regression - F754 scene.save_as stores a backslash path canonically and refuses a path leaving the "
      "mount") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("x");
        const std::string canonical = "assets://scenes/nested/back.scene.json";
        const core::json::document saved = editor.ok(
          "scene.save_as",
          core::json::object({{"path", "scenes\\nested//./back"}})
        );
        CHECK(saved.root()["path"].as_string() == canonical);
        CHECK(editor.context().scene_path() == canonical);
        const std::filesystem::path scenes = editor.project_directory() / "assets" / "scenes";
        CHECK(std::filesystem::is_regular_file(scenes / "nested" / "back.scene.json"));
        const core::json::document opened = editor.ok("scene.open", core::json::object({{"path", canonical}}));
        CHECK(opened.root()["path"].as_string() == canonical);
        CHECK(editor.fail(
          "scene.save_as",
          core::json::object({{"path", "../outside"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.context().scene_path() == canonical);
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "outside.scene.json"));
        CHECK(editor.fail("scene.open", core::json::object({{"path", ".."}})) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "scene.new",
          core::json::object({{"name", "n"}, {"path", "../elsewhere"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.context().scene_path() == canonical);
        // A blank path is still a new, unsaved scene.
        const core::json::document blank = editor.ok("scene.new", core::json::object({{"name", "n"}, {"path", " "}}));
        CHECK(blank.root()["path"].as_string().empty());
    }

    TEST_CASE("regression - F705 project_file_snapshot sees project.json edits and ignores .conjure") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path root = editor.project_directory();
        const std::map<std::string, std::string> before = editor_test::project_file_snapshot(root);
        CHECK(before.contains("project.json"));
        std::error_code ignored = {};
        std::filesystem::create_directories(root / ".conjure", ignored);
        {
            std::ofstream output(root / ".conjure" / "x", std::ios::binary | std::ios::trunc);
            REQUIRE(output.is_open());
            output << "volatile";
        }
        CHECK(editor_test::project_file_snapshot(root) == before);
        // project::save rewrites the helper's text in its own spelling: the same settings compare equal.
        (void) editor.ok("project.save");
        CHECK(editor_test::project_file_snapshot(root) == before);
        (void) editor.ok(
          "project.set_settings",
          core::json::object({{"settings", core::json::object({{"startup_scene", "scenes/test.scene.json"}})}})
        );
        const std::map<std::string, std::string> edited = editor_test::project_file_snapshot(root);
        CHECK(edited != before);
        CHECK(edited.at("project.json") != before.at("project.json"));
        (void) editor.ok("history.undo");
        CHECK(editor_test::project_file_snapshot(root) == before);
    }
}
