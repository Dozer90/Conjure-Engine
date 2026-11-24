#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "editor_test_support.h"

#include "conjure/core/dynamic_library.h"
#include "conjure/core/json/document.h"
#include "conjure/core/process.h"
#include "conjure/editor/editor_application.h"
#include "conjure/editor/editor_options.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/undo_stack.h"
#include "conjure/engine/application.h"
#include "conjure/engine/export.h"
#include "conjure/engine/project.h"
#include "conjure/modules/module_abi.h"

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
        ) == protocol_error_code::PATH_NOT_FOUND);
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
        ) == protocol_error_code::PATH_NOT_FOUND);
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

    TEST_CASE("regression - follow-up FU-PDB project.export publishes managed code by default for a project that "
      "enables scripting") {
        // project.export named no bootstrap project, so step 3 only warned and no export made through the editor
        // shipped managed code; a headless editor, which runs the null host, did not even ask for it. The exported
        // runtime of such a project starts the .NET host and found no managed directory. The fixture's project
        // leaves scripting at its default (enabled) and its dotnet does not exist: an export that runs step 3 fails
        // there, naming that dotnet, before it writes project.json.
        editor_test::headless_editor editor = {};
        (void) editor.ok("scene.save");
        const std::filesystem::path output = editor.project_directory() / "exported";
        const protocol_response response = editor.send(
          "project.export",
          core::json::object({{"directory", output.generic_string()}, {"copyRuntime", false}})
        );
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorMessage.find("publish_managed") != std::string::npos);
        CHECK(response.errorMessage.find(editor_test::missing_dotnet_executable()) != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(output / "project.json"));
    }

    TEST_CASE("regression - follow-up FU-PDB project.export publishes no managed code for a project without scripting "
      "or when told not to") {
        editor_test::headless_editor_options options = {};
        core::json::document args = core::json::object({{"copyRuntime", false}});
        SUBCASE("the project disables scripting") {
            options.extraProjectJson = R"("scripting": {"enabled": false})";
        }
        SUBCASE("publishManaged is false") {
            args.set("publishManaged", false);
        }
        editor_test::headless_editor editor(options);
        (void) editor.ok("scene.save");
        const std::filesystem::path output = editor.project_directory() / "exported";
        args.set("directory", output.generic_string());
        const core::json::document result = editor.ok("project.export", std::move(args));
        CHECK(std::filesystem::is_regular_file(output / "project.json"));
        CHECK_FALSE(std::filesystem::exists(output / "managed"));
        CHECK(result.root()["warnings"].size() == 0);
    }

    TEST_CASE("regression - code sweep FU-NOSCRIPTING project.export of a project that enables scripting fails in an "
      "editor run with --no-scripting") {
        // The default of publishManaged also required this editor's scripting, and the pipeline skipped step 3
        // while scripting was disabled, so the export succeeded and shipped a game whose runtime starts the .NET host
        // and finds no managed code. Now the reply carries the pipeline's refusal, which names --no-scripting and
        // the project's scripts, and nothing is written.
        editor_test::headless_editor_options options = {};
        options.scripting = false;
        editor_test::headless_editor editor(options);
        (void) editor.ok("scene.save");
        const std::filesystem::path output = editor.project_directory() / "exported";
        core::json::document args = core::json::object({{"copyRuntime", false}});
        args.set("directory", output.generic_string());
        std::string scriptsNamed = "names no game scripts";
        SUBCASE("the project has game scripts") {
            const std::filesystem::path scripts = editor.context().project()->root_directory() / "scripts";
            std::error_code error = {};
            std::filesystem::create_directories(scripts, error);
            REQUIRE_FALSE(error);
            std::ofstream stream(scripts / "Game.csproj", std::ios::binary | std::ios::trunc);
            REQUIRE(stream.is_open());
            stream << "<Project Sdk=\"Microsoft.NET.Sdk\" />\n";
            stream.close();
            const std::u8string text = (scripts / "Game.csproj").generic_u8string();
            scriptsNamed = std::string(text.begin(), text.end());
        }
        SUBCASE("the project has no game scripts") {
        }
        SUBCASE("publishManaged is true and the project has no game scripts") {
            args.set("publishManaged", true);
        }
        const protocol_response response = editor.send("project.export", std::move(args));
        REQUIRE_FALSE(response.ok);
        CAPTURE(response.errorMessage);
        CHECK(response.errorCode == protocol_error_code::IO_ERROR);
        CHECK(response.errorMessage.find("publish_managed") != std::string::npos);
        CHECK(response.errorMessage.find("--no-scripting") != std::string::npos);
        CHECK(response.errorMessage.find(scriptsNamed) != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(output));
    }

    TEST_CASE("project.export - an editor run with --no-scripting exports a project without scripting, or when told "
      "not to publish managed code") {
        editor_test::headless_editor_options options = {};
        options.scripting = false;
        core::json::document args = core::json::object({{"copyRuntime", false}});
        SUBCASE("the project disables scripting") {
            options.extraProjectJson = R"("scripting": {"enabled": false})";
        }
        SUBCASE("publishManaged is false") {
            args.set("publishManaged", false);
        }
        editor_test::headless_editor editor(options);
        (void) editor.ok("scene.save");
        const std::filesystem::path output = editor.project_directory() / "exported";
        args.set("directory", output.generic_string());
        const core::json::document result = editor.ok("project.export", std::move(args));
        CHECK(std::filesystem::is_regular_file(output / "project.json"));
        CHECK_FALSE(std::filesystem::exists(output / "managed"));
        CHECK(result.root()["warnings"].size() == 0);
    }

