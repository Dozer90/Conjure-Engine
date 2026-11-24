#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/testing/temp_directory.h"

#include "conjure/core/json/document.h"
#include "conjure/core/process.h"
#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/undo_stack.h"
#include "conjure/engine/application.h"
#include "conjure/engine/export.h"
#include "conjure/engine/project.h"

#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

// script.* (docs/design/04_editor.md 4.4, R15): sources under <project>/scripts, the managed build and the
// script component. The null host stands in for .NET.

using namespace conjure;
using namespace conjure::editor;

namespace {
    void write_file(const std::filesystem::path &path, std::string_view text) {
        std::error_code ignored = {};
        std::filesystem::create_directories(path.parent_path(), ignored);
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        REQUIRE(output.is_open());
        output << text;
    }

    /// <summary>
    /// dotnet build of the generated project against the SDK built with this tree (the way script.compile
    /// runs it, without going through the editor).
    /// </summary>
    core::process_result build_scripts(const std::filesystem::path &project) {
        core::process_desc desc = {};
        desc.executable = "dotnet";
        desc.arguments = {
            "build", project.generic_string(), "-c", "Debug", "--nologo", "-clp:NoSummary",
            "-p:ConjureManagedDir=" + editor_test::managed_directory().generic_string(),
        };
        desc.captureStdout = true;
        desc.captureStderr = true;
        desc.mergeStderr = true;
        desc.timeoutSeconds = 180.0;
        desc.environment = {{"DOTNET_CLI_TELEMETRY_OPTOUT", "1"}, {"DOTNET_NOLOGO", "1"}, {"MSBUILDNODEREUSE", "0"}};
        core::result<core::process_result> run = core::process::run(desc);
        REQUIRE_MESSAGE(run.has_value(), testing::describe_error(run));
        return std::move(run).value();
    }

