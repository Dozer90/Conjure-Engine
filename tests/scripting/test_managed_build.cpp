#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "scripting_test_support.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/process.h"
#include "conjure/core/vfs/native_file_system.h"
#include "conjure/scripting/managed_build.h"
#include "conjure/scripting/scripting_error.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;

namespace {

    /// <summary>
    /// The value of the one TargetFramework element of an MSBuild project file; "" when the element is missing,
    /// unterminated or appears more than once (a second one would silently override the first).
    /// </summary>
    [[nodiscard]] std::string target_framework(std::string_view project) {
        constexpr std::string_view OPEN_TAG = "<TargetFramework>";
        constexpr std::string_view CLOSE_TAG = "</TargetFramework>";
        const usize open = project.find(OPEN_TAG);
        if (open == std::string_view::npos) {
            return {};
        }
        const usize valueBegin = open + OPEN_TAG.size();
        const usize close = project.find(CLOSE_TAG, valueBegin);
        if (close == std::string_view::npos) {
            return {};
        }
        if (project.find(OPEN_TAG, close) != std::string_view::npos) {
            return {};
        }
        return std::string(project.substr(valueBegin, close - valueBegin));
    }

    /// <summary>
    /// The bytes of a build output as a string, for finding the text the compiler records in an assembly or its .pdb;
    /// "" when the file cannot be read.
    /// </summary>
    [[nodiscard]] std::string read_build_output(const std::filesystem::path &file) {
        core::vfs::native_file_system folder(file.parent_path(), true);
        const core::result<std::vector<std::byte>> bytes = folder.read_file(file.filename().generic_string());
        if (!bytes.has_value()) {
            return {};
        }
        const std::vector<std::byte> &data = bytes.value();
        return std::string(reinterpret_cast<const char *>(data.data()), data.size());
    }

    /// <summary>
    /// The .pdb path an assembly records in its CodeView debug entry: the UTF-8 text after the "RSDS" signature, the
    /// 16-byte id and the 4-byte age, up to its terminating zero. "" when no such entry names a .pdb.
    /// </summary>
    [[nodiscard]] std::string recorded_pdb_path(std::string_view assembly) {
        constexpr std::string_view SIGNATURE = "RSDS";
        constexpr usize PATH_OFFSET = 24;
        for (usize at = assembly.find(SIGNATURE); at != std::string_view::npos; at = assembly.find(SIGNATURE, at + 1)) {
            const usize pathBegin = at + PATH_OFFSET;
            const usize pathEnd = assembly.find('\0', pathBegin);
            if (pathBegin >= assembly.size() || pathEnd == std::string_view::npos) {
                continue;
            }
            const std::string_view path = assembly.substr(pathBegin, pathEnd - pathBegin);
            if (path.ends_with(".pdb")) {
                return std::string(path);
            }
        }
        return {};
    }
}

