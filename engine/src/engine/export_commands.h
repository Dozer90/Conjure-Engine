#pragma once

#include "conjure/engine/engine_config.h"
#include "conjure/engine/export.h"

#include "conjure/core/result.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The dotnet command lines of export step 3 (docs/design/06_engine_loop.md 3.6, R39): `dotnet publish` of the
// bootstrap project (unless the export copies a prebuilt SDK, owner decision OWN-INSTALL) and, when the project has
// game scripts, `dotnet build` of them, both into <out>/managed, and
// the MSBuild file both import so that no folder of the exporting machine ships in the managed assemblies (code
// sweep F776, follow-up FU-PDB), and the environment they run in. They live apart from the pipeline so their
// arguments are tested without a .NET SDK.

namespace conjure::engine {

    /// <summary>
    /// The MSBuild project export step 3 imports into every project it builds: the bootstrap, Conjure.Managed
    /// (built through the bootstrap's project reference, which hands it the same global properties) and the game's
    /// scripts. A CI build maps the source roots MSBuild knows to /_/, /_1/, ... in every path the compiler records
    /// (the .pdb path in the assembly, the documents of the .pdb, CallerFilePath), but source control supplies the
    /// root of a project only in a git repository with at least one commit. For a project without one (no
    /// repository, or one without a commit) the file adds the project's directory as a source root. It adds the
    /// project's intermediate directory (below the export's artifacts directory, which may lie outside every
    /// repository) for every project, after the roots source control gave, so the generated sources and the .pdb
    /// path are mapped too. The files of a project with a git root keep the mapping and the Source Link the SDK
    /// gives them; only what lies in the intermediate directory, which source control does not hold, may be mapped
    /// against that root instead of the repository's when the directory lies inside the repository. Builds without
    /// the CI flag never map anything, whether they import the file or not. A source file outside these directories
    /// (a link to a folder elsewhere) keeps its path. The text is engine/src/engine/export_source_roots.targets, which
    /// the build target conjure_managed_sdk_export (managed/CMakeLists.txt) imports into its publish of the prebuilt
    /// SDK as well (owner decision OWN-INSTALL).
    /// </summary>
    [[nodiscard]] std::string_view managed_export_source_roots_targets() noexcept;

    /// <summary>
    /// The directory export step 3 writes for MSBuild: it holds the file of managed_export_source_roots_targets()
    /// and the default artifacts directory of the dotnet commands (artifacts_directory()), and is removed with
    /// everything in it when the object is destroyed, whether the step succeeded or failed. It never lies in the
    /// export output, so nothing of it ships. A removal that fails (a file still held open, by a
    /// virus scanner or a dotnet process that is still exiting) is logged as a warning and leaves the directory
    /// where it was, as does a process that ends before the destructor runs. A moved-from object owns no directory
    /// and removes nothing.
    /// </summary>
    class managed_export_staging {
        private:
            std::filesystem::path mDirectory = {};

        public:
            managed_export_staging() = delete;
            ~managed_export_staging();

            managed_export_staging(const managed_export_staging &) = delete;
            managed_export_staging &operator=(const managed_export_staging &) = delete;
            managed_export_staging(managed_export_staging &&other) noexcept;
            managed_export_staging &operator=(managed_export_staging &&other) noexcept;

        private:
            explicit managed_export_staging(std::filesystem::path directory) noexcept;

        public:
            /// <summary>
            /// Creates 'directory' (its parent must exist) and writes the source roots file into it. A directory
            /// that already exists is ALREADY_EXISTS and stays untouched, since the object would delete it on
            /// destruction; a directory that cannot be created is IO_FAILURE and creates nothing; a file that
            /// cannot be written is IO_FAILURE and removes the directory again, with the limit of the destructor (a
            /// removal that fails is logged as a warning and leaves it).
            /// </summary>
            [[nodiscard]] static core::result<managed_export_staging> create(const std::filesystem::path &directory);

            /// <summary>
            /// The directory this object removes on destruction; empty once moved from.
            /// </summary>
            [[nodiscard]] const std::filesystem::path &directory() const noexcept {
                return mDirectory;
            }

            /// <summary>
            /// The file holding managed_export_source_roots_targets() inside directory().
            /// </summary>
            [[nodiscard]] std::filesystem::path source_roots_targets() const;

