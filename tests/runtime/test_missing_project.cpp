#include "conjure/testing/doctest.h"

#include "runtime_test_support.h"

#include "conjure/testing/temp_directory.h"

#include "conjure/core/process.h"
#include "conjure/core/types.h"
#include "conjure/engine/exit_code.h"

#include <filesystem>
#include <string>
#include <system_error>

// The project.json failures of 08_testing.md 5.2 (runtime row) and docs/ARCHITECTURE.md 11 ("missing
// project.json -> exit 2"): a missing, malformed, invalid or too-new project.json, and a project whose own
// settings do not validate, all exit with RUNTIME_FAILURE (2) naming the cause on standard error (and in the
// log file when --log-file is given), without the help hint that belongs to a command-line fault.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;
using namespace runtime_test;

TEST_SUITE("runtime/missing_project") {

    TEST_CASE("conjure_runtime - a missing project.json exits with RUNTIME_FAILURE (2)") {
        temp_directory directory("runtime_missing_project");

        SUBCASE("the directory exists but holds no project.json") {
            const std::filesystem::path emptyDir = directory.path() / "empty";
            std::error_code ignored = {};
            std::filesystem::create_directories(emptyDir, ignored);
            const core::process_result result = run_runtime({"--project", path_argument(emptyDir), "--headless"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::RUNTIME_FAILURE));
            CHECK(contains(result.standardError, RUNTIME_PREFIX));
            CHECK(contains(result.standardError, "PROJECT_NOT_FOUND"));
            CHECK_FALSE(contains(result.standardError, HELP_HINT));
        }
        SUBCASE("the directory does not exist") {
            const std::filesystem::path absent = directory.path() / "absent";
            const core::process_result result = run_runtime({"--project", path_argument(absent), "--headless"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::RUNTIME_FAILURE));
            CHECK(contains(result.standardError, RUNTIME_PREFIX));
            CHECK(contains(result.standardError, "PROJECT_NOT_FOUND"));
        }
    }

    TEST_CASE("regression - review 4 --log-file names a missing project after the failed start") {
        temp_directory directory("runtime_missing_project_log");
        const std::filesystem::path absent = directory.path() / "absent";
        const std::filesystem::path logFile = directory.path() / "logs" / "start.log";

        // Nothing of the engine ran, so only the runtime's own channel can put the error into the file.
        const core::process_result result =
          run_runtime({"--project", path_argument(absent), "--headless", "--log-file", path_argument(logFile)});
        CHECK(result.exitCode == static_cast<i32>(exit_code::RUNTIME_FAILURE));
        REQUIRE(std::filesystem::is_regular_file(logFile));
        const std::string logText = read_text_file(logFile);
        INFO(logText);
        CHECK(contains(logText, "[ERROR]"));
        CHECK(contains(logText, "[runtime]"));
        CHECK(contains(logText, "conjure_runtime: "));
        CHECK(contains(logText, "PROJECT_NOT_FOUND"));
    }

    TEST_CASE("conjure_runtime - a broken project.json exits with RUNTIME_FAILURE (2) naming the cause") {
        temp_directory directory("runtime_broken_project");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::string project = path_argument(projectDir);

        SUBCASE("malformed JSON reports the parse failure") {
            write_file(projectDir / "project.json", "{ \"schema_version\": 1, \"name\": ");
            const core::process_result result = run_runtime({"--project", project, "--headless"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::RUNTIME_FAILURE));
            CHECK(contains(result.standardError, "PROJECT_PARSE_FAILED"));
        }
        SUBCASE("a missing name is invalid") {
            write_file(projectDir / "project.json", "{ \"schema_version\": 1 }");
            const core::process_result result = run_runtime({"--project", project, "--headless"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::RUNTIME_FAILURE));
            CHECK(contains(result.standardError, "PROJECT_INVALID"));
        }
        SUBCASE("a newer schema is refused") {
            write_file(projectDir / "project.json", "{ \"schema_version\": 99, \"name\": \"future\" }");
            const core::process_result result = run_runtime({"--project", project, "--headless"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::RUNTIME_FAILURE));
            CHECK(contains(result.standardError, "PROJECT_SCHEMA_TOO_NEW"));
        }
    }

    TEST_CASE("regression - review 4 project settings that do not validate exit with RUNTIME_FAILURE (2)") {
        temp_directory directory("runtime_invalid_project_settings");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::string project = path_argument(projectDir);

        SUBCASE("a zero window size is the project's fault, not the command line's") {
            write_file(projectDir / "project.json", project_text({}, "\"window\": { \"width\": 0, \"height\": 0 }"));
            // Not headless: a headless run would keep its own null window size and validate.
            const core::process_result result = run_runtime({"--project", project, "--frames", "1"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::RUNTIME_FAILURE));
            CHECK(contains(result.standardError, RUNTIME_PREFIX));
            CHECK(contains(result.standardError, "project.json settings do not validate"));
            CHECK(contains(result.standardError, "INVALID_CONFIG"));
            CHECK(contains(result.standardError, "window"));
            CHECK_FALSE(contains(result.standardError, HELP_HINT));
        }
        SUBCASE("a frame delta cap below the fixed step is the project's fault, headless or not") {
            write_file(
              projectDir / "project.json",
              project_text({}, "\"time\": { \"fixed_hz\": 60, \"max_frame_delta_seconds\": 0.001 }")
            );
            const core::process_result result = run_runtime({"--project", project, "--headless", "--frames", "1"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::RUNTIME_FAILURE));
            CHECK(contains(result.standardError, "project.json settings do not validate"));
            CHECK(contains(result.standardError, "maxFrameDeltaSeconds"));
        }
        SUBCASE("the same project with a flag that fixes it runs") {
            write_file(projectDir / "project.json", project_text({}, "\"window\": { \"width\": 0, \"height\": 0 }"));
            write_file(projectDir / "assets" / "data" / "notes.bin", "x");
            const core::process_result result =
              run_runtime({"--project", project, "--headless", "--frames", "1"});
            INFO(all_output(result));
            CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
        }
    }
}
