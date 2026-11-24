#include "engine/export_commands.h"

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
    // the roots are mapped; it adds the project's directory and its intermediate directory only when no root came
    // from source control, so a project in a repository keeps the mapping Source Link agrees with. The paths go
    // through MSBuild's Escape so that the item Include takes them literally, never as a list or a wildcard.
    constexpr std::string_view SOURCE_ROOTS_TARGETS =
      "<Project>\n"
      "  <!-- Written by the Conjure export (engine/src/engine/export_commands.cpp) and imported into every project\n"
      "       it builds: maps the source paths of a project that source control gives no source root. -->\n"
      "  <Target Name=\"ConjureExportSourceRoots\" BeforeTargets=\"InitializeSourceRootMappedPaths\"\n"
      "          Condition=\"'$(DeterministicSourcePaths)' == 'true'\">\n"
      "    <ItemGroup>\n"
      "      <_ConjureExportControlledSourceRoot Include=\"@(SourceRoot)\"\n"
      "                                          Condition=\"'%(SourceRoot.SourceControl)' != ''\" />\n"
      "    </ItemGroup>\n"
      "    <ItemGroup Condition=\"'@(_ConjureExportControlledSourceRoot)' == ''\">\n"
      "      <SourceRoot Include=\"$([MSBuild]::Escape($([MSBuild]::NormalizeDirectory("
      "'$(MSBuildProjectDirectory)'))))\" />\n"
      "      <SourceRoot Include=\"$([MSBuild]::Escape($([MSBuild]::NormalizeDirectory("
      "'$(MSBuildProjectDirectory)', '$(BaseIntermediateOutputPath)'))))\" />\n"
      "    </ItemGroup>\n"
      "  </Target>\n"
      "</Project>\n";

    constexpr std::string_view SOURCE_ROOTS_FILE_NAME = "conjure_export_source_roots.targets";

    // The flag changes no input MSBuild's incremental check looks at, so a build sharing its intermediates with a
    // build without the flag reuses the assembly that one compiled: an export after a Release script.compile would
    // ship the machine's paths, and a Release script.compile after an export would hand the debugger /_/ paths. The
    // export therefore compiles into intermediates of its own. The bootstrap gets its own artifacts pivot below the
    // ArtifactsPath of managed/Directory.Build.props; the game's scripts get an artifacts path inside their own
    // obj/, which the template's compile items exclude. Both lie inside the directories the source roots cover:
    // the git repository of the engine or the game, or, outside one, the roots SOURCE_ROOTS_TARGETS adds.
    constexpr std::string_view EXPORT_INTERMEDIATES = "conjure_export";

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

std::vector<std::pair<std::string, std::string>> conjure::engine::managed_export_environment() {
    return {{"DOTNET_CLI_TELEMETRY_OPTOUT", "1"}, {"DOTNET_NOLOGO", "1"}, {"MSBUILDNODEREUSE", "0"}};
}

std::vector<std::vector<std::string>> conjure::engine::managed_export_commands(
  const export_settings &settings,
  const scripting_settings &scripting,
  const std::filesystem::path &managedDirectory,
  const std::filesystem::path &sourceRootsTargets
) {
    // MSBuild resolves a relative property value against the directory of each project it builds, while dotnet
    // resolves -o against its working directory: relative paths would send ConjureManagedDir and the import to a
    // folder other than the one the publish wrote (the editor's script.compile had that defect, code sweep F084).
    CONJURE_ASSERT(managedDirectory.is_absolute(), "managed_export_commands: the managed directory must be absolute");
    CONJURE_ASSERT(
      sourceRootsTargets.is_absolute(),
      "managed_export_commands: the source roots file must be absolute"
    );
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
        std::format(
          "-p:ArtifactsPivots=release_export_{}{}{}",
          selfContained ? "self_contained" : "framework_dependent",
          settings.runtimeIdentifier.empty() ? "" : "_",
          settings.runtimeIdentifier
        ),
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

    std::vector<std::vector<std::string>> commands = {};
    commands.push_back(std::move(publish));
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
        property_value(settings.scriptsProject.parent_path() / "obj" / EXPORT_INTERMEDIATES),
        // Follow-up FU-MANAGEDDIR: the game template (managed/Conjure.Templates/Game and the project file the
        // editor's script.create writes) references Conjure.Managed.dll through this property and stops the build
        // without it. The publish above wrote the dll into the same directory.
        "-p:ConjureManagedDir=" + property_value(managedDirectory),
    };
    map_source_roots(build, sourceRootsTargets);
    if (!scripting.exportDebugSymbols) {
        strip_debug_symbols(build);
    }
    commands.push_back(std::move(build));
    return commands;
}
