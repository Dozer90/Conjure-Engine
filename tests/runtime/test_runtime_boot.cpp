#include "conjure/testing/doctest.h"

#include "runtime_test_support.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "conjure/testing/test_environment.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/process.h"
#include "conjure/core/types.h"
#include "conjure/engine/command_line.h"
#include "conjure/engine/engine_version.h"
#include "conjure/engine/exit_code.h"
#include "conjure/engine/export.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// The boot cases of 08_testing.md 5.2 (runtime row): an exported folder boots headless for 60 frames with exit
// 0, both with the build's runtime and with the copy the export placed into the folder (also without --project:
// the project.json next to the executable, the double-click launch), a development project boots directly, the
// flags reach the engine configuration, a path outside the active code page survives, and --help exits 0 with
// the whole option list on a piped standard output. One [window] case (CONJURE_TEST_WINDOW=1, never in CI) runs
// the runtime on a real window and a real graphics device. The --scripting cases (06 D12): a scripted project
// boots headless without a script runtime, --headless --scripting starts one, and one [dotnet] case runs C#;
// that case exists only in a build that can run it (the scripting module and the managed SDK), so a build
// without them has no entry that could only report itself as skipped.
// One case holds a Windows Distribution build to the Visual C++ runtime DLLs it ships (06 3.6 step 5), and one
// holds every build to the licence notices it writes next to the runtime and an export ships (06 D15).

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

    TEST_CASE("regression - code sweep F071 [window] a windowed run from the build tree exits 0 after thirty frames") {
        CONJURE_TEST_REQUIRE(precondition::REAL_WINDOW);
        log_capture logs = {};
        temp_directory directory("runtime_windowed");
        const std::filesystem::path projectDir = directory.path() / "project";
        write_tiny_project(projectDir);

        // No --headless and no shader directory: a real window, the default graphics device with its swapchain
        // (under the API's debug layer in a Debug build) and the shader library next to the executable (F027).
        // The run used to die at shutdown with 0x87D, the swapchain's buffers released under their last present.
        const core::process_result result = run_runtime({"--project", path_argument(projectDir), "--frames", "30"});
        const std::string output = all_output(result);
        INFO(output);
        CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
        CHECK(contains(output, "device with swapchain"));
        CHECK(contains(output, "shutting down after 30 frame(s)"));
        CHECK_FALSE(contains(output, "[ERROR]"));
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

    TEST_CASE("regression - code sweep F790 a Windows Distribution build ships the Visual C++ runtime DLLs with "
      "the exported game") {
        if (!msvc_runtime_libraries_copied()) {
            // Linux and macOS have no such libraries, a Debug or Development build copies none (developer
            // builds run where the toolchain installed them), and neither does a configure that found no
            // redistributable folder of the compiler's version (it warned). tests/engine/test_export.cpp covers
            // the copy of export step 5 on every platform.
            MESSAGE("this build places no Visual C++ runtime DLLs next to conjure_runtime; nothing to check here");
            return;
        }
        log_capture logs = {};
        temp_directory directory("runtime_export_crt");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        write_tiny_project(projectDir);

        // The build placed the libraries next to the runtime (runtime/CMakeLists.txt): at least the two every
        // architecture has, which conjure_runtime and conjure_shared both import.
        const std::vector<std::filesystem::path> built = msvc_runtime_libraries_in(runtime_bin_dir());
        CHECK(std::find(built.begin(), built.end(), std::filesystem::path("vcruntime140.dll")) != built.end());
        CHECK(std::find(built.begin(), built.end(), std::filesystem::path("msvcp140.dll")) != built.end());

        // The export carries every one of them, whole.
        export_tiny_project(projectDir, outputDir, true);
        CHECK(msvc_runtime_libraries_in(outputDir) == built);
        for (const std::filesystem::path &library : built) {
            CAPTURE(library);
            std::error_code error = {};
            const auto builtSize = std::filesystem::file_size(runtime_bin_dir() / library, error);
            REQUIRE_FALSE(error);
            const auto exportedSize = std::filesystem::file_size(outputDir / library, error);
            REQUIRE_FALSE(error);
            CHECK(builtSize > 0);
            CHECK(exportedSize == builtSize);
        }

        // And the exported runtime starts with the copies next to it, which the loader prefers to installed ones.
        const std::filesystem::path copiedRuntime = outputDir / runtime_executable().filename();
        REQUIRE(std::filesystem::is_regular_file(copiedRuntime));
        const core::process_result result = run_runtime({"--headless", "--frames", "5"}, copiedRuntime);
        const std::string output = all_output(result);
        INFO(output);
        CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
    }

    TEST_CASE("regression - code sweep F200 the build writes the licence notices next to the runtime and an export "
      "ships them") {
        log_capture logs = {};
        temp_directory directory("runtime_export_notices");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        write_tiny_project(projectDir);

        // The build placed the notices next to the runtime (runtime/CMakeLists.txt, cmake/ConjureNotices.cmake).
        const std::filesystem::path builtNotices = runtime_bin_dir() / export_pipeline::NOTICES_FILE_NAME;
        REQUIRE(std::filesystem::is_regular_file(builtNotices));
        const std::string notices = read_text_file(builtNotices);
        // Conjure's own notice and licence open the file.
        const usize ownNotice = notices.find("the Conjure contributors");
        const usize ownLicence = notices.find("Version 2.0, January 2004");
        CHECK(ownNotice != std::string::npos);
        CHECK(ownLicence != std::string::npos);
        CHECK(ownNotice < ownLicence);
        // Then what this build links: yyjson is part of every build (core), and its entry carries the licence
        // text of the pinned source, not only its name.
        const usize yyjsonLicence = notices.find("Copyright (c) 2020 YaoYuan");
        CHECK(contains(notices, "yyjson"));
        CHECK(yyjsonLicence != std::string::npos);
        CHECK(ownLicence < yyjsonLicence);
        CHECK(contains(notices, "Permission is hereby granted, free of charge"));
        // An optional library is named exactly when the build links it: Jolt with the physics module, Dear ImGui
        // with the editor (never in a Distribution build).
        CHECK(contains(notices, "Jolt Physics") == physics_built());
        CHECK(contains(notices, "Dear ImGui") == editor_built());
        // A test tool reaches no binary that ships, and so no notices.
        CHECK_FALSE(contains(notices, "doctest"));

        // The export carries the file whole, and a folder exported without the runtime carries none.
        export_tiny_project(projectDir, outputDir, true);
        const std::filesystem::path exportedNotices = outputDir / export_pipeline::NOTICES_FILE_NAME;
        REQUIRE(std::filesystem::is_regular_file(exportedNotices));
        CHECK(read_text_file(exportedNotices) == notices);
        const std::filesystem::path plainDir = directory.path() / "plain";
        export_tiny_project(projectDir, plainDir, false);
        CHECK(std::filesystem::is_regular_file(plainDir / "project.json"));
        CHECK_FALSE(std::filesystem::exists(plainDir / export_pipeline::NOTICES_FILE_NAME));
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

    TEST_CASE("conjure_runtime - plain --headless boots a scripted project and names --scripting in its log") {
        if (!scripting_built()) {
            MESSAGE("built without the scripting module: conjure_runtime has no scripting subsystem to leave off");
            return;
        }
        log_capture logs = {};
        temp_directory directory("runtime_headless_scripted");
        const std::filesystem::path projectDir = directory.path() / "project";
        // The assembly does not exist: a run that needed .NET or the game assembly would fail to start.
        write_scripted_project(projectDir, "Scripts/bin/Missing.dll");

        const core::process_result result =
          run_runtime({"--project", path_argument(projectDir), "--headless", "--frames", "5"});
        const std::string output = all_output(result);
        INFO(output);
        CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
        CHECK(contains(output, "shutting down after 5 frame(s)"));
        CHECK(contains(output, HEADLESS_SCRIPTING_HINT));
        CHECK_FALSE(contains(output, SCRIPT_MARKER));
    }

    TEST_CASE("regression - code sweep F075 --headless --scripting starts the script runtime of a scripted project") {
        // No .NET needed to see that the flag arrived: the project's game assembly is missing, so the start
        // fails in the scripting subsystem. Which of its errors it is depends on the machine (no .NET runtime,
        // no managed SDK next to the runtime, or both there and the assembly missing); before the flag existed
        // the same command line was an unknown option, and nothing made a headless run reach this subsystem.
        if (!scripting_built()) {
            MESSAGE("built without the scripting module: --scripting has no subsystem to start");
            return;
        }
        log_capture logs = {};
        temp_directory directory("runtime_headless_scripting_flag");
        const std::filesystem::path projectDir = directory.path() / "project";
        write_scripted_project(projectDir, "Scripts/bin/Missing.dll");
        const std::string project = path_argument(projectDir);

        SUBCASE("the start fails in the scripting subsystem with exit 1") {
            const core::process_result result =
              run_runtime({"--project", project, "--headless", "--scripting", "--frames", "5"});
            const std::string output = all_output(result);
            INFO(output);
            CHECK(result.exitCode == static_cast<i32>(exit_code::INIT_FAILURE));
            CHECK(contains(result.standardError, RUNTIME_PREFIX));
            CHECK(contains(result.standardError, "initialisation failed"));
            const bool scriptingError = contains(result.standardError, "SCRIPT_RUNTIME_NOT_FOUND")
              || contains(result.standardError, "SCRIPT_RUNTIME_INIT_FAILED")
              || contains(result.standardError, "SCRIPT_ASSEMBLY_LOAD_FAILED");
            CHECK(scriptingError);
            CHECK_FALSE(contains(output, HEADLESS_SCRIPTING_HINT));
            CHECK_FALSE(contains(output, "shutting down after"));
        }
        SUBCASE("on a machine without a .NET runtime the start fails with SCRIPT_RUNTIME_NOT_FOUND") {
            // The child's .NET host is pointed at an empty directory, which is all a machine without .NET is to it.
            const std::filesystem::path emptyRoot = directory.path() / "no_dotnet";
            std::error_code ignored = {};
            std::filesystem::create_directories(emptyRoot, ignored);
            const core::process_result result = run_runtime(
              {"--project", project, "--headless", "--scripting", "--frames", "5"},
              runtime_executable(),
              dotnet_root_environment(emptyRoot)
            );
            const std::string output = all_output(result);
            INFO(output);
            CHECK(result.exitCode == static_cast<i32>(exit_code::INIT_FAILURE));
            if (!contains(result.standardError, "SCRIPT_RUNTIME_NOT_FOUND")) {
                MESSAGE(
                  "the .NET host of this machine found a runtime although DOTNET_ROOT names an empty directory; "
                  "the start without a runtime could not be exercised here"
                );
                return;
            }
            CHECK(contains(result.standardError, RUNTIME_PREFIX));
            CHECK(contains(result.standardError, "initialisation failed"));
            CHECK_FALSE(contains(output, "shutting down after"));

            // The same machine still boots the project headless without the flag.
            const core::process_result plain = run_runtime(
              {"--project", project, "--headless", "--frames", "5"},
              runtime_executable(),
              dotnet_root_environment(emptyRoot)
            );
            INFO(all_output(plain));
            CHECK(plain.exitCode == static_cast<i32>(exit_code::OK));
            CHECK(contains(all_output(plain), "shutting down after 5 frame(s)"));
        }
        SUBCASE("--scripting with --no-scripting is a bad argument") {
            const core::process_result result =
              run_runtime({"--project", project, "--headless", "--scripting", "--no-scripting", "--frames", "5"});
            INFO(all_output(result));
            CHECK(result.exitCode == static_cast<i32>(exit_code::BAD_ARGUMENTS));
            CHECK(contains(result.standardError, RUNTIME_PREFIX));
            CHECK(contains(result.standardError, "--no-scripting"));
            CHECK(contains(result.standardError, HELP_HINT));
        }
    }

// Compiled only where the build has the scripting module, the managed SDK and the test scripts
// (tests/runtime/CMakeLists.txt): in any other build the DOTNET_RUNTIME probe could only answer no, and a
// [dotnet] case that reports itself as skipped fails the skip check of the CI legs.
#if defined(CONJURE_RUNTIME_TEST_HAS_DOTNET) && CONJURE_RUNTIME_TEST_HAS_DOTNET
    TEST_CASE(
      "regression - code sweep F075 --headless --scripting runs the C# scripts of a project [dotnet]"
      * doctest::timeout(240)
    ) {
        // Skipped only on a machine without a .NET runtime.
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        temp_directory directory("runtime_headless_csharp");
        const std::filesystem::path projectDir = directory.path() / "project";
        write_scripted_project(projectDir, test_scripts_assembly());
        const std::string project = path_argument(projectDir);

        SUBCASE("with --scripting the script's OnDestroy line reaches the output") {
            const core::process_result result =
              run_runtime({"--project", project, "--headless", "--scripting", "--frames", "5"});
            const std::string output = all_output(result);
            INFO(output);
            CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
            CHECK(contains(output, "shutting down after 5 frame(s)"));
            CHECK(contains(output, SCRIPT_MARKER));
            CHECK_FALSE(contains(output, HEADLESS_SCRIPTING_HINT));
        }
        SUBCASE("without it the same project boots and no script runs") {
            const core::process_result result = run_runtime({"--project", project, "--headless", "--frames", "5"});
            const std::string output = all_output(result);
            INFO(output);
            CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
            CHECK(contains(output, "shutting down after 5 frame(s)"));
            CHECK_FALSE(contains(output, SCRIPT_MARKER));
            CHECK(contains(output, HEADLESS_SCRIPTING_HINT));
        }
    }
#endif

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
        // A process without a standard output (the console-less Distribution build) also shows the usage text in
        // a modal information box (code sweep F470), which needs a desktop and is not automated. run_runtime
        // fails the case when the child times out, so a run that returns proves a piped --help never opened it.
        const core::process_result result = run_runtime({"--help"});
        CHECK(result.exitCode == static_cast<i32>(exit_code::OK));
        CHECK(contains(result.standardOutput, USAGE_LINE));
        CHECK(contains(result.standardOutput, engine_version_string()));
        CHECK(result.standardError.empty());

        // Every option line, one by one: the text-mode stream of the child may turn every line feed into CR LF.
        const std::string_view help = command_line_help();
        REQUIRE_FALSE(help.empty());
        usize lineStart = 0;
        while (lineStart < help.size()) {
            usize lineEnd = help.find('\n', lineStart);
            if (lineEnd == std::string_view::npos) {
                lineEnd = help.size();
            }
            const std::string_view line = help.substr(lineStart, lineEnd - lineStart);
            CAPTURE(line);
            CHECK(contains(result.standardOutput, line));
            lineStart = lineEnd + 1;
        }

        // Help wins over every other outcome: nothing is loaded, so a missing project is not an error.
        const core::process_result withProject = run_runtime({"--project", "nowhere", "--bogus", "--help"});
        CHECK(withProject.exitCode == static_cast<i32>(exit_code::OK));
    }
}
