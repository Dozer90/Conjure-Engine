#pragma once

#include "conjure/engine/engine_config.h"

// What a configuration asks of scripting, answered without the scripting module (docs/design/06_engine_loop.md
// D12, code sweep F075). Two places need it: the scripting subsystem, to tell a headless run that holds a
// project's scripts back from one that has none to run, and application::start_subsystems in a build without
// CONJURE_WITH_SCRIPTING, where there is no subsystem to say that the scripts asked for do not run.

namespace conjure::engine {

    /// <summary>
    /// True when the section enables scripting and names a game assembly, the one place a project's C# comes
    /// from. A section that enables scripting without an assembly (which is what a project.json without a
    /// "scripting" section amounts to) starts the script runtime with no script to run.
    /// </summary>
    [[nodiscard]] bool names_game_scripts(const scripting_settings &settings) noexcept;

    /// <summary>
    /// True when a build without the scripting module must not stay silent about this configuration: scripting
    /// is enabled and the configuration either names a game assembly, whose scripts will not run, or belongs
    /// to a headless run outside the editor, where only --scripting or a caller's own configuration enables
    /// scripting. False for a windowed run of a project without a game assembly, which enables scripting by
    /// default and loses nothing, and for the editor without a game assembly, which enables it on its own.
    /// </summary>
    [[nodiscard]] bool asks_for_missing_scripting_module(const engine_config &config) noexcept;
}
