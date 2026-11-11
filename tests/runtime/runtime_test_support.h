#pragma once

#include "conjure/core/process.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// Shared fixtures of the runtime process tests (docs/design/06_engine_loop.md 4.2 and 2.12, 08_testing.md 5.2
// runtime row): the built conjure_runtime spawned through core::process, a tiny project (one authored startup
// scene, one data blob) generated into a temp directory, and its export through engine::export_pipeline.

namespace runtime_test {

    constexpr std::string_view START_SCENE = "scenes/start.scene.json";
    constexpr std::string_view RUNTIME_PREFIX = "conjure_runtime: ";
    constexpr std::string_view USAGE_LINE = "Usage: conjure_runtime [--project <dir>]";
    constexpr std::string_view HELP_HINT = "Run 'conjure_runtime --help'";

    /// <summary>
    /// The runtime built by runtime/CMakeLists.txt and the directory holding it and conjure_shared (the paths
    /// come from tests/runtime/CMakeLists.txt).
    /// </summary>
    [[nodiscard]] const std::filesystem::path &runtime_executable();
    [[nodiscard]] const std::filesystem::path &runtime_bin_dir();

    /// <summary>
    /// Runs the runtime with the arguments (UTF-8) and returns what it produced; a failure to spawn or a
    /// timeout fails the case.
    /// </summary>
    [[nodiscard]] conjure::core::process_result run_runtime(std::vector<std::string> arguments);
    [[nodiscard]] conjure::core::process_result run_runtime(
      std::vector<std::string> arguments,
      const std::filesystem::path &executable
    );

    /// <summary>
    /// Standard output and error of a run, joined: the engine's console sink writes INFO to the former and
    /// WARN and above to the latter.
    /// </summary>
    [[nodiscard]] std::string all_output(const conjure::core::process_result &result);

    [[nodiscard]] bool contains(std::string_view text, std::string_view needle);

    /// <summary>
    /// A path as a UTF-8 command-line argument (generic separators).
    /// </summary>
    [[nodiscard]] std::string path_argument(const std::filesystem::path &path);

    /// <summary>
    /// The whole text of a file; empty when it cannot be read.
    /// </summary>
    [[nodiscard]] std::string read_text_file(const std::filesystem::path &path);

    /// <summary>
    /// Writes a text file, creating its parent directories.
    /// </summary>
    void write_file(const std::filesystem::path &path, std::string_view text);

    /// <summary>
    /// A project.json with the given startup scene (empty = none) and optional extra members.
    /// </summary>
    [[nodiscard]] std::string project_text(std::string_view startupScene, std::string_view extraJson = {});

    /// <summary>
    /// The tiny development project: project.json, an authored startup scene and one data blob.
    /// </summary>
    void write_tiny_project(const std::filesystem::path &projectDir);

    /// <summary>
    /// Exports the project into 'outputDir' through engine::export_pipeline; the runtime, the shared library
    /// and nethost are copied from the build's bin directory when 'copyRuntime'.
    /// </summary>
    void export_tiny_project(
      const std::filesystem::path &projectDir,
      const std::filesystem::path &outputDir,
      bool copyRuntime
    );
}
