#include "engine/export_commands.h"

#include "engine/export_source_roots_targets.h"

#include "conjure/engine/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/error.h"
#include "conjure/core/log.h"

#include <format>
#include <fstream>
#include <string_view>
#include <system_error>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

namespace {

    // Code sweep F776: only an export builds as a CI build. The SDK then turns on DeterministicSourcePaths and maps
    // every source root to /_/, /_1/, ... in the paths the compiler records (the .pdb path in the assembly, the
    // documents of the .pdb, CallerFilePath), and Source Link embeds the URL of the repository's remote when it has
    // one, so no folder of the exporting machine ships with the game. Local builds never pass it, so they keep the
    // real paths a debugger needs (cmake/ConjureManaged.cmake, managed_build_runner).
    constexpr const char *CONTINUOUS_INTEGRATION_BUILD = "-p:ContinuousIntegrationBuild=true";

    // Follow-up FU-PDB: source control supplies a source root only in a git repository with at least one commit, so
    // a project outside one kept every path. The export makes each project it builds import this file (MSBuild
    // imports CustomAfterMicrosoftCommonTargets at the end of the common targets, and the global property reaches
    // the projects the bootstrap references). Its target runs after source control filled SourceRoot and before
    // the roots are mapped; it adds the project's directory only when no root came from source control, so a
    // project in a repository keeps the mapping Source Link agrees with. It adds the intermediate directory always
    // (follow-up FU-ARTIFACTS): it lies below the export's artifacts directory, by default in the system's temporary
    // directory outside every repository, where no root of source control covers the generated sources and the .pdb
    // the CodeView entry names. The paths go through MSBuild's Escape so that the item Include takes them
    // literally, never as a list or a wildcard. The file is engine/src/engine/export_source_roots.targets, embedded
    // through a header the configure generates; the publish of the prebuilt SDK (conjure_managed_sdk_export,
    // managed/CMakeLists.txt, owner decision OWN-INSTALL) imports the same file.
    constexpr std::string_view SOURCE_ROOTS_TARGETS = conjure::engine::detail::EXPORT_SOURCE_ROOTS_TARGETS;

    constexpr std::string_view SOURCE_ROOTS_FILE_NAME = "conjure_export_source_roots.targets";

    // The flag changes no input MSBuild's incremental check looks at, so a build sharing its intermediates with a
    // build without the flag reuses the assembly that one compiled: an export after a Release script.compile would
    // ship the machine's paths, and a Release script.compile after an export would hand the debugger /_/ paths
    // (code sweep F776). Both commands therefore compile below the export's artifacts directory (follow-up
    // FU-ARTIFACTS: before it the publish used managed/Directory.Build.props' default, build/managed_artifacts of
    // the engine's source tree), and inside it into a pivot no build without the flag uses, in case the caller
    // named a folder other builds use too. SOURCE_ROOTS_TARGETS maps the intermediate directories wherever they lie.
    constexpr std::string_view EXPORT_PIVOT_PREFIX = "release_export_";

    // The folder of the staging directory the commands compile in when the settings name no artifacts directory.
    constexpr std::string_view STAGING_ARTIFACTS = "artifacts";

    /// <summary>
    /// The path as UTF-8, the encoding core::process takes; path::string() would give the ANSI code page on
    /// Windows.
    /// </summary>
    [[nodiscard]] std::string utf8(const std::filesystem::path &path) {
        const std::u8string text = path.u8string();
        return std::string(text.begin(), text.end());
    }

    /// <summary>
    /// The path with '/' separators as UTF-8, for messages.
    /// </summary>
    [[nodiscard]] std::string display(const std::filesystem::path &path) {
        const std::u8string text = path.generic_u8string();
        return std::string(text.begin(), text.end());
    }

    /// <summary>
    /// The path as UTF-8 with the characters MSBuild treats specially in a command-line property value escaped
    /// (%XX, which MSBuild decodes): ';' and ',' separate properties, '"' quotes, and '%' starts an escape itself.
    /// dotnet hands -o and --artifacts-path to MSBuild as properties too, so they need the same treatment.
    /// </summary>
    [[nodiscard]] std::string property_value(const std::filesystem::path &path) {
        const std::string text = utf8(path);
        std::string escaped = {};
        escaped.reserve(text.size());
        for (const char character : text) {
            switch (character) {
                case '%': {
                    escaped += "%25";
                    break;
                }
                case ';': {
                    escaped += "%3B";
                    break;
                }
                case ',': {
                    escaped += "%2C";
                    break;
                }
                case '"': {
                    escaped += "%22";
                    break;
                }
                default: {
                    escaped += character;
                    break;
                }
            }
        }
        return escaped;
    }

