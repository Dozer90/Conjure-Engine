#pragma once

#include "conjure/core/enum_flags.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <type_traits>

// Module-provided commands (docs/design/04_editor.md D13, R14): a trivially copyable extension record a
// module registers through modules::module_registrar::register_extension. The editor process turns it into a
// command_descriptor (editor_module_consumer); the runtime has no consumer for the point and ignores it. The
// header carries no editor internals, so an SDK module compiles against it without the editor libraries.

namespace conjure::editor {

    /// <summary>
    /// Properties of a command. MUTATES commands are recorded in the undo stack and bump the scene revision;
    /// NO_UNDO marks the mutating commands whose effect cannot be inverted (file and project operations): they
    /// bump the revision but record nothing. A module command (command_desc) answers with a result document
    /// only and cannot supply an inverse, so the editor treats its MUTATES as MUTATES | NO_UNDO.
    /// ON_DISK marks the undoable commands whose effect lives in the project's files and not in the scene (asset,
    /// script and project file edits): the snapshot play.stop restores does not take such a change back, so while
    /// playing the dispatcher keeps its inverse and play.stop records every kept one as a single undo entry
    /// (docs/design/04_editor.md D21). It needs MUTATES, excludes NO_UNDO, and promises that neither the inverse
    /// nor the replay refers to scene state; the editor drops it from a module command, which has no inverse.
    /// READ_ONLY marks the commands a client holding the inspect-only token may run (docs/design/04_editor.md
    /// D25): the handler promises to change nothing but what belongs to the calling client alone (its event
    /// subscriptions, its own pending reply), that is, not the scene, not a file, not the selection, the camera,
    /// the play state, the console or the history, and to start no program and load no code. A command without
    /// the flag needs the full-control token. The flag excludes MUTATES: the registry refuses the pair for a
    /// built-in command, and the editor drops READ_ONLY from a module command that also says MUTATES. The editor
    /// cannot check a module's promise; a module that flags a command which changes something hands that change
    /// to every inspect-only client.
    /// ASYNC commands may not run inside editor.batch (the registry adds NOT_IN_BATCH) unless they also carry
    /// SYNC_IN_BATCH: the promise that the handler completes its reply before it returns whenever the dispatcher
    /// asks for it (command_dispatcher::requires_synchronous_completion(): a batch child, an undo or redo step, a
    /// batch rollback). The prefab commands that load their asset over frames are such commands (code sweep F736).
    /// SYNC_IN_BATCH needs ASYNC and excludes NOT_IN_BATCH (the registry refuses anything else), and the editor
    /// drops it from a module command, whose handler cannot answer later at all.
    /// </summary>
    enum class command_flags : u16 {
        NONE = 0,
        MUTATES = 1 << 0,
        REQUIRES_PROJECT = 1 << 1,
        REQUIRES_SCENE = 1 << 2,
        EDITING_ONLY = 1 << 3,
        PLAYING_ONLY = 1 << 4,
        ASYNC = 1 << 5,
        UI_ONLY = 1 << 6,
        NOT_IN_BATCH = 1 << 7,
        NO_UNDO = 1 << 8,
        ON_DISK = 1 << 9,
        READ_ONLY = 1 << 10,
        SYNC_IN_BATCH = 1 << 11
    };
    CONJURE_ENUM_FLAGS(command_flags);

    /// <summary>
    /// A module command handler: receives the validated args and returns the result object. userData is the
    /// pointer stored in the record.
    /// </summary>
    using command_handler_fn = core::result<core::json::document> (*)(const core::json::value &args, void *userData);

    /// <summary>
    /// One command a module exports. Strings may point at module literals: the consumer copies what it keeps.
    /// </summary>
    struct command_desc {
        static constexpr core::string_id EXTENSION_POINT{"editor.command"};

        const char *name = nullptr;             // "com.studio.tool.rebuild"
        const char *summary = nullptr;
        const char *argsSchema = nullptr;       // JSON Schema subset; null = "{}"
        const char *resultSchema = nullptr;     // null = "{}"
        command_flags flags = command_flags::NONE;
        command_handler_fn handler = nullptr;
        void *userData = nullptr;
    };

    static_assert(std::is_trivially_copyable_v<command_desc>, "command_desc crosses the module ABI");
}
