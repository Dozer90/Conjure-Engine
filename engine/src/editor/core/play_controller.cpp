#include "conjure/editor/play_controller.h"

#include "conjure/editor/editor_context.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/log_category.h"

#include "conjure/core/platform_detect.h"

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    #include "conjure/scripting/script_runtime.h"
#endif

#include <format>
#include <utility>

namespace conjure::editor {

    std::string_view to_string(play_state state) noexcept {
        switch (state) {
            case play_state::EDITING:
                return "editing";
            case play_state::PLAYING:
                return "playing";
            case play_state::PAUSED:
                return "paused";
        }
        return "";
    }

    play_controller::play_controller(editor_context &context) :
        mContext(&context) {}

    play_controller::~play_controller() = default;

    core::result<void> play_controller::start() {
        if (mState != play_state::EDITING) {
            return make_editor_error(
              protocol_error_code::WRONG_PLAY_STATE,
              std::format("play.start needs the editing state (currently {})", to_string(mState))
            );
        }
        core::result<scene::scene_snapshot> snapshot = scene::capture(mContext->scene());
        if (!snapshot.has_value()) {
            return make_editor_error(
              protocol_error_code::INTERNAL,
              std::format("the scene snapshot failed: {}", snapshot.get_error().to_string())
            );
        }
        mSnapshot.emplace(std::move(snapshot).value());
        const std::span<const core::uuid> selected = mContext->selection().guids();
        mSelectionBackup.assign(selected.begin(), selected.end());
        // Named last, the primary is the primary again when stop() hands the backup to set() (set() drops the
        // duplicate).
        const core::uuid primary = mContext->selection().primary();
        if (!primary.is_null()) {
            mSelectionBackup.push_back(primary);
        }
        mContext->undo().set_frozen(true);
        set_script_mode(true);
        mContext->engine().set_simulation_mode(engine::simulation_mode::RUNNING);
        mState = play_state::PLAYING;
        mSessionStartFrame = mContext->frame_index();
        mSimulationFrames = 0;
        mWarnedUnrecordedEdit = false;
        emit_state();
        return core::result<void>::success();
    }

    core::result<void> play_controller::stop() {
        if (mState == play_state::EDITING) {
            return make_editor_error(protocol_error_code::WRONG_PLAY_STATE, "play.stop: the editor is not playing");
        }
        mContext->engine().set_simulation_mode(engine::simulation_mode::PAUSED);
        if (mPendingStep.has_value()) {
            mPendingStep->reply.fail(protocol_error_code::CANCELLED, "play stopped before the step completed");
            mPendingStep.reset();
        }
        set_script_mode(false);
        if (mSnapshot.has_value()) {
            const core::result<void> restored = scene::restore(mContext->scene(), *mSnapshot);
            if (!restored.has_value()) {
                CONJURE_LOG_ERROR(
                  LOG_EDITOR,
                  "play.stop: restoring the scene snapshot failed ({}); the scene is now empty",
                  restored.get_error().to_string()
                );
                mContext->scene().clear();
            }
            mSnapshot.reset();
        }
        mContext->selection().set(mSelectionBackup);
        mContext->selection().rebuild(mContext->scene());
        mSelectionBackup.clear();
        mContext->undo().set_frozen(false);
        mState = play_state::EDITING;
        mSimulationFrames = 0;
        emit_state();
        mContext->emit_selection_changed();
        return core::result<void>::success();
    }

    core::result<void> play_controller::pause() {
        if (mState != play_state::PLAYING) {
            return make_editor_error(
              protocol_error_code::WRONG_PLAY_STATE,
              std::format("play.pause needs the playing state (currently {})", to_string(mState))
            );
        }
        mContext->engine().set_simulation_mode(engine::simulation_mode::PAUSED);
        mState = play_state::PAUSED;
        emit_state();
        return core::result<void>::success();
    }

    core::result<void> play_controller::resume() {
        if (mState != play_state::PAUSED) {
            return make_editor_error(
              protocol_error_code::WRONG_PLAY_STATE,
              std::format("play.resume needs the paused state (currently {})", to_string(mState))
            );
        }
        if (mPendingStep.has_value()) {
            return make_editor_error(protocol_error_code::BUSY, "a play.step is still running");
        }
        mContext->engine().set_simulation_mode(engine::simulation_mode::RUNNING);
        mState = play_state::PLAYING;
        emit_state();
        return core::result<void>::success();
    }

    core::result<void> play_controller::step(u32 fixedSteps, async_reply reply) {
        if (mState != play_state::PAUSED) {
            return make_editor_error(
              protocol_error_code::WRONG_PLAY_STATE,
              std::format("play.step needs the paused state (currently {})", to_string(mState))
            );
        }
        if (fixedSteps == 0) {
            return make_editor_error(protocol_error_code::INVALID_ARGS, "fixedSteps must be at least 1");
        }
        if (mPendingStep.has_value()) {
            return make_editor_error(protocol_error_code::BUSY, "a play.step is already pending");
        }
        pending_step pending = {};
        pending.reply = reply;
        pending.targetFixedStepIndex = mContext->engine().time().fixedStepIndex + fixedSteps;
        pending.requestedFrame = mContext->frame_index();
        mPendingStep = pending;
        mContext->engine().step_fixed(fixedSteps);
        return core::result<void>::success();
    }

    void play_controller::tick() {
        if (mState == play_state::PLAYING) {
            ++mSimulationFrames;
        }
        if (!mPendingStep.has_value()) {
            return;
        }
        const engine::engine_time &time = mContext->engine().time();
        if (time.fixedStepIndex < mPendingStep->targetFixedStepIndex) {
            return;
        }
        // The engine returns to PAUSED on its own after a forced step; the state already says so.
        ++mSimulationFrames;
        if (mPendingStep->reply.valid()) {
            mPendingStep->reply.succeed(describe());
        }
        mPendingStep.reset();
    }

    void play_controller::apply_engine_mode() {
        switch (mState) {
            case play_state::EDITING:
            case play_state::PAUSED:
                mContext->engine().set_simulation_mode(engine::simulation_mode::PAUSED);
                break;
            case play_state::PLAYING:
                mContext->engine().set_simulation_mode(engine::simulation_mode::RUNNING);
                break;
        }
    }

    void play_controller::note_unrecorded_edit() {
        if (mWarnedUnrecordedEdit || mState == play_state::EDITING) {
            return;
        }
        mWarnedUnrecordedEdit = true;
        mContext->events().emit(
          editor_event::WARNING,
          core::json::object({
            {"code", "UNRECORDED_PLAY_EDIT"},
            {"message", "a mutating command ran during play; it is not recorded for undo and play.stop discards it"},
          })
        );
    }

    core::json::document play_controller::describe() const {
        core::json::document document = core::json::document::object();
        document.set("state", to_string(mState));
        document.set("frame", mContext->frame_index());
        document.set("simulationFrame", mSimulationFrames);
        document.set("simulationTime", mContext->engine().time().time);
        document.set("fixedStepIndex", mContext->engine().time().fixedStepIndex);
        return document;
    }

    void play_controller::emit_state() {
        mContext->events().emit(editor_event::PLAY_STATE, describe());
    }

    void play_controller::set_script_mode(bool playing) {
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
        scripting::script_runtime *runtime = mContext->scripting();
        if (runtime != nullptr) {
            runtime->set_mode(playing ? scripting::script_mode::PLAY : scripting::script_mode::EDIT);
        }
#else
        (void) playing;
#endif
    }
}
