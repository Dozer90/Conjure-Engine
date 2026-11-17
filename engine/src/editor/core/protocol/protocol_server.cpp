#include "conjure/editor/protocol/protocol_server.h"

#include "editor/core/protocol/stdio_transport.h"
#include "editor/core/protocol/tcp_transport.h"

#include "conjure/editor/editor_error.h"
#include "conjure/editor/log_category.h"

#include "conjure/core/vfs/native_file_system.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <fstream>
#include <iostream>
#include <span>
#include <system_error>
#include <utility>

namespace conjure::editor {

    namespace {
        constexpr std::string_view SESSION_FILE_DIRECTORY = ".conjure";
        constexpr std::string_view PORT_FILE_NAME = "ai_port";

        /// <summary>
        /// The id an INVALID_REQUEST answer echoes (docs/design/04_editor.md D7): the integer or string "id" of a
        /// line that is a JSON object, null otherwise. Only failed lines reach this, so the second parse is rare.
        /// </summary>
        [[nodiscard]] core::json::document usable_id_of(std::string_view line) {
            const core::result<core::json::document> parsed = core::json::document::parse(line);
            if (!parsed.has_value() || !parsed.value().root().is_object()) {
                return null_id();
            }
            const core::json::value id = parsed.value().root()["id"];
            if (!id.is_integer() && !id.is_string()) {
                return null_id();
            }
            return core::json::document::from_value(id);
        }

        /// <summary>
        /// The folder of a project that holds the session files and the audit log; empty for an empty root.
        /// </summary>
        [[nodiscard]] std::filesystem::path session_directory(const std::filesystem::path &projectRoot) {
            if (projectRoot.empty()) {
                return {};
            }
            return projectRoot / std::filesystem::path(SESSION_FILE_DIRECTORY);
        }
    }

    protocol_server::protocol_server(event_broadcaster &events) :
        mEvents(&events) {}

    protocol_server::~protocol_server() {
        stop();
    }

    core::result<void> protocol_server::start(const editor_options &options, const std::filesystem::path &projectRoot) {
        if (!options.aiToken.empty() && options.aiToken == options.aiInspectToken) {
            return make_editor_error(
              protocol_error_code::INVALID_ARGS,
              "the full-control token and the inspect-only token must differ"
            );
        }
        mToken = options.aiToken;
        mTokenExplicit = !mToken.empty();
        mInspectToken = options.aiInspectToken;
        mInspectTokenExplicit = !mInspectToken.empty();
        // Tokens exist from here on when one was given, or when add_transport() brought a TCP transport in
        // earlier: that transport must not lose its gate to an empty option.
        ensure_tokens();
        mAudit.set_directory(session_directory(projectRoot));
        if (options.wants_tcp_server()) {
            const u16 fallbacks = options.port_is_explicit() ? u16{0} : AI_PORT_FALLBACK_COUNT;
            std::unique_ptr<tcp_transport> tcp = std::make_unique<tcp_transport>(options.first_port(), fallbacks);
            tcp->set_client_id_allocator([this]() {
                return mEvents->register_client({});
            });
            const core::result<void> started = tcp->start();
            if (!started.has_value()) {
                return started;
            }
            mBoundPort = tcp->bound_port();
            CONJURE_LOG_INFO(LOG_EDITOR, "ai protocol: listening on 127.0.0.1:{}", mBoundPort);
            mTransports.push_back(std::move(tcp));
            ensure_tokens();
            write_session_files(projectRoot);
            if (projectRoot.empty() && !gates_every_transport()) {
                // The generated tokens are known to nobody yet. With a window the user opens a project next
                // and the files follow (set_project_root); headless, only another transport can open one.
                const std::string_view advice =
                  "ai protocol: no project is open, so the token files are not written and no TCP client can "
                  "authenticate until a project is opened; start with --project, --ai-token or "
                  "--ai-inspect-token";
                if (options.headless) {
                    CONJURE_LOG_WARN(LOG_EDITOR, "{}", advice);
                } else {
                    CONJURE_LOG_INFO(LOG_EDITOR, "{}", advice);
                }
            }
        }
        if (options.aiStdio) {
            std::unique_ptr<stdio_transport> stdio = std::make_unique<stdio_transport>(std::cin, std::cout, true);
            stdio->set_client_id_allocator([this]() {
                return mEvents->register_client({});
            });
            CONJURE_TRY(stdio->start());
            mTransports.push_back(std::move(stdio));
        }
        mRunning = true;
        return core::result<void>::success();
    }

