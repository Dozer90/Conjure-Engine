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
// outside a frame. Steps, each all-or-nothing and logged:
//
//   1  build the native game module in Distribution (when <project>/module.json declares a binary)
//   2  assets::asset_cooker::cook -> <dir>/cooked/content.cpak + manifest.json
//   3  dotnet publish Conjure.Bootstrap + dotnet build the game scripts -> <dir>/managed (scripting enabled)
//   4  modules::module_manager::export_layout(<dir>, DISTRIBUTION)
//   5  copy the Distribution conjure_runtime, conjure_shared, shaders/ (and nethost.dll on MSVC) into <dir>
//   6  write <dir>/project.json with "cooked": true and cooked_root "cooked"
//
// The exported folder runs standalone: conjure_runtime --project <dir>.

namespace conjure::engine {

    /// <summary>
    /// Called before each step with its name and the fraction of steps completed so far.
    /// </summary>
    using progress_fn = std::function<void(std::string_view step, f32 fraction)>;

    struct export_settings {
        std::filesystem::path outputDirectory = {};             // created when missing; existing files are replaced
        std::filesystem::path distributionDirectory = {};       // holds conjure_runtime, conjure_shared and shaders/
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
            /// Runs the six steps. EXPORT_FAILED names the step and the cause; nothing of a failed step is
            /// written. INVALID_STATE (core) when the context has no project or no asset database (a cooked
            /// project cannot be exported again). BAD_ARGUMENTS for an empty output directory, the project
            /// directory itself or a folder inside the assets root.
            /// </summary>
            [[nodiscard]] core::result<export_report> run(
              const export_settings &settings,
              const progress_fn &progress = {}
            );
    };
}