#if defined(CONJURE_EDITOR_TEST_MANAGED_EXPORT_DIR)
    // Compiled only where this build published the export-flavoured SDK (conjure_managed_sdk_export needs the .NET SDK
    // at configure time), so no build registers a case that could only skip.
    TEST_CASE("regression - code sweep OWN-INSTALL project.export of an installed editor copies the prebuilt SDK and "
      "needs no .NET SDK for a project without game scripts") {
        // An editor without the engine's source tree answered NOT_AVAILABLE for every export with managed code. An
        // installed editor carries the export-flavoured SDK in the managed/ folder beside it: the stand-in editor
        // folder holds a copy of what conjure_managed_sdk_export published, and the fixture's dotnet does not exist,
        // so the export succeeds only when it copies that SDK without running dotnet.
        const std::filesystem::path published = editor_test::managed_export_directory();
        REQUIRE_MESSAGE(!published.empty(), "this build published no export-flavoured SDK");
        testing::temp_directory directory("editor_installed_layout");
        const std::filesystem::path installed = editor_test::install_managed_export(directory.path() / "editor");
        editor_test::headless_editor editor = {};
        std::filesystem::path bootstrap = {};
        SUBCASE("an editor built without scripting knows no bootstrap project") {
            bootstrap.clear();
        }
        SUBCASE("an installed editor names the bootstrap project of the machine that built it, where nothing lies") {
            bootstrap = directory.path() / "build_machine" / "Conjure.Bootstrap" / "Conjure.Bootstrap.csproj";
        }
        SUBCASE("an editor packaged on the machine that built it finds its bootstrap project, and the SDK still wins") {
            // Publishing the bootstrap from whatever revision that checkout holds now would skip the native API hash
            // check of the prebuilt SDK; the fixture's dotnet does not exist, so a publish would fail the export.
            bootstrap = directory.path() / "build_machine" / "Conjure.Bootstrap" / "Conjure.Bootstrap.csproj";
            std::error_code error = {};
            std::filesystem::create_directories(bootstrap.parent_path(), error);
            REQUIRE_FALSE(error);
            std::ofstream stream(bootstrap, std::ios::binary | std::ios::trunc);
            REQUIRE(stream.is_open());
        }
        editor.context().set_bootstrap_project(bootstrap);
        editor.context().set_prebuilt_managed_directory(installed);
        (void) editor.ok("scene.save");
        const std::filesystem::path output = directory.path() / "exported";
        const core::json::document result = editor.ok(
          "project.export",
          core::json::object({{"directory", output.generic_string()}, {"copyRuntime", false}})
        );
        CAPTURE(result.to_string());
        for (const std::string_view name : {"Conjure.Bootstrap.dll", "Conjure.Bootstrap.runtimeconfig.json",
               "Conjure.Managed.dll"}) {
            CAPTURE(name);
            const std::filesystem::path shipped = output / "managed" / std::filesystem::path(name);
            REQUIRE(std::filesystem::is_regular_file(shipped));
            CHECK(editor_test::read_text_file(shipped) == editor_test::read_text_file(installed / name));
        }
        CHECK_FALSE(std::filesystem::exists(output / "managed" / "conjure_managed_sdk.json"));
        CHECK_FALSE(std::filesystem::exists(output / "managed" / "Conjure.Managed.pdb"));
        CHECK(std::filesystem::is_regular_file(output / "project.json"));
        CHECK(result.root()["warnings"].size() == 0);
    }