    core::result<void> protocol_server::add_transport(std::unique_ptr<protocol_transport> transport) {
        if (transport == nullptr) {
            return make_editor_error(protocol_error_code::INVALID_ARGS, "add_transport: null transport");
        }
        transport->set_client_id_allocator([this]() {
            return mEvents->register_client({});
        });
        CONJURE_TRY(transport->start());
        const bool tcpTransport = transport->kind() == transport_kind::TCP;
        if (tcpTransport) {
            const tcp_transport *tcp = static_cast<const tcp_transport *>(transport.get());
            mBoundPort = tcp->bound_port();
        }
        mTransports.push_back(std::move(transport));
        if (tcpTransport) {
            ensure_tokens();
        }
        mRunning = true;
        return core::result<void>::success();
    }

    loopback_transport &protocol_server::local_loopback() {
        if (mLocalLoopback == nullptr) {
            std::unique_ptr<loopback_transport> transport = std::make_unique<loopback_transport>();
            mLocalLoopback = transport.get();
            // A loopback transport's start() cannot fail.
            (void) add_transport(std::move(transport));
        }
        return *mLocalLoopback;
    }

    void protocol_server::poll(std::vector<protocol_request> &outRequests, usize maxRequests) {
        std::vector<u32> opened = {};
        std::vector<inbound_line> lines = {};
        std::vector<u32> closed = {};
        for (usize index = 0; index < mTransports.size(); ++index) {
            opened.clear();
            lines.clear();
            closed.clear();
            mTransports[index]->poll(opened, lines, closed);
            for (const u32 id : opened) {
                on_client_opened(index, id);
            }
            for (inbound_line &line : lines) {
                client_state *client = find(line.clientId);
                if (client == nullptr || !client->connected || client->closing) {
                    // Nothing after a disconnect (oversized line, failed handshake) is parsed or executed.
                    continue;
                }
                if (line.oversized) {
                    send_line(
                      line.clientId,
                      serialise(make_error_response(
                        null_id().root(),
                        protocol_error_code::MESSAGE_TOO_LARGE,
                        std::format("a line exceeded the limit of {} bytes", MAX_MESSAGE_BYTES)
                      ))
                    );
                    disconnect(line.clientId);
                    continue;
                }
                core::result<protocol_request> parsed = parse_request(line.text);
                if (!parsed.has_value()) {
                    const protocol_error_code code = classify_error(parsed.get_error());
                    // A line that is not JSON (or too long) has no id to echo; a malformed request may have one.
                    const core::json::document id =
                      code == protocol_error_code::INVALID_REQUEST ? usable_id_of(line.text) : null_id();
                    send_line(
                      line.clientId,
                      serialise(make_error_response(id.root(), code, parsed.get_error().message()))
                    );
                    if (code == protocol_error_code::MESSAGE_TOO_LARGE) {
                        disconnect(line.clientId);
                    } else if (!client->spoke) {
                        // Whoever opens with something else than a request is not a protocol client. The case
                        // that matters is a web page posting to the port: its first line is the HTTP request
                        // line, and the JSON it smuggled into the body must never be read (code sweep F187).
                        // The line is not logged: it may be a mistyped session.hello carrying the token.
                        CONJURE_LOG_INFO(
                          LOG_EDITOR,
                          "ai protocol: client {} did not open with a request; disconnecting",
                          line.clientId
                        );
                        disconnect(line.clientId);
                    }
                    continue;
                }
                client->spoke = true;
                protocol_request request = std::move(parsed).value();
                request.clientId = line.clientId;
                if (!check_handshake(*client, request)) {
                    continue;
                }
                mBacklog.push_back(std::move(request));
            }
            for (const u32 id : closed) {
                on_client_closed(id);
            }
            if (!closed.empty() && mTransports[index]->kind() == transport_kind::STDIO) {
                mStdioClosed = true;
            }
        }
        const usize count = std::min(maxRequests, mBacklog.size());
        for (usize index = 0; index < count; ++index) {
            outRequests.push_back(std::move(mBacklog[index]));
        }
        mBacklog.erase(mBacklog.begin(), mBacklog.begin() + static_cast<isize>(count));
    }

    void protocol_server::send(const protocol_response &response) {
        if (response.clientId == 0) {
            return;
        }
        send_line(response.clientId, serialise(response));
    }

