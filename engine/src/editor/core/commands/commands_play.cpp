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
            CONJURE_TRY(invocation.context.play().stop());
            command_outcome outcome = {};
            outcome.result = invocation.context.play().describe();
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
        const std::string stateSchema =
          R"({"type":"object","properties":{"state":{"type":"string","enum":["editing","playing","paused"]},)"
          R"("frame":{"type":"integer"},"simulationFrame":{"type":"integer"},"simulationTime":{"type":"number"},)"
          R"("fixedStepIndex":{"type":"integer"}}})";
        add_command(
          registry,
          make_command(
            "play.start",
            "Snapshots the scene and runs the simulation (EDITING -> PLAYING); the undo stack is frozen until "
            "play.stop.",
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
            "Stops the simulation and restores the scene snapshot taken by play.start (guids preserved).",
            std::string(EMPTY_SCHEMA),
            stateSchema,
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
            "The play state, engine frame, simulation frame and time.",
            std::string(EMPTY_SCHEMA),
            stateSchema,
            command_flags::NONE,
            &play_state
          )
        );
    }
}
