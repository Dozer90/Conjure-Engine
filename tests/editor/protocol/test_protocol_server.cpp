#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "conjure/testing/test_environment.h"

#include "editor_test_support.h"

#include "conjure/core/vfs/native_file_system.h"
#include "conjure/editor/editor_options.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/protocol/access_token.h"
#include "conjure/editor/protocol/loopback_transport.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/protocol/protocol_server.h"
#include "editor/core/net/socket.h"
#include "editor/core/protocol/stdio_transport.h"
#include "editor/core/protocol/tcp_transport.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// protocol_server.h: client bookkeeping over transports, the token handshake and the two levels of access, the
// first-line rule, request caps, event delivery, the backlog policy and what reaches the audit log. The loopback
// transport stands in for TCP where the transport does not matter; the token, session-file and port cases use a
// real socket.

using namespace conjure;
using namespace conjure::editor;

namespace {
    /// <summary>
    /// The response lines of a client's output (events such as client.connected are dropped).
    /// </summary>
    std::vector<std::string> responses_of(std::vector<std::string> lines) {
        std::vector<std::string> responses = {};
        for (std::string &line : lines) {
            if (!parse_event(line).has_value()) {
                responses.push_back(std::move(line));
            }
        }
        return responses;
    }

    std::vector<std::string> events_of(std::vector<std::string> lines, std::string_view name) {
        std::vector<std::string> events = {};
        for (std::string &line : lines) {
            const core::result<protocol_event> event = parse_event(line);
            if (event.has_value() && event.value().name == name) {
                events.push_back(std::move(line));
            }
        }
        return events;
    }

    /// <summary>
    /// What a web page's fetch() puts on the wire when it posts a request to the port: the HTTP request line and
    /// headers first, the JSON it wants executed as the body.
    /// </summary>
    constexpr std::string_view HTTP_SMUGGLED_REQUEST =
      "POST / HTTP/1.1\r\nHost: 127.0.0.1:47820\r\nContent-Type: text/plain\r\nContent-Length: 29\r\n\r\n"
      "{\"id\":1,\"cmd\":\"editor.quit\"}\n";

    /// <summary>
    /// A server with one real TCP transport on an ephemeral port and a scratch project folder for its session
    /// files. The token is the one the server generated.
    /// </summary>
    struct tcp_server_fixture {
        testing::log_capture logs = {};
        testing::temp_directory project = {};
        event_broadcaster events = {};
        protocol_server server;
        std::vector<protocol_request> requests = {};    // every request the server accepted, in order

        tcp_server_fixture() :
            server(events) {
            const core::result<void> added = server.add_transport(std::make_unique<tcp_transport>(u16{0}, u16{0}));
            REQUIRE_MESSAGE(added.has_value(), testing::describe_error(added));
            REQUIRE(server.bound_port() != 0);
            server.set_project_root(project.path());
        }

        [[nodiscard]] std::filesystem::path session_file(std::string_view name) const {
            return project.path() / ".conjure" / std::filesystem::path(name);
        }

        /// <summary>
        /// The token as a client finds it: the content of ai_token without its line end.
        /// </summary>
        [[nodiscard]] std::string token_from_file() const {
            std::string text = editor_test::read_text_file(session_file("ai_token"));
            REQUIRE_FALSE(text.empty());
            REQUIRE(text.back() == '\n');
            text.pop_back();
            return text;
        }

        [[nodiscard]] net::socket_handle connect() {
            const usize before = server.client_count();
            core::result<net::socket_handle> socket = net::connect_loopback(server.bound_port());
            REQUIRE_MESSAGE(socket.has_value(), testing::describe_error(socket));
            for (u32 attempt = 0; attempt < 2000 && server.client_count() == before; ++attempt) {
                server.poll(requests);
            }
            REQUIRE(server.client_count() == before + 1);
            return std::move(socket).value();
        }

        /// <summary>
        /// True when the token file exists. On a volume that keeps no owner-only files (FAT, exFAT) the server
        /// must have said so in a WARN; a MESSAGE then records that the calling case did not run its file half.
        /// </summary>
        [[nodiscard]] bool token_file_written() {
            if (!server.token_file().empty()) {
                return true;
            }
            REQUIRE(logs.contains(core::log_level::WARN, "the token file was not written"));
            REQUIRE(logs.contains(core::log_level::WARN, "cannot be restricted to its owner"));
            MESSAGE("this volume keeps no owner-only files: the token file half of the case did not run");
            return false;
        }

        /// <summary>
        /// Sends 'text' and runs the server until it closed the connection, which it must. Returns what the
        /// client received before that.
        /// </summary>
        [[nodiscard]] std::string send_until_closed(const net::socket_handle &socket, std::string_view text) {
            editor_test::send_all(socket, text);
            std::string received = {};
            std::array<std::byte, 4096> buffer = {};
            bool closed = false;
            for (u32 attempt = 0; attempt < 200000 && !closed; ++attempt) {
                server.poll(requests);
                server.flush();
                usize count = 0;
                const net::io_status status = net::receive(socket, buffer, count);
                if (status == net::io_status::OK) {
                    received.append(reinterpret_cast<const char *>(buffer.data()), count);
                }
                closed = status == net::io_status::CLOSED || status == net::io_status::FAILED;
            }
            REQUIRE(closed);
            return received;
        }

        /// <summary>
        /// Sends 'text' and runs the server until it accepted 'total' requests since the fixture started.
        /// </summary>
        void send_until_accepted(const net::socket_handle &socket, std::string_view text, usize total) {
            editor_test::send_all(socket, text);
            for (u32 attempt = 0; attempt < 200000 && requests.size() < total; ++attempt) {
                server.poll(requests);
            }
            REQUIRE(requests.size() == total);
        }

        /// <summary>
        /// Flushes the server and reads the socket until 'count' complete lines arrived (or the attempts ran
        /// out); returns them without their line ends.
        /// </summary>
        [[nodiscard]] std::vector<std::string> read_lines(const net::socket_handle &socket, usize count) {
            std::string received = {};
            std::array<std::byte, 4096> buffer = {};
            for (u32 attempt = 0; attempt < 200000; ++attempt) {
                if (static_cast<usize>(std::ranges::count(received, '\n')) >= count) {
                    break;
                }
                server.flush();
                usize taken = 0;
                if (net::receive(socket, buffer, taken) == net::io_status::OK) {
                    received.append(reinterpret_cast<const char *>(buffer.data()), taken);
                }
            }
            return editor_test::non_empty_lines(received);
        }
    };

    struct server_fixture {
        testing::log_capture logs = {};
        event_broadcaster events = {};
        protocol_server server;
        loopback_transport *transport = nullptr;

        server_fixture() :
            server(events) {
            std::unique_ptr<loopback_transport> loopback = std::make_unique<loopback_transport>();
            transport = loopback.get();
            const core::result<void> added = server.add_transport(std::move(loopback));
            REQUIRE_MESSAGE(added.has_value(), testing::describe_error(added));
        }

        loopback_client connect() {
            loopback_client client = transport->connect();
            std::vector<protocol_request> requests = {};
            server.poll(requests);
            return client;
        }

        std::vector<protocol_request> poll(usize maxRequests = protocol_server::DEFAULT_REQUESTS_PER_FRAME) {
            std::vector<protocol_request> requests = {};
            server.poll(requests, maxRequests);
            return requests;
        }
    };
}