#endif

    TEST_CASE("project.export - an editor with neither the engine source nor a prebuilt SDK answers NOT_AVAILABLE") {
        // The reply names what is missing on both sides and how to export without managed code; nothing is written.
        testing::temp_directory directory("editor_no_managed_sdk");
        editor_test::headless_editor editor = {};
        std::filesystem::path bootstrap = directory.path() / "gone" / "Conjure.Bootstrap.csproj";
        std::string sourceNamed = "has no '" + bootstrap.generic_string() + "'";
        const std::filesystem::path managed = directory.path() / "editor" / "managed";
        SUBCASE("the source tree is gone and the managed folder is missing") {
        }
        SUBCASE("the source tree is gone and the managed folder holds a development SDK without the manifest") {
            std::error_code error = {};
            std::filesystem::create_directories(managed, error);
            REQUIRE_FALSE(error);
            std::ofstream stream(managed / "Conjure.Managed.dll", std::ios::binary | std::ios::trunc);
            REQUIRE(stream.is_open());
            stream << "development build";
        }
        SUBCASE("the editor was built without scripting") {
            bootstrap.clear();
            sourceNamed = "built without scripting";
        }
        editor.context().set_bootstrap_project(bootstrap);
        editor.context().set_prebuilt_managed_directory(managed);
        (void) editor.ok("scene.save");
        const std::filesystem::path output = directory.path() / "exported";
        const protocol_response response = editor.send(
          "project.export",
          core::json::object({{"directory", output.generic_string()}, {"copyRuntime", false}})
        );
        REQUIRE_FALSE(response.ok);
        CAPTURE(response.errorMessage);
        CHECK(response.errorCode == protocol_error_code::NOT_AVAILABLE);
        CHECK(response.errorMessage.find(sourceNamed) != std::string::npos);
        CHECK(response.errorMessage.find(managed.generic_string()) != std::string::npos);
        CHECK(response.errorMessage.find("conjure_managed_sdk.json") != std::string::npos);
        CHECK(response.errorMessage.find("publishManaged false") != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(output));
    }

#if defined(CONJURE_EDITOR_TEST_MANAGED_EXPORT_DIR)
    // Compiled only where this build published the export-flavoured SDK (conjure_managed_sdk_export needs the .NET SDK
    // at configure time), so no build registers a case that could only skip.
    TEST_CASE("project.export - an installed editor names the .NET SDK the game's scripts need when none is usable") {
        // Owner decision OWN-INSTALL: the .NET SDK is an optional download that only projects with game scripts
        // need. Without a usable one the reply names the version global.json asks for and Microsoft's download page,
        // for a dotnet that cannot be started and for one that answers --version with an error, as a dotnet without
        // an SDK does (conjure_process_child answers every command with exit code 2).
        const std::filesystem::path published = editor_test::managed_export_directory();
        REQUIRE_MESSAGE(!published.empty(), "this build published no export-flavoured SDK");
        testing::temp_directory directory("editor_installed_no_sdk");
        const std::filesystem::path installed = editor_test::install_managed_export(directory.path() / "editor");
        editor_test::headless_editor editor = {};
        std::string found = "could not be started";
        SUBCASE("a dotnet that cannot be started") {
            found = "could not be started";
        }
        SUBCASE("a dotnet without an SDK") {
            editor.context().set_dotnet_executable(CONJURE_EDITOR_TEST_PROCESS_CHILD);
            found = "--version' exited with 2";
        }
        editor.context().set_bootstrap_project({});
        editor.context().set_prebuilt_managed_directory(installed);
        const std::filesystem::path scripts = editor.project_directory() / "scripts";
        std::error_code error = {};
        std::filesystem::create_directories(scripts, error);
        REQUIRE_FALSE(error);
        std::ofstream stream(scripts / "Game.csproj", std::ios::binary | std::ios::trunc);
        REQUIRE(stream.is_open());
        stream << "<Project Sdk=\"Microsoft.NET.Sdk\" />\n";
        stream.close();
        (void) editor.ok("scene.save");
        const std::filesystem::path output = directory.path() / "exported";
        const protocol_response response = editor.send(
          "project.export",
          core::json::object({{"directory", output.generic_string()}, {"copyRuntime", false}})
        );
        REQUIRE_FALSE(response.ok);
        CAPTURE(response.errorMessage);
        CHECK(response.errorCode == protocol_error_code::IO_ERROR);
        CHECK(response.errorMessage.find("publish_managed") != std::string::npos);
        CHECK(response.errorMessage.find(found) != std::string::npos);
        CHECK(response.errorMessage.find(std::string(engine::required_dotnet_sdk_version())) != std::string::npos);
        CHECK(response.errorMessage.find(engine::dotnet_sdk_download_page()) != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(output / "project.json"));
    }
