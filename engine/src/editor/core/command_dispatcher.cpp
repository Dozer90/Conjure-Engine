#include "conjure/editor/command_dispatcher.h"

#include "conjure/editor/editor_context.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/log_category.h"

#include "conjure/core/assert.h"

#include <algorithm>
#include <format>
#include <utility>

namespace conjure::editor {

    namespace {
        // How much of an unknown command's name an error message echoes; real names are far shorter.
        constexpr usize MAX_ECHOED_NAME_BYTES = 128;

        /// <summary>
        /// The name as an error message quotes it: whole when short, otherwise its first MAX_ECHOED_NAME_BYTES
        /// bytes cut back to a code point boundary (a split character would make the response invalid UTF-8,
        /// which the JSON writer refuses) followed by "...".
        /// </summary>
        [[nodiscard]] std::string echoed_name(std::string_view name) {
            if (name.size() <= MAX_ECHOED_NAME_BYTES) {
                return std::string(name);
            }
            usize cut = MAX_ECHOED_NAME_BYTES;
            while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0u) == 0x80u) {
                --cut;
            }
            return std::string(name.substr(0, cut)) + "...";
        }
    }

    command_dispatcher::command_dispatcher(editor_context &context, command_registry &registry, undo_stack &undo) :
        mContext(&context),
        mRegistry(&registry),
        mUndo(&undo) {}

    command_dispatcher::~command_dispatcher() = default;

    std::optional<protocol_response> command_dispatcher::dispatch(const protocol_request &request) {
        std::optional<protocol_response> response = dispatch_internal(request);
        if (response.has_value()) {
            response->clientId = request.clientId;
        }
        mContext->events().emit(
          editor_event::COMMAND_EXECUTED,
          core::json::object({
            {"clientId", request.clientId},
            {"cmd", std::string_view(request.command)},
            {"ok", response.has_value() ? response->ok : true},
            {"async", !response.has_value()},
          })
        );
        return response;
    }

    core::result<core::json::document> command_dispatcher::execute(const command_request &request) {
        protocol_request wrapped = {};
        wrapped.command = request.name;
        wrapped.args = request.args;
        wrapped.clientId = request.clientId;
        wrapped.coalesceKey = request.coalesceKey;
        std::optional<protocol_response> response = dispatch(wrapped);
        if (!response.has_value()) {
            return core::json::document::object();
        }
        if (!response->ok) {
            return make_editor_error(response->errorCode, response->errorMessage);
        }
        return std::move(response->result);
    }

    void command_dispatcher::resolve_completed(std::vector<protocol_response> &outResponses) {
        const f64 now = now_seconds();
        for (usize index = 0; index < mPending.size();) {
            pending_request &pending = mPending[index];
            if (now >= pending.deadlineSeconds) {
                protocol_response timedOut = make_error_response(
                  pending.id.root(),
                  protocol_error_code::TIMEOUT,
                  std::format("'{}' did not complete within {} seconds", pending.command, mAsyncTimeoutSeconds)
                );
                timedOut.clientId = pending.clientId;
                CONJURE_LOG_WARN(LOG_EDITOR, "async command '{}' timed out", pending.command);
                mCompleted.push_back(std::move(timedOut));
                mPending.erase(mPending.begin() + static_cast<isize>(index));
                continue;
            }
            ++index;
        }
        for (protocol_response &response : mCompleted) {
            if (response.clientId == 0) {
                mOnLocalResponse.emit(response);
            }
            outResponses.push_back(std::move(response));
        }
        mCompleted.clear();
    }

    void command_dispatcher::cancel_client(u32 clientId) {
        for (usize index = 0; index < mPending.size();) {
            if (mPending[index].clientId == clientId) {
                CONJURE_LOG_INFO(
                  LOG_EDITOR,
                  "client {} disconnected: pending '{}' cancelled",
                  clientId,
                  mPending[index].command
                );
                mPending.erase(mPending.begin() + static_cast<isize>(index));
                continue;
            }
            ++index;
        }
        std::erase_if(mCompleted, [clientId](const protocol_response &response) {
            return response.clientId == clientId;
        });
        if (mUndo->has_transaction(clientId)) {
            const core::result<void> ended = mUndo->end_transaction(clientId);
            (void) ended;
        }
    }

    void command_dispatcher::cancel_all(protocol_error_code code, std::string_view message) {
        for (pending_request &pending : mPending) {
            protocol_response cancelled = make_error_response(
              pending.id.root(),
              code,
              std::format("'{}' was cancelled: {}", pending.command, message)
            );
            cancelled.clientId = pending.clientId;
            mCompleted.push_back(std::move(cancelled));
        }
        if (!mPending.empty()) {
            CONJURE_LOG_INFO(LOG_EDITOR, "{} pending asynchronous request(s) cancelled: {}", mPending.size(), message);
        }
        mPending.clear();
    }

    usize command_dispatcher::pending_count(u32 clientId) const noexcept {
        usize count = 0;
        for (const pending_request &pending : mPending) {
            if (pending.clientId == clientId) {
                ++count;
            }
        }
        return count;
    }

    void command_dispatcher::complete(u64 ticket, protocol_response response) {
        const auto position = std::find_if(mPending.begin(), mPending.end(), [ticket](const pending_request &pending) {
            return pending.ticket == ticket;
        });
        if (position == mPending.end()) {
            CONJURE_ASSERT(false, "command_dispatcher::complete: ticket {} is not pending", ticket);
            return;
        }
        response.id = position->id;
        response.clientId = position->clientId;
        mPending.erase(position);
        if (ticket == mDispatchingTicket) {
            // Completed inside its own handler: dispatch_internal returns it synchronously. Matching on the
            // ticket (not the client and id, which are not unique) keeps another request's completion that
            // lands meanwhile in the queue where it belongs.
            mSynchronousResponse = std::move(response);
            return;
        }
        mCompleted.push_back(std::move(response));
    }

    bool command_dispatcher::is_pending(u64 ticket) const noexcept {
        for (const pending_request &pending : mPending) {
            if (pending.ticket == ticket) {
                return true;
            }
        }
        return false;
    }

    core::result<std::vector<protocol_response>> command_dispatcher::run_batch(
      const core::json::value &commands,
      std::string_view label,
      bool atomic,
      u32 clientId,
      core::json::document &outDetails
    ) {
        if (mBatch != nullptr) {
            return make_editor_error(protocol_error_code::INVALID_ARGS, "editor.batch cannot be nested");
        }
        if (!commands.is_array()) {
            return make_editor_error(protocol_error_code::INVALID_ARGS, "args.commands: expected an array");
        }
        // Reject children that may not run inside a batch before anything executes (D4).
        usize index = 0;
        for (const core::json::value child : commands) {
            const core::json::value name = child["cmd"];
            if (!child.is_object() || !name.is_string()) {
                outDetails.set("failedIndex", index);
                return make_editor_error(
                  protocol_error_code::INVALID_ARGS,
                  std::format("args.commands[{}]: expected {{cmd, args}}", index)
                );
            }
            const command_descriptor *descriptor = mRegistry->find(name.as_string());
            if (descriptor == nullptr) {
                outDetails.set("failedIndex", index);
                return make_editor_error(
                  protocol_error_code::UNKNOWN_COMMAND,
                  std::format("args.commands[{}]: unknown command '{}'", index, echoed_name(name.as_string()))
                );
            }
            if (has_flag(descriptor->flags, command_flags::NOT_IN_BATCH)) {
                outDetails.set("failedIndex", index);
                return make_editor_error(
                  protocol_error_code::INVALID_ARGS,
                  std::format("args.commands[{}]: '{}' cannot run inside a batch", index, name.as_string())
                );
            }
            ++index;
        }

        batch_state batch = {};
        batch.entry.label = std::string(label);
        batch.entry.clientId = clientId;
        mBatch = &batch;
        std::vector<protocol_response> responses = {};
        std::optional<core::error> failure = std::nullopt;
        index = 0;
        for (const core::json::value child : commands) {
            protocol_request request = {};
            request.id = make_id(static_cast<i64>(index));
            request.command = std::string(child["cmd"].as_string());
            const core::json::value args = child["args"];
            request.args = args.is_object() ? core::json::document::from_value(args) : core::json::document::object();
            request.clientId = clientId;
            std::optional<protocol_response> response = dispatch_internal(request);
            if (!response.has_value()) {
                // Cannot happen: NOT_IN_BATCH covers ASYNC; keep the batch consistent anyway.
                response = make_error_response(request.id.root(), protocol_error_code::INTERNAL, "asynchronous child");
            }
            const bool ok = response->ok;
            responses.push_back(std::move(*response));
            if (!ok) {
                if (atomic) {
                    failure = make_editor_error(responses.back().errorCode, responses.back().errorMessage);
                    outDetails = responses.back().errorDetails;
                    outDetails.set("failedIndex", index);
                    break;
                }
            }
            ++index;
        }
        mBatch = nullptr;

        if (failure.has_value()) {
            // Roll back through the recorded inverses, newest first, without recording.
            ++mSuppressRecording;
            for (usize inverseIndex = batch.entry.undo.size(); inverseIndex > 0; --inverseIndex) {
                const command_inverse &inverse = batch.entry.undo[inverseIndex - 1];
                protocol_request request = {};
                request.id = null_id();
                request.command = inverse.name;
                request.args = inverse.args;
                request.clientId = clientId;
                const std::optional<protocol_response> rolledBack = dispatch_internal(request);
                if (rolledBack.has_value() && !rolledBack->ok) {
                    CONJURE_LOG_ERROR(
                      LOG_EDITOR,
                      "editor.batch rollback: '{}' failed: {}",
                      inverse.name,
                      rolledBack->errorMessage
                    );
                }
            }
            --mSuppressRecording;
            return *failure;
        }

        // A batch dispatched as an undo or redo step records nothing: the history entry it belongs to is the one
        // being applied.
        if (!batch.entry.undo.empty() && mSuppressRecording == 0) {
            if (mUndo->frozen()) {
                keep_play_edits(batch);
            } else if (mUndo->has_transaction(clientId)) {
                for (usize entryIndex = 0; entryIndex < batch.entry.undo.size(); ++entryIndex) {
                    mUndo->record(
                      batch.entry.redo[entryIndex],
                      batch.entry.undo[entryIndex],
                      batch.entry.label,
                      clientId,
                      0
                    );
                }
            } else {
                mUndo->push(std::move(batch.entry));
            }
        }
        return responses;
    }

    core::result<void> command_dispatcher::dispatch_inverses(std::span<const command_inverse> inverses, u32 clientId) {
        ++mSuppressRecording;
        ++mSuppressRevision;
        core::result<void> outcome = core::result<void>::success();
        for (usize index = inverses.size(); index > 0; --index) {
            const command_inverse &inverse = inverses[index - 1];
            protocol_request request = {};
            request.id = null_id();
            request.command = inverse.name;
            request.args = inverse.args;
            request.clientId = clientId;
            const std::optional<protocol_response> response = dispatch_internal(request);
            if (response.has_value() && !response->ok) {
                outcome = make_editor_error(
                  protocol_error_code::INTERNAL,
                  std::format("undo of '{}' failed: {}", inverse.name, response->errorMessage)
                );
                break;
            }
        }
        --mSuppressRevision;
        --mSuppressRecording;
        return outcome;
    }

    core::result<void> command_dispatcher::dispatch_replays(std::span<const command_request> replays, u32 clientId) {
        ++mSuppressRecording;
        ++mSuppressRevision;
        core::result<void> outcome = core::result<void>::success();
        for (const command_request &replay : replays) {
            protocol_request request = {};
            request.id = null_id();
            request.command = replay.name;
            request.args = replay.args;
            request.clientId = clientId;
            const std::optional<protocol_response> response = dispatch_internal(request);
            if (response.has_value() && !response->ok) {
                outcome = make_editor_error(
                  protocol_error_code::INTERNAL,
                  std::format("redo of '{}' failed: {}", replay.name, response->errorMessage)
                );
                break;
            }
        }
        --mSuppressRevision;
        --mSuppressRecording;
        return outcome;
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Internals

    std::optional<protocol_response> command_dispatcher::dispatch_internal(const protocol_request &request) {
        const core::json::value id = request.id.root();
        const command_descriptor *descriptor = mRegistry->find(request.command);
        if (descriptor == nullptr) {
            core::json::document details = core::json::document::object();
            core::json::document suggestions = core::json::document::array();
            for (const std::string &name : mRegistry->closest_names(request.command, 3)) {
                suggestions.push_back(std::string_view(name));
            }
            details.set("didYouMean", suggestions);
            return make_error_response(
              id,
              protocol_error_code::UNKNOWN_COMMAND,
              std::format("unknown command '{}'", echoed_name(request.command)),
              std::move(details)
            );
        }

        const compiled_schema *schema = mRegistry->args_schema(request.command);
        const core::json::value args = request.args.root();
        if (schema != nullptr) {
            const core::result<void> valid = schema->validate(
              args.is_valid() ? args : core::json::document::object().root(),
              "args"
            );
            if (!valid.has_value()) {
                return make_error_response(id, protocol_error_code::INVALID_ARGS, valid.get_error().message());
            }
        }

        core::json::document preconditionDetails = core::json::document::object();
        const std::optional<protocol_error_code> refused = check_preconditions(*descriptor, preconditionDetails);
        if (refused.has_value()) {
            std::string message = "";
            switch (*refused) {
                case protocol_error_code::PROJECT_NOT_OPEN:
                    message = std::format("'{}' needs an open project", request.command);
                    break;
                case protocol_error_code::SCENE_NOT_OPEN:
                    message = std::format("'{}' needs an open scene", request.command);
                    break;
                default:
                    message = std::format(
                      "'{}' is not allowed in the {} play state",
                      request.command,
                      to_string(mContext->play().state())
                    );
                    break;
            }
            return make_error_response(id, *refused, std::move(message), std::move(preconditionDetails));
        }

        const core::json::document emptyArgs = core::json::document::object();
        command_invocation invocation{*mContext, args.is_valid() ? args : emptyArgs.root(), request.clientId, {}, {}};

        if (has_flag(descriptor->flags, command_flags::ASYNC)) {
            if (pending_count(request.clientId) >= mMaxPendingPerClient) {
                return make_error_response(
                  id,
                  protocol_error_code::BUSY,
                  std::format(
                    "client {} already has {} pending asynchronous requests",
                    request.clientId,
                    mMaxPendingPerClient
                  )
                );
            }
            pending_request pending = {};
            pending.id = request.id;
            pending.command = request.command;
            pending.ticket = mNextTicket;
            ++mNextTicket;
            pending.clientId = request.clientId;
            pending.deadlineSeconds = now_seconds() + mAsyncTimeoutSeconds;
            mPending.push_back(pending);
            invocation.reply = async_reply(*this, pending.ticket);
            // A handler may complete its reply before returning (project.export, a screenshot refused up
            // front): complete() parks that response here instead of the queue. Saved and restored so a
            // handler that dispatches further requests cannot disturb an outer one.
            const u64 outerTicket = mDispatchingTicket;
            std::optional<protocol_response> outerSynchronous = std::move(mSynchronousResponse);
            mDispatchingTicket = pending.ticket;
            mSynchronousResponse.reset();
            core::result<command_outcome> outcome = descriptor->handler(invocation);
            std::optional<protocol_response> synchronous = std::move(mSynchronousResponse);
            mDispatchingTicket = outerTicket;
            mSynchronousResponse = std::move(outerSynchronous);
            if (!outcome.has_value()) {
                // The handler failed before arming its reply: answer synchronously and free the slot.
                std::erase_if(mPending, [ticket = pending.ticket](const pending_request &entry) {
                    return entry.ticket == ticket;
                });
                return make_error_response(
                  id,
                  classify_error(outcome.get_error()),
                  outcome.get_error().message(),
                  std::move(invocation.errorDetails)
                );
            }
            if (has_flag(descriptor->flags, command_flags::MUTATES)) {
                if (mSuppressRevision == 0) {
                    mContext->bump_revision();
                }
                if (!has_flag(descriptor->flags, command_flags::NO_UNDO)) {
                    mContext->set_scene_dirty(true);
                }
            }
            if (synchronous.has_value()) {
                return synchronous;
            }
            return std::nullopt;
        }

        core::result<command_outcome> outcome = descriptor->handler(invocation);
        if (!outcome.has_value()) {
            return make_error_response(
              id,
              classify_error(outcome.get_error()),
              outcome.get_error().message(),
              std::move(invocation.errorDetails)
            );
        }
        command_outcome &value = outcome.value();
        if (!value.result.root().is_object()) {
            value.result = core::json::document::object();
        }
        if (has_flag(descriptor->flags, command_flags::MUTATES)) {
            record_outcome(*descriptor, request, value);
            if (!has_flag(descriptor->flags, command_flags::NO_UNDO)) {
                mContext->set_scene_dirty(true);
            }
            if (mSuppressRevision > 0) {
                // An undo or redo step: history.undo / redo bump the revision and announce the step once.
                return make_ok_response(id, std::move(value.result));
            }
            const u64 revision = mContext->bump_revision();
            value.result.set("revision", revision);
            mContext->events().emit(
              editor_event::SCENE_CHANGED,
              core::json::object({
                {"revision", revision},
                {"label", std::string_view(value.label)},
                {"cmd", std::string_view(request.command)},
              })
            );
        }
        return make_ok_response(id, std::move(value.result));
    }

    std::optional<protocol_error_code> command_dispatcher::check_preconditions(
      const command_descriptor &descriptor,
      core::json::document &details
    ) const {
        if (has_flag(descriptor.flags, command_flags::REQUIRES_PROJECT) && !mContext->has_project()) {
            return protocol_error_code::PROJECT_NOT_OPEN;
        }
        if (has_flag(descriptor.flags, command_flags::REQUIRES_SCENE) && !mContext->has_scene()) {
            return protocol_error_code::SCENE_NOT_OPEN;
        }
        const play_state state = mContext->play().state();
        if (has_flag(descriptor.flags, command_flags::EDITING_ONLY) && state != play_state::EDITING) {
            details.set("state", to_string(state));
            return protocol_error_code::WRONG_PLAY_STATE;
        }
        if (has_flag(descriptor.flags, command_flags::PLAYING_ONLY) && state == play_state::EDITING) {
            details.set("state", to_string(state));
            return protocol_error_code::WRONG_PLAY_STATE;
        }
        return std::nullopt;
    }

    void command_dispatcher::record_outcome(
      const command_descriptor &descriptor,
      const protocol_request &request,
      command_outcome &outcome
    ) {
        if (has_flag(descriptor.flags, command_flags::NO_UNDO) || outcome.skipRecording) {
            return;
        }
        if (!outcome.inverse.has_value()) {
            // A programmer error in the handler, never a reason to abort the editor: the mutation stands and is
            // simply not undoable.
            CONJURE_LOG_ERROR(
              LOG_EDITOR,
              "{}: mutating command '{}' returned no inverse; the edit is not recorded for undo",
              to_string(protocol_error_code::INTERNAL),
              descriptor.name
            );
            return;
        }
        command_request replay = {};
        if (outcome.replay.has_value()) {
            replay = std::move(*outcome.replay);
        } else {
            replay.name = request.command;
            replay.args = request.args;
        }
        replay.clientId = request.clientId;
        replay.coalesceKey = request.coalesceKey;
        const bool onDisk = has_flag(descriptor.flags, command_flags::ON_DISK);
        // A running batch always collects its children's inverses, even while playing or while the batch itself
        // runs as an undo or redo step: its atomic rollback needs them. run_batch decides afterwards whether the
        // collected entry reaches the stack.
        if (mBatch != nullptr) {
            mBatch->entry.undo.push_back(std::move(*outcome.inverse));
            mBatch->entry.redo.push_back(std::move(replay));
            mBatch->fileCommands.push_back(onDisk ? descriptor.name : std::string());
            if (mBatch->entry.label.empty()) {
                mBatch->entry.label = outcome.label;
            }
            return;
        }
        if (mSuppressRecording > 0) {
            return;
        }
        if (mUndo->frozen()) {
            // Playing: play.stop restores the scene, so a scene edit has nothing to undo, but it does not restore
            // the project's files, so a file edit keeps its inverse until then (D9, D21).
            if (onDisk) {
                mContext->play().keep_file_edit(descriptor.name, std::move(replay), std::move(*outcome.inverse));
                return;
            }
            mContext->play().note_unrecorded_edit();
            return;
        }
        mUndo->record(
          std::move(replay),
          std::move(*outcome.inverse),
          outcome.label,
          request.clientId,
          request.coalesceKey
        );
    }

    void command_dispatcher::keep_play_edits(batch_state &batch) {
        bool sceneEdit = false;
        for (usize index = 0; index < batch.entry.undo.size(); ++index) {
            const std::string &fileCommand = batch.fileCommands[index];
            if (fileCommand.empty()) {
                sceneEdit = true;
                continue;
            }
            mContext->play().keep_file_edit(
              fileCommand,
              std::move(batch.entry.redo[index]),
              std::move(batch.entry.undo[index])
            );
        }
        if (sceneEdit) {
            mContext->play().note_unrecorded_edit();
        }
    }

    f64 command_dispatcher::now_seconds() const noexcept {
        if (mClock != nullptr) {
            return mClock->now_seconds();
        }
        return mContext->engine().clock().now_seconds();
    }
}
