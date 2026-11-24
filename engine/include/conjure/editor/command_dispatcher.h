#pragma once

#include "conjure/editor/command_registry.h"
#include "conjure/editor/editor_command.h"
#include "conjure/editor/protocol/access_token.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/undo_stack.h"

#include "conjure/core/api.h"
#include "conjure/core/clock.h"
#include "conjure/core/event.h"
#include "conjure/core/json/document.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The one execution path (docs/design/04_editor.md D4, D6, 3.3, 3.4): the scope of the client's token (D25),
// schema validation, preconditions, the handler, undo recording, revision bump, events and the audit record
// (D26), for every source of commands. While playing the undo stack is
// frozen: a scene edit is executed and not recorded, an ON_DISK command's inverse goes to the play controller,
// which makes one undo entry of them at play.stop (D9, D21). Asynchronous handlers park
// their request and answer through async_reply; resolve_completed() hands the finished responses back once per
// frame and times the stale ones out. An asynchronous command that changes the scene (ASYNC | MUTATES, code sweep
// F736) is recorded, dirties the scene, bumps the revision and emits scene.changed when it completes, not when it
// is dispatched; inside a batch, an undo or redo step or a rollback it must complete inside its handler
// (SYNC_IN_BATCH). Only a request left parked counts toward a client's cap of pending requests (BUSY beyond it):
// one that must complete inside its handler is never refused for it. Main thread only.

namespace conjure::editor {

    class editor_context;

    class CONJURE_API command_dispatcher {
        public:
            static constexpr u32 DEFAULT_MAX_PENDING_PER_CLIENT = 8;
            static constexpr f64 DEFAULT_ASYNC_TIMEOUT_SECONDS = 120.0;

            /// <summary>
            /// What a running editor.batch collects instead of the undo stack. fileCommands runs parallel to
            /// entry.undo: the name of the ON_DISK child that recorded the pair, "" for a scene edit, so a batch
            /// that ran while playing can hand its file edits to the play controller (D21).
            /// </summary>
            struct batch_state {
                undo_entry entry = {};
                std::vector<std::string> fileCommands = {};
                u32 depth = 0;
            };

        private:
            struct pending_request {
                core::json::document id = {};
                std::string command = "";
                core::json::document args = core::json::document::object();    // kept for MUTATES (the replay)
                u64 ticket = 0;
                u64 coalesceKey = 0;
                command_flags flags = command_flags::NONE;
                u32 clientId = 0;
                f64 deadlineSeconds = 0.0;
            };

            editor_context *mContext = nullptr;
            command_registry *mRegistry = nullptr;
            undo_stack *mUndo = nullptr;
            const core::clock *mClock = nullptr;
            std::vector<pending_request> mPending = {};
            std::vector<protocol_response> mCompleted = {};
            std::optional<protocol_response> mSynchronousResponse = std::nullopt;   // completed inside its handler
            batch_state *mBatch = nullptr;
            u64 mDispatchingTicket = 0;                     // the ASYNC ticket whose handler is running, 0 otherwise
            core::event<const protocol_response &> mOnLocalResponse = {};
            u64 mNextTicket = 1;
            u32 mMaxPendingPerClient = DEFAULT_MAX_PENDING_PER_CLIENT;
            u32 mSuppressRecording = 0;
            u32 mSuppressRevision = 0;                      // > 0 while history.undo / redo apply a step
            f64 mAsyncTimeoutSeconds = DEFAULT_ASYNC_TIMEOUT_SECONDS;

        public:
            command_dispatcher(editor_context &context, command_registry &registry, undo_stack &undo);
            ~command_dispatcher();

            command_dispatcher(const command_dispatcher &) = delete;
            command_dispatcher &operator=(const command_dispatcher &) = delete;

        public:
            /// <summary>
            /// Validates and executes one request. Returns the response, or nullopt when the command is
            /// asynchronous and will answer through resolve_completed() later. A known command the client's
            /// token does not open (required_access(flags) above what protocol_server::access_of_client
            /// reports) answers UNAUTHORISED with details {required, granted} before its arguments are
            /// validated; the client stays connected. Every request of a transport client, refused ones and
            /// unknown commands included, is handed to protocol_server::record_command once it was
            /// dispatched; so is every child of an editor.batch.
            /// </summary>
            [[nodiscard]] std::optional<protocol_response> dispatch(const protocol_request &request);

            /// <summary>
            /// UI convenience: dispatches from client 0 and returns the result object or the error (the
            /// error's code is the protocol code). An asynchronous command answers through on_local_response;
            /// when such a reply of client 0 fails, complete() also logs a WARN "ui: <command> failed: <message>".
            /// </summary>
            [[nodiscard]] core::result<core::json::document> execute(const command_request &request);

            /// <summary>
            /// Moves the responses of completed and timed-out asynchronous requests into outResponses.
            /// </summary>
            void resolve_completed(std::vector<protocol_response> &outResponses);

            /// <summary>
            /// Drops the pending requests of a disconnected client (CANCELLED, logged) and commits its
            /// open transaction.
            /// </summary>
            void cancel_client(u32 clientId);

            /// <summary>
            /// Completes every pending request with the given error (CANCELLED when the engine restarts or
            /// shuts down); the responses are delivered by the next resolve_completed().
            /// </summary>
            void cancel_all(protocol_error_code code, std::string_view message);

            [[nodiscard]] usize pending_count(u32 clientId) const noexcept;
            [[nodiscard]] usize pending_count() const noexcept {
                return mPending.size();
            }