#endif

#if CONJURE_EDITOR_TEST_HAS_DOTNET && defined(CONJURE_EDITOR_TEST_RUNTIME_EXECUTABLE)
    TEST_CASE("regression - follow-up FU-PDB and code sweep OWN-INSTALL project.export of a scripted project ships "
      "managed code that the exported runtime runs [dotnet][slow]" * doctest::timeout(1800.0)) {
        // tests/assets/projects/scripted (the project the nightly exports) holds scripts/Game.csproj as
        // script.create writes it and a startup scene whose one entity carries Game.ExportProbe, which logs a line
        // when it is created. The editor exports a copy with the real dotnet and the runtime of this tree; the
        // exported runtime, started headless with scripting, runs the script from the assemblies the export shipped.
        // ExportProbe uses features of C# 14 (a setter through the field keyword, an extension property): a
        // LangVersion below 14 in the project file fails the export, and the second line shows what they computed.
        // The editor publishes the bootstrap from the engine source it was built from, or, standing in for an
        // installed editor without that source (owner decision OWN-INSTALL), copies the export-flavoured SDK of the
        // managed/ folder of a stand-in editor folder and builds only the game's scripts against it.
        REQUIRE_MESSAGE(editor_test::dotnet_available(), "no dotnet answers --version: the [dotnet] case cannot run");
        testing::temp_directory directory = {};
        bool installed = false;
        SUBCASE("from the engine source this editor was built from") {
            installed = false;
        }
        SUBCASE("from the prebuilt SDK of an installed editor") {
            installed = true;
        }
        const std::filesystem::path project = directory.path() / "scripted";
        std::error_code copyError = {};
        std::filesystem::copy(
          std::filesystem::path(CONJURE_EDITOR_TEST_SCRIPTED_PROJECT),
          project,
          std::filesystem::copy_options::recursive,
          copyError
        );
        REQUIRE_MESSAGE(!copyError, "copying the scripted project: " << copyError.message());

        editor_test::headless_editor editor = {};
        editor.context().set_dotnet_executable("dotnet");
        if (installed) {
            const std::filesystem::path published = editor_test::managed_export_directory();
            REQUIRE_MESSAGE(!published.empty(), "this build published no export-flavoured SDK");
            editor.context().set_bootstrap_project({});
            editor.context().set_prebuilt_managed_directory(
              editor_test::install_managed_export(directory.path() / "editor")
            );
        }
        editor.trust_project(project);
        (void) editor.ok("project.open", core::json::object({{"path", project.generic_string()}}));
        // The engine restarts into the project between frames.
        editor.pump(1);
        REQUIRE_FALSE(editor.application().has_pending_project_open());
        REQUIRE(editor.context().project()->root_directory() == project);
        const std::filesystem::path runtime(CONJURE_EDITOR_TEST_RUNTIME_EXECUTABLE);
        const std::filesystem::path output = directory.path() / "exported";
        const core::json::document result = editor.ok(
          "project.export",
          core::json::object({
            {"directory", output.generic_string()},
            {"distributionDirectory", runtime.parent_path().generic_string()},
          })
        );
        CAPTURE(result.to_string());
        for (const std::string_view name : {"Conjure.Bootstrap.dll", "Conjure.Managed.dll", "Game.dll"}) {
            CAPTURE(name);
            REQUIRE(std::filesystem::is_regular_file(output / "managed" / std::filesystem::path(name)));
        }
        REQUIRE(result.root()["runtimePath"].is_string());

        const std::filesystem::path logFile = directory.path() / "runtime.log";
        core::process_desc run = {};
        run.executable = std::string(result.root()["runtimePath"].as_string());
        run.arguments = {
            "--project", output.generic_string(), "--headless", "--scripting", "--frames", "10",
            "--log-level", "info", "--log-file", logFile.generic_string(),
        };
        run.captureStdout = true;
        run.captureStderr = true;
        run.mergeStderr = true;
        run.timeoutSeconds = 120.0;
        const core::result<core::process_result> ran = core::process::run(run);
        REQUIRE_MESSAGE(ran.has_value(), testing::describe_error(ran));
        CHECK_FALSE(ran.value().timedOut);
        CHECK_MESSAGE(ran.value().exitCode == 0, ran.value().standardOutput);
        const std::string log = editor_test::read_text_file(logFile);
        CHECK_MESSAGE(log.find("ExportProbe ran in the exported game") != std::string::npos, log);
        CHECK_MESSAGE(log.find("ExportProbe C# 14: lives 0, seven squared 49") != std::string::npos, log);
    }
