#pragma once

#include "conjure/editor/editor_options.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/protocol/access_token.h"
#include "conjure/editor/protocol/audit_log.h"
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
// (access, open transaction bookkeeping is the undo stack's), parses inbound lines into requests, serialises
// responses and events, applies the token handshake, the first-line rule, the MESSAGE_TOO_LARGE disconnect
// and the backlog policy (drop `log` events above 8 MiB, disconnect above 64 MiB), and keeps the audit log of
// the commands its clients had dispatched. Main thread only.
//
// Who must present a token (decisions D23 and D24 of 04_editor.md, code sweep F740 and F187): a TCP client
// always, because any program of the machine and any web page can reach a loopback port; a stdio or loopback
// client only when a token was set explicitly (--ai-token, --ai-inspect-token, set_token, set_inspect_token),
// because the first is the process that started the editor and the second runs inside it. A TCP transport
// never runs without tokens: they are generated when none was set. A client presents a token in
// session.hello, which must then be its first request.
//
// There are two tokens (decision D25): the full-control one opens every command, the inspect-only one the
// commands flagged READ_ONLY (required_access in editor_command.h is the rule, the dispatcher applies it).
// The server only records what a client was granted (client_state::access). The editor itself writes
// neither token into a log line, an event, an error message or an audit record (with the one limit
// audit_log.h states), and it checks the two places where a client's slip would otherwise be passed on to
// others: the client name of session.hello before it is logged, and the command name in command.executed
// (contains_token). What a client sends as ordinary data, an entity name say, is that client's to guard.
// A token is never held against text a client sent with an ordinary comparison or search, whose duration
// would tell how much of it the text had right: access_of and contains_token use the constant_time_
// functions of access_token.h, and so does the audit log.

namespace conjure::editor {

    struct client_state {
        u32 id = 0;
        transport_kind kind = transport_kind::LOCAL;
        usize transportIndex = 0;
        u64 droppedLogEvents = 0;                           // since the last events.dropped report
        access_level access = access_level::NONE;           // what the transport gave or the last hello granted
        bool connected = true;
        bool closing = false;                               // disconnect() was called; later lines are ignored
        bool spoke = false;                                 // a line of the client parsed as a request

        /// <summary>
        /// True once the client may send commands and receives events: at once on a transport that
        /// requires no token, after a session.hello with one of the two tokens otherwise.
        /// </summary>
        [[nodiscard]] bool authenticated() const noexcept {
            return access != access_level::NONE;
        }
    };

    class CONJURE_API protocol_server {
        public:
            static constexpr usize LOG_DROP_BACKLOG_BYTES = 8 * MIB;
            static constexpr usize DISCONNECT_BACKLOG_BYTES = 64 * MIB;
            static constexpr usize DEFAULT_REQUESTS_PER_FRAME = 256;

            /// <summary>
            /// The longest text contains_token() searches. A longer one is answered with true unsearched.
            /// </summary>
            static constexpr usize TOKEN_SEARCH_BYTES = KIB;

        private:
            event_broadcaster *mEvents = nullptr;
            std::vector<std::unique_ptr<protocol_transport>> mTransports = {};
            loopback_transport *mLocalLoopback = nullptr;   // owned by mTransports; created by local_loopback()
            std::vector<client_state> mClients = {};
            std::vector<protocol_request> mBacklog = {};    // parsed requests beyond the per-frame cap
            std::vector<u32> mClosedQueue = {};             // disconnected since the last take_closed()
            std::string mToken = "";                        // grants FULL_CONTROL; "" = nobody presents one
            std::string mInspectToken = "";                 // grants INSPECT; set whenever mToken is
            std::filesystem::path mSessionDirectory = {};   // where this session's files were written; empty = none
            std::filesystem::path mPortFile = {};
            std::filesystem::path mTokenFile = {};
            std::filesystem::path mInspectTokenFile = {};
            audit_log mAudit = {};
            u16 mBoundPort = 0;
            bool mTokenExplicit = false;                    // --ai-token / set_token gave mToken
            bool mInspectTokenExplicit = false;             // --ai-inspect-token / set_inspect_token gave it
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
            /// when the port was not explicit) and stdio with --ai-stdio. The tokens are options.aiToken and
            /// options.aiInspectToken; either one, when given, gates every transport. A token that was not
            /// given is generated (generate_access_token) as soon as one is needed, that is, when a TCP
            /// transport starts or the other token was given; tokens that are both generated gate the TCP
            /// transport alone. With a TCP transport and a project directory the session files are written
            /// as set_project_root describes, and the audit log follows the project directory whatever the
            /// transports. Fails with INVALID_ARGS, before anything starts, when the two options name the
            /// same token, and with IO_ERROR when an explicit port is busy or stdio cannot start. Without any
            /// transport the server runs for the local client.
            /// </summary>
            [[nodiscard]] core::result<void> start(
              const editor_options &options,
              const std::filesystem::path &projectRoot
            );

