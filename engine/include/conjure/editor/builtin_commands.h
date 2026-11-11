#pragma once

#include "conjure/editor/command_registry.h"

#include "conjure/core/api.h"

// The built-in command catalogue (docs/design/04_editor.md 4.4): every command of the session, editor,
// history, console, project, scene, entity, component, asset, prefab, script, play, input, view, physics,
// selection, reflection, module and profiler groups. Implementations live in
// engine/src/editor/core/commands/commands_<group>.cpp, one free function per command plus its schema.

namespace conjure::editor {

    /// <summary>
    /// Registers every built-in command. Registering twice is a programmer error (asserted; the duplicates
    /// are refused by the registry).
    /// </summary>
    CONJURE_API void register_builtin_commands(command_registry &registry);

    /// <summary>
    /// Number of commands register_builtin_commands adds.
    /// </summary>
    [[nodiscard]] CONJURE_API usize builtin_command_count() noexcept;
}