#endif

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

    TEST_CASE("regression - code sweep F200 F790 project.export tells the client what the runtime step left out") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("scene.save");
        const std::filesystem::path root = editor.project_directory();
        const std::filesystem::path distribution = root / "distribution";
        std::error_code ignored = {};
        std::filesystem::create_directories(distribution, ignored);
        const auto standIn = [&distribution](std::string_view name) {
            std::ofstream stream(distribution / name, std::ios::binary | std::ios::trunc);
            REQUIRE(stream.is_open());
            stream << "stand-in";
        };
        standIn("conjure_runtime");
        standIn("conjure_runtime.exe");
        standIn(core::dynamic_library::decorate_name("conjure_shared"));
        const auto exported = [&editor, &root, &distribution](std::string_view folder) {
            return editor.ok(
              "project.export",
              core::json::object({
                {"directory", (root / folder).generic_string()},
                {"distributionDirectory", distribution.generic_string()},
                {"publishManaged", false},
              })
            );
        };
        const auto warned = [](const core::json::document &result, std::string_view text) {
            for (const core::json::value warning : result.root()["warnings"]) {
                if (warning.as_string().find(text) != std::string_view::npos) {
                    return true;
                }
            }
            return false;
        };
        SUBCASE("no notices and only the debug Visual C++ runtime: both are named, and the debug runtime stays") {
            standIn("vcruntime140d.dll");
            const core::json::document result = exported("export_bare");
            CHECK(warned(result, "holds no THIRD_PARTY_NOTICES.txt"));
            CHECK(warned(result, "debug Visual C++ runtime (vcruntime140d.dll)"));
            CHECK_FALSE(std::filesystem::exists(root / "export_bare" / "vcruntime140d.dll"));
            // Only a build that imports the DLL runtime of Microsoft's standard library needs the libraries.
            const bool needsLibraries = CONJURE_PLATFORM_WINDOWS == 1 && CONJURE_STDLIB_MSVC == 1
              && (modules::ABI_SIGNATURE_RUNTIME_FLAGS & modules::ABI_RUNTIME_DYNAMIC_CRT) != 0;
            CHECK(warned(result, "holds no Visual C++ runtime DLL") == needsLibraries);
        }
        SUBCASE("the notices and the redistributable runtime are copied without a word") {
            standIn("THIRD_PARTY_NOTICES.txt");
            standIn("vcruntime140.dll");
            const core::json::document result = exported("export_complete");
            CHECK_FALSE(warned(result, "THIRD_PARTY_NOTICES.txt"));
            CHECK_FALSE(warned(result, "Visual C++"));
            CHECK(std::filesystem::is_regular_file(root / "export_complete" / "THIRD_PARTY_NOTICES.txt"));
            CHECK(std::filesystem::is_regular_file(root / "export_complete" / "vcruntime140.dll"));
        }
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
        // What --project <dir> --trust-project --export <out> [--distribution <dir>] parse into.
        const auto exportWith = [&project](const std::filesystem::path &output, const std::filesystem::path &from) {
            editor_options options = {};
            options.headless = true;
            options.aiPort = u16{0};
            options.projectPath = project;
            options.trustProject = true;
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

    TEST_CASE("regression - code sweep F650 project.export refuses the removed config argument and project.create "
      "the removed template") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Exported");
        (void) editor.ok("scene.save");
        const std::filesystem::path output = editor.project_directory() / "export_with_config";
        // Every spelling the argument used to accept is now an unknown argument, whatever its value.
        for (const std::string_view value : std::array<std::string_view, 3>{"development", "distribution", "Bogus"}) {
            CAPTURE(value);
            const protocol_response refused = editor.send(
              "project.export",
              core::json::object({
                {"directory", output.generic_string()},
                {"config", value},
                {"copyRuntime", false},
                {"publishManaged", false},
              })
            );
            REQUIRE_FALSE(refused.ok);
            CHECK(refused.errorCode == protocol_error_code::INVALID_ARGS);
            CHECK(refused.errorMessage.find("config") != std::string::npos);
        }
        CHECK_FALSE(std::filesystem::exists(output));
        // The same request without it exports.
        (void) editor.ok(
          "project.export",
          core::json::object({
            {"directory", output.generic_string()},
            {"copyRuntime", false},
            {"publishManaged", false},
          })
        );
        CHECK(std::filesystem::is_regular_file(output / "project.json"));

        const std::filesystem::path created = editor.project_directory() / "from_template";
        const protocol_response templated = editor.send(
          "project.create",
          core::json::object({{"path", created.generic_string()}, {"name", "Templated"}, {"template", "empty"}})
        );
        REQUIRE_FALSE(templated.ok);
        CHECK(templated.errorCode == protocol_error_code::INVALID_ARGS);
        CHECK(templated.errorMessage.find("template") != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(created));
        (void) editor.ok(
          "project.create",
          core::json::object({{"path", created.generic_string()}, {"name", "Templated"}})
        );
        CHECK(std::filesystem::is_regular_file(created / "project.json"));

        // The catalogue no longer advertises either argument.
        const core::json::document exportSchema = editor.ok(
          "editor.schema",
          core::json::object({{"command", "project.export"}})
        );
        const std::string exportText = exportSchema.to_string();
        CHECK(exportText.find("\"config\"") == std::string::npos);
        CHECK(exportText.find("\"directory\"") != std::string::npos);
        const core::json::document createSchema = editor.ok(
          "editor.schema",
          core::json::object({{"command", "project.create"}})
        );
        CHECK(createSchema.to_string().find("\"template\"") == std::string::npos);
    }

    TEST_CASE("regression - code sweep F656 scene.open and project.open answer PATH_NOT_FOUND for a missing file "
      "and IO_ERROR for one that cannot be read") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        (void) editor.ok("scene.save");

        const protocol_response scene = editor.send("scene.open", core::json::object({{"path", "scenes/absent"}}));
        REQUIRE_FALSE(scene.ok);
        CHECK(scene.errorCode == protocol_error_code::PATH_NOT_FOUND);
        CHECK(scene.errorMessage.find("assets://scenes/absent.scene.json") != std::string::npos);
        // The open scene is untouched by the refusal.
        CHECK(editor.context().scene_path() == "assets://scenes/test.scene.json");
        (void) editor.ok("entity.get", core::json::object({{"entity", "Kept"}}));
        // A folder named like a scene exists and cannot be read as one.
        std::error_code ignored = {};
        REQUIRE(std::filesystem::create_directories(
          editor.project_directory() / "assets" / "scenes" / "folder.scene.json",
          ignored
        ));
        CHECK(editor.fail(
          "scene.open",
          core::json::object({{"path", "scenes/folder"}})
        ) == protocol_error_code::IO_ERROR);
        CHECK(editor.context().scene_path() == "assets://scenes/test.scene.json");
        (void) editor.ok("entity.get", core::json::object({{"entity", "Kept"}}));

        // No directory, and a directory without a project.json: both are a missing path.
        const std::filesystem::path nowhere = editor.project_directory() / "nowhere";
        CHECK(editor.fail(
          "project.open",
          core::json::object({{"path", nowhere.generic_string()}})
        ) == protocol_error_code::PATH_NOT_FOUND);
        const std::filesystem::path empty = editor.project_directory() / "empty_directory";
        REQUIRE(std::filesystem::create_directories(empty, ignored));
        CHECK(editor.fail(
          "project.open",
          core::json::object({{"path", empty.generic_string()}})
        ) == protocol_error_code::PATH_NOT_FOUND);

        // A project.json that exists but is not a file the engine can read is IO_ERROR.
        const std::filesystem::path unreadable = editor.project_directory() / "unreadable";
        REQUIRE(std::filesystem::create_directories(unreadable / "project.json", ignored));
        const protocol_response project = editor.send(
          "project.open",
          core::json::object({{"path", unreadable.generic_string()}})
        );
        REQUIRE_FALSE(project.ok);
        CHECK(project.errorCode == protocol_error_code::IO_ERROR);
        CHECK(project.errorMessage.find("project.json") != std::string::npos);
        CHECK_FALSE(editor.application().has_pending_project_open());
        // So is a path that names a file where the project's folder is expected.
        const protocol_response notFolder = editor.send(
          "project.open",
          core::json::object({{"path", (editor.project_directory() / "project.json").generic_string()}})
        );
        REQUIRE_FALSE(notFolder.ok);
        CHECK(notFolder.errorCode == protocol_error_code::IO_ERROR);
        CHECK(notFolder.errorMessage.find("not a directory") != std::string::npos);
        CHECK_FALSE(editor.application().has_pending_project_open());

        // One whose content is wrong is still the request's fault.
        const std::filesystem::path malformed = editor.project_directory() / "malformed";
        REQUIRE(std::filesystem::create_directories(malformed, ignored));
        {
            std::ofstream output(malformed / "project.json", std::ios::binary | std::ios::trunc);
            REQUIRE(output.is_open());
            output << "{ not a project";
        }
        CHECK(editor.fail(
          "project.open",
          core::json::object({{"path", malformed.generic_string()}})
        ) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("regression - code sweep F656 project.export answers PATH_NOT_FOUND for a distribution directory that "
      "is not there") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Exported");
        (void) editor.ok("scene.save");
        const std::filesystem::path root = editor.project_directory();
        const std::filesystem::path output = root / "export_refused";

        const protocol_response missing = editor.send(
          "project.export",
          core::json::object({
            {"directory", output.generic_string()},
            {"distributionDirectory", (root / "no_such_build").generic_string()},
            {"publishManaged", false},
          })
        );
        REQUIRE_FALSE(missing.ok);
        CHECK(missing.errorCode == protocol_error_code::PATH_NOT_FOUND);
        CHECK(missing.errorMessage.find("no_such_build") != std::string::npos);
        // Refused before the pipeline ran: nothing was cooked or written.
        CHECK_FALSE(std::filesystem::exists(output));

        // A file where the folder is expected exists and cannot be used as one: IO_ERROR.
        const std::filesystem::path file = root / "build.txt";
        {
            std::ofstream stream(file, std::ios::binary | std::ios::trunc);
            REQUIRE(stream.is_open());
            stream << "not a folder";
        }
        const protocol_response wrongKind = editor.send(
          "project.export",
          core::json::object({
            {"directory", output.generic_string()},
            {"distributionDirectory", file.generic_string()},
            {"publishManaged", false},
          })
        );
        REQUIRE_FALSE(wrongKind.ok);
        CHECK(wrongKind.errorCode == protocol_error_code::IO_ERROR);
        CHECK(wrongKind.errorMessage.find("not a directory") != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(output));

        // The folder is only needed when the runtime is copied from it.
        (void) editor.ok(
          "project.export",
          core::json::object({
            {"directory", output.generic_string()},
            {"distributionDirectory", (root / "no_such_build").generic_string()},
            {"copyRuntime", false},
            {"publishManaged", false},
          })
        );
        CHECK(std::filesystem::is_regular_file(output / "project.json"));
    }

    TEST_CASE("regression - code sweep F089 scene.save and scene.save_as answer WRONG_PLAY_STATE while playing") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        (void) editor.ok("scene.save");
        const std::filesystem::path scenes = editor.project_directory() / "assets" / "scenes";
        const std::string saved = editor_test::read_text_file(scenes / "test.scene.json");

        (void) editor.ok("play.start");
        // The scene now holds play-session state that play.stop discards: it must not reach the file.
        (void) editor.create_entity("Transient");
        const protocol_response save = editor.send("scene.save");
        REQUIRE_FALSE(save.ok);
        CHECK(save.errorCode == protocol_error_code::WRONG_PLAY_STATE);
        CHECK(save.errorDetails.root()["state"].as_string() == "playing");
        (void) editor.ok("play.pause");
        const protocol_response saveAs = editor.send("scene.save_as", core::json::object({{"path", "scenes/played"}}));
        REQUIRE_FALSE(saveAs.ok);
        CHECK(saveAs.errorCode == protocol_error_code::WRONG_PLAY_STATE);
        CHECK(saveAs.errorDetails.root()["state"].as_string() == "paused");
        CHECK(editor_test::read_text_file(scenes / "test.scene.json") == saved);
        CHECK_FALSE(std::filesystem::exists(scenes / "played.scene.json"));
        CHECK(editor.context().scene_path() == "assets://scenes/test.scene.json");

        (void) editor.ok("play.stop");
        (void) editor.ok("scene.save");
        CHECK(editor_test::read_text_file(scenes / "test.scene.json") == saved);
        (void) editor.ok("scene.save_as", core::json::object({{"path", "scenes/played"}}));
        CHECK(std::filesystem::is_regular_file(scenes / "played.scene.json"));
    }

    TEST_CASE("regression - code sweep F089 project.set_settings while playing is kept and undone after play.stop") {
        editor_test::headless_editor editor = {};
        const std::map<std::string, std::string> before = editor_test::project_file_snapshot(
          editor.project_directory()
        );
        (void) editor.ok("play.start");
        (void) editor.ok(
          "project.set_settings",
          core::json::object({{"settings", core::json::object({{"startup_scene", "scenes/live.scene.json"}})}})
        );
        CHECK(editor.context().undo().size() == 0);
        CHECK(editor.context().play().kept_file_edits() == 1);
        CHECK(editor.ok("play.stop").root()["fileChanges"].as_u64() == 1);
        // play.stop leaves the project file as the session wrote it.
        CHECK(editor.ok("project.info").root()["startupScene"].as_string() == "scenes/live.scene.json");
        const core::result<engine::project> onDisk = engine::project::load(editor.project_directory());
        CONJURE_REQUIRE_OK(onDisk);
        CHECK(onDisk.value().description().startupScene.generic_string() == "scenes/live.scene.json");
        REQUIRE(editor.context().undo().size() == 1);
        CHECK(editor.context().undo().entries().back()->label == play_controller::FILE_EDITS_LABEL);

        (void) editor.ok("history.undo");
        CHECK(editor.ok("project.info").root()["startupScene"].as_string().empty());
        CHECK(editor_test::project_file_snapshot(editor.project_directory()) == before);
        (void) editor.ok("history.redo");
        CHECK(editor.ok("project.info").root()["startupScene"].as_string() == "scenes/live.scene.json");
    }

    TEST_CASE("regression - code sweep F187 project.export resolves relative directories against the project root") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Exported");
        (void) editor.ok("scene.save");
        const std::filesystem::path root = editor.context().project()->root_directory();

        // The folder the runtime is copied from is checked first, so nothing is written for this half: a relative
        // distributionDirectory that does not exist is looked for in the project, and the answer says where.
        const protocol_response missing = editor.send(
          "project.export",
          core::json::object({
            {"directory", "exported/refused"},
            {"distributionDirectory", "no_such_build"},
            {"publishManaged", false},
          })
        );
        REQUIRE_FALSE(missing.ok);
        CHECK(missing.errorCode == protocol_error_code::PATH_NOT_FOUND);
        const std::string expectedBuild = (root / "no_such_build").generic_string();
        CHECK_MESSAGE(missing.errorMessage.find(expectedBuild) != std::string::npos, missing.errorMessage);
        CHECK_FALSE(std::filesystem::exists(root / "exported"));

        const core::json::document result = editor.ok(
          "project.export",
          core::json::object({{"directory", "exported/relative"}, {"copyRuntime", false}, {"publishManaged", false}})
        );
        const std::filesystem::path expected = root / "exported" / "relative";
        // The answer names the folder that was written, so a client need not know the rule to find it.
        CHECK(std::filesystem::path(std::string(result.root()["directory"].as_string())) == expected);
        CHECK(std::filesystem::is_regular_file(expected / "project.json"));
    }
}
