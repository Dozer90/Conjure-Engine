#pragma once

#include "conjure/engine/engine_config.h"
#include "conjure/engine/export.h"

#include <filesystem>
#include <string>
#include <vector>

// The dotnet command lines of export step 3 (docs/design/06_engine_loop.md 3.6, R39): `dotnet publish` of the
// bootstrap project and, when the project has game scripts, `dotnet build` of them, both into <out>/managed.
// They live apart from the pipeline so their arguments are tested without a .NET SDK.

namespace conjure::engine {

    /// <summary>
    /// The argument lists (without the dotnet executable) export step 3 runs, in order: the publish of
    /// settings.bootstrapProject, then the build of settings.scriptsProject when it is set. Both leave the debug
    /// symbols out (-p:DebugType=none, -p:DebugSymbols=false) unless scripting.exportDebugSymbols is set. A
    /// framework-dependent publish also leaves out the bootstrap apphost (-p:UseAppHost=false), which nothing
    /// runs; a self-contained publish keeps it because the SDK requires one there (NETSDK1067).
    /// </summary>
    [[nodiscard]] std::vector<std::vector<std::string>> managed_export_commands(
      const export_settings &settings,
      const scripting_settings &scripting,
      const std::filesystem::path &managedDirectory
    );
}