TEST_SUITE("editor/protocol/protocol_server") {

    TEST_CASE("protocol_server - a connecting client is registered as an event client and announced") {
        server_fixture fixture = {};
        std::vector<std::string> seen = {};
        (void) fixture.events.register_client([&seen](const protocol_event &event) {
            seen.push_back(event.name);
        });
        loopback_client client = fixture.connect();
        CHECK(fixture.server.client_count() == 1);
        CHECK(fixture.server.is_running());
        const client_state *state = fixture.server.client(client.client_id());
        REQUIRE(state != nullptr);
        CHECK(state->kind == transport_kind::LOOPBACK);
        CHECK(state->authenticated());
        CHECK(state->access == access_level::FULL_CONTROL);
        CHECK_FALSE(state->spoke);
        CHECK(fixture.events.has_client(client.client_id()));
        REQUIRE_FALSE(seen.empty());
        CHECK(seen.back() == "client.connected");
    }

    TEST_CASE("protocol_server - parses requests, answers malformed lines and routes responses") {
        server_fixture fixture = {};
        loopback_client client = fixture.connect();
        client.send(R"({"id":1,"cmd":"editor.state"})");
        client.send("not json");
        client.send(R"({"id":2})");
        const std::vector<protocol_request> requests = fixture.poll();
        REQUIRE(requests.size() == 1);
        CHECK(requests[0].command == "editor.state");
        CHECK(requests[0].clientId == client.client_id());
        std::vector<std::string> replies = responses_of(client.receive_all());
        REQUIRE(replies.size() == 2);
        CHECK(replies[0].find("PARSE_ERROR") != std::string::npos);
        CHECK(replies[1].find("INVALID_REQUEST") != std::string::npos);
        protocol_response response = make_ok_response(requests[0].id.root(), core::json::object({{"ok", 1}}));
        response.clientId = client.client_id();
        fixture.server.send(response);
        replies = responses_of(client.receive_all());
        REQUIRE(replies.size() == 1);
        CHECK(replies[0] == R"({"id":1,"ok":true,"result":{"ok":1}})");
        // A response to client 0 (the UI) goes nowhere.
        response.clientId = 0;
        fixture.server.send(response);
        CHECK(client.receive_all().empty());
    }

    TEST_CASE("protocol_server - an INVALID_REQUEST answer echoes an integer or string id") {
        server_fixture fixture = {};
        loopback_client client = fixture.connect();
        // A client opens with a request (the first-line rule); the malformed ones follow it.
        client.send(R"({"id":0,"cmd":"session.hello"})");
        client.send(R"({"id":7,"cmd":5})");
        client.send(R"({"id":"a","cmd":"x","args":3})");
        client.send(R"({"id":[1],"cmd":"x"})");
        client.send("[1, 2]");
        client.send("not json");
        CHECK(fixture.poll().size() == 1);
        const std::vector<std::string> replies = responses_of(client.receive_all());
        REQUIRE(replies.size() == 5);
        CHECK(replies[0].find(R"("id":7,"ok":false)") != std::string::npos);
        CHECK(replies[0].find("INVALID_REQUEST") != std::string::npos);
        CHECK(replies[1].find(R"("id":"a","ok":false)") != std::string::npos);
        CHECK(replies[1].find("INVALID_REQUEST") != std::string::npos);
        // An id that is neither an integer nor a string, a message that is not an object and a line that is not
        // JSON have nothing to echo.
        CHECK(replies[2].find(R"("id":null)") != std::string::npos);
        CHECK(replies[2].find("INVALID_REQUEST") != std::string::npos);
        CHECK(replies[3].find(R"("id":null)") != std::string::npos);
        CHECK(replies[3].find("INVALID_REQUEST") != std::string::npos);
        CHECK(replies[4].find(R"("id":null)") != std::string::npos);
        CHECK(replies[4].find("PARSE_ERROR") != std::string::npos);
        CHECK(client.is_connected());
    }

    TEST_CASE("protocol_server - with a token no event reaches a client before its session.hello") {
        server_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.server.set_token("secret"));
        loopback_client client = fixture.connect();
        (void) fixture.events.emit(editor_event::SCENE_CHANGED, core::json::object({{"revision", 1}}));
        (void) fixture.events.emit(editor_event::SELECTION_CHANGED, core::json::object({{"guids", "none"}}));
        CHECK(client.receive_all().empty());

        client.send(R"({"id":1,"cmd":"session.hello","args":{"token":"secret"}})");
        const std::vector<protocol_request> requests = fixture.poll();
        REQUIRE(requests.size() == 1);
        CHECK(requests[0].command == "session.hello");
        (void) fixture.events.emit(editor_event::SCENE_CHANGED, core::json::object({{"revision", 2}}));
        const std::vector<std::string> lines = events_of(client.receive_all(), "scene.changed");
        REQUIRE(lines.size() == 1);
        const core::result<protocol_event> event = parse_event(lines[0]);
        CONJURE_REQUIRE_OK(event);
        CHECK(event.value().data.root()["revision"].as_i64() == 2);
    }

    TEST_CASE("protocol_server - an oversized line answers MESSAGE_TOO_LARGE and disconnects") {
        event_broadcaster events = {};
        protocol_server server(events);
        std::unique_ptr<loopback_transport> loopback = std::make_unique<loopback_transport>(32);
        loopback_transport *transport = loopback.get();
        CONJURE_REQUIRE_OK(server.add_transport(std::move(loopback)));
        loopback_client client = transport->connect();
        std::vector<protocol_request> requests = {};
        server.poll(requests);
        client.send(std::string(100, 'x'));
        server.poll(requests);
        const std::vector<std::string> replies = responses_of(client.receive_all());
        REQUIRE(replies.size() == 1);
        CHECK(replies[0].find("MESSAGE_TOO_LARGE") != std::string::npos);
        server.poll(requests);
        CHECK_FALSE(client.is_connected());
        CHECK(server.client_count() == 0);
        std::vector<u32> closed = {};
        server.take_closed(closed);
        REQUIRE(closed.size() == 1);
        CHECK(closed[0] == client.client_id());
        server.take_closed(closed);
        CHECK(closed.size() == 1);
    }

    TEST_CASE("regression - requests queued behind an oversized line in the same packet are not executed") {
        event_broadcaster events = {};
        protocol_server server(events);
        std::unique_ptr<loopback_transport> loopback = std::make_unique<loopback_transport>(32);
        loopback_transport *transport = loopback.get();
        CONJURE_REQUIRE_OK(server.add_transport(std::move(loopback)));
        loopback_client client = transport->connect();
        std::vector<protocol_request> requests = {};
        server.poll(requests);
        // One packet: a valid request, an oversized line, then another valid request.
        client.send_raw(
          std::string(R"({"id":1,"cmd":"a"})") + "\n" + std::string(100, 'x') + "\n" + R"({"id":2,"cmd":"b"})" + "\n"
        );
        server.poll(requests);
        REQUIRE(requests.size() == 1);
        CHECK(requests[0].id.root().as_i64() == 1);
        const std::vector<std::string> replies = responses_of(client.receive_all());
        REQUIRE(replies.size() == 1);
        CHECK(replies[0].find("MESSAGE_TOO_LARGE") != std::string::npos);
        server.poll(requests);
        CHECK_FALSE(client.is_connected());
        CHECK(server.client_count() == 0);
    }

    TEST_CASE("regression - a failed handshake answers UNAUTHORISED once however many lines were queued") {
        server_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.server.set_token("secret"));
        loopback_client client = fixture.connect();
        client.send(R"({"id":1,"cmd":"editor.state"})");
        client.send(R"({"id":2,"cmd":"editor.state"})");
        client.send(R"({"id":3,"cmd":"session.hello","args":{"token":"secret"}})");
        const std::vector<protocol_request> requests = fixture.poll();
        CHECK(requests.empty());
        const std::vector<std::string> replies = responses_of(client.receive_all());
        REQUIRE(replies.size() == 1);
        CHECK(replies[0].find("UNAUTHORISED") != std::string::npos);
        CHECK(replies[0].find("\"id\":1") != std::string::npos);
        (void) fixture.poll();
        CHECK_FALSE(client.is_connected());
    }

    TEST_CASE("protocol_server - local_loopback hands out one shared transport for in-process clients") {
        server_fixture fixture = {};
        const usize before = fixture.server.transport_count();
        loopback_transport &first = fixture.server.local_loopback();
        loopback_transport &second = fixture.server.local_loopback();
        CHECK(&first == &second);
        CHECK(fixture.server.transport_count() == before + 1);
        loopback_client clientA = first.connect();
        (void) fixture.poll();
        clientA.disconnect();
        (void) fixture.poll();
        loopback_client clientB = fixture.server.local_loopback().connect();
        (void) fixture.poll();
        CHECK(clientB.is_connected());
        CHECK(fixture.server.transport_count() == before + 1);
        fixture.server.stop();
        CHECK(fixture.server.transport_count() == 0);
        loopback_transport &fresh = fixture.server.local_loopback();
        CHECK(fixture.server.transport_count() == 1);
        CHECK(fixture.server.is_running());
        (void) fresh;
    }

    TEST_CASE("protocol_server - with a token the first request must be a matching session.hello") {
        server_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.server.set_token("secret"));
        CHECK(fixture.server.token() == "secret");
        loopback_client wrong = fixture.connect();
        CHECK_FALSE(fixture.server.client(wrong.client_id())->authenticated());
        CHECK(fixture.server.client(wrong.client_id())->access == access_level::NONE);
        wrong.send(R"({"id":1,"cmd":"editor.state"})");
        std::vector<protocol_request> requests = fixture.poll();
        CHECK(requests.empty());
        std::vector<std::string> replies = responses_of(wrong.receive_all());
        REQUIRE(replies.size() == 1);
        CHECK(replies[0].find("UNAUTHORISED") != std::string::npos);
        (void) fixture.poll();
        CHECK_FALSE(wrong.is_connected());
        CHECK(fixture.logs.contains(core::log_level::INFO, "did not open with session.hello and a valid token"));
        CHECK_FALSE(fixture.logs.contains("secret"));

        loopback_client right = fixture.connect();
        right.send(R"({"id":1,"cmd":"session.hello","args":{"token":"secret"}})");
        requests = fixture.poll();
        REQUIRE(requests.size() == 1);
        CHECK(requests[0].command == "session.hello");
        CHECK(fixture.server.client(right.client_id())->authenticated());
        CHECK(fixture.server.client(right.client_id())->access == access_level::FULL_CONTROL);
        right.send(R"({"id":2,"cmd":"editor.state"})");
        requests = fixture.poll();
        REQUIRE(requests.size() == 1);
    }

    TEST_CASE("protocol_server - the per-frame cap leaves a backlog that drains in order") {
        server_fixture fixture = {};
        loopback_client client = fixture.connect();
        for (int index = 0; index < 5; ++index) {
            client.send(std::format(R"({{"id":{},"cmd":"editor.state"}})", index));
        }
        std::vector<protocol_request> first = fixture.poll(2);
        REQUIRE(first.size() == 2);
        CHECK(first[0].id.root().as_i64() == 0);
        CHECK(fixture.server.backlog_count() == 3);
        std::vector<protocol_request> second = fixture.poll(2);
        REQUIRE(second.size() == 2);
        CHECK(second[0].id.root().as_i64() == 2);
        std::vector<protocol_request> third = fixture.poll(2);
        REQUIRE(third.size() == 1);
        CHECK(third[0].id.root().as_i64() == 4);
        CHECK(fixture.server.backlog_count() == 0);
    }

    TEST_CASE("protocol_server - a disconnecting client drops its backlog and is removed") {
        server_fixture fixture = {};
        loopback_client client = fixture.connect();
        client.send(R"({"id":1,"cmd":"a"})");
        client.send(R"({"id":2,"cmd":"b"})");
        (void) fixture.poll(1);
        CHECK(fixture.server.backlog_count() == 1);
        client.disconnect();
        (void) fixture.poll();
        CHECK(fixture.server.backlog_count() == 0);
        CHECK(fixture.server.client_count() == 0);
        CHECK_FALSE(fixture.events.has_client(client.client_id()));
    }

    TEST_CASE("protocol_server - events are serialised to subscribed clients") {
        server_fixture fixture = {};
        loopback_client client = fixture.connect();
        (void) fixture.events.emit(editor_event::SCENE_CHANGED, core::json::object({{"revision", 3}}));
        (void) fixture.events.emit_log(core::log_level::INFO, core::json::object({{"text", "not subscribed"}}));
        const std::vector<std::string> lines = events_of(client.receive_all(), "scene.changed");
        REQUIRE(lines.size() == 1);
        const core::result<protocol_event> event = parse_event(lines[0]);
        CONJURE_REQUIRE_OK(event);
        CHECK(event.value().name == "scene.changed");
        CHECK(event.value().data.root()["revision"].as_i64() == 3);
    }

    TEST_CASE("protocol_server - log events beyond the backlog are counted and reported as events.dropped") {
        server_fixture fixture = {};
        loopback_client client = fixture.connect();
        fixture.events.set_subscriptions(client.client_id(), default_subscriptions() | event_bit(editor_event::LOG));
        // Fill the outbound queue past the log-drop threshold without the client reading.
        const std::string payload(1024 * 1024, 'p');
        for (int index = 0; index < 9; ++index) {
            (void) fixture.events.emit(editor_event::PROGRESS, core::json::object({{"blob", payload}}));
        }
        (void) fixture.events.emit_log(core::log_level::INFO, core::json::object({{"text", "dropped"}}));
        (void) fixture.events.emit_log(core::log_level::INFO, core::json::object({{"text", "dropped too"}}));
        CHECK(fixture.server.client(client.client_id())->droppedLogEvents == 2);
        fixture.server.flush();
        // Still above the threshold: nothing reported yet.
        CHECK(fixture.server.client(client.client_id())->droppedLogEvents == 2);
        (void) client.receive_all();
        fixture.server.flush();
        CHECK(fixture.server.client(client.client_id())->droppedLogEvents == 0);
        const std::vector<std::string> lines = events_of(client.receive_all(), "events.dropped");
        REQUIRE(lines.size() == 1);
        const core::result<protocol_event> event = parse_event(lines[0]);
        CONJURE_REQUIRE_OK(event);
        CHECK(event.value().name == "events.dropped");
        CHECK(event.value().data.root()["count"].as_i64() == 2);
    }

    TEST_CASE("protocol_server - a client with too much queued output is disconnected on flush") {
        server_fixture fixture = {};
        fixture.logs.allow_errors();
        loopback_client client = fixture.connect();
        const std::string payload(8 * 1024 * 1024, 'q');
        for (int index = 0; index < 9; ++index) {
            (void) fixture.events.emit(editor_event::PROGRESS, core::json::object({{"blob", payload}}));
        }
        fixture.server.flush();
        (void) fixture.poll();
        CHECK(fixture.server.client_count() == 0);
        CHECK_FALSE(client.is_connected());
    }

    TEST_CASE("protocol_server - start with an explicit port 0 runs no TCP transport") {
        event_broadcaster events = {};
        protocol_server server(events);
        editor_options options = {};
        options.aiPort = u16{0};
        CONJURE_REQUIRE_OK(server.start(options, {}));
        CHECK(server.bound_port() == 0);
        CHECK(server.is_running());
        CHECK(server.port_file().empty());
        server.stop();
        CHECK_FALSE(server.is_running());
    }

    TEST_CASE("protocol_server - start binds a port, falls back when busy and writes the port file") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        testing::log_capture logs = {};
        testing::temp_directory project = {};
        event_broadcaster firstEvents = {};
        protocol_server first(firstEvents);
        editor_options options = {};
        options.headless = true;
        options.aiPort = u16{0};
        // An ephemeral explicit port through the transport, then a second server on the same port with
        // fallbacks enabled (no explicit port means DEFAULT_AI_PORT, so drive it through options twice).
        editor_options explicitPort = options;
        explicitPort.aiPort = DEFAULT_AI_PORT;
        const core::result<void> started = first.start(explicitPort, project.path());
        if (!started.has_value()) {
            // The default port is taken by something else on this machine: the fallback path is still testable.
            explicitPort.aiPort.reset();
            CONJURE_REQUIRE_OK(first.start(explicitPort, project.path()));
        }
        CHECK(first.bound_port() != 0);
        CHECK(std::filesystem::is_regular_file(first.port_file()));
        CHECK(editor_test::read_text_file(first.port_file()) == std::format("{}\n", first.bound_port()));
        // No --ai-token: start() generated one for the TCP transport and wrote it beside the port (F740).
        CHECK(first.token().size() == ACCESS_TOKEN_LENGTH);
        CHECK(first.requires_token(transport_kind::TCP));
        CHECK_FALSE(first.requires_token(transport_kind::STDIO));
        CHECK_FALSE(logs.contains(first.token()));
        if (!first.token_file().empty()) {
            CHECK(first.token_file() == project.path() / ".conjure" / "ai_token");
            CHECK(editor_test::read_text_file(first.token_file()) == first.token() + "\n");
            CHECK(core::vfs::is_private_file(first.token_file()).value_or(false));
        }

        // A second server without a project, headless and with a generated token: nobody can learn the token,
        // and the log says what to do about it without naming it.
        event_broadcaster secondEvents = {};
        protocol_server second(secondEvents);
        editor_options fallback = options;
        fallback.aiPort.reset();
        CONJURE_REQUIRE_OK(second.start(fallback, {}));
        CHECK(second.bound_port() != 0);
        CHECK(second.bound_port() != first.bound_port());
        CHECK(second.token().size() == ACCESS_TOKEN_LENGTH);
        CHECK(second.token() != first.token());
        CHECK(second.token_file().empty());
        CHECK(second.port_file().empty());
        CHECK(logs.contains(core::log_level::WARN, "no project is open"));
        CHECK_FALSE(logs.contains(second.token()));

        event_broadcaster thirdEvents = {};
        protocol_server third(thirdEvents);
        editor_options busy = options;
        busy.aiPort = first.bound_port();
        CHECK_FALSE(third.start(busy, {}).has_value());
        second.stop();

        // --ai-token: the given token is the one in force, on every transport, and it is the one in the file.
        testing::temp_directory otherProject = {};
        editor_options chosen = options;
        chosen.aiPort.reset();
        chosen.aiToken = "tok-7f3a9c";
        CONJURE_REQUIRE_OK(second.start(chosen, otherProject.path()));
        CHECK(second.token() == "tok-7f3a9c");
        CHECK(second.requires_token(transport_kind::TCP));
        CHECK(second.requires_token(transport_kind::STDIO));
        CHECK(second.requires_token(transport_kind::LOOPBACK));
        if (!second.token_file().empty()) {
            CHECK(editor_test::read_text_file(second.token_file()) == "tok-7f3a9c\n");
        }
        CHECK_FALSE(logs.contains("tok-7f3a9c"));
        second.stop();
        CHECK_FALSE(std::filesystem::exists(otherProject.path() / ".conjure" / "ai_token"));
        first.stop();
        CHECK_FALSE(std::filesystem::exists(project.path() / ".conjure" / "ai_token"));
    }

    TEST_CASE("regression - F092 protocol_server set_project_root moves the port file to the new project") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        testing::log_capture logs = {};
        testing::temp_directory directory = {};
        const std::filesystem::path projectA = directory.path() / "a";
        const std::filesystem::path projectB = directory.path() / "b";
        event_broadcaster events = {};
        protocol_server server(events);
        CONJURE_REQUIRE_OK(server.add_transport(std::make_unique<tcp_transport>(u16{0}, u16{0})));
        REQUIRE(server.bound_port() != 0);
        const std::string expected = std::format("{}\n", server.bound_port());

        server.set_project_root(projectA);
        const std::filesystem::path portFileA = projectA / ".conjure" / "ai_port";
        CHECK(server.port_file() == portFileA);
        REQUIRE(std::filesystem::is_regular_file(portFileA));
        CHECK(editor_test::read_text_file(portFileA) == expected);
        // The token file travels with the port file (F740), unless the volume keeps no owner-only files.
        const std::filesystem::path tokenFileA = projectA / ".conjure" / "ai_token";
        const bool tokenFiles = !server.token_file().empty();
        const std::string expectedToken = server.token() + "\n";
        if (tokenFiles) {
            CHECK(server.token_file() == tokenFileA);
            CHECK(editor_test::read_text_file(tokenFileA) == expectedToken);
        }

        server.set_project_root(projectB);
        const std::filesystem::path portFileB = projectB / ".conjure" / "ai_port";
        const std::filesystem::path tokenFileB = projectB / ".conjure" / "ai_token";
        CHECK_FALSE(std::filesystem::exists(portFileA));
        CHECK_FALSE(std::filesystem::exists(tokenFileA));
        CHECK(server.port_file() == portFileB);
        REQUIRE(std::filesystem::is_regular_file(portFileB));
        CHECK(editor_test::read_text_file(portFileB) == expected);
        if (tokenFiles) {
            CHECK(server.token_file() == tokenFileB);
            CHECK(editor_test::read_text_file(tokenFileB) == expectedToken);
            CHECK(core::vfs::is_private_file(tokenFileB).value_or(false));
        }

        server.set_project_root({});
        CHECK_FALSE(std::filesystem::exists(portFileB));
        CHECK_FALSE(std::filesystem::exists(tokenFileB));
        CHECK(server.port_file().empty());
        CHECK(server.token_file().empty());
        server.stop();
    }

    TEST_CASE("protocol_server - set_project_root writes no port file without a TCP transport") {
        server_fixture fixture = {};
        testing::temp_directory project = {};
        fixture.server.set_project_root(project.path());
        CHECK(fixture.server.port_file().empty());
        CHECK(fixture.server.token_file().empty());
        CHECK_FALSE(std::filesystem::exists(project.path() / ".conjure" / "ai_port"));
        CHECK_FALSE(std::filesystem::exists(project.path() / ".conjure" / "ai_token"));
    }

    TEST_CASE("regression - F158 protocol_server stdio_closed turns true once the stdio client's input ended") {
        server_fixture fixture = {};
        // A loopback client leaving is not the stdio client leaving.
        loopback_client client = fixture.connect();
        client.disconnect();
        std::istringstream input(R"({"id":1,"cmd":"session.hello"})" "\n");
        std::ostringstream output = {};
        CONJURE_REQUIRE_OK(fixture.server.add_transport(std::make_unique<stdio_transport>(input, output, false)));
        const std::vector<protocol_request> first = fixture.poll();
        REQUIRE(first.size() == 1);
        CHECK(first[0].command == "session.hello");
        CHECK_FALSE(fixture.server.stdio_closed());
        // The end of input is reported one poll later, after the last line's response could still go out.
        (void) fixture.poll();
        CHECK(fixture.server.stdio_closed());
        fixture.server.stop();
        CHECK_FALSE(fixture.server.stdio_closed());
    }

    TEST_CASE("protocol_server - without a TCP transport there is no token and nothing is gated") {
        server_fixture fixture = {};
        CHECK(fixture.server.token().empty());
        CHECK(fixture.server.inspect_token().empty());
        CHECK(fixture.server.token_file().empty());
        CHECK(fixture.server.inspect_token_file().empty());
        CHECK_FALSE(fixture.server.contains_token(""));
        CHECK_FALSE(fixture.server.contains_token("anything"));
        CHECK_FALSE(fixture.server.requires_token(transport_kind::TCP));
        CHECK_FALSE(fixture.server.requires_token(transport_kind::STDIO));
        CHECK_FALSE(fixture.server.requires_token(transport_kind::LOOPBACK));
        // No token means nothing can be presented that grants anything: "" does not match "".
        CHECK(fixture.server.access_of("") == access_level::NONE);
        CHECK(fixture.server.access_of("anything") == access_level::NONE);
    }

    TEST_CASE("protocol_server - access_of grants full control for the exact token only") {
        server_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.server.set_token("tok-7f3a9c"));
        CHECK(fixture.server.access_of("tok-7f3a9c") == access_level::FULL_CONTROL);
        CHECK(fixture.server.access_of("") == access_level::NONE);
        CHECK(fixture.server.access_of("tok-7f3a9") == access_level::NONE);
        CHECK(fixture.server.access_of("tok-7f3a9c ") == access_level::NONE);
        CHECK(fixture.server.access_of("TOK-7F3A9C") == access_level::NONE);
        CHECK(fixture.server.access_of("tok-7f3a9ctok-7f3a9c") == access_level::NONE);
    }

    TEST_CASE("protocol_server - accept_hello trusts in-process clients and checks the token of gated ones") {
        server_fixture fixture = {};
        // Connected before any token existed: the client is in, and stays in whatever happens next.
        loopback_client early = fixture.connect();
        const u32 unknownClient = 9999;
        CHECK(fixture.server.accept_hello(early.client_id(), ""));
        CHECK(fixture.server.accept_hello(unknownClient, "whatever"));

        CONJURE_REQUIRE_OK(fixture.server.set_token("tok-7f3a9c"));
        CHECK_FALSE(fixture.server.accept_hello(early.client_id(), "wrong"));
        CHECK_FALSE(fixture.server.accept_hello(early.client_id(), ""));
        CHECK(fixture.server.client(early.client_id())->authenticated());
        CHECK(fixture.server.accept_hello(early.client_id(), "tok-7f3a9c"));
        CHECK(fixture.server.client(early.client_id())->access == access_level::FULL_CONTROL);
        // No transport knows the id: an --exec file or the UI, trusted with any token or none.
        CHECK(fixture.server.accept_hello(unknownClient, "wrong"));
        CHECK(fixture.server.accept_hello(unknownClient, ""));
    }

    TEST_CASE("regression - code sweep F740 a TCP transport gets a generated token, written beside the port for its "
      "owner only") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_server_fixture fixture = {};
        const std::string token = fixture.server.token();
        CHECK(token.size() == ACCESS_TOKEN_LENGTH);
        CHECK(fixture.server.requires_token(transport_kind::TCP));
        CHECK(fixture.server.port_file() == fixture.session_file("ai_port"));
        CHECK(std::filesystem::is_regular_file(fixture.session_file("ai_port")));
        // The token never reaches the log, whatever else happened to the file.
        CHECK_FALSE(fixture.logs.contains(token));
        if (!fixture.token_file_written()) {
            return;
        }

        CHECK(fixture.server.token_file() == fixture.session_file("ai_token"));
        CHECK(fixture.token_from_file() == token);
        const core::result<bool> isPrivate = core::vfs::is_private_file(fixture.server.token_file());
        CONJURE_REQUIRE_OK(isPrivate);
        CHECK(*isPrivate);
        // Nothing but the three session files (the port and the two tokens): the temporaries the tokens were
        // written through are gone, and no audit log exists before a command was recorded.
        std::vector<std::string> entries = {};
        for (const std::filesystem::directory_entry &entry :
          std::filesystem::directory_iterator(fixture.project.path() / ".conjure")) {
            entries.push_back(entry.path().filename().generic_string());
        }
        std::sort(entries.begin(), entries.end());
        CHECK(entries == std::vector<std::string>{"ai_port", "ai_token", "ai_token_inspect"});

        fixture.server.stop();
        CHECK_FALSE(std::filesystem::exists(fixture.session_file("ai_token")));
        CHECK_FALSE(std::filesystem::exists(fixture.session_file("ai_port")));
        CHECK(fixture.server.token_file().empty());
        CHECK(fixture.server.port_file().empty());
    }

    TEST_CASE("regression - code sweep F740 a TCP client is refused and disconnected unless it presents the token") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_server_fixture fixture = {};
        const std::string token = fixture.server.token();
        REQUIRE(token.size() == ACCESS_TOKEN_LENGTH);
        std::string firstLine = "";
        // The report names the subcase, never the line: two of them carry the token, and no test prints one.
        std::string_view label = "";
        SUBCASE("a first request that is not session.hello") {
            label = "another command first";
            firstLine = R"({"id":1,"cmd":"editor.state"})";
        }
        SUBCASE("session.hello without a token") {
            label = "no token";
            firstLine = R"({"id":1,"cmd":"session.hello","args":{"clientName":"intruder"}})";
        }
        SUBCASE("session.hello with an empty token") {
            label = "empty token";
            firstLine = R"({"id":1,"cmd":"session.hello","args":{"token":""}})";
        }
        SUBCASE("session.hello with a token of the right length that is wrong in its last digit") {
            label = "wrong last digit";
            std::string wrong = token;
            wrong.back() = wrong.back() == '0' ? '1' : '0';
            firstLine = std::format(R"({{"id":1,"cmd":"session.hello","args":{{"token":"{}"}}}})", wrong);
        }
        SUBCASE("session.hello with the token followed by more text") {
            label = "token followed by more text";
            firstLine = std::format(R"({{"id":1,"cmd":"session.hello","args":{{"token":"{}0"}}}})", token);
        }
        SUBCASE("session.hello whose token is not a string") {
            label = "token not a string";
            firstLine = R"({"id":1,"cmd":"session.hello","args":{"token":12345}})";
        }
        CAPTURE(label);
        const net::socket_handle socket = fixture.connect();
        // An event emitted while the client is unknown must not reach it either.
        (void) fixture.events.emit(editor_event::SCENE_CHANGED, core::json::object({{"revision", 1}}));
        const std::string reply = fixture.send_until_closed(
          socket,
          firstLine + "\n" + R"({"id":2,"cmd":"editor.quit"})" + "\n"
        );
        const std::vector<std::string> lines = responses_of(editor_test::non_empty_lines(reply));
        REQUIRE(lines.size() == 1);
        CHECK(lines[0].find("UNAUTHORISED") != std::string::npos);
        CHECK(lines[0].find(R"("id":1)") != std::string::npos);
        // The refusal says where the token is, never what it is.
        CHECK(lines[0].find("ai_token") != std::string::npos);
        CHECK(reply.find(token) == std::string::npos);
        CHECK(fixture.requests.empty());
        CHECK(fixture.server.client_count() == 0);
        // The refusal is logged, without the presented text or the command name: either could hold a token.
        CHECK(fixture.logs.contains(core::log_level::INFO, "did not open with session.hello and a valid token"));
        CHECK_FALSE(fixture.logs.contains(token));
        CHECK_FALSE(fixture.logs.contains("editor.state"));
        CHECK_FALSE(fixture.logs.contains("intruder"));
    }

    TEST_CASE("regression - code sweep F740 a TCP client that presents the token from the file is let in") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_server_fixture fixture = {};
        if (!fixture.token_file_written()) {
            return;
        }
        const std::string token = fixture.token_from_file();
        const net::socket_handle socket = fixture.connect();
        (void) fixture.events.emit(editor_event::SCENE_CHANGED, core::json::object({{"revision", 1}}));
        fixture.server.flush();
        CHECK(editor_test::receive_some(socket, 1, 200).empty());

        fixture.send_until_accepted(
          socket,
          std::format(R"({{"id":1,"cmd":"session.hello","args":{{"token":"{}"}}}})", token) + "\n",
          1
        );
        CHECK(fixture.requests[0].command == "session.hello");
        CHECK(fixture.server.client(fixture.requests[0].clientId)->access == access_level::FULL_CONTROL);
        fixture.send_until_accepted(socket, std::string(R"({"id":2,"cmd":"editor.state"})") + "\n", 2);
        CHECK(fixture.requests[1].command == "editor.state");

        // Events flow from the hello on, and none of them carries the token.
        (void) fixture.events.emit(editor_event::SCENE_CHANGED, core::json::object({{"revision", 2}}));
        const std::vector<std::string> events = fixture.read_lines(socket, 1);
        REQUIRE(events.size() == 1);
        CHECK(events[0].find("scene.changed") != std::string::npos);
        CHECK(events[0].find(R"("revision":2)") != std::string::npos);
        CHECK(events[0].find(token) == std::string::npos);
        CHECK_FALSE(fixture.logs.contains(token));
    }

    TEST_CASE("protocol_server - the generated token gates TCP alone, a token set explicitly gates every transport") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_server_fixture fixture = {};
        const std::string generated = fixture.server.token();
        CHECK(fixture.server.requires_token(transport_kind::TCP));
        CHECK_FALSE(fixture.server.requires_token(transport_kind::STDIO));
        CHECK_FALSE(fixture.server.requires_token(transport_kind::LOOPBACK));

        // A stdio client (the process that started the editor) and a loopback client (code inside it) are in
        // at once.
        std::istringstream input(R"({"id":1,"cmd":"editor.state"})" "\n");
        std::ostringstream output = {};
        CONJURE_REQUIRE_OK(fixture.server.add_transport(std::make_unique<stdio_transport>(input, output, false)));
        std::unique_ptr<loopback_transport> loopback = std::make_unique<loopback_transport>();
        loopback_transport *transport = loopback.get();
        CONJURE_REQUIRE_OK(fixture.server.add_transport(std::move(loopback)));
        loopback_client early = transport->connect();
        fixture.server.poll(fixture.requests);
        REQUIRE(fixture.requests.size() == 1);
        CHECK(fixture.requests[0].command == "editor.state");
        CHECK(output.str().find("UNAUTHORISED") == std::string::npos);
        CHECK(fixture.server.client(early.client_id())->authenticated());
        CHECK(fixture.server.accept_hello(early.client_id(), "any text, the transport is not gated"));

        CONJURE_REQUIRE_OK(fixture.server.set_token("tok-7f3a9c"));
        CHECK(fixture.server.token() == "tok-7f3a9c");
        CHECK(fixture.server.requires_token(transport_kind::TCP));
        CHECK(fixture.server.requires_token(transport_kind::STDIO));
        CHECK(fixture.server.requires_token(transport_kind::LOOPBACK));
        // Connected clients keep their access; a new one starts without any.
        CHECK(fixture.server.client(early.client_id())->authenticated());
        loopback_client late = transport->connect();
        fixture.server.poll(fixture.requests);
        CHECK_FALSE(fixture.server.client(late.client_id())->authenticated());

        // An empty token returns to the default: a fresh generated token for the TCP transport alone.
        CONJURE_REQUIRE_OK(fixture.server.set_token(""));
        const std::string regenerated = fixture.server.token();
        CHECK(regenerated.size() == ACCESS_TOKEN_LENGTH);
        CHECK(regenerated != generated);
        CHECK(fixture.server.requires_token(transport_kind::TCP));
        CHECK_FALSE(fixture.server.requires_token(transport_kind::LOOPBACK));
        CHECK(fixture.server.access_of(generated) == access_level::NONE);
        CHECK(fixture.server.access_of("tok-7f3a9c") == access_level::NONE);
        CHECK(fixture.server.access_of(regenerated) == access_level::FULL_CONTROL);
        CHECK_FALSE(fixture.logs.contains(generated));
        CHECK_FALSE(fixture.logs.contains(regenerated));
        CHECK_FALSE(fixture.logs.contains("tok-7f3a9c"));
        // The streams of the stdio transport end with this scope: the server lets go of them first.
        fixture.server.stop();
    }

    TEST_CASE("protocol_server - start with no token in the options keeps an earlier TCP transport gated") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_server_fixture fixture = {};
        REQUIRE(fixture.server.token().size() == ACCESS_TOKEN_LENGTH);
        editor_options options = {};
        options.headless = true;
        options.aiPort = u16{0};
        CONJURE_REQUIRE_OK(fixture.server.start(options, {}));
        // The options named no token and asked for no TCP server of their own; the transport that is there
        // still has one, and still asks for it.
        CHECK(fixture.server.token().size() == ACCESS_TOKEN_LENGTH);
        CHECK(fixture.server.requires_token(transport_kind::TCP));
        CHECK_FALSE(fixture.server.requires_token(transport_kind::LOOPBACK));
        const net::socket_handle socket = fixture.connect();
        const std::string reply = fixture.send_until_closed(
          socket,
          std::string(R"({"id":1,"cmd":"editor.state"})") + "\n"
        );
        CHECK(reply.find("UNAUTHORISED") != std::string::npos);
        CHECK(fixture.requests.empty());
    }

    TEST_CASE("protocol_server - set_token rewrites the token file, and the file stays private") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_server_fixture fixture = {};
        if (!fixture.token_file_written()) {
            return;
        }
        SUBCASE("in a session that has all its files") {
            CHECK_FALSE(fixture.server.port_file().empty());
        }
        SUBCASE("in a session whose port file could not be written") {
            // The token files are written before the port file and stay when it fails, so they must follow a
            // new token as well: a file left naming the old one would name a token that no longer works.
            fixture.server.set_project_root({});
            const std::filesystem::path blocked = fixture.session_file("ai_port");
            std::filesystem::create_directories(blocked);
            {
                std::ofstream keep(blocked / "keep.txt", std::ios::binary);
                keep << "in the way";
            }
            fixture.server.set_project_root(fixture.project.path());
            CHECK(fixture.server.port_file().empty());
            REQUIRE(fixture.server.token_file() == fixture.session_file("ai_token"));
            CHECK(fixture.token_from_file() == fixture.server.token());
        }
        CONJURE_REQUIRE_OK(fixture.server.set_inspect_token("look-0b6e55"));
        CHECK(editor_test::read_text_file(fixture.session_file("ai_token_inspect")) == "look-0b6e55\n");
        CONJURE_REQUIRE_OK(fixture.server.set_token("tok-7f3a9c"));
        CHECK(fixture.token_from_file() == "tok-7f3a9c");
        CHECK(core::vfs::is_private_file(fixture.server.token_file()).value_or(false));
        CONJURE_REQUIRE_OK(fixture.server.set_token(""));
        CHECK(fixture.token_from_file() == fixture.server.token());
        CHECK(fixture.token_from_file().size() == ACCESS_TOKEN_LENGTH);
        CHECK(core::vfs::is_private_file(fixture.server.token_file()).value_or(false));
    }

    TEST_CASE("protocol_server - a token file that cannot be written is reported and the other session files are "
              "still there") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        access_level blockedLevel = access_level::NONE;
        std::string_view warning = "";
        std::string_view option = "";
        SUBCASE("the full-control token") {
            blockedLevel = access_level::FULL_CONTROL;
            warning = "the token file was not written";
            option = "--ai-token";
        }
        SUBCASE("the inspect-only token") {
            blockedLevel = access_level::INSPECT;
            warning = "the inspect token file was not written";
            option = "--ai-inspect-token";
        }
        const access_level otherLevel =
          blockedLevel == access_level::INSPECT ? access_level::FULL_CONTROL : access_level::INSPECT;
        CAPTURE(to_string(blockedLevel));
        testing::log_capture logs = {};
        testing::temp_directory project = {};
        // A folder where the token file belongs, with something in it so that nothing can clear it away.
        const std::filesystem::path blocked =
          project.path() / ".conjure" / std::filesystem::path(token_file_name(blockedLevel));
        std::filesystem::create_directories(blocked);
        {
            std::ofstream keep(blocked / "keep.txt", std::ios::binary);
            keep << "in the way";
        }
        event_broadcaster events = {};
        protocol_server server(events);
        CONJURE_REQUIRE_OK(server.add_transport(std::make_unique<tcp_transport>(u16{0}, u16{0})));
        server.set_project_root(project.path());

        const bool fullBlocked = blockedLevel == access_level::FULL_CONTROL;
        const std::filesystem::path &blockedFile = fullBlocked ? server.token_file() : server.inspect_token_file();
        const std::filesystem::path &otherFile = fullBlocked ? server.inspect_token_file() : server.token_file();
        CHECK(blockedFile.empty());
        CHECK(logs.contains(core::log_level::WARN, warning));
        CHECK(logs.contains(core::log_level::WARN, option));
        CHECK_FALSE(logs.contains(server.token()));
        CHECK_FALSE(logs.contains(server.inspect_token()));
        CHECK(std::filesystem::is_regular_file(project.path() / ".conjure" / "ai_port"));
        CHECK(std::filesystem::is_regular_file(blocked / "keep.txt"));
        if (otherFile.empty()) {
            MESSAGE("this volume keeps no owner-only files: the other token's file could not be looked at");
        } else {
            // The other token's file was written as always, and only its own WARN was logged.
            CHECK(otherFile == project.path() / ".conjure" / std::filesystem::path(token_file_name(otherLevel)));
            CHECK(std::filesystem::is_regular_file(otherFile));
            CHECK(logs.count(core::log_level::WARN) == 1);
        }
        // The server still asks for its tokens: a client that was given one out of band gets in.
        CHECK(server.requires_token(transport_kind::TCP));
        CHECK(server.access_of(server.token()) == access_level::FULL_CONTROL);
        CHECK(server.access_of(server.inspect_token()) == access_level::INSPECT);
        server.stop();
        CHECK(std::filesystem::is_regular_file(blocked / "keep.txt"));
    }

    TEST_CASE("regression - code sweep F187 a client whose first line is not a request is disconnected") {
        server_fixture fixture = {};
        std::string payload = "";
        std::string_view code = "PARSE_ERROR";
        SUBCASE("an HTTP request with the JSON in its body") {
            payload = std::string(HTTP_SMUGGLED_REQUEST);
        }
        SUBCASE("text that is not JSON") {
            payload = "hello\n{\"id\":1,\"cmd\":\"editor.quit\"}\n";
        }
        SUBCASE("an empty line") {
            payload = "\n{\"id\":1,\"cmd\":\"editor.quit\"}\n";
        }
        SUBCASE("JSON that is not a request") {
            payload = "{\"id\":1}\n{\"id\":1,\"cmd\":\"editor.quit\"}\n";
            code = "INVALID_REQUEST";
        }
        SUBCASE("a JSON array") {
            payload = "[1,2]\n{\"id\":1,\"cmd\":\"editor.quit\"}\n";
            code = "INVALID_REQUEST";
        }
        CAPTURE(payload);
        loopback_client client = fixture.connect();
        client.send_raw(payload);
        CHECK(fixture.poll().empty());
        const std::vector<std::string> replies = responses_of(client.receive_all());
        REQUIRE(replies.size() == 1);
        CHECK(replies[0].find(code) != std::string::npos);
        CHECK(fixture.poll().empty());
        CHECK_FALSE(client.is_connected());
        CHECK(fixture.server.client_count() == 0);
        CHECK(fixture.server.backlog_count() == 0);
        // The line itself is never logged: it may be a mistyped session.hello that carries the token.
        CHECK(fixture.logs.contains(core::log_level::INFO, "did not open with a request"));
        CHECK_FALSE(fixture.logs.contains("editor.quit"));
        CHECK_FALSE(fixture.logs.contains("HTTP"));
    }

    TEST_CASE("regression - code sweep F187 the first-line rule holds on TCP and on stdio as on the loopback") {
        SUBCASE("a real socket") {
            CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
            tcp_server_fixture fixture = {};
            const net::socket_handle socket = fixture.connect();
            const std::string reply = fixture.send_until_closed(socket, HTTP_SMUGGLED_REQUEST);
            const std::vector<std::string> lines = responses_of(editor_test::non_empty_lines(reply));
            REQUIRE(lines.size() == 1);
            CHECK(lines[0].find("PARSE_ERROR") != std::string::npos);
            CHECK(fixture.requests.empty());
            CHECK(fixture.server.client_count() == 0);
        }
        SUBCASE("standard input") {
            server_fixture fixture = {};
            std::istringstream input{std::string(HTTP_SMUGGLED_REQUEST)};
            std::ostringstream output = {};
            CONJURE_REQUIRE_OK(fixture.server.add_transport(std::make_unique<stdio_transport>(input, output, false)));
            CHECK(fixture.poll().empty());
            CHECK(fixture.poll().empty());
            fixture.server.flush();
            const std::vector<std::string> lines = responses_of(editor_test::non_empty_lines(output.str()));
            REQUIRE(lines.size() == 1);
            CHECK(lines[0].find("PARSE_ERROR") != std::string::npos);
            CHECK(fixture.server.stdio_closed());
            CHECK(fixture.server.client_count() == 0);
            // The streams end with this scope: the server lets go of them first.
            fixture.server.stop();
        }
    }

    TEST_CASE("protocol_server - a malformed line after a first request is answered and the client stays") {
        server_fixture fixture = {};
        loopback_client client = fixture.connect();
        client.send(R"({"id":1,"cmd":"session.hello"})");
        client.send(std::string(HTTP_SMUGGLED_REQUEST.substr(0, HTTP_SMUGGLED_REQUEST.find('\r'))));
        client.send("");
        client.send(R"({"id":2,"cmd":"session.ping"})");
        const std::vector<protocol_request> requests = fixture.poll();
        REQUIRE(requests.size() == 2);
        CHECK(requests[1].command == "session.ping");
        CHECK(fixture.server.client(client.client_id())->spoke);
        const std::vector<std::string> replies = responses_of(client.receive_all());
        REQUIRE(replies.size() == 2);
        CHECK(replies[0].find("PARSE_ERROR") != std::string::npos);
        CHECK(replies[1].find("PARSE_ERROR") != std::string::npos);
        (void) fixture.poll();
        CHECK(client.is_connected());
    }

    TEST_CASE("protocol_server - access_of tells the two tokens apart and grants nothing for anything else") {
        server_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.server.set_token("tok-7f3a9c"));
        CONJURE_REQUIRE_OK(fixture.server.set_inspect_token("look-51d0e2"));
        CHECK(fixture.server.access_of("tok-7f3a9c") == access_level::FULL_CONTROL);
        CHECK(fixture.server.access_of("look-51d0e2") == access_level::INSPECT);
        for (const std::string_view presented :
          {"", "look-51d0e", "look-51d0e2 ", "LOOK-51D0E2", "look-51d0e2look-51d0e2", "tok-7f3a9clook-51d0e2"}) {
            CAPTURE(presented);
            CHECK(fixture.server.access_of(presented) == access_level::NONE);
        }
    }

    TEST_CASE("protocol_server - a token set for one level gets a generated one for the other, and gates every "
              "transport") {
        server_fixture fixture = {};
        SUBCASE("the inspect-only token alone") {
            CONJURE_REQUIRE_OK(fixture.server.set_inspect_token("look-51d0e2"));
            CHECK(fixture.server.inspect_token() == "look-51d0e2");
            // Nobody was told the full-control token: such a session can be looked at and nothing else.
            const std::string generated = fixture.server.token();
            CHECK(generated.size() == ACCESS_TOKEN_LENGTH);
            CHECK(fixture.server.access_of(generated) == access_level::FULL_CONTROL);
            CHECK(fixture.server.requires_token(transport_kind::TCP));
            CHECK(fixture.server.requires_token(transport_kind::STDIO));
            CHECK(fixture.server.requires_token(transport_kind::LOOPBACK));
            CHECK_FALSE(fixture.logs.contains(generated));
            CHECK_FALSE(fixture.logs.contains("look-51d0e2"));

            // Taking it back leaves nothing that needs a token: none lingers to be accepted.
            CONJURE_REQUIRE_OK(fixture.server.set_inspect_token(""));
            CHECK(fixture.server.token().empty());
            CHECK(fixture.server.inspect_token().empty());
            CHECK_FALSE(fixture.server.requires_token(transport_kind::LOOPBACK));
            CHECK(fixture.server.access_of("look-51d0e2") == access_level::NONE);
            CHECK(fixture.server.access_of(generated) == access_level::NONE);
        }
        SUBCASE("the full-control token alone") {
            CONJURE_REQUIRE_OK(fixture.server.set_token("tok-7f3a9c"));
            const std::string generated = fixture.server.inspect_token();
            CHECK(generated.size() == ACCESS_TOKEN_LENGTH);
            CHECK(generated != fixture.server.token());
            CHECK(fixture.server.access_of(generated) == access_level::INSPECT);
            CHECK(fixture.server.requires_token(transport_kind::LOOPBACK));
            CONJURE_REQUIRE_OK(fixture.server.set_token(""));
            CHECK(fixture.server.token().empty());
            CHECK(fixture.server.inspect_token().empty());
        }
        SUBCASE("both: taking one back keeps every transport gated by the other") {
            CONJURE_REQUIRE_OK(fixture.server.set_token("tok-7f3a9c"));
            CONJURE_REQUIRE_OK(fixture.server.set_inspect_token("look-51d0e2"));
            CONJURE_REQUIRE_OK(fixture.server.set_token(""));
            CHECK(fixture.server.requires_token(transport_kind::LOOPBACK));
            CHECK(fixture.server.token().size() == ACCESS_TOKEN_LENGTH);
            CHECK(fixture.server.access_of("tok-7f3a9c") == access_level::NONE);
            CHECK(fixture.server.access_of("look-51d0e2") == access_level::INSPECT);
        }
    }

    TEST_CASE("protocol_server - one text cannot be both tokens") {
        server_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.server.set_token("same-secret"));
        const std::string generatedInspect = fixture.server.inspect_token();

        const core::result<void> asInspect = fixture.server.set_inspect_token("same-secret");
        CONJURE_CHECK_ERROR(asInspect, protocol_error_code::INVALID_ARGS);
        CHECK(asInspect.get_error().message().find("same-secret") == std::string::npos);
        CHECK(fixture.server.inspect_token() == generatedInspect);
        CHECK(fixture.server.access_of("same-secret") == access_level::FULL_CONTROL);

        // The generated token of the other level is refused as well, not only one that was set.
        CONJURE_CHECK_ERROR(fixture.server.set_token(generatedInspect), protocol_error_code::INVALID_ARGS);
        CHECK(fixture.server.token() == "same-secret");

        CONJURE_REQUIRE_OK(fixture.server.set_inspect_token("look-51d0e2"));
        CONJURE_CHECK_ERROR(fixture.server.set_token("look-51d0e2"), protocol_error_code::INVALID_ARGS);
        CHECK(fixture.server.token() == "same-secret");
        CHECK(fixture.server.access_of("look-51d0e2") == access_level::INSPECT);
        CHECK_FALSE(fixture.logs.contains("same-secret"));
        CHECK_FALSE(fixture.logs.contains("look-51d0e2"));
    }

    TEST_CASE("protocol_server - start takes both tokens from the options and refuses two that are equal") {
        testing::log_capture logs = {};
        event_broadcaster events = {};
        protocol_server server(events);
        editor_options options = {};
        options.headless = true;
        options.aiPort = u16{0};

        SUBCASE("both given") {
            options.aiToken = "tok-7f3a9c";
            options.aiInspectToken = "look-51d0e2";
            CONJURE_REQUIRE_OK(server.start(options, {}));
            CHECK(server.token() == "tok-7f3a9c");
            CHECK(server.inspect_token() == "look-51d0e2");
            CHECK(server.requires_token(transport_kind::LOOPBACK));
        }
        SUBCASE("the inspect-only token alone gates every transport and generates the other") {
            options.aiInspectToken = "look-51d0e2";
            CONJURE_REQUIRE_OK(server.start(options, {}));
            CHECK(server.inspect_token() == "look-51d0e2");
            CHECK(server.token().size() == ACCESS_TOKEN_LENGTH);
            CHECK(server.requires_token(transport_kind::STDIO));
            CHECK(server.requires_token(transport_kind::LOOPBACK));
        }
        SUBCASE("neither given and no TCP transport: no tokens") {
            CONJURE_REQUIRE_OK(server.start(options, {}));
            CHECK(server.token().empty());
            CHECK(server.inspect_token().empty());
            CHECK_FALSE(server.requires_token(transport_kind::LOOPBACK));
        }
        SUBCASE("the same text for both is INVALID_ARGS and nothing starts") {
            options.aiToken = "same-secret";
            options.aiInspectToken = "same-secret";
            const core::result<void> started = server.start(options, {});
            CONJURE_CHECK_ERROR(started, protocol_error_code::INVALID_ARGS);
            CHECK(started.get_error().message().find("same-secret") == std::string::npos);
            CHECK_FALSE(server.is_running());
            CHECK(server.transport_count() == 0);
            CHECK(server.token().empty());
            CHECK(server.inspect_token().empty());
        }
        CHECK_FALSE(logs.contains("tok-7f3a9c"));
        CHECK_FALSE(logs.contains("look-51d0e2"));
        server.stop();
    }

    TEST_CASE("protocol_server - the last accepted session.hello decides a client's access") {
        server_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.server.set_token("tok-7f3a9c"));
        CONJURE_REQUIRE_OK(fixture.server.set_inspect_token("look-51d0e2"));
        loopback_client client = fixture.connect();
        const u32 id = client.client_id();
        CHECK(fixture.server.access_of_client(id) == access_level::NONE);

        CHECK(fixture.server.accept_hello(id, "look-51d0e2"));
        CHECK(fixture.server.access_of_client(id) == access_level::INSPECT);
        CHECK(fixture.server.client(id)->authenticated());
        // A hello that is refused changes nothing.
        CHECK_FALSE(fixture.server.accept_hello(id, "wrong"));
        CHECK_FALSE(fixture.server.accept_hello(id, ""));
        CHECK(fixture.server.access_of_client(id) == access_level::INSPECT);
        // The other token raises the client, and the inspect token lowers it again.
        CHECK(fixture.server.accept_hello(id, "tok-7f3a9c"));
        CHECK(fixture.server.access_of_client(id) == access_level::FULL_CONTROL);
        CHECK(fixture.server.accept_hello(id, "look-51d0e2"));
        CHECK(fixture.server.access_of_client(id) == access_level::INSPECT);

        // A client id no transport knows is the editor's own code: full control, whatever it presents.
        const u32 unknownClient = 9999;
        CHECK(fixture.server.access_of_client(unknownClient) == access_level::FULL_CONTROL);
        CHECK(fixture.server.accept_hello(unknownClient, "look-51d0e2"));
        CHECK(fixture.server.access_of_client(unknownClient) == access_level::FULL_CONTROL);
    }

    TEST_CASE("protocol_server - a client of a transport that asks for no token can restrict itself with the "
              "inspect token") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        // The TCP transport makes the tokens exist; the loopback beside it is not gated by generated ones.
        tcp_server_fixture fixture = {};
        std::unique_ptr<loopback_transport> loopback = std::make_unique<loopback_transport>();
        loopback_transport *transport = loopback.get();
        CONJURE_REQUIRE_OK(fixture.server.add_transport(std::move(loopback)));
        loopback_client client = transport->connect();
        fixture.server.poll(fixture.requests);
        const u32 id = client.client_id();
        REQUIRE_FALSE(fixture.server.requires_token(transport_kind::LOOPBACK));
        CHECK(fixture.server.access_of_client(id) == access_level::FULL_CONTROL);

        CHECK(fixture.server.accept_hello(id, fixture.server.inspect_token()));
        CHECK(fixture.server.access_of_client(id) == access_level::INSPECT);
        // Any other text is accepted, as before, and leaves the client as it is.
        CHECK(fixture.server.accept_hello(id, "not a token"));
        CHECK(fixture.server.accept_hello(id, ""));
        CHECK(fixture.server.access_of_client(id) == access_level::INSPECT);
        CHECK(fixture.server.accept_hello(id, fixture.server.token()));
        CHECK(fixture.server.access_of_client(id) == access_level::FULL_CONTROL);
    }

    TEST_CASE("regression - code sweep F187 a TCP transport gets an inspect-only token beside the full-control one, "
              "each in its own owner-only file") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_server_fixture fixture = {};
        const std::string token = fixture.server.token();
        const std::string inspect = fixture.server.inspect_token();
        CHECK(inspect.size() == ACCESS_TOKEN_LENGTH);
        CHECK(inspect != token);
        CHECK(fixture.server.access_of(inspect) == access_level::INSPECT);
        CHECK(fixture.server.access_of(token) == access_level::FULL_CONTROL);
        CHECK(fixture.server.contains_token("say " + inspect + " twice"));
        CHECK(fixture.server.contains_token(token));
        CHECK_FALSE(fixture.server.contains_token(inspect.substr(1)));
        // A text at the search limit is searched; a longer one is not, and counts as holding a token.
        const std::string longest(protocol_server::TOKEN_SEARCH_BYTES, 'x');
        CHECK_FALSE(fixture.server.contains_token(longest));
        CHECK(fixture.server.contains_token(longest.substr(token.size()) + token));
        CHECK(fixture.server.contains_token(longest + "x"));
        CHECK_FALSE(fixture.logs.contains(inspect));
        if (!fixture.token_file_written()) {
            return;
        }

        CHECK(fixture.server.inspect_token_file() == fixture.session_file("ai_token_inspect"));
        CHECK(editor_test::read_text_file(fixture.session_file("ai_token_inspect")) == inspect + "\n");
        CHECK(fixture.token_from_file() == token);
        const core::result<bool> isPrivate = core::vfs::is_private_file(fixture.server.inspect_token_file());
        CONJURE_REQUIRE_OK(isPrivate);
        CHECK(*isPrivate);

        SUBCASE("the files follow the project") {
            const testing::temp_directory other = {};
            fixture.server.set_project_root(other.path());
            CHECK_FALSE(std::filesystem::exists(fixture.session_file("ai_token_inspect")));
            CHECK_FALSE(std::filesystem::exists(fixture.session_file("ai_token")));
            const std::filesystem::path moved = other.path() / ".conjure" / "ai_token_inspect";
            CHECK(fixture.server.inspect_token_file() == moved);
            CHECK(editor_test::read_text_file(moved) == inspect + "\n");
            // The directory goes with this scope: the server lets go of its files first.
            fixture.server.stop();
            CHECK_FALSE(std::filesystem::exists(moved));
        }
        SUBCASE("stop removes both token files") {
            fixture.server.stop();
            CHECK_FALSE(std::filesystem::exists(fixture.session_file("ai_token_inspect")));
            CHECK_FALSE(std::filesystem::exists(fixture.session_file("ai_token")));
            CHECK(fixture.server.inspect_token_file().empty());
        }
        SUBCASE("set_inspect_token rewrites its file, which stays private, and leaves the other token alone") {
            CONJURE_REQUIRE_OK(fixture.server.set_inspect_token("look-51d0e2"));
            CHECK(editor_test::read_text_file(fixture.session_file("ai_token_inspect")) == "look-51d0e2\n");
            CHECK(core::vfs::is_private_file(fixture.server.inspect_token_file()).value_or(false));
            CHECK(fixture.token_from_file() == token);
            CHECK(fixture.server.access_of(inspect) == access_level::NONE);
            CONJURE_REQUIRE_OK(fixture.server.set_inspect_token(""));
            const std::string regenerated = fixture.server.inspect_token();
            CHECK(regenerated.size() == ACCESS_TOKEN_LENGTH);
            CHECK(regenerated != inspect);
            CHECK(editor_test::read_text_file(fixture.session_file("ai_token_inspect")) == regenerated + "\n");
            CHECK(fixture.server.access_of("look-51d0e2") == access_level::NONE);
        }
    }

    TEST_CASE("regression - code sweep F187 a TCP client that presents the inspect token is let in with inspect "
              "access") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        tcp_server_fixture fixture = {};
        const std::string token = fixture.server.token();
        const std::string inspect = fixture.server.inspect_token();
        const net::socket_handle socket = fixture.connect();
        fixture.send_until_accepted(
          socket,
          std::format(R"({{"id":1,"cmd":"session.hello","args":{{"token":"{}"}}}})", inspect) + "\n",
          1
        );
        const u32 id = fixture.requests[0].clientId;
        CHECK(fixture.server.client(id)->access == access_level::INSPECT);
        CHECK(fixture.server.access_of_client(id) == access_level::INSPECT);
        // The server lets every later request through: which of them an inspect-only client may run is the
        // dispatcher's decision, from the command's flags.
        fixture.send_until_accepted(socket, std::string(R"({"id":2,"cmd":"entity.create"})") + "\n", 2);
        CHECK(fixture.requests[1].command == "entity.create");

        // An inspect-only client hears events like any other, and neither token is in them.
        (void) fixture.events.emit(editor_event::SCENE_CHANGED, core::json::object({{"revision", 2}}));
        const std::vector<std::string> events = fixture.read_lines(socket, 1);
        REQUIRE(events.size() == 1);
        CHECK(events[0].find("scene.changed") != std::string::npos);
        CHECK(events[0].find(inspect) == std::string::npos);
        CHECK(events[0].find(token) == std::string::npos);
        CHECK_FALSE(fixture.logs.contains(inspect));
        CHECK_FALSE(fixture.logs.contains(token));
    }

    TEST_CASE("regression - code sweep F187 the commands of transport clients are recorded under the project, with "
              "transport and access and without a token") {
        // Declared before the server, which closes its log before the folder goes.
        const testing::temp_directory project = {};
        server_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.server.set_token("tok-7f3a9c"));
        CONJURE_REQUIRE_OK(fixture.server.set_inspect_token("look-51d0e2"));
        fixture.server.set_project_root(project.path());
        const std::filesystem::path log = project.path() / ".conjure" / "ai_audit.log";
        CHECK(fixture.server.audit().file() == log);
        // Nothing is created for a session in which no client did anything.
        CHECK_FALSE(std::filesystem::exists(project.path() / ".conjure"));

        loopback_client client = fixture.connect();
        const u32 id = client.client_id();
        REQUIRE(fixture.server.accept_hello(id, "look-51d0e2"));
        const core::json::document helloArgs = core::json::object({{"token", "look-51d0e2"}, {"clientName", "agent"}});
        audit_entry hello = {};
        hello.command = "session.hello";
        hello.args = helloArgs.root();
        hello.clientId = id;
        fixture.server.record_command(hello);

        const core::json::document createArgs = core::json::object({{"name", "my token is tok-7f3a9c"}});
        audit_entry refused = {};
        refused.command = "entity.create";
        refused.args = createArgs.root();
        refused.clientId = id;
        refused.ok = false;
        refused.error = protocol_error_code::UNAUTHORISED;
        fixture.server.record_command(refused);

        // The editor's own code (client 0, an --exec run, an editor.exec_file task) is not a protocol client.
        audit_entry inProcess = {};
        inProcess.command = "editor.quit";
        inProcess.clientId = 9999;
        fixture.server.record_command(inProcess);
        inProcess.clientId = 0;
        fixture.server.record_command(inProcess);

        REQUIRE(fixture.server.accept_hello(id, "tok-7f3a9c"));
        audit_entry raised = {};
        raised.command = "editor.quit";
        raised.clientId = id;
        fixture.server.record_command(raised);
        // flush() is what hands the records to the file, once per frame.
        fixture.server.flush();

        const std::string text = editor_test::read_text_file(log);
        CHECK(text.find("tok-7f3a9c") == std::string::npos);
        CHECK(text.find("look-51d0e2") == std::string::npos);
        const std::vector<core::json::document> records = editor_test::read_json_lines(log);
        REQUIRE(records.size() == 3);
        CHECK(records[0].root()["cmd"].as_string() == "session.hello");
        CHECK(records[0].root()["clientId"].as_u64() == id);
        CHECK(records[0].root()["transport"].as_string() == "loopback");
        CHECK(records[0].root()["access"].as_string() == "inspect");
        CHECK(records[0].root()["args"]["token"].as_string() == audit_log::WITHHELD);
        CHECK(records[0].root()["args"]["clientName"].as_string() == "agent");
        CHECK(records[1].root()["cmd"].as_string() == "entity.create");
        CHECK(records[1].root()["access"].as_string() == "inspect");
        CHECK(records[1].root()["error"].as_string() == "UNAUTHORISED");
        CHECK(records[1].root()["args"]["name"].as_string() == "my token is [token]");
        CHECK(records[2].root()["cmd"].as_string() == "editor.quit");
        CHECK(records[2].root()["access"].as_string() == "full_control");
        CHECK_FALSE(fixture.logs.contains("tok-7f3a9c"));
        CHECK_FALSE(fixture.logs.contains("look-51d0e2"));

        SUBCASE("the log follows the project, and without one nothing is recorded") {
            const testing::temp_directory other = {};
            fixture.server.set_project_root(other.path());
            fixture.server.record_command(raised);
            fixture.server.set_project_root({});
            fixture.server.record_command(raised);
            fixture.server.flush();
            CHECK(editor_test::read_json_lines(log).size() == 3);
            const std::filesystem::path moved = other.path() / ".conjure" / "ai_audit.log";
            CHECK(editor_test::read_json_lines(moved).size() == 1);
            CHECK(fixture.server.audit().file().empty());
        }
        SUBCASE("stop closes the log and leaves it on disk") {
            fixture.server.stop();
            CHECK_FALSE(fixture.server.audit().is_open());
            CHECK(editor_test::read_json_lines(log).size() == 3);
        }
    }
}
