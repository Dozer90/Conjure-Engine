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
// runs exactly n fixed steps through application::step_fixed and answers once they ran.

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
            bool mWarnedUnrecordedEdit = false;

        public:
            explicit play_controller(editor_context &context);
            ~play_controller();

            play_controller(const play_controller &) = delete;
            play_controller &operator=(const play_controller &) = delete;

        public:
            /// <summary>
            /// EDITING -> PLAYING: snapshot, freeze undo, PLAY scripts, RUNNING engine. WRONG_PLAY_STATE
            /// otherwise; a failed snapshot is INTERNAL and nothing changes.
            /// </summary>
            [[nodiscard]] core::result<void> start();

            /// <summary>
            /// PLAYING or PAUSED -> EDITING: PAUSED engine, EDIT scripts, snapshot restored (a failed restore
            /// logs an ERROR and leaves the scene empty), selection remapped, undo thawed, play.state emitted.
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
            /// Frames simulated since play.start (0 while EDITING).
            /// </summary>
            [[nodiscard]] u64 simulation_frame() const noexcept {
                return mSimulationFrames;
            }

            [[nodiscard]] bool has_pending_step() const noexcept {
                return mPendingStep.has_value();
            }

            /// <summary>
            /// The state and frame as the play.state event and commands report them.
            /// </summary>
            [[nodiscard]] core::json::document describe() const;

        private:
            void emit_state();
            void set_script_mode(bool playing);
    };
}