            /// <summary>
            /// Completes a parked request (called by async_reply). A failure of client 0 (the UI, which does not
            /// wait for its answers) is logged as a WARN naming the command.
            /// </summary>
            void complete(u64 ticket, protocol_response response);

            /// <summary>
            /// Completes the parked request of an ASYNC | MUTATES command with its outcome (called by
            /// async_reply::succeed_mutation): records it the way a synchronous mutation is recorded at dispatch,
            /// with the request's args, client and coalesce key (the undo stack, the client's transaction, the
            /// running batch, or the play controller while frozen), marks the scene dirty unless NO_UNDO and,
            /// outside an undo or redo step, bumps the revision, sets result.revision and emits scene.changed.
            /// A completion inside the handler (a batch child, a replay) is recorded under the state it runs in.
            /// Asserted: the ticket is pending and its command is MUTATES.
            /// </summary>
            void complete_mutation(u64 ticket, command_outcome outcome);

            /// <summary>
            /// True while the ticket is parked and not completed.
            /// </summary>
            [[nodiscard]] bool is_pending(u64 ticket) const noexcept;

            /// <summary>
            /// Runs the children of editor.batch (docs 3.4): one undo entry, atomic rollback through the
            /// recorded inverses on a failure. Returns the children's responses; the error of the failing
            /// child with details.failedIndex when atomic.
            /// </summary>
            [[nodiscard]] core::result<std::vector<protocol_response>> run_batch(
              const core::json::value &commands,
              std::string_view label,
              bool atomic,
              u32 clientId,
              core::json::document &outDetails
            );

            /// <summary>
            /// Dispatches recorded inverses or replays with undo recording suppressed (history.undo/redo). The
            /// commands neither bump the revision nor emit scene.changed: the history command does both once per
            /// step.
            /// </summary>
            [[nodiscard]] core::result<void> dispatch_inverses(std::span<const command_inverse> inverses, u32 clientId);
            [[nodiscard]] core::result<void> dispatch_replays(std::span<const command_request> replays, u32 clientId);

            /// <summary>
            /// The most requests one client may leave parked. An ASYNC request beyond it answers BUSY, except one
            /// that must complete inside its handler (requires_synchronous_completion()), which cannot stay parked
            /// and is never refused. A SYNC_IN_BATCH request may also finish inside its handler elsewhere (a prefab
            /// that is already loaded), so it is counted only when its handler leaves it parked: the dispatcher
            /// answers BUSY then and drops its reply, and the handler checks exceeds_pending_limit() before it
            /// starts waiting, so it can refuse with nothing changed.
            /// </summary>
            void set_max_pending_per_client(u32 count) noexcept {
                mMaxPendingPerClient = count;
            }

            [[nodiscard]] u32 max_pending_per_client() const noexcept {
                return mMaxPendingPerClient;
            }

            /// <summary>
            /// True when the client has more parked requests than max_pending_per_client(). Inside an ASYNC
            /// handler the request being dispatched is parked already and counts.
            /// </summary>
            [[nodiscard]] bool exceeds_pending_limit(u32 clientId) const noexcept {
                return pending_count(clientId) > mMaxPendingPerClient;
            }

            void set_async_timeout_seconds(f64 seconds) noexcept {
                mAsyncTimeoutSeconds = seconds;
            }

            [[nodiscard]] f64 async_timeout_seconds() const noexcept {
                return mAsyncTimeoutSeconds;
            }

            /// <summary>
            /// The clock deadlines are measured with (the engine's clock by default).
            /// </summary>
            void set_clock(const core::clock *clock) noexcept {
                mClock = clock;
            }

            /// <summary>
            /// True while a batch runs (nested batches are refused).
            /// </summary>
            [[nodiscard]] bool in_batch() const noexcept {
                return mBatch != nullptr;
            }

            /// <summary>
            /// True while history.undo / redo or a batch rollback dispatch recorded inverses and replays. Handlers
            /// use it to accept values a client may not send but an earlier state legitimately held (a legacy
            /// entity name restored by undo).
            /// </summary>
            [[nodiscard]] bool replaying() const noexcept {
                return mSuppressRecording > 0;
            }

            /// <summary>
            /// True while a request dispatched now must answer before its handler returns: inside a batch (its
            /// children are recorded into one entry and rolled back together), an undo or redo step and a batch
            /// rollback (replaying()). A SYNC_IN_BATCH handler checks it and completes its reply synchronously; an
            /// ASYNC handler that does not is answered with INTERNAL and its reply is dropped.
            /// </summary>
            [[nodiscard]] bool requires_synchronous_completion() const noexcept {
                return mBatch != nullptr || mSuppressRecording > 0;
            }

            /// <summary>
            /// Responses of asynchronous commands issued by client 0 (the UI).
            /// </summary>
            [[nodiscard]] core::event<const protocol_response &> &on_local_response() noexcept {
                return mOnLocalResponse;
            }

        private:
            [[nodiscard]] std::optional<protocol_response> dispatch_internal(const protocol_request &request);
            [[nodiscard]] std::optional<protocol_error_code> check_preconditions(
              const command_descriptor &descriptor,
              core::json::document &details
            ) const;
            void record_outcome(command_flags flags, const protocol_request &request, command_outcome &outcome);
            [[nodiscard]] protocol_response conclude_mutation(
              command_flags flags,
              const protocol_request &request,
              command_outcome &outcome
            );
            void keep_play_edits(batch_state &batch);
            [[nodiscard]] access_level access_of_client(u32 clientId) const noexcept;
            void audit(
              const protocol_request &request,
              const std::optional<protocol_response> &response,
              bool batchChild
            ) const;
            [[nodiscard]] f64 now_seconds() const noexcept;
    };
}
