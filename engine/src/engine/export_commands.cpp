#include "engine/export_commands.h"

#include <utility>

using namespace conjure;
using namespace conjure::engine;

namespace {

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
    };
    if (!scripting.exportDebugSymbols) {
        strip_debug_symbols(build);
    }
    commands.push_back(std::move(build));
    return commands;
}
