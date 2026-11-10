#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/process.h"
#include "conjure/scripting/managed_build.h"
#include "conjure/scripting/scripting_error.h"

#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;

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
          "build", "Game/Game.csproj", "-c", "Release", "--nologo", "-clp:NoSummary",
          "-p:ContinuousIntegrationBuild=true"
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
          "-p:ContinuousIntegrationBuild=true", "-p:ScriptVariant=V2"
        };
        CHECK(debug.arguments == expectedDebug);
        CHECK(debug.timeoutSeconds == doctest::Approx(30.0));
        bool telemetryOff = false;
        for (const auto &[name, value] : debug.environment) {
            telemetryOff = telemetryOff || (name == "DOTNET_CLI_TELEMETRY_OPTOUT" && value == "1");
        }
        CHECK(telemetryOff);
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
