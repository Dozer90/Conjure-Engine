#pragma once

#include "conjure/editor/editor_command.h"
#include "conjure/editor/undo_stack.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/scene/scene_snapshot.h"

#include <optional>
#include <string_view>
#include <vector>

// Play mode in the editing world (docs/design/04_editor.md D9, 3.7). EDITING keeps the engine PAUSED and the
// script runtime in EDIT mode; play.start captures a scene snapshot, freezes the undo stack and runs the engine;
// play.stop restores the snapshot (guids preserved), remaps the selection and thaws the undo stack. A scene edit
// made while playing is not recorded and goes with the restore; a file edit (an ON_DISK command, D21) stays on
// disk, so the controller keeps its inverse and play.stop pushes all of them as one undo entry. play.step
// runs exactly n fixed steps through application::step_fixed and answers once they ran. The clocks play.* report
// belong to the play session: play.start records the engine's fixedTime and fixedStepIndex, the session's time
// and fixed steps count from there (they stop while paused), and the ended session's values stay until the next
// play.start.

namespace conjure::editor {

    class editor_context;

    enum class play_state : u8 {
        EDITING = 0,
        PLAYING,
        PAUSED
    };

    /// <summary>
    /// The wire name of a state ("editing").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(play_state state) noexcept;

    class CONJURE_API play_controller {
        public:
            /// <summary>
            /// The label of the undo entry stop() pushes for the file edits of a play session.
            /// </summary>
            static constexpr std::string_view FILE_EDITS_LABEL = "File changes during play";

        private:
            struct pending_step {
                async_reply reply = {};
                u64 targetFixedStepIndex = 0;
                u64 requestedFrame = 0;
            };

            editor_context *mContext = nullptr;
            std::optional<scene::scene_snapshot> mSnapshot = std::nullopt;
            std::vector<core::uuid> mSelectionBackup = {};
            std::optional<pending_step> mPendingStep = std::nullopt;
            undo_entry mFileEdits = {};                 // the ON_DISK commands of this session, pushed by stop()
            play_state mState = play_state::EDITING;
            u64 mSessionStartFrame = 0;
            u64 mSimulationFrames = 0;
            u64 mSessionStartFixedStep = 0;
            f64 mSessionStartFixedTime = 0.0;
            u64 mEndedFixedSteps = 0;
            u64 mStateChanges = 0;
            f64 mEndedSimulationTime = 0.0;
            bool mWarnedUnrecordedEdit = false;
            bool mWarnedFileEdit = false;

        public:
            explicit play_controller(editor_context &context);
            ~play_controller();

            play_controller(const play_controller &) = delete;
            play_controller &operator=(const play_controller &) = delete;

        public:
            /// <summary>
            /// EDITING -> PLAYING: snapshot, freeze undo, PLAY scripts, RUNNING engine, the session's clocks
            /// back to 0. WRONG_PLAY_STATE otherwise; a failed snapshot is INTERNAL and nothing changes.
            /// </summary>
            [[nodiscard]] core::result<void> start();

            /// <summary>
            /// PLAYING or PAUSED -> EDITING: PAUSED engine, EDIT scripts, snapshot restored (a failed restore
            /// logs an ERROR and leaves the scene empty), selection remapped, undo thawed, the file edits kept by
            /// keep_file_edit() pushed as one undo entry labelled FILE_EDITS_LABEL (also after a failed restore:
            /// the files do not depend on the scene), play.state emitted with the ended session's clocks, which
            /// stay until the next start().
            /// </summary>
            [[nodiscard]] core::result<void> stop();

            /// <summary>
            /// PLAYING -> PAUSED.
            /// </summary>
            [[nodiscard]] core::result<void> pause();

            /// <summary>
            /// PAUSED -> PLAYING.
            /// </summary>
            [[nodiscard]] core::result<void> resume();

            /// <summary>
            /// PAUSED only: the next frame runs exactly fixedSteps fixed steps; the reply completes in tick()
            /// once engine_time.fixedStepIndex advanced by that many. A second step while one is pending is
            /// BUSY; zero steps are INVALID_ARGS.
            /// </summary>
            [[nodiscard]] core::result<void> step(u32 fixedSteps, async_reply reply);

            /// <summary>
            /// Once per frame: completes a pending step and counts simulation frames.
            /// </summary>
            void tick();

            /// <summary>
            /// Puts the engine into the mode the state requires (PAUSED while EDITING): called after the
            /// engine is (re)initialised.
            /// </summary>
            void apply_engine_mode();

            /// <summary>
            /// A scene edit ran while not EDITING: it is not recorded for undo and stop() discards it with the
            /// snapshot restore. Emits warning UNRECORDED_PLAY_EDIT once per session; its message names the
            /// ON_DISK commands, whose file changes are kept instead.
            /// </summary>
            void note_unrecorded_edit();

            /// <summary>
            /// Keeps the replay and the inverse of an ON_DISK command that ran while not EDITING (code sweep
            /// F089): its effect lives in the project's files, which the snapshot restore does not touch, so
            /// stop() pushes everything kept, in the order it ran, as one undo entry. Emits warning PLAY_FILE_EDIT
            /// once per session with the command's name as cmd. Ignored while EDITING, when the undo stack
            /// records the command itself.
            /// </summary>
            void keep_file_edit(std::string_view command, command_request replay, command_inverse inverse);

            /// <summary>
            /// The file edits kept in this play session so far (what the entry stop() pushes will hold); 0 while
            /// EDITING.
            /// </summary>
            [[nodiscard]] usize kept_file_edits() const noexcept {
                return mFileEdits.undo.size();
            }

            [[nodiscard]] play_state state() const noexcept {
                return mState;
            }

            [[nodiscard]] bool is_editing() const noexcept {
                return mState == play_state::EDITING;
            }

            /// <summary>
            /// How many state transitions start(), stop(), pause() and resume() made so far (failed calls do not
            /// count). An asynchronous command compares it at completion with the value at dispatch: equal values
            /// mean the play state never changed in between, even across a stop and a start that ended in the
            /// state it began in (code sweep F736).
            /// </summary>
            [[nodiscard]] u64 state_changes() const noexcept {
                return mStateChanges;
            }

            /// <summary>
            /// Frames simulated in this play session (play.step counts one); while EDITING the count of the
            /// session that ended last (0 before the first play.start).
            /// </summary>
            [[nodiscard]] u64 simulation_frame() const noexcept {
                return mSimulationFrames;
            }

            /// <summary>
            /// Fixed steps run in this play session, from the engine's fixedStepIndex at play.start; while EDITING
            /// the count of the session that ended last (0 before the first play.start).
            /// </summary>
            [[nodiscard]] u64 simulation_fixed_steps() const noexcept;

            /// <summary>
            /// Simulated seconds of this play session, from the engine's fixedTime at play.start: it advances only
            /// with fixed steps, so it stops while paused; while EDITING the time of the session that ended last.
            /// </summary>
            [[nodiscard]] f64 simulation_time() const noexcept;

            [[nodiscard]] bool has_pending_step() const noexcept {
                return mPendingStep.has_value();
            }

            /// <summary>
            /// The play.state event and the play.* results: {state, frame (the engine frame), simulationFrame,
            /// simulationTime, fixedStepIndex}, the last three from this play session (simulation_frame(),
            /// simulation_time(), simulation_fixed_steps()).
            /// </summary>
            [[nodiscard]] core::json::document describe() const;

        private:
            void emit_state();
            void set_script_mode(bool playing);
    };
}
