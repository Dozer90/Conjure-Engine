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
        audit(request, response, false);
        // Every subscribed client hears this event, an inspect-only one among them, and the name of an unknown
        // command is whatever its sender typed: a token must not travel in it.
        const protocol_server *server = mContext->server();
        const bool nameHoldsToken = server != nullptr && server->contains_token(request.command);
        mContext->events().emit(
          editor_event::COMMAND_EXECUTED,
          core::json::object({
            {"clientId", request.clientId},
            {"cmd", nameHoldsToken ? audit_log::TOKEN_MARK : std::string_view(request.command)},
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
            return core::make_error(response->errorCode, response->errorMessage);
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
        if (!response.ok && position->clientId == 0 && ticket != mDispatchingTicket) {
            // The UI fires its commands and moves on: without this line a parked UI request that fails later, a
            // prefab.instantiate from the asset browser among them, would fail silently. A failure inside the
            // handler is returned synchronously, and ui::support::run_command logs that one the same way.
            CONJURE_LOG_WARN(LOG_EDITOR, "ui: {} failed: {}", position->command, response.errorMessage);
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

    void command_dispatcher::complete_mutation(u64 ticket, command_outcome outcome) {
        const auto position = std::find_if(mPending.begin(), mPending.end(), [ticket](const pending_request &pending) {
            return pending.ticket == ticket;
        });
        if (position == mPending.end()) {
            CONJURE_ASSERT(false, "command_dispatcher::complete_mutation: ticket {} is not pending", ticket);
            return;
        }
        if (!has_flag(position->flags, command_flags::MUTATES)) {
            // A programmer error in the handler: answer with the result, as succeed() would, and record nothing.
            CONJURE_ASSERT(false, "command_dispatcher::complete_mutation: '{}' does not mutate", position->command);
            protocol_response plain = {};
            plain.ok = true;
            plain.result = std::move(outcome.result);
            complete(ticket, std::move(plain));
            return;
        }
        protocol_request request = {};
        request.id = position->id;
        request.command = position->command;
        request.args = position->args;
        request.coalesceKey = position->coalesceKey;
        request.clientId = position->clientId;
        const command_flags flags = position->flags;
        protocol_response response = conclude_mutation(flags, request, outcome);
        complete(ticket, std::move(response));
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
            return core::make_error(protocol_error_code::INVALID_ARGS, "editor.batch cannot be nested");
        }
        if (!commands.is_array()) {
            return core::make_error(protocol_error_code::INVALID_ARGS, "args.commands: expected an array");
        }
        // Reject children that may not run inside a batch before anything executes (D4).
        usize index = 0;
        for (const core::json::value child : commands) {
            const core::json::value name = child["cmd"];
            if (!child.is_object() || !name.is_string()) {
                outDetails.set("failedIndex", index);
                return core::make_error(
                  protocol_error_code::INVALID_ARGS,
                  std::format("args.commands[{}]: expected {{cmd, args}}", index)
                );
            }
            const command_descriptor *descriptor = mRegistry->find(name.as_string());
            if (descriptor == nullptr) {
                outDetails.set("failedIndex", index);
                return core::make_error(
                  protocol_error_code::UNKNOWN_COMMAND,
                  std::format("args.commands[{}]: unknown command '{}'", index, echoed_name(name.as_string()))
                );
            }
            if (has_flag(descriptor->flags, command_flags::NOT_IN_BATCH)) {
                outDetails.set("failedIndex", index);
                return core::make_error(
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
                // Cannot happen: NOT_IN_BATCH covers ASYNC, and dispatch_internal answers a SYNC_IN_BATCH child that
                // did not complete inside its handler with INTERNAL; keep the batch consistent anyway.
                response = make_error_response(request.id.root(), protocol_error_code::INTERNAL, "asynchronous child");
            }
            audit(request, response, true);
            const bool ok = response->ok;
            responses.push_back(std::move(*response));
            if (!ok) {
                if (atomic) {
                    failure = core::make_error(responses.back().errorCode, responses.back().errorMessage);
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
                outcome = core::make_error(
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
                outcome = core::make_error(
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

        // The scope of the client's token (D25), before anything of the request is looked at: an inspect-only
        // client learns nothing about the arguments or the state a command it may not run would have met.
        const access_level required = required_access(descriptor->flags);
        const access_level granted = access_of_client(request.clientId);
        if (granted < required) {
            return make_error_response(
              id,
              protocol_error_code::UNAUTHORISED,
              std::format(
                "'{}' needs {} access and this client has {}: the inspect-only token runs the commands flagged "
                "read_only, the full-control token every command",
                request.command,
                to_string(required),
                to_string(granted)
              ),
              core::json::object({{"required", to_string(required)}, {"granted", to_string(granted)}})
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
            const auto busy = [this, &id, &request]() {
                return make_error_response(
                  id,
                  protocol_error_code::BUSY,
                  std::format(
                    "client {} already has {} pending asynchronous requests",
                    request.clientId,
                    mMaxPendingPerClient
                  )
                );
            };
            // Only a request left parked counts toward the cap. One that must complete inside its handler (a batch
            // child, a replay of an undo or redo step) cannot stay parked and is never refused; a SYNC_IN_BATCH
            // request may finish inside its handler anywhere (a resident prefab) and is checked once it returns.
            const bool syncInBatch = has_flag(descriptor->flags, command_flags::SYNC_IN_BATCH);
            if (!requires_synchronous_completion() && !syncInBatch &&
              pending_count(request.clientId) >= mMaxPendingPerClient) {
                return busy();
            }
            const u64 ticket = mNextTicket;
            ++mNextTicket;
            pending_request pending = {};
            pending.id = request.id;
            pending.command = request.command;
            if (has_flag(descriptor->flags, command_flags::MUTATES)) {
                // complete_mutation records the request as the replay unless the outcome names another one.
                pending.args = request.args;
            }
            pending.ticket = ticket;
            pending.coalesceKey = request.coalesceKey;
            pending.flags = descriptor->flags;
            pending.clientId = request.clientId;
            pending.deadlineSeconds = now_seconds() + mAsyncTimeoutSeconds;
            mPending.push_back(std::move(pending));
            invocation.reply = async_reply(*this, ticket);
            // A handler may complete its reply before returning (project.export, a screenshot refused up front, a
            // prefab command inside a batch): complete() parks that response here instead of the queue. Saved and
            // restored so a handler that dispatches further requests cannot disturb an outer one.
            const u64 outerTicket = mDispatchingTicket;
            std::optional<protocol_response> outerSynchronous = std::move(mSynchronousResponse);
            mDispatchingTicket = ticket;
            mSynchronousResponse.reset();
            core::result<command_outcome> outcome = descriptor->handler(invocation);
            std::optional<protocol_response> synchronous = std::move(mSynchronousResponse);
            mDispatchingTicket = outerTicket;
            mSynchronousResponse = std::move(outerSynchronous);
            const auto release = [this, ticket]() {
                std::erase_if(mPending, [ticket](const pending_request &entry) {
                    return entry.ticket == ticket;
                });
            };
            if (!outcome.has_value()) {
                // The handler failed before arming its reply: answer synchronously and free the slot.
                release();
                return make_error_response(
                  id,
                  classify_error(outcome.get_error()),
                  outcome.get_error().message(),
                  std::move(invocation.errorDetails)
                );
            }
            // The recording, the dirty flag, the revision and scene.changed of an ASYNC | MUTATES command belong to
            // its completion (complete_mutation), which ran already when the handler answered synchronously.
            if (synchronous.has_value()) {
                return synchronous;
            }
            if (requires_synchronous_completion()) {
                // A batch child or a replay that answered later would land outside the entry, the rollback or the
                // undo step it belongs to: its reply is dropped, so a later completion is ignored.
                release();
                CONJURE_LOG_ERROR(
                  LOG_EDITOR,
                  "{}: '{}' did not complete inside its handler in a batch or an undo / redo step",
                  to_string(protocol_error_code::INTERNAL),
                  request.command
                );
                return make_error_response(
                  id,
                  protocol_error_code::INTERNAL,
                  std::format("'{}' must complete inside a batch or an undo / redo step", request.command)
                );
            }
            if (syncInBatch && exceeds_pending_limit(request.clientId)) {
                // Left parked beyond the cap: refused, and its reply dropped, as a timed-out one is (the handler
                // changes nothing for a reply that is no longer valid). The prefab commands refuse before parking.
                release();
                return busy();
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
        if (has_flag(descriptor->flags, command_flags::MUTATES)) {
            return conclude_mutation(descriptor->flags, request, value);
        }
        if (!value.result.root().is_object()) {
            value.result = core::json::document::object();
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

    protocol_response command_dispatcher::conclude_mutation(
      command_flags flags,
      const protocol_request &request,
      command_outcome &outcome
    ) {
        const core::json::value id = request.id.root();
        if (!outcome.result.root().is_object()) {
            outcome.result = core::json::document::object();
        }
        record_outcome(flags, request, outcome);
        if (!has_flag(flags, command_flags::NO_UNDO)) {
            mContext->set_scene_dirty(true);
        }
        if (mSuppressRevision > 0) {
            // An undo or redo step: history.undo / redo bump the revision and announce the step once.
            return make_ok_response(id, std::move(outcome.result));
        }
        const u64 revision = mContext->bump_revision();
        outcome.result.set("revision", revision);
        mContext->events().emit(
          editor_event::SCENE_CHANGED,
          core::json::object({
            {"revision", revision},
            {"label", std::string_view(outcome.label)},
            {"cmd", std::string_view(request.command)},
          })
        );
        return make_ok_response(id, std::move(outcome.result));
    }

    void command_dispatcher::record_outcome(
      command_flags flags,
      const protocol_request &request,
      command_outcome &outcome
    ) {
        if (has_flag(flags, command_flags::NO_UNDO) || outcome.skipRecording) {
            return;
        }
        if (!outcome.inverse.has_value()) {
            // A programmer error in the handler, never a reason to abort the editor: the mutation stands and is
            // simply not undoable.
            CONJURE_LOG_ERROR(
              LOG_EDITOR,
              "{}: mutating command '{}' returned no inverse; the edit is not recorded for undo",
              to_string(protocol_error_code::INTERNAL),
              request.command
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
        const bool onDisk = has_flag(flags, command_flags::ON_DISK);
        // A running batch always collects its children's inverses, even while playing or while the batch itself
        // runs as an undo or redo step: its atomic rollback needs them. run_batch decides afterwards whether the
        // collected entry reaches the stack.
        if (mBatch != nullptr) {
            mBatch->entry.undo.push_back(std::move(*outcome.inverse));
            mBatch->entry.redo.push_back(std::move(replay));
            mBatch->fileCommands.push_back(onDisk ? request.command : std::string());
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
                mContext->play().keep_file_edit(request.command, std::move(replay), std::move(*outcome.inverse));
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

    access_level command_dispatcher::access_of_client(u32 clientId) const noexcept {
        const protocol_server *server = mContext->server();
        // Without a server every caller is in-process code (tests, the UI): the editor itself.
        return server == nullptr ? access_level::FULL_CONTROL : server->access_of_client(clientId);
    }

    void command_dispatcher::audit(
      const protocol_request &request,
      const std::optional<protocol_response> &response,
      bool batchChild
    ) const {
        protocol_server *server = mContext->server();
        if (server == nullptr) {
            return;
        }
        audit_entry entry = {};
        entry.command = request.command;
        entry.args = request.args.root();
        entry.clientId = request.clientId;
        entry.ok = !response.has_value() || response->ok;
        entry.error = response.has_value() ? response->errorCode : protocol_error_code::NONE;
        entry.async = !response.has_value();
        entry.batchChild = batchChild;
        server->record_command(entry);
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