TEST_SUITE("scripting/managed_build") {

    TEST_CASE("managed_build_runner - parses error and warning lines and ignores the rest") {
        const std::string output =
          "MSBuild version 17.8.3+195e7f5a3 for .NET\n"
          "  Determining projects to restore...\n"
          "C:\\work\\Game\\Player.cs(12,9): error CS0103: The name 'speed' does not exist in the current context "
          "[C:\\work\\Game\\Game.csproj]\n"
          "/home/dev/game/Enemy.cs(3,1): warning CS0168: The variable 'x' is declared but never used "
          "[/home/dev/game/Game.csproj]\n"
          "C:\\work\\Game\\Player.cs(12,9): error CS0103: The name 'speed' does not exist in the current context "
          "[C:\\work\\Game\\Game.csproj]\n"
          "    0 Warning(s)\n";
        const std::vector<build_diagnostic> diagnostics = managed_build_runner::parse_output(output);
        REQUIRE(diagnostics.size() == 2);
        CHECK(diagnostics[0].isError);
        CHECK(diagnostics[0].file == "C:\\work\\Game\\Player.cs");
        CHECK(diagnostics[0].line == 12);
        CHECK(diagnostics[0].column == 9);
        CHECK(diagnostics[0].code == "CS0103");
        CHECK(diagnostics[0].message == "The name 'speed' does not exist in the current context");
        CHECK_FALSE(diagnostics[1].isError);
        CHECK(diagnostics[1].file == "/home/dev/game/Enemy.cs");
        CHECK(diagnostics[1].line == 3);
        CHECK(diagnostics[1].column == 1);
        CHECK(diagnostics[1].code == "CS0168");
        CHECK(diagnostics[1].message == "The variable 'x' is declared but never used");

        build_report report = {};
        report.diagnostics = diagnostics;
        CHECK(report.error_count() == 1);
        CHECK(report.warning_count() == 1);
    }

    TEST_CASE("managed_build_runner - parses paths with spaces, line-only locations and tool diagnostics") {
        const std::string output =
          "C:\\My Games\\Space Game\\Scripts\\Big Boss.cs(7,15): error CS1002: ; expected "
          "[C:\\My Games\\Space Game\\Game.csproj]\n"
          "Scripts/Util.cs(44): warning CS8632: The annotation for nullable reference types should only be used\n"
          "MSBUILD : error MSB1009: Project file does not exist.\n"
          "CSC : error CS2001: Source file 'Missing.cs' could not be found. [Game.csproj]\n"
          "Not a diagnostic: error without code\n";
        const std::vector<build_diagnostic> diagnostics = managed_build_runner::parse_output(output);
        REQUIRE(diagnostics.size() == 4);
        CHECK(diagnostics[0].file == "C:\\My Games\\Space Game\\Scripts\\Big Boss.cs");
        CHECK(diagnostics[0].line == 7);
        CHECK(diagnostics[0].column == 15);
        CHECK(diagnostics[0].message == "; expected");
        CHECK(diagnostics[1].file == "Scripts/Util.cs");
        CHECK(diagnostics[1].line == 44);
        CHECK(diagnostics[1].column == 0);
        CHECK_FALSE(diagnostics[1].isError);
        CHECK(diagnostics[2].file == "MSBUILD");
        CHECK(diagnostics[2].line == 0);
        CHECK(diagnostics[2].code == "MSB1009");
        CHECK(diagnostics[2].message == "Project file does not exist.");
        CHECK(diagnostics[3].file == "CSC");
        CHECK(diagnostics[3].code == "CS2001");
        CHECK(diagnostics[3].message == "Source file 'Missing.cs' could not be found.");
        CHECK(managed_build_runner::parse_output("").empty());
    }

    TEST_CASE("managed_build_runner - the command line follows the configuration and options") {
        build_request request = {};
        request.projectPath = "Game/Game.csproj";
        request.configuration = "Release";
        const core::process_desc release = managed_build_runner::command_line(request, "dotnet");
        CHECK(release.executable == "dotnet");
        const std::vector<std::string> expectedRelease = {
          "build", "Game/Game.csproj", "-c", "Release", "--nologo", "-clp:NoSummary"
        };
        CHECK(release.arguments == expectedRelease);
        CHECK(release.captureStdout);
        CHECK(release.mergeStderr);
        CHECK(release.timeoutSeconds == doctest::Approx(600.0));

        request.configuration = "Debug";
        request.outputDirectory = "out/dir";
        request.properties = {{"ScriptVariant", "V2"}};
        request.timeoutSeconds = 30.0;
        const core::process_desc debug = managed_build_runner::command_line(request, "C:/dotnet/dotnet.exe");
        CHECK(debug.executable == "C:/dotnet/dotnet.exe");
        const std::vector<std::string> expectedDebug = {
          "build", "Game/Game.csproj", "-c", "Debug", "-o", "out/dir", "--nologo", "-clp:NoSummary",
          "-p:ScriptVariant=V2"
        };
        CHECK(debug.arguments == expectedDebug);
        CHECK(debug.timeoutSeconds == doctest::Approx(30.0));
        bool telemetryOff = false;
        for (const auto &[name, value] : debug.environment) {
            telemetryOff = telemetryOff || (name == "DOTNET_CLI_TELEMETRY_OPTOUT" && value == "1");
        }
        CHECK(telemetryOff);
    }

    TEST_CASE("regression - code sweep F776 command_line never makes a script build a CI build") {
        // ContinuousIntegrationBuild maps the source paths of the .pdb to /_/ in a git repository with a commit, and
        // the debugger then cannot find the user's scripts. Only an export passes it
        // (engine/src/engine/export_commands.cpp); a caller that wants it anyway names it as a property, and it is
        // passed once.
        const auto isContinuousIntegration = [](const std::string &argument) {
            return argument.starts_with("-p:ContinuousIntegrationBuild");
        };
        build_request request = {};
        request.projectPath = "Game/Game.csproj";
        std::ptrdiff_t expected = 0;

        SUBCASE("the defaults") {
            request.configuration = "Debug";
        }
        SUBCASE("every option") {
            request.configuration = "Release";
            request.outputDirectory = "out/dir";
            request.properties = {{"ScriptVariant", "V2"}};
        }
        SUBCASE("a caller that asks for it as a property") {
            request.properties = {{"ContinuousIntegrationBuild", "true"}};
            expected = 1;
        }

        const core::process_desc desc = managed_build_runner::command_line(request, "dotnet");
        CHECK(std::count_if(desc.arguments.begin(), desc.arguments.end(), isContinuousIntegration) == expected);
    }

    TEST_CASE("regression - code sweep F776 the managed build keeps the real paths in its debug information [dotnet]") {
        // cmake/ConjureManaged.cmake used to build as a CI build, which maps the paths the compiler records to /_/
        // when the build tree lies in the git checkout (build/<name> does): the .pdb path in the assembly, the .pdb's
        // documents and the Source Link map. A debugger then found neither the symbols nor the sources.
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        // The SDK maps only paths below the repository's root, so for a build tree outside the checkout the recorded
        // .pdb path is the machine's either way; the Source Link check still tells, when the checkout has a remote.
        const std::filesystem::path buildTree =
          scripting_test::managed_directory().lexically_relative(scripting_test::source_root());
        if (buildTree.empty() || *buildTree.begin() == "..") {
            MESSAGE("the build tree lies outside the checkout: the recorded .pdb path cannot show a CI build, only the "
                    "Source Link check of the .pdb can (and only when the checkout has a remote)");
        }
        const std::filesystem::path assemblies[] = {
          scripting_test::managed_directory() / "Conjure.Managed.dll",
          scripting_test::managed_directory() / "Conjure.Bootstrap.dll",
          scripting_test::test_scripts_assembly("v1"),
        };
        for (const std::filesystem::path &assembly : assemblies) {
            const std::string assemblyPath = assembly.generic_string();
            CAPTURE(assemblyPath);
            const std::string recorded = recorded_pdb_path(read_build_output(assembly));
            CAPTURE(recorded);
            CHECK_FALSE(recorded.empty());
            CHECK_FALSE(recorded.starts_with("/_/"));
            const std::filesystem::path recordedPath(std::u8string(recorded.begin(), recorded.end()));
            std::error_code probe = {};
            CHECK(recordedPath.is_absolute());
            CHECK(std::filesystem::is_regular_file(recordedPath, probe));

            // The Source Link map, present when the checkout has a remote, keys the documents by their source root.
            const std::string symbols = read_build_output(std::filesystem::path(assembly).replace_extension(".pdb"));
            CHECK_FALSE(symbols.empty());
            CHECK(symbols.find("\"/_/") == std::string::npos);
        }
    }

    TEST_CASE("managed_build_runner - command_line passes non-ASCII paths as UTF-8") {
        // "Jörg" spelt as UTF-8 bytes: path::string() would hand the ANSI code page to dotnet on Windows.
        const std::string_view folderUtf8 = "J\xC3\xB6rg";
        const std::u8string folder(folderUtf8.begin(), folderUtf8.end());
        build_request request = {};
        request.projectPath = std::filesystem::path(folder) / "Game.csproj";
        request.configuration = "Debug";
        request.outputDirectory = std::filesystem::path(folder) / "bin";
        const core::process_desc desc = managed_build_runner::command_line(request, "dotnet");

        const std::u8string projectUtf8 = request.projectPath.u8string();
        const std::u8string outputUtf8 = request.outputDirectory.u8string();
        REQUIRE(desc.arguments.size() >= 6);
        CHECK(desc.arguments[1] == std::string(projectUtf8.begin(), projectUtf8.end()));
        CHECK(desc.arguments[1].starts_with(folderUtf8));
        CHECK(desc.arguments[4] == "-o");
        CHECK(desc.arguments[5] == std::string(outputUtf8.begin(), outputUtf8.end()));
        CHECK(desc.arguments[5].starts_with(folderUtf8));
    }

    TEST_CASE("msbuild_escape - escapes the characters MSBuild splits on or decodes and keeps every other byte") {
        CHECK(msbuild_escape("") == "");
        CHECK(msbuild_escape("C:/plain path/bin") == "C:/plain path/bin");
        CHECK(msbuild_escape("a%b;c,d\"e") == "a%25b%3Bc%2Cd%22e");
        // An escape already in the text is escaped again: the value is literal, never pre-escaped.
        CHECK(msbuild_escape("%3B") == "%253B");
        // "Jörg" as UTF-8 bytes passes unchanged.
        CHECK(msbuild_escape("J\xC3\xB6rg") == "J\xC3\xB6rg");
    }

    TEST_CASE("regression - follow-up FU-MANAGEDDIR command_line escapes the output directory and property values") {
        // Unescaped, MSBuild split a ',' or ';' in -o or a -p: value into further properties (MSB1006) and decoded a
        // '%' followed by two hex digits; the editor's script.compile passes the managed directory as a property.
        const std::string_view folderUtf8 = "out, 1;%J\xC3\xB6rg";
        const std::u8string folder(folderUtf8.begin(), folderUtf8.end());
        build_request request = {};
        request.projectPath = "Game/Game.csproj";
        request.configuration = "Debug";
        request.outputDirectory = std::filesystem::path(folder) / "bin";
        request.properties = {{"ConjureManagedDir", std::string(folderUtf8) + "/managed"}};
        const core::process_desc desc = managed_build_runner::command_line(request, "dotnet");

        const std::string escapedFolder = "out%2C 1%3B%25J\xC3\xB6rg";
        const std::u8string outputUtf8 = request.outputDirectory.u8string();
        const std::string output(outputUtf8.begin(), outputUtf8.end());
        REQUIRE(desc.arguments.size() >= 6);
        CHECK(desc.arguments[4] == "-o");
        CHECK(desc.arguments[5] == msbuild_escape(output));
        CHECK(desc.arguments[5].starts_with(escapedFolder));
        CHECK(desc.arguments.back() == "-p:ConjureManagedDir=" + escapedFolder + "/managed");
    }

    TEST_CASE("regression - code sweep F778 global.json pins the .NET SDK major the managed projects target") {
        // Without a global.json every dotnet build ran the newest SDK installed, so a machine with a newer SDK
        // than CI built (and judged warnings, which are errors here) differently. The pin, the projects' target
        // framework and the host pack minimum of the configure are one decision and must name the same major.
        const i32 major = scripting_test::dotnet_major();
        CAPTURE(major);
        CHECK(major == 10);

        const core::result<core::json::document> global =
          core::json::document::parse_file(scripting_test::source_root() / "global.json");
        CONJURE_REQUIRE_OK(global);
        const core::json::value sdk = global.value().root()["sdk"];
        REQUIRE(sdk.is_object());
        CHECK(sdk["version"].as_string() == std::format("{}.0.100", major));
        CHECK(sdk["rollForward"].as_string() == "latestFeature");
        REQUIRE(sdk["allowPrerelease"].is_bool());
        CHECK_FALSE(sdk["allowPrerelease"].as_bool(true));

        core::vfs::native_file_system sources(scripting_test::source_root(), true);
        const std::string expected = std::format("net{}.0", major);
        constexpr std::string_view PROJECTS[] = {
          "managed/Directory.Build.props",
          "managed/Conjure.Templates/Game/Game.csproj",
        };
        for (const std::string_view project : PROJECTS) {
            CAPTURE(project);
            const core::result<std::string> text = sources.read_text_file(project);
            CONJURE_REQUIRE_OK(text);
            CHECK(target_framework(text.value()) == expected);
        }
    }

    TEST_CASE("managed_build_runner - a dotnet that cannot start yields a launch failure on the main thread") {
        log_capture logs = {};
        managed_build_runner runner = {};
        runner.set_dotnet_executable("conjure-no-such-dotnet-executable");
        CHECK(runner.dotnet_executable() == "conjure-no-such-dotnet-executable");
        CHECK_FALSE(runner.is_building());
        bool completed = false;
        build_report received = {};
        runner.build("Game.csproj", "Debug", [&completed, &received](const build_report &report) {
            completed = true;
            received = report;
        });
        runner.wait();
        CHECK_FALSE(runner.is_building());
        CHECK_FALSE(completed);                                  // the callback waits for the main-thread drain
        CHECK(core::jobs::drain_main_thread_queue() >= 1);
        CHECK(completed);
        CHECK_FALSE(received.succeeded);
        CHECK_FALSE(received.launch.has_value());
        CHECK(received.diagnostics.empty());
        CHECK(runner.build_count() == 1);
        CHECK(logs.count(core::log_level::WARN) == 1);
    }

    TEST_CASE("managed_build_runner - builds run on a job when a job system is given") {
        log_capture logs = {};
        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = 2;
        CONJURE_REQUIRE_OK(jobs.start(desc));
        {
            managed_build_runner runner(&jobs);
            runner.set_dotnet_executable("conjure-no-such-dotnet-executable");
            bool completed = false;
            runner.build("Game.csproj", "Debug", [&completed](const build_report &) {
                completed = true;
            });
            runner.wait();
            CHECK_FALSE(runner.is_building());
            CHECK(core::jobs::drain_main_thread_queue() >= 1);
            CHECK(completed);
        }
        jobs.stop();
    }
}