    void protocol_server::flush() {
        for (const std::unique_ptr<protocol_transport> &transport : mTransports) {
            transport->flush();
        }
        for (client_state &client : mClients) {
            if (!client.connected) {
                continue;
            }
            protocol_transport *transport = transport_of(client);
            if (transport == nullptr) {
                continue;
            }
            const usize pending = transport->pending_bytes(client.id);
            if (pending > DISCONNECT_BACKLOG_BYTES) {
                CONJURE_LOG_WARN(
                  LOG_EDITOR,
                  "ai protocol: client {} has {} bytes queued; disconnecting",
                  client.id,
                  pending
                );
                disconnect(client.id);
                continue;
            }
            if (client.droppedLogEvents > 0 && pending < LOG_DROP_BACKLOG_BYTES) {
                const u64 dropped = client.droppedLogEvents;
                client.droppedLogEvents = 0;
                mEvents->emit_to(client.id, editor_event::EVENTS_DROPPED, core::json::object({{"count", dropped}}));
            }
        }
        mAudit.flush();
    }

    void protocol_server::stop() {
        for (client_state &client : mClients) {
            if (client.connected) {
                client.connected = false;
                mEvents->unregister_client(client.id);
            }
        }
        mClients.clear();
        mBacklog.clear();
        for (const std::unique_ptr<protocol_transport> &transport : mTransports) {
            transport->stop();
        }
        mTransports.clear();
        mLocalLoopback = nullptr;
        remove_session_files();
        mAudit.set_directory({});
        mBoundPort = 0;
        mRunning = false;
        mStdioClosed = false;
    }

    void protocol_server::set_project_root(const std::filesystem::path &projectRoot) {
        remove_session_files();
        write_session_files(projectRoot);
        mAudit.set_directory(session_directory(projectRoot));
    }

    void protocol_server::disconnect(u32 clientId) {
        client_state *client = find(clientId);
        if (client == nullptr || !client->connected) {
            return;
        }
        client->closing = true;
        protocol_transport *transport = transport_of(*client);
        if (transport != nullptr) {
            transport->disconnect(clientId);
        }
    }

    usize protocol_server::client_count() const noexcept {
        usize count = 0;
        for (const client_state &client : mClients) {
            if (client.connected) {
                ++count;
            }
        }
        return count;
    }

    const client_state *protocol_server::client(u32 clientId) const noexcept {
        for (const client_state &entry : mClients) {
            if (entry.id == clientId) {
                return &entry;
            }
        }
        return nullptr;
    }

    core::result<void> protocol_server::set_token(std::string token) {
        if (!token.empty() && token == mInspectToken) {
            return make_editor_error(
              protocol_error_code::INVALID_ARGS,
              "set_token: the full-control token must differ from the inspect-only token"
            );
        }
        mToken = std::move(token);
        mTokenExplicit = !mToken.empty();
        ensure_tokens();
        rewrite_token_files();
        return core::result<void>::success();
    }

    core::result<void> protocol_server::set_inspect_token(std::string token) {
        if (!token.empty() && token == mToken) {
            return make_editor_error(
              protocol_error_code::INVALID_ARGS,
              "set_inspect_token: the inspect-only token must differ from the full-control token"
            );
        }
        mInspectToken = std::move(token);
        mInspectTokenExplicit = !mInspectToken.empty();
        ensure_tokens();
        rewrite_token_files();
        return core::result<void>::success();
    }

    bool protocol_server::requires_token(transport_kind kind) const noexcept {
        if (mToken.empty()) {
            return false;
        }
        return kind == transport_kind::TCP || gates_every_transport();
    }

    access_level protocol_server::access_of(std::string_view presented) const noexcept {
        if (mToken.empty()) {
            // Without tokens nothing can be presented that grants anything: "" must not match "".
            return access_level::NONE;
        }
        // Both comparisons run whatever the first one says, so the time taken does not tell which token a
        // guess was closer to.
        const bool fullControl = constant_time_equals(presented, mToken);
        const bool inspect = !mInspectToken.empty() && constant_time_equals(presented, mInspectToken);
        if (fullControl) {
            return access_level::FULL_CONTROL;
        }
        return inspect ? access_level::INSPECT : access_level::NONE;
    }

    bool protocol_server::contains_token(std::string_view text) const noexcept {
        if (mToken.empty()) {
            return false;
        }
        if (text.size() > TOKEN_SEARCH_BYTES) {
            // Not searched: the search costs the text's length times the token's, and the text is a client's.
            return true;
        }
        // Not a substring search, which takes longer the more of a token the text holds: an inspect-only
        // client could time that and work out the full-control token. Both searches always run.
        const bool fullControl = constant_time_contains(text, mToken);
        const bool inspect = constant_time_contains(text, mInspectToken);
        return fullControl || inspect;
    }

