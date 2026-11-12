#include "conjure/editor/protocol/protocol_server.h"

#include "editor/core/protocol/stdio_transport.h"
#include "editor/core/protocol/tcp_transport.h"

#include "conjure/editor/editor_error.h"
#include "conjure/editor/log_category.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <iostream>
#include <system_error>
#include <utility>

namespace conjure::editor {

    namespace {
        constexpr std::string_view PORT_FILE_DIRECTORY = ".conjure";
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
    }

    protocol_server::protocol_server(event_broadcaster &events) :
        mEvents(&events) {}

    protocol_server::~protocol_server() {
        stop();
    }

    core::result<void> protocol_server::start(const editor_options &options, const std::filesystem::path &projectRoot) {
        mToken = options.aiToken;
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
            write_port_file(projectRoot);
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
        if (transport->kind() == transport_kind::TCP) {
            const tcp_transport *tcp = static_cast<const tcp_transport *>(transport.get());
            mBoundPort = tcp->bound_port();
        }
        mTransports.push_back(std::move(transport));
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
                    }
                    continue;
                }
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
        remove_port_file();
        mBoundPort = 0;
        mRunning = false;
        mStdioClosed = false;
    }

    void protocol_server::set_project_root(const std::filesystem::path &projectRoot) {
        remove_port_file();
        write_port_file(projectRoot);
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

    void protocol_server::set_token(std::string token) {
        mToken = std::move(token);
    }

    void protocol_server::mark_authenticated(u32 clientId) {
        client_state *entry = find(clientId);
        if (entry != nullptr) {
            entry->authenticated = true;
        }
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
        client.authenticated = mToken.empty();
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
        if (!client->authenticated) {
            // With --ai-token nothing, not even an event, reaches a client before its valid session.hello.
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
        if (client.authenticated) {
            return true;
        }
        const bool hello = request.command == "session.hello";
        const bool tokenMatches = hello && request.args.root()["token"].as_string() == mToken;
        if (hello && tokenMatches) {
            client.authenticated = true;
            return true;
        }
        send_line(
          client.id,
          serialise(make_error_response(
            request.id.root(),
            protocol_error_code::UNAUTHORISED,
            "the first request must be session.hello with the editor's token"
          ))
        );
        disconnect(client.id);
        return false;
    }

    void protocol_server::write_port_file(const std::filesystem::path &projectRoot) {
        if (projectRoot.empty() || mBoundPort == 0) {
            return;
        }
        std::error_code ignored = {};
        const std::filesystem::path directory = projectRoot / std::filesystem::path(PORT_FILE_DIRECTORY);
        std::filesystem::create_directories(directory, ignored);
        mPortFile = directory / std::filesystem::path(PORT_FILE_NAME);
        std::ofstream stream(mPortFile, std::ios::binary | std::ios::trunc);
        if (!stream.is_open()) {
            CONJURE_LOG_WARN(LOG_EDITOR, "ai protocol: cannot write {}", mPortFile.generic_string());
            mPortFile.clear();
            return;
        }
        stream << mBoundPort << '\n';
    }

    void protocol_server::remove_port_file() {
        if (mPortFile.empty()) {
            return;
        }
        std::error_code ignored = {};
        std::filesystem::remove(mPortFile, ignored);
        mPortFile.clear();
    }
}
