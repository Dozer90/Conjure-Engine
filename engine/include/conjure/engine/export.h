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
//   3  dotnet publish Conjure.Bootstrap + dotnet build the game scripts -> <dir>/managed (scripting enabled)
//   4  modules::module_manager::export_layout(<dir>, DISTRIBUTION)
//   5  copy the Distribution conjure_runtime, conjure_shared and shaders/ into <dir>, with THIRD_PARTY_NOTICES.txt
//      (the licence notices the build wrote next to the runtime) and what an MSVC build placed there: nethost.dll
//      and the Visual C++ runtime DLLs (every vcruntime140*.dll and msvcp140*.dll but those of the debug runtime)
//   6  write <dir>/project.json with "cooked": true and cooked_root "cooked"
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
        bool publishManaged = true;                             // step 3 (skipped when scripting is disabled)
        std::string dotnetExecutable = "dotnet";
        std::filesystem::path bootstrapProject = {};            // managed/Conjure.Bootstrap/Conjure.Bootstrap.csproj
        std::filesystem::path scriptsProject = {};              // <project>/Scripts/*.csproj; empty = no game scripts
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
            /// directory itself or a folder inside the assets root. A distribution directory without Visual C++
            /// runtime DLLs is not an error: a pipeline built with MSVC against the DLL runtime reports it as a
            /// warning. Debug runtime DLLs in it are not copied and are named in a warning. Neither is a
            /// directory without NOTICES_FILE_NAME an error: the report warns that the game carries no licence
            /// notices.
            /// </summary>
            [[nodiscard]] core::result<export_report> run(
              const export_settings &settings,
              const progress_fn &progress = {}
            );
    };
}
