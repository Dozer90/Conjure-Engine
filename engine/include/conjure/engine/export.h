#pragma once

#include "conjure/engine/engine_context.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <array>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// The export pipeline (docs/design/06_engine_loop.md 3.6, ARCHITECTURE.md 8, R21): the one owner of
// `conjure_editor --export <dir>` and the editor's project.export command. Synchronous, main thread,
// outside a frame. Steps, each logged; the first that fails ends the export with EXPORT_FAILED, the steps
// before it keep their output, and step 5 (a copy file by file) keeps the files it had copied:
//
//   1  build the native game module in Distribution (when <project>/module.json declares a binary)
//   2  assets::asset_cooker::cook -> <dir>/cooked/content.cpak + manifest.json
//   3  dotnet publish Conjure.Bootstrap + dotnet build the game scripts -> <dir>/managed (scripting enabled), both
//      compiling in export_settings::artifactsDirectory, by default a folder the step removes when it ends. An
//      installed editor has no Conjure.Bootstrap project: it names the prebuilt SDK of its install layout instead
//      (export_settings::prebuiltManagedDirectory, owner decision OWN-INSTALL), which the step copies, building only
//      the game scripts against it, so a project without game scripts needs no .NET SDK at all. Before the first
//      dotnet command the step asks dotnet for its SDK (find_dotnet_sdk() below) and fails
//      naming the version global.json asks for and Microsoft's download page when there is no usable one. With
//      scripting disabled in the configuration (--no-scripting) an export that asks for managed code of a project
//      that enables scripting is refused before step 1 (EXPORT_FAILED naming this step), so nothing is written
//   4  modules::module_manager::export_layout(<dir>, DISTRIBUTION)
//   5  copy the Distribution conjure_runtime, conjure_shared and shaders/ into <dir>, with THIRD_PARTY_NOTICES.txt
//      (the licence notices the build wrote next to the runtime) and what an MSVC build placed there: nethost.dll
//      and the Visual C++ runtime DLLs (every vcruntime140*.dll and msvcp140*.dll but those of the debug runtime)
//   6  write <dir>/project.json with "cooked": true, cooked_root "cooked" and rendering.gpu_validation "off" (a
//      Distribution build never enables GPU validation, code sweep F458; --gpu-validation still sets it for a run)
//
// The exported folder runs standalone: conjure_runtime --project <dir>. On Windows that holds without the
// Microsoft Visual C++ Redistributable when the distribution directory held the Visual C++ runtime DLLs, as the
// bin folder of an MSVC Distribution build does when its configure found the toolchain's redistributable folder
// (runtime/CMakeLists.txt warns when it did not; a build against the static runtime needs none and copies none).
// A folder without them is exported all the same, and a pipeline built against Microsoft's standard library and
// its DLL runtime (/MD) adds a warning to the report. The pipeline knows its own build only: a distribution
// directory produced by another build is judged by the build that exports it. The debug runtime (a 'd' after
// "140" or at the end of the name: vcruntime140d.dll, msvcp140_1d.dll) must not be redistributed; step 5 leaves
// it in the distribution directory and names it in a warning. The Universal CRT, part of Windows 10 and later,
// is not copied. A game module that imports another redistributable library (concrt140.dll, an OpenMP runtime)
// ships it itself.
//
// Licence notices (06 D15): the exported binaries are Conjure (Apache-2.0) and the third-party software linked
// into it, and their licences ask that the notices travel with the binaries. THIRD_PARTY_NOTICES.txt holds them
// for the build that produced the distribution directory; step 5 copies it whenever it copies the runtime. A
// distribution directory without the file is exported all the same, with a warning in the report. The file
// covers the engine only: what a game adds (native modules, NuGet packages, assets) is the game's to list.

namespace conjure::engine {

    // The .NET SDK the engine's managed code needs (owner decision OWN-INSTALL): the version global.json at the root
    // of the engine's source names, baked in when the engine is configured so that an installed editor, which has no
    // source tree, still knows it; Microsoft's download page for it; and the probe that export step 3 and the editor's
    // script.compile run when they find no usable SDK. The SDK is an optional download: only an export of a project
    // with game scripts, an export from an engine source tree and script.compile build managed code. Nothing here
    // downloads anything.

    /// <summary>
    /// The SDK version global.json at the root of the engine's source names ("10.0.100"), as it read when the engine
    /// was configured.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view required_dotnet_sdk_version() noexcept;

    /// <summary>
    /// Microsoft's official download page of the release that holds required_dotnet_sdk_version(), named by its
    /// major and minor version ("https://dotnet.microsoft.com/download/dotnet/10.0").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string dotnet_sdk_download_page();