            /// <summary>
            /// Adds and starts a transport (tests add a loopback_transport). A TCP transport added while no
            /// token is set gets generated ones, as in start().
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
            /// at once), the token handshake is enforced, closed clients are unregistered. Three things
            /// disconnect a client and drop its later lines of the same poll: MESSAGE_TOO_LARGE, a failed
            /// handshake, and a first line that is not a request (PARSE_ERROR or INVALID_REQUEST before any
            /// line of the client parsed; on every transport, so that an HTTP request sent to the port by a
            /// web page ends at its request line and the JSON in its body is never read). At most maxRequests
            /// are returned; the rest wait for the next poll.
            /// </summary>
            void poll(std::vector<protocol_request> &outRequests, usize maxRequests = DEFAULT_REQUESTS_PER_FRAME);

            /// <summary>
            /// Queues a response for its client (clientId 0 is the local UI: nothing is sent).
            /// </summary>
            void send(const protocol_response &response);

            /// <summary>
            /// Writes queued bytes without blocking, applies the backlog policy and flushes the audit log.
            /// </summary>
            void flush();

            /// <summary>
            /// Disconnects every client, stops the transports, removes the session files (the port and the
            /// two tokens) and closes the audit log, which stays on disk. The tokens themselves are kept
            /// until the next start(), set_token() or set_inspect_token().
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
            /// Sets the full-control token explicitly, as --ai-token does: clients of every transport must
            /// then present a token in session.hello. An empty token returns to the default: a new token is
            /// generated if one is needed (a TCP transport runs, or the inspect token was set explicitly),
            /// and unless the inspect token was set explicitly stdio and loopback clients present nothing
            /// again. INVALID_ARGS, with nothing changed, when the token is not empty and equals the
            /// inspect token: one text cannot grant two levels. Clients that are connected keep the access
            /// they have. The token files of the session are written again with the tokens that now hold,
            /// also in a session whose port file could not be written.
            /// </summary>
            [[nodiscard]] core::result<void> set_token(std::string token);

            /// <summary>
            /// Sets the inspect-only token explicitly, as --ai-inspect-token does, with the rules of
            /// set_token: every transport is then gated, an empty token returns to a generated one (or to
            /// none when nothing needs a token), and a token equal to the full-control one is INVALID_ARGS.
            /// </summary>
            [[nodiscard]] core::result<void> set_inspect_token(std::string token);

            /// <summary>
            /// The token that grants FULL_CONTROL ("" when no client has to present one). For in-process
            /// clients such as the AI session panel; never log it or send it anywhere.
            /// </summary>
            [[nodiscard]] const std::string &token() const noexcept {
                return mToken;
            }

            /// <summary>
            /// The token that grants INSPECT ("" exactly when token() is). Never log it or send it anywhere.
            /// </summary>
            [[nodiscard]] const std::string &inspect_token() const noexcept {
                return mInspectToken;
            }

            /// <summary>
            /// True when a client of this kind has to present a token before anything else: TCP whenever
            /// tokens exist (always, once a TCP transport runs), stdio and loopback only when a token of
            /// either level was set explicitly.
            /// </summary>
            [[nodiscard]] bool requires_token(transport_kind kind) const noexcept;

            /// <summary>
            /// What a presented token grants: FULL_CONTROL for the full-control token, INSPECT for the
            /// inspect-only one, NONE for anything else and whenever the server has no tokens. Both
            /// comparisons are constant_time_equals and both always run.
            /// </summary>
            [[nodiscard]] access_level access_of(std::string_view presented) const noexcept;

