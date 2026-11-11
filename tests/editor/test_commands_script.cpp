#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/protocol/protocol_message.h"

#include <filesystem>
#include <string>

// script.* (docs/design/04_editor.md 4.4, R15): sources under <project>/scripts, the managed build and the
// script component. The null host stands in for .NET.

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/commands_script") {

    TEST_CASE("script.create - writes the template and the project file, undo deletes the source") {
        editor_test::headless_editor editor = {};
        const core::json::document created = editor.ok(
          "script.create",
          core::json::object({{"name", "CellBehaviour"}})
        );
        CHECK(created.root()["path"].as_string() == "CellBehaviour.cs");
        CHECK(created.root()["className"].as_string() == "Game.CellBehaviour");
        const std::filesystem::path file = editor.project_directory() / "scripts" / "CellBehaviour.cs";
        CHECK(std::filesystem::is_regular_file(file));
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "scripts" / "Game.csproj"));
        const std::string source = editor_test::read_text_file(file);
        CHECK(source.find("class CellBehaviour : ScriptBehaviour") != std::string::npos);
        CHECK(source.find("namespace Game;") != std::string::npos);
        CHECK(source.find("{NAME}") == std::string::npos);
        const core::json::document empty = editor.ok(
          "script.create",
          core::json::object({{"name", "Marker"}, {"template", "Empty"}, {"namespace", "Sweeper"}})
        );
        CHECK(empty.root()["className"].as_string() == "Sweeper.Marker");
        CHECK(editor.fail(
          "script.create",
          core::json::object({{"name", "CellBehaviour"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "script.create",
          core::json::object({{"name", "1bad"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "script.create",
          core::json::object({{"name", "X"}, {"template", "Weird"}})
        ) == protocol_error_code::INVALID_ARGS);
        (void) editor.ok("history.undo");
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "scripts" / "Game.csproj"));
        (void) editor.ok("history.undo");
        CHECK_FALSE(std::filesystem::exists(file));
        // The first script created the project file: its undo removes it with the last source.
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "scripts" / "Game.csproj"));
        (void) editor.ok("history.redo");
        CHECK(editor_test::read_text_file(file) == source);
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "scripts" / "Game.csproj"));
        // A plain script.delete keeps the project file.
        (void) editor.ok("script.delete", core::json::object({{"path", "CellBehaviour.cs"}}));
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "scripts" / "Game.csproj"));
    }

    TEST_CASE("script.write / read / delete - edit sources with undo and reject paths outside scripts/") {
        editor_test::headless_editor editor = {};
        const core::json::document written = editor.ok(
          "script.write",
          core::json::object({{"path", "Sub/New.cs"}, {"source", "// first"}})
        );
        CHECK(written.root()["created"].as_bool());
        CHECK(written.root()["bytes"].as_i64() == 8);
        CHECK(editor.ok(
          "script.read",
          core::json::object({{"path", "Sub/New.cs"}})
        ).root()["source"].as_string() == "// first");
        (void) editor.ok("script.write", core::json::object({{"path", "Sub/New.cs"}, {"source", "// second"}}));
        (void) editor.ok("history.undo");
        CHECK(editor.ok(
          "script.read",
          core::json::object({{"path", "Sub/New.cs"}})
        ).root()["source"].as_string() == "// first");
        (void) editor.ok("script.delete", core::json::object({{"path", "Sub/New.cs"}}));
        CHECK(editor.fail(
          "script.read",
          core::json::object({{"path", "Sub/New.cs"}})
        ) == protocol_error_code::PATH_NOT_FOUND);
        (void) editor.ok("history.undo");
        CHECK(editor.ok(
          "script.read",
          core::json::object({{"path", "Sub/New.cs"}})
        ).root()["source"].as_string() == "// first");
        CHECK(editor.fail(
          "script.write",
          core::json::object({{"path", "../escape.cs"}, {"source", "x"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "script.write",
          core::json::object({
            {"path", (editor.project_directory() / "assets" / "x.cs").generic_string()},
            {"source", "x"},
          })
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "script.write",
          core::json::object({{"path", "notes.txt"}, {"source", "x"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "script.delete",
          core::json::object({{"path", "Missing.cs"}})
        ) == protocol_error_code::PATH_NOT_FOUND);
        CHECK(editor.fail("script.read", core::json::object({{"path", ""}})) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("script.list - lists sources and the classes the null runtime knows") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("script.create", core::json::object({{"name", "Beta"}}));
        (void) editor.ok("script.create", core::json::object({{"name", "Alpha"}}));
        const core::json::document list = editor.ok("script.list");
        REQUIRE(list.root()["scripts"].size() == 2);
        CHECK(list.root()["scripts"][0]["className"].as_string() == "Game.Alpha");
        CHECK(list.root()["scripts"][1]["path"].as_string() == "Beta.cs");
        CHECK(list.root()["scripts"][1]["fields"].is_array());
        CHECK(list.root()["loadedClasses"].is_array());
        CHECK(list.root()["project"].as_string().find("Game.csproj") != std::string::npos);
    }

    TEST_CASE("regression - script.list reads the namespace and class name from the source") {
        editor_test::headless_editor editor = {};
        (void) editor.ok(
          "script.create",
          core::json::object({{"name", "Marker"}, {"template", "Empty"}, {"namespace", "Sweeper"}})
        );
        // A hand-written file whose class name differs from the file stem, with a block-scoped namespace and a
        // comment that mentions another class.
        (void) editor.ok(
          "script.write",
          core::json::object({
            {"path", "misc/odd_file.cs"},
            {"source",
             "using Conjure;\n// derived from the base class in Shared\nnamespace Studio.Game\n{\n"
             "    public sealed class RealName : ScriptBehaviour\n    {\n    }\n}\n"},
          })
        );
        (void) editor.ok("script.write", core::json::object({{"path", "Bare.cs"}, {"source", "// nothing declared"}}));
        const core::json::document list = editor.ok("script.list");
        REQUIRE(list.root()["scripts"].size() == 3);
        CHECK(list.root()["scripts"][0]["path"].as_string() == "Bare.cs");
        CHECK(list.root()["scripts"][0]["className"].as_string() == "Game.Bare");
        CHECK(list.root()["scripts"][1]["path"].as_string() == "Marker.cs");
        CHECK(list.root()["scripts"][1]["className"].as_string() == "Sweeper.Marker");
        CHECK(list.root()["scripts"][2]["path"].as_string() == "misc/odd_file.cs");
        CHECK(list.root()["scripts"][2]["className"].as_string() == "Studio.Game.RealName");
    }

    TEST_CASE("script.compile - without dotnet the reply is NOT_AVAILABLE with a clear message") {
        editor_test::headless_editor editor = {};
        CHECK(editor.fail("script.compile") == protocol_error_code::PATH_NOT_FOUND);
        (void) editor.ok("script.create", core::json::object({{"name", "Compiled"}}));
        const protocol_response response = editor.send("script.compile");
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorCode == protocol_error_code::NOT_AVAILABLE);
        CHECK(response.errorMessage.find("dotnet") != std::string::npos);
    }

    TEST_CASE("script.compile - with dotnet present the build runs and reports diagnostics or succeeds") {
        if (!editor_test::dotnet_available()) {
            MESSAGE("dotnet is not installed: the real build path is exercised only where it is");
            return;
        }
        editor_test::headless_editor editor = {};
        editor.context().set_dotnet_executable("dotnet");
        editor.log().allow_errors();
        (void) editor.ok("script.create", core::json::object({{"name", "Compiled"}}));
        const protocol_response response = editor.send("script.compile");
        if (response.ok) {
            CHECK(response.result.root().contains("succeeded"));
            CHECK(response.result.root()["diagnostics"].is_array());
        } else {
            // Conjure.Managed.dll is not next to the test executable: the build fails for that reason.
            CHECK(response.errorCode == protocol_error_code::COMPILE_FAILED);
        }
    }

    TEST_CASE("script.attach - adds the script component with fields, undo removes it or restores the previous set") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Scripted");
        const editor_test::editor_snapshot bare = editor_test::capture_snapshot(editor);
        const core::json::document attached = editor.ok(
          "script.attach",
          core::json::object({
            {"entity", guid},
            {"className", "Game.CellBehaviour"},
            {"fields", core::json::object({{"mines", 10}})},
          })
        );
        CHECK(attached.root()["className"].as_string() == "Game.CellBehaviour");
        CHECK(attached.root()["scripts"]["scripts"].size() == 1);
        CHECK(attached.root()["scripts"]["scripts"][0]["fields"]["mines"].as_i64() == 10);
        const core::json::document info = editor.ok("entity.get", core::json::object({{"entity", guid}}));
        CHECK(info.root()["components"].contains("conjure.scripting.script_component"));
        const editor_test::editor_snapshot one = editor_test::capture_snapshot(editor);
        (void) editor.ok("script.attach", core::json::object({{"entity", guid}, {"className", "Game.Timer"}}));
        CHECK(editor.ok(
          "component.get",
          core::json::object({{"entity", guid}, {"path", "script"}})
        ).root()["value"]["scripts"].size() == 2);
        (void) editor.ok("history.undo");
        const editor_test::editor_snapshot backToOne = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(one == backToOne, editor_test::describe_difference(one, backToOne));
        (void) editor.ok("history.undo");
        const editor_test::editor_snapshot backToBare = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(bare == backToBare, editor_test::describe_difference(bare, backToBare));
        CHECK(editor.fail(
          "script.attach",
          core::json::object({{"entity", guid}, {"className", ""}})
        ) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("script.reload / script.gc - answer through the null runtime") {
        editor_test::headless_editor editor = {};
        editor.log().allow_errors();
        const core::json::document gc = editor.ok("script.gc");
        CHECK(gc.root().contains("zombies"));
        const protocol_response reload = editor.send(
          "script.reload", core::json::object(
            {{"assembly", (editor.project_directory() / "scripts" / "bin" / "Game.dll").generic_string()}}
          )
        );
        if (reload.ok) {
            CHECK(reload.result.root().contains("reloadCount"));
        } else {
            CHECK(reload.errorCode == protocol_error_code::COMPILE_FAILED);
        }
    }
}
