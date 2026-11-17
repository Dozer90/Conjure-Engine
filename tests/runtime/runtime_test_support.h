#pragma once

#include "conjure/core/process.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Shared fixtures of the runtime process tests (docs/design/06_engine_loop.md 4.2 and 2.12, 08_testing.md 5.2
// runtime row): the built conjure_runtime spawned through core::process, a tiny project (one authored startup
// scene, one data blob) generated into a temp directory, its export through engine::export_pipeline, a
// scripted variant of the project for the --scripting cases (06 D12), the Visual C++ runtime DLLs a Windows
// Distribution build places next to the runtime (06 3.6 step 5), and which optional libraries the build links
// (what its licence notices have to name, 06 D15).

namespace runtime_test {

    constexpr std::string_view START_SCENE = "scenes/start.scene.json";
    constexpr std::string_view RUNTIME_PREFIX = "conjure_runtime: ";
    constexpr std::string_view USAGE_LINE = "Usage: conjure_runtime [--project <dir>]";
    constexpr std::string_view HELP_HINT = "Run 'conjure_runtime --help'";

    // The script of the scripted project: Conjure.TestScripts.DestroyProbe destroys its entity in its first
    // update and logs SCRIPT_MARKER from OnDestroy, which is how a run shows on its output that C# executed.
    constexpr std::string_view SCRIPT_CLASS = "Conjure.TestScripts.DestroyProbe";
    constexpr std::string_view SCRIPT_MARKER = "DestroyProbe.OnDestroy";
    // What the scripting subsystem logs when a headless run leaves a scripted project's scripts off.
    constexpr std::string_view HEADLESS_SCRIPTING_HINT = "unless --scripting is given";

    /// <summary>
    /// The runtime built by runtime/CMakeLists.txt and the directory holding it and conjure_shared (the paths
    /// come from tests/runtime/CMakeLists.txt).
    /// </summary>
    [[nodiscard]] const std::filesystem::path &runtime_executable();
    [[nodiscard]] const std::filesystem::path &runtime_bin_dir();

    /// <summary>
    /// Runs the runtime with the arguments (UTF-8) and returns what it produced; a failure to spawn or a
    /// timeout fails the case. 'environment' adds to, or overrides, the variables the child inherits.
    /// </summary>
    [[nodiscard]] conjure::core::process_result run_runtime(std::vector<std::string> arguments);
    [[nodiscard]] conjure::core::process_result run_runtime(
      std::vector<std::string> arguments,
      const std::filesystem::path &executable
    );
    [[nodiscard]] conjure::core::process_result run_runtime(
      std::vector<std::string> arguments,
      const std::filesystem::path &executable,
      std::vector<std::pair<std::string, std::string>> environment
    );

    /// <summary>
    /// The variables that make the child's .NET host look for a runtime in 'root' and nowhere else: DOTNET_ROOT
    /// and its per-architecture spellings. With an empty directory the child runs like a machine without .NET.
    /// </summary>
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> dotnet_root_environment(
      const std::filesystem::path &root
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
    /// True when this build has the scripting module, so that conjure_runtime has a scripting subsystem and
    /// write_scripted_project can author its scene.
    /// </summary>
    [[nodiscard]] bool scripting_built() noexcept;

    /// <summary>
    /// True when this build links the editor's libraries (Dear ImGui, ImGuizmo) into conjure_shared, as every
    /// build but a Distribution one does unless the editor is switched off; and true when it links the physics
    /// module (Jolt). The licence notices next to the runtime name what the build links.
    /// </summary>
    [[nodiscard]] bool editor_built() noexcept;
    [[nodiscard]] bool physics_built() noexcept;

    /// <summary>
    /// The v1 build of Conjure.TestScripts.dll; empty when the build has no managed SDK (the [dotnet] case that
    /// uses it is compiled only in a build that has one).
    /// </summary>
    [[nodiscard]] std::filesystem::path test_scripts_assembly();

    /// <summary>
    /// The tiny project with scripting enabled: its startup scene carries one entity with the SCRIPT_CLASS
    /// script, and project.json names 'gameAssembly' (absolute, or relative to the project) with hot reload
    /// off. The assembly need not exist: only a run that starts the script runtime opens it. Fails the case
    /// when the build has no scripting module (ask scripting_built() first).
    /// </summary>
    void write_scripted_project(const std::filesystem::path &projectDir, const std::filesystem::path &gameAssembly);

    /// <summary>
    /// True when this build copies the toolchain's Visual C++ runtime DLLs next to conjure_runtime: an MSVC
    /// Distribution build whose configure found the redistributable folder (runtime/CMakeLists.txt). Every
    /// other build places none there.
    /// </summary>
    [[nodiscard]] bool msvc_runtime_libraries_copied() noexcept;

    /// <summary>
    /// The vcruntime140*.dll and msvcp140*.dll directly in 'directory', as file names in byte order; empty
    /// when the directory holds none or cannot be read. The debug runtime, which no build copies and no export
    /// ships, would be listed like the release one.
    /// </summary>
    [[nodiscard]] std::vector<std::filesystem::path> msvc_runtime_libraries_in(
      const std::filesystem::path &directory
    );

    /// <summary>
    /// Exports the project into 'outputDir' through engine::export_pipeline; the runtime, the shared library,
    /// the licence notices (THIRD_PARTY_NOTICES.txt), nethost and the Visual C++ runtime DLLs the build's bin
    /// directory holds are copied from it when 'copyRuntime'.
    /// </summary>
    void export_tiny_project(
      const std::filesystem::path &projectDir,
      const std::filesystem::path &outputDir,
      bool copyRuntime
    );
}