    bool protocol_server::accept_hello(u32 clientId, std::string_view presented) noexcept {
        client_state *entry = find(clientId);
        if (entry == nullptr) {
            return true;
        }
        const access_level granted = access_of(presented);
        if (granted != access_level::NONE) {
            entry->access = granted;
            return true;
        }
        return !requires_token(entry->kind);
    }

    access_level protocol_server::access_of_client(u32 clientId) const noexcept {
        const client_state *entry = client(clientId);
        return entry == nullptr ? access_level::FULL_CONTROL : entry->access;
    }

    void protocol_server::record_command(audit_entry entry) {
        const client_state *sender = client(entry.clientId);
        if (sender == nullptr) {
            return;
        }
        entry.transport = sender->kind;
        entry.access = sender->access;
        const std::array<std::string_view, 2> secrets = {mToken, mInspectToken};
        mAudit.record(entry, secrets);
    }

    client_state *protocol_server::find(u32 clientId) noexcept {
        for (client_state &entry : mClients) {
            if (entry.id == clientId) {
                return &entry;
            }
        }
        return nullptr;
    }

    protocol_transport *protocol_server::transport_of(const client_state &client) noexcept {
        if (client.transportIndex >= mTransports.size()) {
            return nullptr;
        }
        return mTransports[client.transportIndex].get();
    }

    void protocol_server::send_line(u32 clientId, std::string_view line) {
        client_state *client = find(clientId);
        if (client == nullptr || !client->connected) {
            return;
        }
        protocol_transport *transport = transport_of(*client);
        if (transport != nullptr) {
            transport->send(clientId, line);
        }
    }

    void protocol_server::on_client_opened(usize transportIndex, u32 clientId) {
        client_state client = {};
        client.id = clientId;
        client.transportIndex = transportIndex;
        client.kind = mTransports[transportIndex]->kind();
        client.access = requires_token(client.kind) ? access_level::NONE : access_level::FULL_CONTROL;
        client.connected = true;
        mClients.push_back(client);
        // The allocator registered the client without a sink; install the serialising one now.
        mEvents->unregister_client(clientId);
        mEvents->register_client(clientId, [this, clientId](const protocol_event &event) {
            deliver_event(clientId, event);
        });
        CONJURE_LOG_INFO(LOG_EDITOR, "ai protocol: client {} connected ({})", clientId, to_string(client.kind));
        mEvents->emit(
          editor_event::CLIENT_CONNECTED,
          core::json::object({{"clientId", clientId}, {"transport", to_string(client.kind)}})
        );
    }

    void protocol_server::on_client_closed(u32 clientId) {
        client_state *client = find(clientId);
        if (client == nullptr || !client->connected) {
            return;
        }
        client->connected = false;
        mEvents->unregister_client(clientId);
        std::erase_if(mClients, [clientId](const client_state &entry) {
            return entry.id == clientId;
        });
        std::erase_if(mBacklog, [clientId](const protocol_request &request) {
            return request.clientId == clientId;
        });
        mClosedQueue.push_back(clientId);
        CONJURE_LOG_INFO(LOG_EDITOR, "ai protocol: client {} disconnected", clientId);
        mEvents->emit(editor_event::CLIENT_DISCONNECTED, core::json::object({{"clientId", clientId}}));
    }

    void protocol_server::take_closed(std::vector<u32> &out) {
        for (const u32 id : mClosedQueue) {
            out.push_back(id);
        }
        mClosedQueue.clear();
    }

    void protocol_server::deliver_event(u32 clientId, const protocol_event &event) {
        client_state *client = find(clientId);
        if (client == nullptr || !client->connected) {
            return;
        }
        if (!client->authenticated()) {
            // Where a token is required nothing, not even an event, reaches a client before its valid
            // session.hello.
            return;
        }
        protocol_transport *transport = transport_of(*client);
        if (transport == nullptr) {
            return;
        }
        if (event.name == "log" && transport->pending_bytes(clientId) > LOG_DROP_BACKLOG_BYTES) {
            ++client->droppedLogEvents;
            return;
        }
        transport->send(clientId, serialise(event));
    }

