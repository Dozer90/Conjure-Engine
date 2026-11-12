#pragma once

#include "conjure/editor/editor_options.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/protocol/loopback_transport.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/protocol/protocol_transport.h"

#include "conjure/core/api.h"
#include "conjure/core/log.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// The protocol server (docs/design/04_editor.md 2.4, 3.2, 3.12, 6): owns the transports, the per-client state
// (authentication, open transaction bookkeeping is the undo stack's), parses inbound lines into requests,
// serialises responses and events, applies the token handshake, the MESSAGE_TOO_LARGE disconnect and the
// backlog policy (drop `log` events above 8 MiB, disconnect above 64 MiB). Main thread only.

namespace conjure::editor {

    struct client_state {
        u32 id = 0;
        transport_kind kind = transport_kind::LOCAL;
        usize transportIndex = 0;
        u64 droppedLogEvents = 0;                           // since the last events.dropped report
        bool authenticated = false;
        bool connected = true;
        bool closing = false;                               // disconnect() was called; later lines are ignored
    };

    class CONJURE_API protocol_server {
        public:
            static constexpr usize LOG_DROP_BACKLOG_BYTES = 8 * MIB;
            static constexpr usize DISCONNECT_BACKLOG_BYTES = 64 * MIB;
            static constexpr usize DEFAULT_REQUESTS_PER_FRAME = 256;

        private:
            event_broadcaster *mEvents = nullptr;
            std::vector<std::unique_ptr<protocol_transport>> mTransports = {};
            loopback_transport *mLocalLoopback = nullptr;   // owned by mTransports; created by local_loopback()
            std::vector<client_state> mClients = {};
            std::vector<protocol_request> mBacklog = {};    // parsed requests beyond the per-frame cap
            std::vector<u32> mClosedQueue = {};             // disconnected since the last take_closed()
            std::string mToken = "";
            std::filesystem::path mPortFile = {};
            u16 mBoundPort = 0;
            bool mRunning = false;
            bool mStdioClosed = false;                      // a stdio transport reported its client closed

        public:
            explicit protocol_server(event_broadcaster &events);
            ~protocol_server();

            protocol_server(const protocol_server &) = delete;
            protocol_server &operator=(const protocol_server &) = delete;

        public:
            /// <summary>
            /// Starts the transports the options ask for: TCP on options.first_port() (nine fallback ports
            /// when the port was not explicit; the chosen port is written to &lt;project&gt;/.conjure/ai_port when a
            /// project directory is given) and stdio with --ai-stdio. Fails with IO_ERROR when an explicit
            /// port is busy or stdio cannot start. Without any transport the server runs for the local client.
            /// </summary>
            [[nodiscard]] core::result<void> start(
              const editor_options &options,
              const std::filesystem::path &projectRoot
            );

            /// <summary>
            /// Adds and starts a transport (tests add a loopback_transport).
            /// </summary>
            [[nodiscard]] core::result<void> add_transport(std::unique_ptr<protocol_transport> transport);

            /// <summary>
            /// The one loopback transport in-process clients connect through (the AI session panel): created
            /// and added on the first call, the same instance afterwards, so reconnecting clients never
            /// accumulate transports. stop() releases it with the other transports.
            /// </summary>
            [[nodiscard]] loopback_transport &local_loopback();

            /// <summary>
            /// Transports currently polled.
            /// </summary>
            [[nodiscard]] usize transport_count() const noexcept {
                return mTransports.size();
            }

            /// <summary>
            /// Accepts, reads and parses: new clients are registered with the broadcaster (client.connected is
            /// emitted to the others), lines become requests (PARSE_ERROR / INVALID_REQUEST responses are sent
            /// at once, MESSAGE_TOO_LARGE and a failed handshake disconnect and drop the client's later lines
            /// of the same poll), the token handshake is enforced, closed clients are unregistered. At most
            /// maxRequests are returned; the rest wait for the next poll.
            /// </summary>
            void poll(std::vector<protocol_request> &outRequests, usize maxRequests = DEFAULT_REQUESTS_PER_FRAME);

            /// <summary>
            /// Queues a response for its client (clientId 0 is the local UI: nothing is sent).
            /// </summary>
            void send(const protocol_response &response);

            /// <summary>
            /// Writes queued bytes without blocking and applies the backlog policy.
            /// </summary>
            void flush();

            /// <summary>
            /// Disconnects every client, stops the transports and removes the port file.
            /// </summary>
            void stop();

            /// <summary>
            /// Disconnects one client (its pending replies are the dispatcher's to cancel).
            /// </summary>
            void disconnect(u32 clientId);

            /// <summary>
            /// Moves the ids of the clients that disconnected since the last call into 'out' (the dispatcher
            /// cancels their pending requests).
            /// </summary>
            void take_closed(std::vector<u32> &out);

            /// <summary>
            /// The TCP port in use, 0 without a TCP transport.
            /// </summary>
            [[nodiscard]] u16 bound_port() const noexcept {
                return mBoundPort;
            }

            [[nodiscard]] usize client_count() const noexcept;

            [[nodiscard]] const client_state *client(u32 clientId) const noexcept;

            /// <summary>
            /// The token clients must present in session.hello ("" = no handshake required).
            /// </summary>
            void set_token(std::string token);

            [[nodiscard]] const std::string &token() const noexcept {
                return mToken;
            }

            /// <summary>
            /// Marks a client authenticated (session.hello with the right token).
            /// </summary>
            void mark_authenticated(u32 clientId);

            [[nodiscard]] bool is_running() const noexcept {
                return mRunning;
            }

            /// <summary>
            /// Requests parsed but held back by the per-frame cap.
            /// </summary>
            [[nodiscard]] usize backlog_count() const noexcept {
                return mBacklog.size();
            }

            /// <summary>
            /// The file the bound port was written to (empty when none).
            /// </summary>
            [[nodiscard]] const std::filesystem::path &port_file() const noexcept {
                return mPortFile;
            }

            /// <summary>
            /// Follows a project switch: removes the current port file and, while a TCP transport is bound
            /// (bound_port() != 0), writes the port to &lt;projectRoot&gt;/.conjure/ai_port. An empty root only
            /// removes the file.
            /// </summary>
            void set_project_root(const std::filesystem::path &projectRoot);

            /// <summary>
            /// True once a stdio transport reported its client closed (standard input ended, or the client was
            /// disconnected). A headless --ai-stdio editor without a TCP server quits on it.
            /// </summary>
            [[nodiscard]] bool stdio_closed() const noexcept {
                return mStdioClosed;
            }

        private:
            [[nodiscard]] client_state *find(u32 clientId) noexcept;
            [[nodiscard]] protocol_transport *transport_of(const client_state &client) noexcept;
            void send_line(u32 clientId, std::string_view line);
            void on_client_opened(usize transportIndex, u32 clientId);
            void on_client_closed(u32 clientId);
            void deliver_event(u32 clientId, const protocol_event &event);
            [[nodiscard]] bool check_handshake(client_state &client, const protocol_request &request);
            void write_port_file(const std::filesystem::path &projectRoot);
            void remove_port_file();
    };
}