    /// <summary>
    /// The MSBuild properties that keep the .pdb files out of a Release output.
    /// </summary>
    void strip_debug_symbols(std::vector<std::string> &arguments) {
        arguments.emplace_back("-p:DebugType=none");
        arguments.emplace_back("-p:DebugSymbols=false");
    }

    /// <summary>
    /// The arguments that make a command map the source paths: the CI build and the import of the source roots file.
    /// </summary>
    void map_source_roots(std::vector<std::string> &arguments, const std::filesystem::path &sourceRootsTargets) {
        arguments.emplace_back(CONTINUOUS_INTEGRATION_BUILD);
        arguments.push_back("-p:CustomAfterMicrosoftCommonTargets=" + property_value(sourceRootsTargets));
    }

    /// <summary>
    /// The publish of settings.bootstrapProject into 'managedDirectory' (export step 3 from an engine source tree).
    /// 'artifacts' and 'pivot' are the escaped --artifacts-path value and the ArtifactsPivots property.
    /// </summary>
    [[nodiscard]] std::vector<std::string> publish_command(
      const export_settings &settings,
      const std::filesystem::path &managedDirectory,
      const std::filesystem::path &sourceRootsTargets,
      const std::string &artifacts,
      const std::string &pivot,
      const scripting_settings &scripting
    ) {
        const bool selfContained = scripting.exportMode == script_export_mode::SELF_CONTAINED;
        std::vector<std::string> publish = {
            "publish",
            utf8(settings.bootstrapProject),
            "-c",
            "Release",
            "-o",
            property_value(managedDirectory),
            "--self-contained",
            selfContained ? "true" : "false",
            "--artifacts-path",
            artifacts,
            pivot,
        };
        map_source_roots(publish, sourceRootsTargets);
        if (!settings.runtimeIdentifier.empty()) {
            publish.push_back("-r");
            publish.push_back(settings.runtimeIdentifier);
        }
        // hostfxr loads Conjure.Bootstrap.dll and never starts the apphost the SDK writes next to it.
        if (!selfContained) {
            publish.emplace_back("-p:UseAppHost=false");
        }
        if (!scripting.exportDebugSymbols) {
            strip_debug_symbols(publish);
        }
        return publish;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

managed_export_staging::managed_export_staging(std::filesystem::path directory) noexcept :
    mDirectory(std::move(directory)) {}

managed_export_staging::~managed_export_staging() {
    if (mDirectory.empty()) {
        return;
    }
    std::error_code error = {};
    std::filesystem::remove_all(mDirectory, error);
    if (error) {
        CONJURE_LOG_WARN(
          LOG_ENGINE,
          "export: could not remove the MSBuild staging directory '{}': {}",
          display(mDirectory),
          error.message()
        );
    }
}

managed_export_staging::managed_export_staging(managed_export_staging &&other) noexcept :
    mDirectory(std::exchange(other.mDirectory, std::filesystem::path())) {}

managed_export_staging &managed_export_staging::operator=(managed_export_staging &&other) noexcept {
    if (this == &other) {
        return *this;
    }
    managed_export_staging released(std::move(*this));
    mDirectory = std::exchange(other.mDirectory, std::filesystem::path());
    return *this;
}


/*===========================================================================*/
/*===========================================================================*/
// Public functions

std::string_view conjure::engine::managed_export_source_roots_targets() noexcept {
    return SOURCE_ROOTS_TARGETS;
}

core::result<managed_export_staging> managed_export_staging::create(const std::filesystem::path &directory) {
    std::error_code error = {};
    const bool created = std::filesystem::create_directory(directory, error);
    if (error) {
        return core::error(
          core::error_code::IO_FAILURE,
          std::format("cannot create '{}': {}", display(directory), error.message())
        );
    }
    if (!created) {
        return core::error(
          core::error_code::ALREADY_EXISTS,
          std::format("'{}' already exists", display(directory))
        );
    }

    // Owned from here on: a failed write below removes the directory again.
    managed_export_staging staging(directory);
    const std::filesystem::path file = staging.source_roots_targets();
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream.write(SOURCE_ROOTS_TARGETS.data(), static_cast<std::streamsize>(SOURCE_ROOTS_TARGETS.size()));
    stream.close();
    if (!stream) {
        return core::error(core::error_code::IO_FAILURE, std::format("cannot write '{}'", display(file)));
    }
    return staging;
}

std::filesystem::path managed_export_staging::source_roots_targets() const {
    return mDirectory / SOURCE_ROOTS_FILE_NAME;
}

std::filesystem::path managed_export_staging::artifacts_directory() const {
    return mDirectory / STAGING_ARTIFACTS;
}

std::vector<std::pair<std::string, std::string>> conjure::engine::managed_export_environment() {
    return {{"DOTNET_CLI_TELEMETRY_OPTOUT", "1"}, {"DOTNET_NOLOGO", "1"}, {"MSBUILDNODEREUSE", "0"}};
}

std::vector<std::vector<std::string>> conjure::engine::managed_export_commands(
  const export_settings &settings,
  const scripting_settings &scripting,
  const std::filesystem::path &managedDirectory,
  const std::filesystem::path &sourceRootsTargets,
  const std::filesystem::path &artifactsDirectory,
  const std::filesystem::path &prebuiltSdkDirectory
) {
    // MSBuild resolves a relative property value against the directory of each project it builds, while dotnet
    // resolves -o against its working directory: relative paths would send ConjureManagedDir, the import and the
    // intermediates to a folder other than the one meant (the editor's script.compile had that defect, code sweep
    // F084).
    CONJURE_ASSERT(managedDirectory.is_absolute(), "managed_export_commands: the managed directory must be absolute");
    CONJURE_ASSERT(
      sourceRootsTargets.is_absolute(),
      "managed_export_commands: the source roots file must be absolute"
    );
    CONJURE_ASSERT(
      artifactsDirectory.is_absolute(),
      "managed_export_commands: the artifacts directory must be absolute"
    );
    CONJURE_ASSERT(
      prebuiltSdkDirectory.empty() || prebuiltSdkDirectory.is_absolute(),
      "managed_export_commands: the prebuilt SDK directory must be absolute"
    );
    const bool selfContained = scripting.exportMode == script_export_mode::SELF_CONTAINED;
    const std::string pivot = std::format(
      "-p:ArtifactsPivots={}{}{}{}",
      EXPORT_PIVOT_PREFIX,
      selfContained ? "self_contained" : "framework_dependent",
      settings.runtimeIdentifier.empty() ? "" : "_",
      settings.runtimeIdentifier
    );
    const std::string artifacts = property_value(artifactsDirectory);
    std::vector<std::vector<std::string>> commands = {};
    // Owner decision OWN-INSTALL: an installed editor has no bootstrap project to publish. The pipeline copies the
    // prebuilt SDK into the managed directory instead, and only the game's scripts are built, against that SDK.
    const bool prebuilt = !prebuiltSdkDirectory.empty();
    if (!prebuilt) {
        commands.push_back(
          publish_command(settings, managedDirectory, sourceRootsTargets, artifacts, pivot, scripting)
        );
    }
    if (settings.scriptsProject.empty()) {
        return commands;
    }
    std::vector<std::string> build = {
        "build",
        utf8(settings.scriptsProject),
        "-c",
        "Release",
        "-o",
        property_value(managedDirectory),
        "--artifacts-path",
        artifacts,
        pivot,
        // Follow-up FU-MANAGEDDIR: the game template (managed/Conjure.Templates/Game and the project file the
        // editor's script.create writes) references Conjure.Managed.dll through this property and stops the build
        // without it. The publish above wrote the dll into the managed directory; with a prebuilt SDK the build
        // references it where it was installed.
        "-p:ConjureManagedDir=" + property_value(prebuilt ? prebuiltSdkDirectory : managedDirectory),
    };
    map_source_roots(build, sourceRootsTargets);
    if (!scripting.exportDebugSymbols) {
        strip_debug_symbols(build);
    }
    commands.push_back(std::move(build));
    return commands;
}
