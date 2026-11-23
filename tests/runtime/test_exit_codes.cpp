#include "conjure/testing/doctest.h"

#include "runtime_test_support.h"

#include "conjure/testing/temp_directory.h"

#include "conjure/core/process.h"
#include "conjure/core/types.h"
#include "conjure/engine/exit_code.h"

#include <filesystem>
#include <string>
#include <system_error>

// The remaining exit codes of 06_engine_loop.md 2.12: a command-line fault exits with BAD_ARGUMENTS (3) and the
// runtime's own "conjure_runtime: <message>" line plus the usage text or the help hint on standard error; a
// failed startup exits with INIT_FAILURE (1) with the engine's error on standard error and, when --log-file is
// given, in the log file together with the runtime's summary line.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;
using namespace runtime_test;

TEST_SUITE("runtime/exit_codes") {

    TEST_CASE("conjure_runtime - bad arguments exit with BAD_ARGUMENTS (3)") {
        temp_directory directory("runtime_bad_arguments");
        const std::filesystem::path projectDir = directory.path() / "project";
        write_file(projectDir / "project.json", project_text({}));
        const std::string project = path_argument(projectDir);

        SUBCASE("no --project and no project.json next to the executable") {
            const core::process_result result = run_runtime({"--headless", "--frames", "1"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::BAD_ARGUMENTS));
            CHECK(contains(result.standardError, RUNTIME_PREFIX));
            CHECK(contains(result.standardError, "--project <dir> is required"));
            CHECK(contains(result.standardError, USAGE_LINE));
            CHECK(result.standardOutput.empty());
        }
        SUBCASE("a flag without its value") {
            const core::process_result result = run_runtime({"--project"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::BAD_ARGUMENTS));
            CHECK(contains(result.standardError, RUNTIME_PREFIX));
            CHECK(contains(result.standardError, "missing value"));
            CHECK(contains(result.standardError, HELP_HINT));
        }
        SUBCASE("a non-numeric frame count") {
            const core::process_result result =
              run_runtime({"--project", project, "--headless", "--frames", "abc"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::BAD_ARGUMENTS));
            CHECK(contains(result.standardError, RUNTIME_PREFIX));
            CHECK(contains(result.standardError, "--frames"));
            CHECK(contains(result.standardError, HELP_HINT));
        }
        SUBCASE("an unknown option is not passed anywhere") {
            const core::process_result result =
              run_runtime({"--project", project, "--headless", "--exec", "commands.json"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::BAD_ARGUMENTS));
            CHECK(contains(result.standardError, "conjure_runtime: unknown option(s): --exec commands.json"));
            CHECK(contains(result.standardError, USAGE_LINE));
            CHECK(result.standardOutput.empty());
        }
        SUBCASE("a merged configuration a flag made invalid") {
            // The null device is only valid headless (06 2.3 validate()); the project alone is fine.
            const core::process_result result = run_runtime({"--project", project, "--rhi", "null", "--frames", "1"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::BAD_ARGUMENTS));
            CHECK(contains(result.standardError, RUNTIME_PREFIX));
            CHECK(contains(result.standardError, "INVALID_CONFIG"));
            CHECK(contains(result.standardError, HELP_HINT));
            CHECK_FALSE(contains(result.standardError, "project.json settings do not validate"));
        }
    }

    TEST_CASE("conjure_runtime - a failed startup exits with INIT_FAILURE (1)") {
        temp_directory directory("runtime_init_failure");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::string project = path_argument(projectDir);

        SUBCASE("the startup scene does not exist") {
            write_file(projectDir / "project.json", project_text("scenes/missing.scene.json"));
            write_file(projectDir / "assets" / "data" / "notes.bytes", "x");
            const core::process_result result = run_runtime({"--project", project, "--headless", "--frames", "1"});
            const std::string output = all_output(result);
            INFO(output);
            CHECK(result.exitCode == static_cast<i32>(exit_code::INIT_FAILURE));
            CHECK(contains(output, "STARTUP_SCENE_FAILED"));
            CHECK(contains(result.standardError, "conjure_runtime: initialisation failed"));
        }
        SUBCASE("a cooked project without its cooked root") {
            write_file(projectDir / "project.json", project_text({}, "\"cooked\": true, \"cooked_root\": \"cooked\""));
            const core::process_result result = run_runtime({"--project", project, "--headless", "--frames", "1"});
            const std::string output = all_output(result);
            INFO(output);
            CHECK(result.exitCode == static_cast<i32>(exit_code::INIT_FAILURE));
            CHECK(contains(output, "cooked root"));
        }
        SUBCASE("a cooked project whose cooked root holds no pack") {
            write_file(projectDir / "project.json", project_text({}, "\"cooked\": true, \"cooked_root\": \"cooked\""));
            std::error_code ignored = {};
            std::filesystem::create_directories(projectDir / "cooked", ignored);
            const core::process_result result = run_runtime({"--project", project, "--headless", "--frames", "1"});
            const std::string output = all_output(result);
            INFO(output);
            CHECK(result.exitCode == static_cast<i32>(exit_code::INIT_FAILURE));
            CHECK(contains(output, "ASSET_DATABASE_FAILED"));
        }
    }

    TEST_CASE("regression - review 4 --log-file names the error after a failed start") {
        temp_directory directory("runtime_init_failure_log");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path logFile = directory.path() / "logs" / "start.log";
        write_file(projectDir / "project.json", project_text("scenes/missing.scene.json"));
        write_file(projectDir / "assets" / "data" / "notes.bytes", "x");

        const core::process_result result = run_runtime(
          {"--project", path_argument(projectDir), "--headless", "--frames", "1", "--log-file", path_argument(logFile)}
        );
        CHECK(result.exitCode == static_cast<i32>(exit_code::INIT_FAILURE));
        REQUIRE(std::filesystem::is_regular_file(logFile));
        const std::string logText = read_text_file(logFile);
        INFO(logText);
        // The engine's line of the failed step (file sink installed by startup step 1) ...
        CHECK(contains(logText, "STARTUP_SCENE_FAILED"));
        // ... and the runtime's summary, written after the engine removed its sink while unwinding.
        CHECK(contains(logText, "[runtime] conjure_runtime: initialisation failed"));
    }
}