    /// <summary>
    /// True when 'version', the text `dotnet --version` prints ("10.0.103", "11.0.100-preview.1"), names
    /// required_dotnet_sdk_version() or a later SDK. Major, minor and patch compare as numbers, and a prerelease of
    /// the required version itself is older than it. Whitespace around the text is ignored; text that does not start
    /// with three dot-separated numbers, or that continues after them with anything but a '-' or '+' suffix, is false.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_usable_dotnet_sdk_version(std::string_view version) noexcept;

    /// <summary>
    /// The message for a build that found no usable SDK: 'detail' (what was found instead), the version required as
    /// global.json names it and the download page. It never asks for anything to be fetched automatically.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string missing_dotnet_sdk_message(std::string_view detail);

    /// <summary>
    /// Runs `<dotnet> --version` in 'workingDirectory' (dotnet reads the global.json in it or above it from there)
    /// and returns the version of the SDK it selects. DEPENDENCY_FAILURE (core) carrying missing_dotnet_sdk_message()
    /// when dotnet could not be started, did not answer within 'timeoutSeconds' (0 waits without a limit), exited
    /// with a code other than 0 (no SDK installed, or none that such a global.json accepts) or printed a version that
    /// is_usable_dotnet_sdk_version() refuses. 'workingDirectory' must exist; empty runs in the current directory.
    /// Blocks the calling thread while dotnet runs.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<std::string> find_dotnet_sdk(
      std::string_view dotnet,
      const std::filesystem::path &workingDirectory,
      f64 timeoutSeconds
    );

    /// <summary>
    /// Called before each step with its name and the fraction of steps completed so far.
    /// </summary>
    using progress_fn = std::function<void(std::string_view step, f32 fraction)>;

    struct export_settings {
        std::filesystem::path outputDirectory = {};             // created when missing; existing files are replaced
        // Holds conjure_runtime, conjure_shared, THIRD_PARTY_NOTICES.txt and shaders/ (and, from an MSVC build,
        // nethost.dll and the release Visual C++ runtime DLLs).
        std::filesystem::path distributionDirectory = {};
        std::vector<std::string> alwaysInclude = {};            // asset paths ("assets://...") cooked with the closure
        bool buildNativeModule = true;                          // step 1 (skipped without a native game module)
        std::string cmakeExecutable = "cmake";
        // Step 3. With scripting disabled in the engine's configuration it publishes nothing, and run() refuses the
        // export unless the project disables scripting as well.
        bool publishManaged = true;
        std::string dotnetExecutable = "dotnet";
        std::filesystem::path bootstrapProject = {};            // managed/Conjure.Bootstrap/Conjure.Bootstrap.csproj
        // Owner decision OWN-INSTALL: a folder holding the prebuilt, export-flavoured SDK (Conjure.Bootstrap and
        // Conjure.Managed as the build target conjure_managed_sdk_export publishes them into
        // <build>/bin/managed_export, with export_pipeline::MANAGED_SDK_MANIFEST_NAME), which an installed editor finds
        // in the managed/ folder beside it. Set instead of bootstrapProject (both set is BAD_ARGUMENTS): step 3 copies
        // every regular file directly in it into <dir>/managed but the manifest, the .pdb files only with
        // scripting.exportDebugSymbols, then builds scriptsProject against it (without game scripts it runs no
        // dotnet). It is framework-dependent and portable, so runtimeIdentifier does not apply to it and a
        // self-contained export (scripting.exportMode) is refused. A relative path is resolved against the working
        // directory.
        std::filesystem::path prebuiltManagedDirectory = {};
        // <project>/scripts/Game.csproj as script.create writes it; empty = no game scripts.
        std::filesystem::path scriptsProject = {};
        // Where both dotnet commands of step 3 compile (--artifacts-path: their obj/ and bin/ trees, never the
        // assemblies that ship). Empty = a folder of the step's own staging directory below the system's temporary
        // directory, removed with it when the step ends (a removal that fails leaves it there, with a warning in the
        // log), so an export leaves no intermediates in the engine's or the game's folders. A folder named here is
        // kept, and the export compiles into a pivot of its own in it (release_export_*), so no build without the
        // export's CI flag shares its intermediates. A relative path is resolved against the working directory; one
        // inside the output directory or the assets root is BAD_ARGUMENTS.
        std::filesystem::path artifactsDirectory = {};
        std::string runtimeIdentifier = {};                     // dotnet -r; empty = the host's
        bool copyRuntime = true;                                // step 5
        bool copyShaders = true;
        f64 processTimeoutSeconds = 1800.0;                     // per spawned tool
    };

    struct export_report {
        u32 assetCount = 0;
        u64 packBytes = 0;
        u64 contentHash = 0;
        u32 modulesExported = 0;
        bool nativeModuleBuilt = false;
        bool managedPublished = false;
        std::vector<std::string> steps = {};                    // completed steps in order
        std::vector<std::string> copiedFiles = {};              // paths relative to the output directory
        std::vector<std::string> warnings = {};
        f64 elapsedSeconds = 0.0;
    };

    class CONJURE_API export_pipeline {
        public:
            /// <summary>
            /// The step names reported through progress_fn and export_report::steps, in order.
            /// </summary>
            static constexpr std::array<std::string_view, 6> STEP_NAMES = {
                "build_game_module",
                "cook_assets",
                "publish_managed",
                "export_modules",
                "copy_runtime",
                "write_project",
            };

            /// <summary>
            /// The file of licence notices a build writes next to conjure_runtime and step 5 copies into the
            /// output: Conjure's own notice and licence, then those of the third-party software that build
            /// links (docs/design/06_engine_loop.md D15).
            /// </summary>
            static constexpr std::string_view NOTICES_FILE_NAME = "THIRD_PARTY_NOTICES.txt";

            /// <summary>
            /// The file that marks a folder as the prebuilt SDK of export_settings::prebuiltManagedDirectory. The build
            /// target conjure_managed_sdk_export writes it after its publish succeeded: a JSON object with
            /// "schema_version" 1, "kind" "conjure_managed_sdk_export", "host_mode" "framework_dependent" and
            /// "native_api_hash", the CONJURE_NATIVE_API_HASH (conjure/scripting/native_api_hash.h) of the build, as
            /// hexadecimal text ("0x...").
            /// </summary>
            static constexpr std::string_view MANAGED_SDK_MANIFEST_NAME = "conjure_managed_sdk.json";

        private:
            engine_context *mContext = nullptr;

        public:
            /// <summary>
            /// Binds the pipeline to a running application's context: it needs the project, the asset
            /// database and manager, the module manager and the job system.
            /// </summary>
            explicit export_pipeline(engine_context &context);
            ~export_pipeline();

            export_pipeline(const export_pipeline &) = delete;
            export_pipeline &operator=(const export_pipeline &) = delete;

        public:
            /// <summary>
            /// Runs the six steps. EXPORT_FAILED names the step and the cause; the steps before it keep their
            /// output, and step 5, which copies file by file, keeps the files it copied before the one that
            /// failed. INVALID_STATE (core) when the context has no project or no asset database (a cooked
            /// project cannot be exported again). BAD_ARGUMENTS for an empty output directory, the project
            /// directory itself or a folder inside the assets root, and for an artifacts directory inside the output
            /// directory or the assets root (checked once the output directory exists, before step 1; the refusal
            /// removes the folders the export created for the output again, so it writes nothing).
            /// EXPORT_FAILED naming publish_managed, before the output directory is created, when publishManaged is
            /// set and the project enables scripting but the engine's configuration disables it (--no-scripting):
            /// the exported runtime would start the .NET host and find no managed code. BAD_ARGUMENTS, before the
            /// output directory is created, when the settings name both bootstrapProject and
            /// prebuiltManagedDirectory, and, before step 1, when step 3 would copy a prebuilt SDK that lies inside
            /// the output directory (an existing one, then, which keeps what it held). EXPORT_FAILED naming
            /// publish_managed, before the output directory is created, when step 3 would copy a prebuilt SDK whose
            /// folder lacks MANAGED_SDK_MANIFEST_NAME, holds one of another kind, schema, host mode or native API hash
            /// than this engine's, misses Conjure.Bootstrap.dll, its runtimeconfig.json or Conjure.Managed.dll, or
            /// when the export is self-contained. EXPORT_FAILED naming publish_managed and carrying
            /// missing_dotnet_sdk_message() when step 3 is about to run dotnet and
            /// find_dotnet_sdk() finds no usable SDK; steps 1 and 2 keep their output, and so does the copy of a
            /// prebuilt SDK in <dir>/managed. A
            /// distribution directory without Visual C++ runtime DLLs is not an error: a pipeline built with MSVC
            /// against the DLL runtime reports it as a warning. Debug runtime DLLs in it are not copied and are
            /// named in a warning. Neither is a directory without NOTICES_FILE_NAME an error: the report warns
            /// that the game carries no licence notices.
            /// </summary>
            [[nodiscard]] core::result<export_report> run(
              const export_settings &settings,
              const progress_fn &progress = {}
            );
    };
}
