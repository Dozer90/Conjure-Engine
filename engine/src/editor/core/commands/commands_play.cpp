#include "editor/core/commands/command_support.h"

#include "conjure/editor/play_controller.h"

#include <format>

// play.* (docs/design/04_editor.md 3.7, D9): the play controller does the work; the commands only map
// arguments and answer with describe().

namespace conjure::editor::support {

    namespace {
        core::result<command_outcome> play_start(command_invocation &invocation) {
            CONJURE_TRY(invocation.context.play().start());
            command_outcome outcome = {};
            outcome.result = invocation.context.play().describe();
            return outcome;
        }

        core::result<command_outcome> play_stop(command_invocation &invocation) {
            play_controller &play = invocation.context.play();
            // Read before the stop: it moves the kept file edits into the undo stack.
            const usize fileChanges = play.kept_file_edits();
            CONJURE_TRY(play.stop());
            command_outcome outcome = {};
            outcome.result = play.describe();
            outcome.result.set("fileChanges", fileChanges);
            return outcome;
        }

        core::result<command_outcome> play_pause(command_invocation &invocation) {
            CONJURE_TRY(invocation.context.play().pause());
            command_outcome outcome = {};
            outcome.result = invocation.context.play().describe();
            return outcome;
        }

        core::result<command_outcome> play_resume(command_invocation &invocation) {
            CONJURE_TRY(invocation.context.play().resume());
            command_outcome outcome = {};
            outcome.result = invocation.context.play().describe();
            return outcome;
        }

        core::result<command_outcome> play_step(command_invocation &invocation) {
            CONJURE_TRY_ASSIGN(const u32 fixedSteps, u32_or(invocation.args, "fixedSteps", 1));
            if (fixedSteps == 0 || fixedSteps > 100000) {
                return invalid_args("args.fixedSteps: expected 1..100000");
            }
            CONJURE_TRY(invocation.context.play().step(fixedSteps, invocation.reply));
            return command_outcome{};
        }

        core::result<command_outcome> play_state(command_invocation &invocation) {
            command_outcome outcome = {};
            outcome.result = invocation.context.play().describe();
            return outcome;
        }
    }

    void register_play_commands(command_registry &registry) {
        // The simulation clocks belong to the play session (play_controller.h); profiler.frame_stats reports the
        // engine's clocks under two of the same names.
        const std::string stateProperties =
          R"("state":{"type":"string","enum":["editing","playing","paused"]},)"
          R"("frame":{"type":"integer","description":"the engine frame, engine_time.frameIndex"},)"
          R"("simulationFrame":{"type":"integer","description":"frames simulated in this play session"},)"
          R"("simulationTime":{"type":"number","description":"simulated seconds of this play session: fixed )"
          R"(steps only, so it stops while paused"},)"
          R"("fixedStepIndex":{"type":"integer","description":"fixed steps run in this play session"})";
        const std::string stateDescription =
          R"("description":"play.start sets the session clocks to 0; while editing they keep the values of the )"
          R"(session that ended last")";
        const std::string stateSchema = std::format(
          R"({{"type":"object","properties":{{{}}},{}}})",
          stateProperties,
          stateDescription
        );
        // play.stop adds the file changes of the session it ended (D21).
        const std::string stopSchema = std::format(
          R"({{"type":"object","properties":{{{},"fileChanges":{{"type":"integer","description":"on_disk )"
          R"(commands that ran in the ended play session; when not 0 they are now the undo entry '{}'"}}}},{}}})",
          stateProperties,
          play_controller::FILE_EDITS_LABEL,
          stateDescription
        );
        add_command(
          registry,
          make_command(
            "play.start",
            "Snapshots the scene and runs the simulation (EDITING -> PLAYING); the undo stack is frozen until "
            "play.stop: a scene edit is executed without being recorded, an on_disk command (a file change) is kept "
            "for play.stop.",
            std::string(EMPTY_SCHEMA),
            stateSchema,
            command_flags::REQUIRES_SCENE | command_flags::EDITING_ONLY | command_flags::NOT_IN_BATCH,
            &play_start
          )
        );
        add_command(
          registry,
          make_command(
            "play.stop",
            "Stops the simulation and restores the scene snapshot taken by play.start (guids preserved), which "
            "discards the scene edits made while playing. The file changes made while playing (on_disk commands) "
            "stay on disk and become one undo entry, 'File changes during play'; fileChanges counts them.",
            std::string(EMPTY_SCHEMA),
            stopSchema,
            command_flags::REQUIRES_SCENE | command_flags::PLAYING_ONLY | command_flags::NOT_IN_BATCH,
            &play_stop
          )
        );
        add_command(
          registry,
          make_command(
            "play.pause",
            "PLAYING -> PAUSED: the fixed loop stops, UPDATE systems flagged RUNS_WHILE_PAUSED still run.",
            std::string(EMPTY_SCHEMA),
            stateSchema,
            command_flags::REQUIRES_SCENE | command_flags::PLAYING_ONLY,
            &play_pause
          )
        );
        add_command(
          registry,
          make_command(
            "play.resume",
            "PAUSED -> PLAYING.",
            std::string(EMPTY_SCHEMA),
            stateSchema,
            command_flags::REQUIRES_SCENE | command_flags::PLAYING_ONLY,
            &play_resume
          )
        );
        add_command(
          registry,
          make_command(
            "play.step",
            "PAUSED only: runs exactly fixedSteps fixed steps on the next frame and answers once they ran.",
            R"({"type":"object","properties":{"fixedSteps":{"type":"integer","minimum":1}}})",
            stateSchema,
            command_flags::ASYNC | command_flags::REQUIRES_SCENE | command_flags::PLAYING_ONLY,
            &play_step
          )
        );
        add_command(
          registry,
          make_command(
            "play.state",
            "The play state, engine frame and the clocks of the play session: simulation frames, simulated time "
            "and fixed steps.",
            std::string(EMPTY_SCHEMA),
            stateSchema,
            command_flags::READ_ONLY,
            &play_state
          )
        );
    }
}
