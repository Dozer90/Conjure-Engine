#include "engine/export_commands.h"

#include <format>
#include <string_view>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

namespace {

    // Code sweep F776: only an export builds as a CI build. In a git repository with at least one commit, for the
    // project being built, the SDK then maps the source root to /_/ in the paths the compiler records (the .pdb path
    // in the assembly, the documents of the .pdb, CallerFilePath), and Source Link embeds the URL of the repository's
    // remote when it has one, so no folder of the exporting machine ships with the game. Outside such a repository
    // the SDK finds no source root and the paths stay as they are. Local builds never pass it, so they keep the real
    // paths a debugger needs (cmake/ConjureManaged.cmake, managed_build_runner).
    constexpr const char *CONTINUOUS_INTEGRATION_BUILD = "-p:ContinuousIntegrationBuild=true";

    // The flag changes no input MSBuild's incremental check looks at, so a build sharing its intermediates with a
    // build without the flag reuses the assembly that one compiled: an export after a Release script.compile would
    // ship the machine's paths, and a Release script.compile after an export would hand the debugger /_/ paths. The
    // export therefore compiles into intermediates of its own. The bootstrap gets its own artifacts pivot below the
    // ArtifactsPath of managed/Directory.Build.props (inside the engine's repository, so the recorded .pdb path is
    // mapped too); the game's scripts get an artifacts path inside their own obj/, which the template's compile items
    // exclude and which lies in the game's repository for the same reason.
    constexpr std::string_view EXPORT_INTERMEDIATES = "conjure_export";

    /// <summary>
    /// The MSBuild properties that keep the .pdb files out of a Release output.
    /// </summary>
    void strip_debug_symbols(std::vector<std::string> &arguments) {
        arguments.emplace_back("-p:DebugType=none");
        arguments.emplace_back("-p:DebugSymbols=false");
    }
}

std::vector<std::vector<std::string>> conjure::engine::managed_export_commands(
  const export_settings &settings,
  const scripting_settings &scripting,
  const std::filesystem::path &managedDirectory
) {
    const bool selfContained = scripting.exportMode == script_export_mode::SELF_CONTAINED;
    std::vector<std::string> publish = {
        "publish",
        settings.bootstrapProject.string(),
        "-c",
        "Release",
        "-o",
        managedDirectory.string(),
        "--self-contained",
        selfContained ? "true" : "false",
        CONTINUOUS_INTEGRATION_BUILD,
        std::format(
          "-p:ArtifactsPivots=release_export_{}{}{}",
          selfContained ? "self_contained" : "framework_dependent",
          settings.runtimeIdentifier.empty() ? "" : "_",
          settings.runtimeIdentifier
        ),
    };
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
        settings.scriptsProject.string(),
        "-c",
        "Release",
        "-o",
        managedDirectory.string(),
        "--artifacts-path",
        (settings.scriptsProject.parent_path() / "obj" / EXPORT_INTERMEDIATES).string(),
        CONTINUOUS_INTEGRATION_BUILD,
    };
    if (!scripting.exportDebugSymbols) {
        strip_debug_symbols(build);
    }
    commands.push_back(std::move(build));
    return commands;
}