    bool protocol_server::check_handshake(client_state &client, const protocol_request &request) {
        if (client.authenticated()) {
            return true;
        }
        if (request.command == "session.hello") {
            client.access = access_of(request.args.root()["token"].as_string());
        }
        if (client.authenticated()) {
            return true;
        }
        // The message names where the tokens are, never a token, and not what was presented either.
        send_line(
          client.id,
          serialise(make_error_response(
            request.id.root(),
            protocol_error_code::UNAUTHORISED,
            "the first request must be session.hello with one of the editor's tokens (args.token); the editor "
            "writes them to <project>/.conjure/ai_token (full control) and ai_token_inspect (inspect only)"
          ))
        );
        // The audit log leaves refusals out (04 D26), so this line is the only trace of the refusal that matters
        // most against a local program or a web page. It names neither the presented text nor the command name:
        // either could hold a token.
        CONJURE_LOG_INFO(
          LOG_EDITOR,
          "ai protocol: client {} did not open with session.hello and a valid token; disconnecting",
          client.id
        );
        disconnect(client.id);
        return false;
    }

    bool protocol_server::has_tcp_transport() const noexcept {
        return std::ranges::any_of(mTransports, [](const std::unique_ptr<protocol_transport> &transport) {
            return transport->kind() == transport_kind::TCP;
        });
    }

    bool protocol_server::gates_every_transport() const noexcept {
        return mTokenExplicit || mInspectTokenExplicit;
    }

    void protocol_server::ensure_tokens() {
        if (!has_tcp_transport() && !gates_every_transport()) {
            // Nothing asks for a token: no generated one may linger, or it would still be accepted.
            mToken.clear();
            mInspectToken.clear();
            return;
        }
        if (mToken.empty()) {
            mToken = generate_access_token();
        }
        if (mInspectToken.empty()) {
            mInspectToken = generate_access_token();
        }
    }

    void protocol_server::write_session_files(const std::filesystem::path &projectRoot) {
        if (projectRoot.empty() || mBoundPort == 0) {
            return;
        }
        std::error_code ignored = {};
        const std::filesystem::path directory = session_directory(projectRoot);
        std::filesystem::create_directories(directory, ignored);
        mSessionDirectory = directory;
        // The tokens first: a client that finds the port file can then read a token beside it.
        write_token_files(directory);
        mPortFile = directory / std::filesystem::path(PORT_FILE_NAME);
        std::ofstream stream(mPortFile, std::ios::binary | std::ios::trunc);
        if (!stream.is_open()) {
            CONJURE_LOG_WARN(LOG_EDITOR, "ai protocol: cannot write {}", mPortFile.generic_string());
            mPortFile.clear();
            return;
        }
        stream << mBoundPort << '\n';
    }

    void protocol_server::rewrite_token_files() {
        if (mSessionDirectory.empty()) {
            return;
        }
        // The token files of this session name the old tokens. The directory is remembered by itself: the port
        // file cannot mark it, because the token files are written also where the port file could not be.
        write_token_files(mSessionDirectory);
    }

    void protocol_server::write_token_files(const std::filesystem::path &directory) {
        mTokenFile = write_token_file(directory, access_level::FULL_CONTROL);
        mInspectTokenFile = write_token_file(directory, access_level::INSPECT);
    }

    std::filesystem::path protocol_server::write_token_file(
      const std::filesystem::path &directory,
      access_level level
    ) {
        const bool fullControl = level == access_level::FULL_CONTROL;
        const std::filesystem::path file = directory / std::filesystem::path(token_file_name(level));
        const std::string content = (fullControl ? mToken : mInspectToken) + '\n';
        const core::result<void> written = core::vfs::write_private_file(
          file,
          std::as_bytes(std::span<const char>(content.data(), content.size()))
        );
        if (written.has_value()) {
            return file;
        }
        // A token file of an earlier session, or one written before the token changed, would name a token
        // that no longer works.
        std::error_code ignored = {};
        std::filesystem::remove(file, ignored);
        if (fullControl) {
            CONJURE_LOG_WARN(
              LOG_EDITOR,
              "ai protocol: the token file was not written ({}); TCP clients need a token given with --ai-token",
              written.get_error().message()
            );
        } else {
            CONJURE_LOG_WARN(
              LOG_EDITOR,
              "ai protocol: the inspect token file was not written ({}); inspect-only TCP clients need a token "
              "given with --ai-inspect-token",
              written.get_error().message()
            );
        }
        return {};
    }

    void protocol_server::remove_session_files() {
        std::error_code ignored = {};
        if (!mPortFile.empty()) {
            std::filesystem::remove(mPortFile, ignored);
            mPortFile.clear();
        }
        if (!mTokenFile.empty()) {
            std::filesystem::remove(mTokenFile, ignored);
            mTokenFile.clear();
        }
        if (!mInspectTokenFile.empty()) {
            std::filesystem::remove(mInspectTokenFile, ignored);
            mInspectTokenFile.clear();
        }
        mSessionDirectory.clear();
    }
}
