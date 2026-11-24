#pragma once

#include "conjure/editor/command_desc.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/protocol/access_token.h"

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

// Commands are data (docs/design/04_editor.md D2, D3, 2.1). Every mutation of the editor, whether it comes from
// a panel, a protocol client, an --exec file or the undo stack, is a command_request handed to the
// command_dispatcher. A mutating handler returns the inverse request that undoes it and, when the forward
// request is not deterministic (assigned guids), a normalised replay request for redo.

namespace conjure::editor {

    class command_dispatcher;
    class editor_context;

    struct command_request {
        std::string name = "";                              // "entity.create"
        core::json::document args = core::json::document::object();
        u32 clientId = 0;                                   // 0 = the local UI
        u64 coalesceKey = 0;                                // non-zero: merge with the previous entry of equal key
    };

    struct command_inverse {
        std::string name = "";
        core::json::document args = core::json::document::object();
    };

    struct command_outcome {
        core::json::document result = core::json::document::object();
        std::optional<command_inverse> inverse = std::nullopt;      // mandatory for MUTATES without NO_UNDO
        std::optional<command_request> replay = std::nullopt;       // normalised forward request; default original
        std::string label = "";                                     // undo label, "Rename 'Wall_3'"
        bool skipRecording = false;                                 // this run of a MUTATES command has nothing to undo
    };

    /// <summary>
    /// The handle an ASYNC handler keeps to answer later. Copyable (a job completion captures it by value);
    /// the completion itself runs on the main thread. A second completion is a programmer error (asserted,
    /// ignored).
    /// </summary>
    class CONJURE_API async_reply {
        private:
            command_dispatcher *mDispatcher = nullptr;
            u64 mTicket = 0;

        public:
            async_reply() noexcept;
            async_reply(command_dispatcher &dispatcher, u64 ticket) noexcept;
            ~async_reply();

            async_reply(const async_reply &) = default;
            async_reply &operator=(const async_reply &) = default;

        public:
            /// <summary>
            /// Completes the parked request with a result object. For a command that is not MUTATES; a MUTATES
            /// command completes with succeed_mutation() (a plain succeed() of one records nothing, bumps no
            /// revision and leaves the dirty flag alone).
            /// </summary>
            void succeed(core::json::document result) const;

            /// <summary>
            /// Completes the parked request of an ASYNC | MUTATES command with what its change produced. The
            /// dispatcher does now what it does for a synchronous mutation at dispatch: records the inverse and the
            /// replay (the undo stack, the client's open transaction, coalescing, a running batch, or the play
            /// controller while playing), marks the scene dirty unless NO_UNDO, bumps the revision, adds it to the
            /// result and emits scene.changed; an undo or redo step does neither of the last three (the history
            /// command does). The caller has already applied the change; on a reply that is no longer valid (timed
            /// out, cancelled) nothing is recorded, so a handler checks is_valid() before it changes anything.
            /// </summary>
            void succeed_mutation(command_outcome outcome) const;

            /// <summary>
            /// Completes the parked request with an error.
            /// </summary>
            void fail(
              protocol_error_code code,
              std::string message,
              core::json::document details = core::json::document::object()
            ) const;

            /// <summary>
            /// True while the reply refers to a parked request that was not completed yet.
            /// </summary>
            [[nodiscard]] bool is_valid() const noexcept;

            /// <summary>
            /// The former name of is_valid(), kept for one wave (code sweep F624); returns is_valid().
            /// </summary>
            [[deprecated("use is_valid()")]] [[nodiscard]] bool valid() const noexcept;

            [[nodiscard]] u64 ticket() const noexcept {
                return mTicket;
            }
    };

    /// <summary>
    /// What a handler receives. A handler that fails with a detailed error (candidates, failedIndex) fills
    /// errorDetails before returning the error; the dispatcher copies it into the response.
    /// </summary>
    struct command_invocation {
        editor_context &context;
        const core::json::value &args;
        u32 clientId = 0;
        async_reply reply = {};                                     // valid only for ASYNC handlers
        core::json::document errorDetails = core::json::document::object();
    };

    using command_handler = std::function<core::result<command_outcome>(command_invocation &invocation)>;

    struct command_descriptor {
        std::string name = "";
        std::string summary = "";
        std::string argsSchema = "{}";                              // JSON Schema subset, compiled at registration
        std::string resultSchema = "{}";
        command_flags flags = command_flags::NONE;
        std::string owner = "editor";                               // module name; module.unload removes by owner
        command_handler handler = {};
    };

    /// <summary>
    /// True when the descriptor carries the flag.
    /// </summary>
    [[nodiscard]] inline bool has_command_flag(const command_descriptor &descriptor, command_flags flag) noexcept {
        return has_flag(descriptor.flags, flag);
    }

    /// <summary>
    /// What a client must have been granted to run a command with these flags (decision D25 of 04_editor.md):
    /// INSPECT for a READ_ONLY command, FULL_CONTROL for every other one. The flags are the whole rule: no
    /// command is decided by its name, so a command added later needs full control until it is flagged.
    /// </summary>
    [[nodiscard]] constexpr access_level required_access(command_flags flags) noexcept {
        return has_flag(flags, command_flags::READ_ONLY) ? access_level::INSPECT : access_level::FULL_CONTROL;
    }

    /// <summary>
    /// The lower-case names of the set flags ("mutates", "requires_scene", ...), in enumeration order.
    /// </summary>
    [[nodiscard]] CONJURE_API core::json::document command_flag_names(command_flags flags);

    /// <summary>
    /// The wire spelling of one flag ("mutates"); "" for a value that is not a single flag.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view command_flag_name(command_flags flag) noexcept;
}
