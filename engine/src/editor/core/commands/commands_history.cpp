#include "editor/core/commands/command_support.h"

#include "conjure/editor/log_category.h"

#include <format>

// history.* (docs/design/04_editor.md 3.3): undo and redo dispatch the recorded inverses and replays with
// recording suppressed and bump the revision themselves.

namespace conjure::editor::support {

    namespace {
        core::result<command_outcome> apply_history(command_invocation &invocation, bool undo) {
            editor_context &context = invocation.context;
            undo_stack &stack = context.undo();
            CONJURE_TRY_ASSIGN(const u32 count, u32_or(invocation.args, "count", 1));
            if (count == 0) {
                return invalid_args("args.count: expected at least 1");
            }
            if (stack.frozen()) {
                return core::make_error(
                  protocol_error_code::WRONG_PLAY_STATE,
                  "the undo stack is frozen while playing"
                );
            }
            const std::string_view verb = undo ? "Undo" : "Redo";
            const std::string_view command = undo ? "history.undo" : "history.redo";
            if (stack.has_open_transaction()) {
                // Ending any client's transaction pushes its entry at the cursor, which would drop what this
                // step moves into the redo tail (D4).
                return core::make_error(
                  protocol_error_code::INVALID_REQUEST,
                  std::format("{} is refused while a transaction is open; end it first", command)
                );
            }
            core::json::document labels = core::json::document::array();
            u32 applied = 0;
            for (u32 step = 0; step < count; ++step) {
                std::optional<undo_entry> entry = undo ? stack.pop_for_undo() : stack.pop_for_redo();
                if (!entry.has_value()) {
                    break;
                }
                const core::result<void> done = undo
                  ? context.dispatcher().dispatch_inverses(entry->undo, invocation.clientId)
                  : context.dispatcher().dispatch_replays(entry->redo, invocation.clientId);
                if (!done.has_value()) {
                    stack.discard_at_cursor(undo);
                    CONJURE_LOG_ERROR(LOG_EDITOR, "history.{}: {}", undo ? "undo" : "redo", done.get_error().message());
                    // The commands applied before the failure changed the scene and announced nothing.
                    const u64 failedRevision = context.bump_revision();
                    context.set_scene_dirty(true);
                    context.events().emit(
                      editor_event::SCENE_CHANGED,
                      core::json::object({
                        {"revision", failedRevision},
                        {"label", std::format("{} {} (failed)", verb, entry->label)},
                        {"cmd", command},
                      })
                    );
                    return core::make_error(protocol_error_code::INTERNAL, done.get_error().message());
                }
                labels.push_back(std::string_view(entry->label));
                ++applied;
                const u64 revision = context.bump_revision();
                context.set_scene_dirty(true);
                context.events().emit(
                  editor_event::SCENE_CHANGED,
                  core::json::object({
                    {"revision", revision},
                    {"label", std::format("{} {}", verb, entry->label)},
                    {"cmd", command},
                  })
                );
            }
            context.selection().rebuild(context.scene());
            command_outcome outcome = {};
            outcome.result = core::json::object({
              {"applied", applied},
              {"labels", labels},
              {"canUndo", stack.can_undo()},
              {"canRedo", stack.can_redo()},
              {"revision", context.revision()},
            });
            return outcome;
        }

        core::result<command_outcome> history_undo(command_invocation &invocation) {
            return apply_history(invocation, true);
        }

        core::result<command_outcome> history_redo(command_invocation &invocation) {
            return apply_history(invocation, false);
        }

        core::result<command_outcome> history_list(command_invocation &invocation) {
            const undo_stack &stack = invocation.context.undo();
            CONJURE_TRY_ASSIGN(const u32 limit, u32_or(invocation.args, "limit", 100));
            core::json::document entries = core::json::document::array();
            const std::vector<const undo_entry *> all = stack.entries();
            const usize start = all.size() > limit ? all.size() - limit : 0;
            for (usize index = start; index < all.size(); ++index) {
                entries.push_back(core::json::object({
                  {"label", std::string_view(all[index]->label)},
                  {"clientId", all[index]->clientId},
                  {"commands", all[index]->redo.size()},
                  {"bytes", all[index]->byteSize},
                }));
            }
            command_outcome outcome = {};
            outcome.result = core::json::object({
              {"entries", entries},
              {"cursor", stack.cursor()},
              {"total", all.size()},
              {"canUndo", stack.can_undo()},
              {"canRedo", stack.can_redo()},
            });
            return outcome;
        }

        core::result<command_outcome> history_clear(command_invocation &invocation) {
            undo_stack &stack = invocation.context.undo();
            if (stack.has_open_transaction()) {
                return core::make_error(
                  protocol_error_code::INVALID_REQUEST,
                  "history.clear is refused while a transaction is open; end it first"
                );
            }
            const usize cleared = stack.size();
            stack.clear();
            command_outcome outcome = {};
            outcome.result = core::json::object({{"cleared", cleared}});
            return outcome;
        }
    }

    void register_history_commands(command_registry &registry) {
        add_command(
          registry,
          make_command(
            "history.undo",
            "Undoes the most recent entries.",
            R"({"type":"object","properties":{"count":{"type":"integer","minimum":1}}})",
            R"({"type":"object","properties":{"applied":{"type":"integer"},"labels":{"type":"array"}}})",
            command_flags::NOT_IN_BATCH,
            &history_undo
          )
        );
        add_command(
          registry,
          make_command(
            "history.redo",
            "Redoes the most recently undone entries.",
            R"({"type":"object","properties":{"count":{"type":"integer","minimum":1}}})",
            R"({"type":"object","properties":{"applied":{"type":"integer"},"labels":{"type":"array"}}})",
            command_flags::NOT_IN_BATCH,
            &history_redo
          )
        );
        add_command(
          registry,
          make_command(
            "history.list",
            "The undo entries (newest last) and the cursor.",
            R"({"type":"object","properties":{"limit":{"type":"integer","minimum":1}}})",
            R"({"type":"object","properties":{"entries":{"type":"array"},"cursor":{"type":"integer"}}})",
            command_flags::READ_ONLY,
            &history_list
          )
        );
        add_command(
          registry,
          make_command(
            "history.clear",
            "Forgets every undo entry.",
            R"({"type":"object"})",
            R"({"type":"object","properties":{"cleared":{"type":"integer"}}})",
            command_flags::NONE,
            &history_clear
          )
        );
    }
}
