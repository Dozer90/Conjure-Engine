#pragma once

#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// A client of the editor's AI protocol for tests (docs/design/08_testing.md 4.6, 04_editor.md 2.3): one
// request per send(), newline-delimited JSON on the wire, the response matched by its echoed id, every
// streamed event buffered on the side.
//
//   protocol_client client(std::make_unique<scripted_protocol_channel>(handler));
//   core::result<core::json::document> reply = client.send("entity.create", json::object({{"name", "Player"}}));
//   CHECK(protocol_client::response_ok(reply.value()));
//
// The client speaks through a protocol_channel, the client end of a connection: the in-memory
// scripted_protocol_channel below answers from a handler (fake servers, and the adapter an in-process
// editor_core session plugs its loopback transport into), a TCP channel is what connect_tcp of the editor
// tests supplies. A response for an unknown id is dropped with a WARN; a line that is not JSON is a
// PARSE_FAILURE of the transport, since the server only ever writes JSON.

namespace conjure::testing {

    /// <summary>
    /// The client end of one protocol connection: whole lines in, whole lines out.
    /// </summary>
    class protocol_channel {
        public:
            protocol_channel();
            virtual ~protocol_channel();

            protocol_channel(const protocol_channel &) = delete;
            protocol_channel &operator=(const protocol_channel &) = delete;

        public:
            /// <summary>
            /// Writes one message (the newline is the channel's business). INVALID_STATE when closed.
            /// </summary>
            [[nodiscard]] virtual core::result<void> send_line(std::string_view line) = 0;

            /// <summary>
            /// The next inbound message; an implementation pumps the server it is connected to while nothing
            /// is queued. INVALID_STATE when the channel is closed and nothing is queued, TIMEOUT when the
            /// server stops answering.
            /// </summary>
            [[nodiscard]] virtual core::result<std::string> receive_line() = 0;

            [[nodiscard]] virtual bool is_open() const noexcept = 0;

            /// <summary>
            /// Ends the connection; later sends fail and receives drain what is queued, then fail.
            /// </summary>
            virtual void close() = 0;
    };

    /// <summary>
    /// An in-memory channel whose server is a handler: every sent line is handed to it together with an
    /// output list it may append response and event lines to. Lines may also be queued from outside
    /// (unsolicited events). Everything happens on the calling thread; a handler may send through the
    /// channel again (its own lines are queued first, then the outer call's).
    /// </summary>
    class scripted_protocol_channel final : public protocol_channel {
        public:
            using handler = std::function<void(std::string_view line, std::vector<std::string> &outLines)>;

        private:
            handler mHandler = {};
            std::deque<std::string> mInbound = {};
            std::vector<std::string> mSent = {};
            bool mOpen = true;

        public:
            explicit scripted_protocol_channel(handler serverHandler);
            ~scripted_protocol_channel() override;

        public:
            [[nodiscard]] core::result<void> send_line(std::string_view line) override;
            [[nodiscard]] core::result<std::string> receive_line() override;
            [[nodiscard]] bool is_open() const noexcept override;
            void close() override;

            /// <summary>
            /// Queues a line the client will receive before anything the handler produces later.
            /// </summary>
            void inject_line(std::string_view line);

            /// <summary>
            /// Every line sent through the channel, in order.
            /// </summary>
            [[nodiscard]] std::span<const std::string> sent_lines() const noexcept {
                return mSent;
            }

            /// <summary>
            /// Lines queued for the client that it has not received yet.
            /// </summary>
            [[nodiscard]] usize pending_count() const noexcept {
                return mInbound.size();
            }
    };

    class protocol_client {
        public:
            /// <summary>
            /// Inbound lines one send() consumes before giving up on a response (a server that only streams
            /// events, or never answers, becomes a TIMEOUT instead of a hang).
            /// </summary>
            static constexpr u32 MAX_LINES_PER_REQUEST = 100000;

        private:
            std::unique_ptr<protocol_channel> mChannel = nullptr;
            std::vector<core::json::document> mEvents = {};
            u64 mNextId = 1;
            u64 mLastRequestId = 0;
            u64 mRequestCount = 0;
            u64 mDroppedResponses = 0;

        public:
            explicit protocol_client(std::unique_ptr<protocol_channel> channel);
            ~protocol_client();

            protocol_client(const protocol_client &) = delete;
            protocol_client &operator=(const protocol_client &) = delete;
            protocol_client(protocol_client &&) noexcept = default;
            protocol_client &operator=(protocol_client &&) noexcept = default;

        public:
            /// <summary>
            /// Sends {"id", "cmd", "args"} and blocks until the response with that id arrives; events seen
            /// meanwhile are buffered. The response document is returned whether it says ok or not; the
            /// result is an error only when the transport fails (INVALID_STATE closed, TIMEOUT, PARSE_FAILURE).
            /// </summary>
            [[nodiscard]] core::result<core::json::document> send(
              std::string_view command,
              const core::json::document &params
            );

            /// <summary>
            /// send() with an empty args object.
            /// </summary>
            [[nodiscard]] core::result<core::json::document> send(std::string_view command);

            /// <summary>
            /// Sends the text verbatim (one line, no framing added) and returns the first response the
            /// server writes back, whatever its id: how a garbage line is proven to yield an error document.
            /// </summary>
            [[nodiscard]] core::result<core::json::document> send_raw(std::string_view rawMessage);

            /// <summary>
            /// Events received so far, oldest first.
            /// </summary>
            [[nodiscard]] std::span<const core::json::document> events() const noexcept {
                return mEvents;
            }

            void clear_events();

            /// <summary>
            /// Receives one line without sending anything: an event is buffered and nullopt is returned, a
            /// response is returned. Used to collect events a server pushes between requests; fails like
            /// send() when the channel is closed or has nothing queued.
            /// </summary>
            [[nodiscard]] core::result<std::optional<core::json::document>> receive();

            [[nodiscard]] bool is_connected() const noexcept;

            /// <summary>
            /// Closes the channel; every later send fails with INVALID_STATE.
            /// </summary>
            void disconnect();

            /// <summary>
            /// The id the last send() used (0 before the first).
            /// </summary>
            [[nodiscard]] u64 last_request_id() const noexcept {
                return mLastRequestId;
            }

            [[nodiscard]] u64 request_count() const noexcept {
                return mRequestCount;
            }

            /// <summary>
            /// Responses whose id matched no outstanding request and were dropped.
            /// </summary>
            [[nodiscard]] u64 dropped_response_count() const noexcept {
                return mDroppedResponses;
            }

            /// <summary>
            /// True when the message is an event ({"event": ...}) rather than a response.
            /// </summary>
            [[nodiscard]] static bool is_event(const core::json::value &message) noexcept;

            /// <summary>
            /// The "ok" member of a response (false when absent).
            /// </summary>
            [[nodiscard]] static bool response_ok(const core::json::document &response) noexcept;

            /// <summary>
            /// The "error.code" string of a failed response ("" when absent).
            /// </summary>
            [[nodiscard]] static std::string_view error_code_of(const core::json::document &response) noexcept;

            /// <summary>
            /// The "error.message" string of a failed response ("" when absent).
            /// </summary>
            [[nodiscard]] static std::string_view error_message_of(const core::json::document &response) noexcept;

        private:
            [[nodiscard]] core::result<core::json::document> await_response(u64 id, bool anyId);
    };
}