            /// <summary>
            /// The folder inside directory() where the dotnet commands compile when export_settings names no
            /// artifacts directory. It is not created here: dotnet creates it, and the destructor removes it with
            /// directory().
            /// </summary>
            [[nodiscard]] std::filesystem::path artifacts_directory() const;
    };

    /// <summary>
    /// The variables export step 3 adds to the environment of every dotnet command it runs: no telemetry, no
    /// first-run banner (DOTNET_CLI_TELEMETRY_OPTOUT, DOTNET_NOLOGO) and no MSBuild node that outlives the build
    /// (MSBUILDNODEREUSE=0). Such a node inherits the output pipe of the command and keeps it open, so the export
    /// would wait for its process timeout. The editor's script builds set the same three
    /// (managed_build_runner::command_line in engine/src/scripting/managed_build.cpp).
    /// </summary>
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> managed_export_environment();

    /// <summary>
    /// The argument lists (without the dotnet executable) export step 3 runs, in order: the publish of
    /// settings.bootstrapProject, then the build of settings.scriptsProject when it is set. With a
    /// 'prebuiltSdkDirectory' (export_settings::prebuiltManagedDirectory, resolved; owner decision OWN-INSTALL) there
    /// is no publish, since the pipeline copies that SDK into 'managedDirectory' itself, and the build references
    /// Conjure.Managed.dll in 'prebuiltSdkDirectory' (-p:ConjureManagedDir) while it still writes into
    /// 'managedDirectory'; without game scripts the list is then empty. Both commands build as a CI build
    /// (-p:ContinuousIntegrationBuild=true) and import 'sourceRootsTargets', the file of
    /// managed_export_source_roots_targets() (-p:CustomAfterMicrosoftCommonTargets), so the source paths the
    /// compiler records are mapped to /_/, /_1/, ... for a project in a git repository with a commit and for one
    /// without; the editor's and the engine's own builds pass neither and keep the real paths a debugger needs.
    /// Both compile below 'artifactsDirectory' (--artifacts-path, follow-up FU-ARTIFACTS), which the bootstrap's
    /// project reference hands to Conjure.Managed too, so neither managed/Directory.Build.props' default
    /// (build/managed_artifacts of the engine's source tree) nor the scripts project's own folder receives
    /// intermediates. Inside it both use a pivot of their own
    /// (-p:ArtifactsPivots=release_export_<framework_dependent|self_contained>[_<rid>]), because MSBuild's
    /// incremental check does not see the CI flag and a build without it must never share their intermediates. The
    /// build finds Conjure.Managed.dll in 'managedDirectory', where the publish put it, or in 'prebuiltSdkDirectory'
    /// (-p:ConjureManagedDir, the property by which managed/Conjure.Templates/Game and the project file of the
    /// editor's script.create reference it, and without which their build stops). 'managedDirectory',
    /// 'sourceRootsTargets', 'artifactsDirectory' and a non-empty 'prebuiltSdkDirectory' must be absolute (asserted):
    /// MSBuild would resolve a relative property value against each project's own directory and dotnet resolves -o
    /// against its working directory. Both leave the debug symbols out
    /// (-p:DebugType=none, -p:DebugSymbols=false) unless scripting.exportDebugSymbols is set. A framework-dependent
    /// publish also leaves out the bootstrap apphost (-p:UseAppHost=false), which nothing runs; a self-contained
    /// publish keeps it because the SDK requires one there (NETSDK1067). Paths are passed as UTF-8; those MSBuild
    /// reads as property values (-o, --artifacts-path and the -p: ones) are MSBuild-escaped, so a '%', ';', ',' or
    /// '"' in them reaches MSBuild as itself instead of splitting the property. A global property overrides the
    /// value a project sets, so during the export's builds the import replaces any CustomAfterMicrosoftCommonTargets
    /// the game's project sets itself; a project that needs a file of its own imported after the common targets uses
    /// a Directory.Build.targets, which the export leaves alone.
    /// </summary>
    [[nodiscard]] std::vector<std::vector<std::string>> managed_export_commands(
      const export_settings &settings,
      const scripting_settings &scripting,
      const std::filesystem::path &managedDirectory,
      const std::filesystem::path &sourceRootsTargets,
      const std::filesystem::path &artifactsDirectory,
      const std::filesystem::path &prebuiltSdkDirectory
    );
}