    /// <summary>
    /// How many times needle occurs in text, counting occurrences that do not overlap.
    /// </summary>
    usize count_occurrences(std::string_view text, std::string_view needle) {
        usize count = 0;
        usize position = text.find(needle);
        while (position != std::string_view::npos) {
            ++count;
            position = text.find(needle, position + needle.size());
        }
        return count;
    }
}

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

    TEST_CASE("script.create - the generated Game.csproj leaves bin/ and obj/ out of the compile glob") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("script.create", core::json::object({{"name", "Mover"}}));
        const std::filesystem::path projectFile = editor.project_directory() / "scripts" / "Game.csproj";
        const std::string project = editor_test::read_text_file(projectFile);
        CHECK(project.find("<Compile Include=\"**/*.cs\" Exclude=\"bin/**;obj/**\" />") != std::string::npos);
    }

    TEST_CASE("regression - code sweep F778 script.create writes a Game.csproj that targets .NET 10") {
        // The engine's managed projects and managed/Conjure.Templates/Game/Game.csproj moved to net10.0; a game
        // project the editor generates must target the same framework as the Conjure.Managed it references.
        editor_test::headless_editor editor = {};
        (void) editor.ok("script.create", core::json::object({{"name", "Mover"}}));
        const std::string project = editor_test::read_text_file(editor.project_directory() / "scripts" / "Game.csproj");
        CHECK(project.find("<TargetFramework>net10.0</TargetFramework>") != std::string::npos);
        CHECK(project.find("net8.0") == std::string::npos);
    }

    TEST_CASE("script.create - the Behaviour template moves through Transform.Translate, not an assignment to a copy") {
        // ScriptBehaviour.Transform returns a struct: assigning its Position is CS1612, a method call is not. The
        // [dotnet][slow] case below compiles the templates.
        editor_test::headless_editor editor = {};
        (void) editor.ok("script.create", core::json::object({{"name", "Mover"}}));
        const std::string behaviour = editor_test::read_text_file(editor.project_directory() / "scripts" / "Mover.cs");
        CHECK(behaviour.find("Transform.Translate(") != std::string::npos);
        CHECK(behaviour.find("Transform.Position =") == std::string::npos);
        CHECK(behaviour.find("* 0.0f") == std::string::npos);
    }

    TEST_CASE("script.create - the Behaviour and Empty templates compile against Conjure.Managed [dotnet][slow]"
      * doctest::timeout(600)) {
        CONJURE_TEST_REQUIRE(testing::precondition::DOTNET_RUNTIME);
        REQUIRE_MESSAGE(editor_test::dotnet_available(), "no dotnet answers --version: the [dotnet] case cannot run");
        editor_test::headless_editor editor = {};
        (void) editor.ok("script.create", core::json::object({{"name", "Mover"}}));
        (void) editor.ok("script.create", core::json::object({{"name", "Marker"}, {"template", "Empty"}}));
        const std::filesystem::path scripts = editor.project_directory() / "scripts";
        const core::process_result first = build_scripts(scripts / "Game.csproj");
        CHECK_FALSE(first.timedOut);
        CHECK_MESSAGE(first.exitCode == 0, first.standardOutput);
        // A second build sees the first one's obj/ output: the project must not compile it again (CS2002).
        const core::process_result second = build_scripts(scripts / "Game.csproj");
        CHECK_FALSE(second.timedOut);
        CHECK_MESSAGE(second.exitCode == 0, second.standardOutput);
        // MSBuild's output never counts as a script.
        const core::json::document list = editor.ok("script.list");
        CHECK(list.root()["scripts"].size() == 2);
    }

    TEST_CASE("script.create - the generated Game.csproj pins C# 14") {
        // managed/Directory.Build.props and the game template pin LangVersion 14 as well. MSBuild takes the last value
        // of a property, so the project file holds exactly one pin. The [dotnet][slow] case below compiles a script
        // that uses C# 14 in such a project.
        editor_test::headless_editor editor = {};
        (void) editor.ok("script.create", core::json::object({{"name", "Mover"}}));
        const std::string project = editor_test::read_text_file(editor.project_directory() / "scripts" / "Game.csproj");
        CHECK(project.find("<LangVersion>14</LangVersion>") != std::string::npos);
        CHECK(count_occurrences(project, "<LangVersion>") == 1);
    }

    TEST_CASE("script.create - a script that uses C# 14 compiles in the generated Game.csproj [dotnet][slow]"
      * doctest::timeout(600)) {
        CONJURE_TEST_REQUIRE(testing::precondition::DOTNET_RUNTIME);
        REQUIRE_MESSAGE(editor_test::dotnet_available(), "no dotnet answers --version: the [dotnet] case cannot run");
        // The probe of the test scripts (managed/Conjure.TestScripts/LanguageVersionProbe.cs) uses features of C# 14:
        // written into the project beside a template script, it compiles only under a pin of 14 or above.
        editor_test::headless_editor editor = {};
        (void) editor.ok("script.create", core::json::object({{"name", "Mover"}}));
        const std::filesystem::path probeFile(CONJURE_EDITOR_TEST_LANGUAGE_PROBE);
        const std::string probe = editor_test::read_text_file(probeFile);
        REQUIRE(probe.find("class LanguageVersionProbe") != std::string::npos);
        (void) editor.ok("script.write", editor_test::script_write_args("LanguageVersionProbe.cs", probe));
        const core::process_result built = build_scripts(editor.project_directory() / "scripts" / "Game.csproj");
        CHECK_FALSE(built.timedOut);
        CHECK_MESSAGE(
          built.exitCode == 0,
          "the C# 14 probe did not compile (is LangVersion below 14?):\n" << built.standardOutput
        );
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

    TEST_CASE("regression - code sweep F656 script.read, script.write, script.delete and script.reload answer "
      "IO_ERROR for a path that exists but is not a file") {
        editor_test::headless_editor editor = {};
        // A directory named like a script: it exists, so it is not PATH_NOT_FOUND, and it cannot be read.
        const std::filesystem::path folder = editor.project_directory() / "scripts" / "Folder.cs";
        std::error_code ignored = {};
        REQUIRE(std::filesystem::create_directories(folder, ignored));
        const protocol_response read = editor.send("script.read", core::json::object({{"path", "Folder.cs"}}));
        REQUIRE_FALSE(read.ok);
        CHECK(read.errorCode == protocol_error_code::IO_ERROR);
        CHECK(read.errorMessage.find("not a regular file") != std::string::npos);
        CHECK(editor.fail(
          "script.delete",
          core::json::object({{"path", "Folder.cs"}})
        ) == protocol_error_code::IO_ERROR);
        // script.write reads what it replaces before it writes.
        CHECK(editor.fail(
          "script.write",
          editor_test::script_write_args("Folder.cs", "// x")
        ) == protocol_error_code::IO_ERROR);
        CHECK(std::filesystem::is_directory(folder));
        const protocol_response missing = editor.send("script.read", core::json::object({{"path", "Absent.cs"}}));
        REQUIRE_FALSE(missing.ok);
        CHECK(missing.errorCode == protocol_error_code::PATH_NOT_FOUND);
        CHECK(missing.errorMessage.find("no such file") != std::string::npos);

        // script.reload: a directory where the assembly is expected exists, so it is not "run script.compile first".
        const protocol_response reload = editor.send(
          "script.reload",
          core::json::object({{"assembly", folder.generic_string()}})
        );
        REQUIRE_FALSE(reload.ok);
        CHECK(reload.errorCode == protocol_error_code::IO_ERROR);
        CHECK(reload.errorMessage.find("not a regular file") != std::string::npos);
        const protocol_response unbuilt = editor.send(
          "script.reload",
          core::json::object({{"assembly", (folder / "Game.dll").generic_string()}})
        );
        REQUIRE_FALSE(unbuilt.ok);
        CHECK(unbuilt.errorCode == protocol_error_code::PATH_NOT_FOUND);
        CHECK(unbuilt.errorMessage.find("script.compile") != std::string::npos);
    }

    TEST_CASE("script.write - an absolute path differing only in drive-letter case is inside scripts/ and a symlink "
      "out of it is refused") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path project = editor.project_directory();
        // The extension test stays case-sensitive: script.list and MSBuild's glob on Linux see only ".cs".
        CHECK(editor.fail(
          "script.write",
          core::json::object({{"path", "Other.CS"}, {"source", "x"}})
        ) == protocol_error_code::INVALID_ARGS);

        // Drive letters exist only where paths have a root name such as "C:".
        const std::string rootName = project.root_name().generic_string();
        if (rootName.size() == 2 && rootName[1] == ':') {
            std::string flipped = (project / "scripts" / "Flipped.cs").generic_string();
            const unsigned char letter = static_cast<unsigned char>(flipped[0]);
            flipped[0] = static_cast<char>(std::islower(letter) ? std::toupper(letter) : std::tolower(letter));
            const core::json::document written = editor.ok(
              "script.write",
              core::json::object({{"path", flipped}, {"source", "// flipped"}})
            );
            CHECK(written.root()["path"].as_string() == "Flipped.cs");
            CHECK(std::filesystem::is_regular_file(project / "scripts" / "Flipped.cs"));
        } else {
            MESSAGE("no drive letter on this platform: the case half runs on Windows only");
        }

        std::error_code linkError = {};
        std::filesystem::create_directories(project / "outside", linkError);
        std::filesystem::create_directories(project / "scripts", linkError);
        std::filesystem::create_directory_symlink(project / "outside", project / "scripts" / "link", linkError);
        if (linkError) {
            MESSAGE("the OS refused a directory symlink (" << linkError.message() << "): the link half is skipped");
            return;
        }
        CHECK(editor.fail(
          "script.write",
          core::json::object({{"path", "link/x.cs"}, {"source", "x"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK_FALSE(std::filesystem::exists(project / "outside" / "x.cs"));
        // A link to a missing file outside: writing through it would create the target.
        std::filesystem::create_symlink(
          project / "outside" / "gone.cs",
          project / "scripts" / "dangling.cs",
          linkError
        );
        if (linkError) {
            MESSAGE("the OS refused a file symlink (" << linkError.message() << "): the dangling half is skipped");
            return;
        }
        CHECK(editor.fail(
          "script.write",
          core::json::object({{"path", "dangling.cs"}, {"source", "x"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK_FALSE(std::filesystem::exists(project / "outside" / "gone.cs"));
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

    TEST_CASE("script.list - MSBuild's bin/ and obj/ output is not listed and does not keep the project file alive") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("script.create", core::json::object({{"name", "Alpha"}}));
        const std::filesystem::path scripts = editor.project_directory() / "scripts";
        write_file(scripts / "obj" / "Debug" / "Game.AssemblyInfo.cs", "// generated by MSBuild");
        write_file(scripts / "bin" / "Debug" / "Extra.cs", "// build output");
        const core::json::document list = editor.ok("script.list");
        REQUIRE(list.root()["scripts"].size() == 1);
        CHECK(list.root()["scripts"][0]["path"].as_string() == "Alpha.cs");
        const core::json::document deleted = editor.ok(
          "script.delete",
          core::json::object({{"path", "Alpha.cs"}, {"project", true}})
        );
        CHECK(deleted.root()["projectRemoved"].as_bool());
        CHECK_FALSE(std::filesystem::exists(scripts / "Game.csproj"));
        // A nested directory that merely happens to be called obj is source, not build output.
        write_file(scripts / "Sub" / "obj" / "Nested.cs", "// a script");
        CHECK(editor.ok("script.list").root()["scripts"].size() == 1);
        // Nor can a script be written into the output directories, where it would never build or show.
        CHECK(editor.fail(
          "script.write",
          core::json::object({{"path", "obj/X.cs"}, {"source", "// x"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "script.write",
          core::json::object({{"path", "bin/Debug/X.cs"}, {"source", "// x"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK_FALSE(std::filesystem::exists(scripts / "obj" / "X.cs"));
        (void) editor.ok("script.write", core::json::object({{"path", "Sub/obj/Y.cs"}, {"source", "// y"}}));
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

    TEST_CASE("script.compile - without a usable .NET SDK the reply is NOT_AVAILABLE naming the SDK and where to get "
      "it") {
        // Owner decision OWN-INSTALL: the message names the version global.json asks for and Microsoft's download
        // page, for a dotnet that cannot be started, for one that answers every command with an error, as a dotnet
        // without an SDK does (conjure_process_child answers build and --version with exit code 2), whose build
        // reports no compiler error, and for one with an older SDK only, whose build fails with NETSDK1045 (a copy
        // of conjure_process_child that replays that line for build and an older version for --version).
        editor_test::headless_editor editor = {};
        CHECK(editor.fail("script.compile") == protocol_error_code::PATH_NOT_FOUND);
        (void) editor.ok("script.create", core::json::object({{"name", "Compiled"}}));
        const testing::temp_directory tools("editor_script_compile_old_sdk");
        std::string found = {};
        SUBCASE("a dotnet that cannot be started") {
            found = std::format("'{}' could not be started", editor_test::missing_dotnet_executable());
        }
        SUBCASE("a dotnet without an SDK") {
            editor.context().set_dotnet_executable(CONJURE_EDITOR_TEST_PROCESS_CHILD);
            found = "--version' exited with 2";
        }
        SUBCASE("a dotnet with an older SDK only") {
            const core::result<std::filesystem::path> fake = testing::replaying_copy(
              tools,
              std::filesystem::path(CONJURE_EDITOR_TEST_PROCESS_CHILD),
              "old_dotnet",
              "when build echo-exit 1 /sdk/Microsoft.NET.TargetFrameworkInference.targets(166,5): error NETSDK1045: "
              "The current .NET SDK does not support targeting .NET 10.0. [/project/scripts/Game.csproj]\n"
              "echo 8.0.100\n"
            );
            CONJURE_REQUIRE_OK(fake);
            const std::u8string fakeText = fake.value().u8string();
            editor.context().set_dotnet_executable(std::string(fakeText.begin(), fakeText.end()));
            found = "--version' reports the SDK '8.0.100'";
        }
        const protocol_response response = editor.send("script.compile");
        REQUIRE_FALSE(response.ok);
        CAPTURE(response.errorMessage);
        CHECK(response.errorCode == protocol_error_code::NOT_AVAILABLE);
        CHECK(response.errorMessage.find(found) != std::string::npos);
        CHECK(response.errorMessage.find(std::string(engine::required_dotnet_sdk_version())) != std::string::npos);
        CHECK(response.errorMessage.find(engine::dotnet_sdk_download_page()) != std::string::npos);
        CHECK_FALSE(editor.context().script_compile_pending());
    }

    TEST_CASE("regression - code sweep OWN-INSTALL script.compile builds and asks dotnet for its SDK in the scripts "
      "root") {
        // dotnet picks its SDK through the global.json in or above the directory it runs in. The build used to run in
        // the editor's current directory and the probe in the scripts root, so the probe could find an SDK the failed
        // build could not use and the reply was a COMPILE_FAILED without diagnostics. The fake dotnet fails the build
        // without a diagnostic and answers --version with its current directory, which the message quotes.
        editor_test::headless_editor editor = {};
        (void) editor.ok("script.create", core::json::object({{"name", "Compiled"}}));
        const testing::temp_directory tools("editor_script_compile_probe_directory");
        const core::result<std::filesystem::path> fake = testing::replaying_copy(
          tools,
          std::filesystem::path(CONJURE_EDITOR_TEST_PROCESS_CHILD),
          "cwd_dotnet",
          "when build exit 1\ncwd\n"
        );
        CONJURE_REQUIRE_OK(fake);
        const std::u8string fakeText = fake.value().u8string();
        editor.context().set_dotnet_executable(std::string(fakeText.begin(), fakeText.end()));

        const protocol_response response = editor.send("script.compile");
        REQUIRE_FALSE(response.ok);
        CAPTURE(response.errorMessage);
        CHECK(response.errorCode == protocol_error_code::NOT_AVAILABLE);
        const std::string_view opening = "reports the SDK '";
        const usize start = response.errorMessage.find(opening);
        REQUIRE(start != std::string::npos);
        const usize first = start + opening.size();
        const usize end = response.errorMessage.find('\'', first);
        REQUIRE(end != std::string::npos);
        const std::filesystem::path probed(response.errorMessage.substr(first, end - first));
        std::error_code error = {};
        CHECK(std::filesystem::equivalent(probed, editor.project_directory() / "scripts", error));
        CHECK_FALSE(error);
        CHECK_FALSE(editor.context().script_compile_pending());
    }

    TEST_CASE("script.compile - an editor configured with no managed directory uses the SDK next to the executable") {
        // Code sweep F084: the editor is configured with no managed directory, as a shipped one is, so the SDK
        // script.compile builds against is the one next to the executable, which for this test executable is the
        // build's bin/managed. The [dotnet][slow] case below runs the real build against it.
        editor_test::headless_editor editor = {};
        const engine::application *app = editor.context().engine_context().app;
        REQUIRE(app != nullptr);
        CHECK(app->desc().config.scripting.managedDirectory.empty());
        CHECK(app->managed_directory().is_absolute());
        std::error_code error = {};
        const std::filesystem::path used = std::filesystem::weakly_canonical(app->managed_directory(), error);
        REQUIRE_FALSE(error);
        const std::filesystem::path built = std::filesystem::weakly_canonical(editor_test::managed_directory(), error);
        REQUIRE_FALSE(error);
        CHECK(used == built);
    }

    TEST_CASE("regression - code sweep F084 script.compile builds against the managed SDK next to the executable "
      "[dotnet][slow]" * doctest::timeout(600)) {
        CONJURE_TEST_REQUIRE(testing::precondition::DOTNET_RUNTIME);
        REQUIRE_MESSAGE(editor_test::dotnet_available(), "no dotnet answers --version: the [dotnet] case cannot run");
        std::error_code probe = {};
        editor_test::headless_editor editor = {};
        editor.context().set_dotnet_executable("dotnet");
        // The editor is configured with no managed directory, as a shipped one is: the SDK is the one next to
        // the executable, which for this test executable is the build's bin/managed.
        const engine::application *app = editor.context().engine_context().app;
        REQUIRE(app != nullptr);
        REQUIRE(app->desc().config.scripting.managedDirectory.empty());
        REQUIRE(std::filesystem::equivalent(app->managed_directory(), editor_test::managed_directory(), probe));

        (void) editor.ok("script.create", core::json::object({{"name", "Compiled"}}));
        (void) editor.ok("script.create", core::json::object({{"name", "Marker"}, {"template", "Empty"}}));
        const protocol_response response = editor.send("script.compile");
        // The build used to get ConjureManagedDir=managed, which MSBuild resolved against <project>/scripts,
        // and so answered COMPILE_FAILED whatever the scripts said.
        REQUIRE_MESSAGE(response.ok, response.errorMessage << " " << response.errorDetails.to_string());
        CHECK(response.result.root()["succeeded"].as_bool(false));
        CHECK(response.result.root()["errors"].as_i64(-1) == 0);
        CHECK(response.result.root()["exitCode"].as_i64(-1) == 0);
        CHECK(response.result.root()["diagnostics"].is_array());
        // Where script.reload looks for it, whatever the environment says: under a Visual Studio developer
        // prompt (which CTest runs in when tools/build.py starts it) Platform=x64 is set, and MSBuild left to
        // itself then writes bin/x64/Debug/Game.dll.
        const std::filesystem::path scripts = editor.project_directory() / "scripts";
        CHECK(std::filesystem::is_regular_file(scripts / "bin" / "Debug" / "Game.dll"));
        CHECK_FALSE(std::filesystem::exists(scripts / "bin" / "x64", probe));

        // A source that does not compile is still the build's verdict, with its diagnostics.
        (void) editor.ok(
          "script.write",
          core::json::object({{"path", "Broken.cs"}, {"source", "namespace Game; public class Broken { int x = ; }"}})
        );
        editor.log().allow_errors();
        const protocol_response broken = editor.send("script.compile");
        REQUIRE_FALSE(broken.ok);
        CHECK(broken.errorCode == protocol_error_code::COMPILE_FAILED);
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
        const core::json::document gc = editor.ok("script.gc");
        CHECK(gc.root().contains("zombies"));
        const std::filesystem::path assembly = editor.project_directory() / "scripts" / "bin" / "Game.dll";
        const core::json::document args = core::json::object({{"assembly", assembly.generic_string()}});
        // No build output yet: the reply says so instead of blaming a compilation that never ran (F511, F702).
        const protocol_response missing = editor.send("script.reload", args);
        REQUIRE_FALSE(missing.ok);
        CHECK(missing.errorCode == protocol_error_code::PATH_NOT_FOUND);
        CHECK(missing.errorMessage.find("script.compile") != std::string::npos);
        // Without an assembly argument the default is the Debug output until a compile succeeded.
        const protocol_response defaulted = editor.send("script.reload");
        REQUIRE_FALSE(defaulted.ok);
        CHECK(defaulted.errorCode == protocol_error_code::PATH_NOT_FOUND);
        CHECK(defaulted.errorMessage.find("scripts/bin/Debug/Game.dll") != std::string::npos);
        // A compile that succeeded moves the default to its configuration's output.
        editor.context().set_last_script_configuration("Release");
        const protocol_response release = editor.send("script.reload");
        REQUIRE_FALSE(release.ok);
        CHECK(release.errorCode == protocol_error_code::PATH_NOT_FOUND);
        CHECK(release.errorMessage.find("scripts/bin/Release/Game.dll") != std::string::npos);

        // The null host loads any file that exists. The first load of a fresh runtime is not a reload.
        write_file(assembly, "not really an assembly");
        const core::json::document first = editor.ok("script.reload", args);
        const core::json::document second = editor.ok("script.reload", args);
        CHECK(first.root()["assembly"].as_string() == assembly.generic_string());
        CHECK(second.root()["reloadCount"].as_u64() == first.root()["reloadCount"].as_u64() + 1);
    }

    TEST_CASE("script.compile - an engine that stops while a compile runs waits for it and cancels the reply") {
        // The build, and the SDK probe after a failed one, run on the engine's job system: the task waits for them
        // while that still runs, and the reply is CANCELLED instead of answered by an engine that is going away.
        editor_test::headless_editor editor = {};
        (void) editor.ok("script.create", core::json::object({{"name", "Compiled"}}));
        editor_test::parked_client client(editor);
        REQUIRE_FALSE(client.dispatch("script.compile").has_value());
        REQUIRE(editor.context().script_compile_pending());
        editor.context().release_engine_work();
        const protocol_response reply = client.wait();
        CHECK(reply.errorCode == protocol_error_code::CANCELLED);
        CHECK_FALSE(editor.context().script_compile_pending());
        // The engine of a headless_editor did not stop: the next compile runs (and fails for the missing dotnet).
        CHECK(editor.fail("script.compile") == protocol_error_code::NOT_AVAILABLE);
    }

    TEST_CASE("regression - F511 script.compile refuses an unsafe configuration and answers BUSY while one runs") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("script.create", core::json::object({{"name", "Compiled"}}));
        CHECK(editor.fail(
          "script.compile",
          core::json::object({{"configuration", "-p:X=1"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "script.compile",
          core::json::object({{"configuration", "../Debug"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK_FALSE(editor.context().script_compile_pending());

        std::optional<protocol_response> captured = std::nullopt;
        const u32 client = editor.context().events().register_client({});
        editor.context().register_response_sink(client, [&captured](const protocol_response &response) {
            captured = response;
        });
        protocol_request first = {};
        first.id = make_id(1);
        first.command = "script.compile";
        first.clientId = client;
        const std::optional<protocol_response> immediate = editor.context().dispatcher().dispatch(first);
        REQUIRE_FALSE(immediate.has_value());
        CHECK(editor.context().script_compile_pending());
        CHECK(editor.fail("script.compile") == protocol_error_code::BUSY);
        for (u32 frame = 0; frame < 100000 && !captured.has_value(); ++frame) {
            editor.pump(1);
        }
        REQUIRE(captured.has_value());
        CHECK(captured->errorCode == protocol_error_code::NOT_AVAILABLE);
        CHECK_FALSE(editor.context().script_compile_pending());
        // The guard is gone with the first compile: the next one runs (and fails for the missing dotnet again).
        CHECK(editor.fail("script.compile") == protocol_error_code::NOT_AVAILABLE);
        CHECK(editor.context().last_script_configuration() == "Debug");
        editor.context().unregister_response_sink(client);
        editor.context().events().unregister_client(client);
    }

    TEST_CASE("regression - F188 script.read of a source that is not UTF-8 answers a parseable reply with U+FFFD") {
        editor_test::headless_editor editor = {};
        write_file(editor.project_directory() / "scripts" / "Latin.cs", "// Jos\xE9\n");
        const protocol_response response = editor.send("script.read", core::json::object({{"path", "Latin.cs"}}));
        REQUIRE(response.ok);
        const core::result<protocol_response> parsed = parse_response(serialise(response));
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().result.root()["source"].as_string() == "// Jos\xEF\xBF\xBD\n");
        CHECK(parsed.value().result.root()["path"].as_string() == "Latin.cs");
    }

    TEST_CASE("regression - code sweep F089 script.create and script.delete while playing are undone after play.stop") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path scripts = editor.project_directory() / "scripts";
        (void) editor.ok("script.create", core::json::object({{"name", "Old"}}));
        const std::string oldSource = editor_test::read_text_file(scripts / "Old.cs");
        const usize historyBefore = editor.context().undo().size();

        (void) editor.ok("play.start");
        (void) editor.ok("script.create", core::json::object({{"name", "Live"}, {"template", "Empty"}}));
        (void) editor.ok("script.delete", core::json::object({{"path", "Old.cs"}}));
        CHECK(editor.context().undo().size() == historyBefore);
        CHECK(editor.context().play().kept_file_edits() == 2);
        CHECK(editor.fail("history.undo") == protocol_error_code::WRONG_PLAY_STATE);
        const std::string liveSource = editor_test::read_text_file(scripts / "Live.cs");

        CHECK(editor.ok("play.stop").root()["fileChanges"].as_u64() == 2);
        CHECK(std::filesystem::is_regular_file(scripts / "Live.cs"));
        CHECK_FALSE(std::filesystem::exists(scripts / "Old.cs"));
        REQUIRE(editor.context().undo().size() == historyBefore + 1);
        CHECK(editor.context().undo().entries().back()->label == play_controller::FILE_EDITS_LABEL);

        (void) editor.ok("history.undo");
        CHECK_FALSE(std::filesystem::exists(scripts / "Live.cs"));
        CHECK(editor_test::read_text_file(scripts / "Old.cs") == oldSource);
        CHECK(std::filesystem::is_regular_file(scripts / "Game.csproj"));
        (void) editor.ok("history.redo");
        CHECK(editor_test::read_text_file(scripts / "Live.cs") == liveSource);
        CHECK_FALSE(std::filesystem::exists(scripts / "Old.cs"));
    }

    TEST_CASE("regression - code sweep F187 script.reload reads a relative assembly path against the project root") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path root = editor.context().project()->root_directory();
        const core::json::document args = core::json::object({{"assembly", "scripts/bin/Game.dll"}});
        const protocol_response missing = editor.send("script.reload", args);
        REQUIRE_FALSE(missing.ok);
        CHECK(missing.errorCode == protocol_error_code::PATH_NOT_FOUND);

        // The null host loads any file that exists.
        const std::filesystem::path assembly = root / "scripts" / "bin" / "Game.dll";
        write_file(assembly, "not really an assembly");
        const core::json::document loaded = editor.ok("script.reload", args);
        CHECK(std::filesystem::path(std::string(loaded.root()["assembly"].as_string())) == assembly);
    }
}
