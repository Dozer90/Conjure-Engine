#include "conjure/testing/doctest.h"

#include "runtime_test_support.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/process.h"
#include "conjure/core/types.h"
#include "conjure/engine/engine_version.h"
#include "conjure/engine/exit_code.h"

#include <filesystem>
#include <string>

// The boot cases of 08_testing.md 5.2 (runtime row): an exported folder boots headless for 60 frames with exit
// 0, both with the build's runtime and with the copy the export placed into the folder (also without --project:
// the project.json next to the executable, the double-click launch), a development project boots directly, the
// flags reach the engine configuration, a path outside the active code page survives, and --help exits 0.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;
using namespace runtime_test;

TEST_SUITE("runtime/runtime_boot") {

    TEST_CASE("conjure_runtime - boots an exported folder headless for 60 frames and exits 0") {
        log_capture logs = {};
        temp_directory directory("runtime_export_boot");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        write_tiny_project(projectDir);
        export_tiny_project(projectDir, outputDir, false);

        const core::process_result result =
          run_runtime({"--project", path_argument(outputDir), "--headless", "--frames", "60"});
        const std::string output = all_output(result);
        INFO(output);
        CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
        CHECK(contains(output, "startup scene 'assets://scenes/start.scene.json' loaded"));
        CHECK(contains(output, "shutting down after 60 frame(s)"));
        CHECK(contains(output, "cooked asset(s) in the pack"));
    }

    TEST_CASE("conjure_runtime - the exported folder runs standalone from its own copy of the runtime") {
        log_capture logs = {};
        temp_directory directory("runtime_export_standalone");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        write_tiny_project(projectDir);
        export_tiny_project(projectDir, outputDir, true);
        const std::filesystem::path copiedRuntime = outputDir / runtime_executable().filename();
        REQUIRE(std::filesystem::is_regular_file(copiedRuntime));

        const core::process_result result =
          run_runtime({"--project", path_argument(outputDir), "--headless", "--frames", "60"}, copiedRuntime);
        const std::string output = all_output(result);
        INFO(output);
        CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
        CHECK(contains(output, "shutting down after 60 frame(s)"));
    }

    TEST_CASE("regression - review 4 without --project the runtime runs the project.json next to the executable") {
        log_capture logs = {};
        temp_directory directory("runtime_double_click");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        write_tiny_project(projectDir);
        export_tiny_project(projectDir, outputDir, true);
        const std::filesystem::path copiedRuntime = outputDir / runtime_executable().filename();
        REQUIRE(std::filesystem::is_regular_file(copiedRuntime));

        // The way a player starts an exported folder: no arguments but the ones a launcher would add.
        const core::process_result result = run_runtime({"--headless", "--frames", "5"}, copiedRuntime);
        const std::string output = all_output(result);
        INFO(output);
        CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
        CHECK(contains(output, "startup scene 'assets://scenes/start.scene.json' loaded"));
        CHECK(contains(output, "shutting down after 5 frame(s)"));

#if CONJURE_DISTRIBUTION
        // Distribution hides the console, so without --log-file the engine log lands next to the project.
        const std::filesystem::path defaultLog = outputDir / "conjure_runtime.log";
        REQUIRE(std::filesystem::is_regular_file(defaultLog));
        const std::string logText = read_text_file(defaultLog);
        INFO(logText);
        CHECK(contains(logText, "shutting down after 5 frame(s)"));
#endif
    }

    TEST_CASE("conjure_runtime - a development project boots directly and quits after --frames") {
        log_capture logs = {};
        temp_directory directory("runtime_dev_boot");
        const std::filesystem::path projectDir = directory.path() / "project";
        write_tiny_project(projectDir);

        const core::process_result result =
          run_runtime({"--project", path_argument(projectDir), "--headless", "--frames", "5"});
        const std::string output = all_output(result);
        INFO(output);
        CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
        CHECK(contains(output, "startup scene 'assets://scenes/start.scene.json' loaded"));
        CHECK(contains(output, "shutting down after 5 frame(s)"));
    }

    TEST_CASE("conjure_runtime - --log-level, --width, --height and --jobs reach the engine configuration") {
        log_capture logs = {};
        temp_directory directory("runtime_flags");
        const std::filesystem::path projectDir = directory.path() / "project";
        write_file(projectDir / "project.json", project_text({}));
        write_file(projectDir / "assets" / "data" / "notes.bin", "x");
        const std::string project = path_argument(projectDir);

        SUBCASE("warn silences the INFO lines") {
            const core::process_result result =
              run_runtime({"--project", project, "--headless", "--frames", "2", "--log-level", "warn"});
            const std::string output = all_output(result);
            INFO(output);
            CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
            CHECK_FALSE(contains(output, "shutting down after"));
        }
        SUBCASE("the window size reaches the null window") {
            const core::process_result result = run_runtime(
              {"--project", project, "--headless", "--frames", "1", "--width", "320", "--height", "200"}
            );
            const std::string output = all_output(result);
            INFO(output);
            CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
            // platform_subsystem logs the window it created (06 3.4 step 6).
            CHECK(contains(output, "window 320 x 200"));
        }
        SUBCASE("--jobs 0 runs every job inline") {
            const core::process_result result =
              run_runtime({"--project", project, "--headless", "--frames", "1", "--jobs", "0"});
            INFO(all_output(result));
            CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
        }
        SUBCASE("--jobs auto resolves the worker count") {
            const core::process_result result =
              run_runtime({"--project", project, "--headless", "--frames", "1", "--jobs", "auto"});
            INFO(all_output(result));
            CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
        }
        SUBCASE("a non-numeric --jobs is a bad argument") {
            const core::process_result result =
              run_runtime({"--project", project, "--headless", "--frames", "1", "--jobs", "abc"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::BAD_ARGUMENTS));
            CHECK(contains(result.standardError, RUNTIME_PREFIX));
            CHECK(contains(result.standardError, "--jobs"));
            CHECK(contains(result.standardError, HELP_HINT));
        }
        SUBCASE("more than 256 workers is an invalid merged configuration caused by the flag") {
            const core::process_result result =
              run_runtime({"--project", project, "--headless", "--frames", "1", "--jobs", "300"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::BAD_ARGUMENTS));
            CHECK(contains(result.standardError, RUNTIME_PREFIX));
            CHECK(contains(result.standardError, "INVALID_CONFIG"));
            CHECK(contains(result.standardError, "jobs.workerCount"));
        }
        SUBCASE("an invalid log level is a bad argument") {
            const core::process_result result =
              run_runtime({"--project", project, "--headless", "--log-level", "loud"});
            CHECK(result.exitCode == static_cast<i32>(exit_code::BAD_ARGUMENTS));
            CHECK(contains(result.standardError, RUNTIME_PREFIX));
            CHECK(contains(result.standardError, "--log-level"));
        }
    }

    TEST_CASE("regression - review 4 a project path outside the active code page reaches the engine intact") {
        log_capture logs = {};
        temp_directory directory("runtime_utf8_path");
        // Latin-1 (ü), Greek (λ) and CJK (日本) characters: no single legacy code page holds them all.
        const std::filesystem::path projectDir =
          directory.path() / std::filesystem::path(std::u8string_view(u8"projekt_ü_λ_日本"));
        write_tiny_project(projectDir);

        const core::process_result result =
          run_runtime({"--project", path_argument(projectDir), "--headless", "--frames", "2"});
        const std::string output = all_output(result);
        INFO(output);
        CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
        CHECK(contains(output, "startup scene 'assets://scenes/start.scene.json' loaded"));
        CHECK_FALSE(contains(output, "PROJECT_NOT_FOUND"));
    }

    TEST_CASE("conjure_runtime - --help prints the usage and exits 0") {
        const core::process_result result = run_runtime({"--help"});
        CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
        CHECK(contains(result.standardOutput, USAGE_LINE));
        CHECK(contains(result.standardOutput, engine_version_string()));
        CHECK(contains(result.standardOutput, "--frames <n>"));
        CHECK(contains(result.standardOutput, "--headless"));
        CHECK(result.standardError.empty());

        // Help wins over every other outcome: nothing is loaded, so a missing project is not an error.
        const core::process_result withProject = run_runtime({"--project", "nowhere", "--bogus", "--help"});
        CHECK(withProject.exitCode == static_cast<i32>(exit_code::OK));
    }
}
