#pragma once

#include "conjure/editor/editor_command.h"

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
// play.stop restores the snapshot (guids preserved), remaps the selection and thaws the undo stack. play.step
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
            play_state mState = play_state::EDITING;
            u64 mSessionStartFrame = 0;
            u64 mSimulationFrames = 0;
            u64 mSessionStartFixedStep = 0;
            f64 mSessionStartFixedTime = 0.0;
            u64 mEndedFixedSteps = 0;
            f64 mEndedSimulationTime = 0.0;
            bool mWarnedUnrecordedEdit = false;

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
            /// logs an ERROR and leaves the scene empty), selection remapped, undo thawed, play.state emitted with
            /// the ended session's clocks, which stay until the next start().
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
            /// A mutating command ran while not EDITING: emits warning UNRECORDED_PLAY_EDIT once per session.
            /// </summary>
            void note_unrecorded_edit();

            [[nodiscard]] play_state state() const noexcept {
                return mState;
            }

            [[nodiscard]] bool is_editing() const noexcept {
                return mState == play_state::EDITING;
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