            /// <summary>
            /// True when 'text' may hold one of the two tokens: it contains one, or it is longer than
            /// TOKEN_SEARCH_BYTES and is not searched. Text a client sent must be checked with this before
            /// it is logged or passed on to other clients, and withheld when the answer is true. The search
            /// is constant_time_contains for each token, both always run: how long it takes depends on the
            /// lengths of the text and of the tokens, not on how much of a token the text holds, so a client
            /// that may only look cannot time its way to the full-control token. Always false while the
            /// server has no tokens.
            /// </summary>
            [[nodiscard]] bool contains_token(std::string_view text) const noexcept;

            /// <summary>
            /// Decides the session.hello of a client that already passed the handshake (the command handler
            /// calls it); the last accepted hello decides what the client may do. On a transport that
            /// requires a token: false when 'presented' is neither token, and the client keeps the access
            /// it had; otherwise the client gets what the token grants, which lowers a full-control client
            /// that presents the inspect token and raises an inspect-only one that presents the other. On a
            /// transport that requires none: always true, and a client that presents one of the tokens all
            /// the same gets what it grants, so a client can restrict itself; any other text leaves it as it
            /// was. True for a client id no transport knows: an in-process client (an --exec file, the UI)
            /// is trusted with any token or none.
            /// </summary>
            [[nodiscard]] bool accept_hello(u32 clientId, std::string_view presented) noexcept;

            /// <summary>
            /// What the client may do right now: what its transport gave it or its last accepted
            /// session.hello granted. FULL_CONTROL for a client id no transport knows (client 0, an --exec
            /// run, an editor.exec_file task): in-process code is the editor itself.
            /// </summary>
            [[nodiscard]] access_level access_of_client(u32 clientId) const noexcept;

            /// <summary>
            /// Records one dispatched command in the audit log (the dispatcher calls it; 'entry' carries
            /// the command, its arguments and its outcome, the server adds the client's transport and
            /// access). Only commands of transport clients are recorded: a client id no transport knows is
            /// the editor's own code, and nothing is written for it. Nothing is written either while no
            /// project is open, because the log lives in the project.
            /// </summary>
            void record_command(audit_entry entry);

            /// <summary>
            /// The audit log, for its file names and its size limit.
            /// </summary>
            [[nodiscard]] audit_log &audit() noexcept {
                return mAudit;
            }

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
            /// The file the full-control token was written to (empty when none: no TCP transport, no
            /// project, or a volume that cannot restrict a file to its owner).
            /// </summary>
            [[nodiscard]] const std::filesystem::path &token_file() const noexcept {
                return mTokenFile;
            }

            /// <summary>
            /// The file the inspect-only token was written to (empty when none, as for token_file()).
            /// </summary>
            [[nodiscard]] const std::filesystem::path &inspect_token_file() const noexcept {
                return mInspectTokenFile;
            }

            /// <summary>
            /// Follows a project switch: removes the current session files and, while a TCP transport is
            /// bound (bound_port() != 0), writes three files under &lt;projectRoot&gt;/.conjure: first
            /// ai_token and ai_token_inspect, each a token and a newline, through
            /// core::vfs::write_private_file so that only the user running the editor can read them, then
            /// ai_port, the port and a newline. A client that finds ai_port therefore finds the tokens
            /// beside it. When a token file cannot be written (a FAT or exFAT volume keeps no owner-only
            /// files, or something is in the way) a WARN says so, its accessor is empty, a file of that
            /// name left by an earlier session is removed if it can be, and TCP clients need a token given
            /// with --ai-token or --ai-inspect-token. An empty root only removes the files. The files are
            /// not shared: a second editor on the same project overwrites them, and whichever editor stops
            /// first removes them. The audit log moves with the project as well, whatever the transports:
            /// later commands are recorded under &lt;projectRoot&gt;/.conjure, or nowhere for an empty root.
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
            [[nodiscard]] bool has_tcp_transport() const noexcept;
            [[nodiscard]] bool gates_every_transport() const noexcept;
            void ensure_tokens();
            void write_session_files(const std::filesystem::path &projectRoot);
            void rewrite_token_files();
            void write_token_files(const std::filesystem::path &directory);
            [[nodiscard]] std::filesystem::path write_token_file(
              const std::filesystem::path &directory,
              access_level level
            );
            void remove_session_files();
    };
}
