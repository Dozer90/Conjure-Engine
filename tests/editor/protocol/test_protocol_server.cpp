#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "conjure/testing/test_environment.h"

#include "editor_test_support.h"

#include "conjure/editor/editor_options.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/protocol/loopback_transport.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/protocol/protocol_server.h"
#include "editor/core/protocol/stdio_transport.h"
#include "editor/core/protocol/tcp_transport.h"

#include <filesystem>
#include <format>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// protocol_server.h: client bookkeeping over transports, the token handshake, request caps, event delivery and
// the backlog policy. The loopback transport stands in for TCP; the port tests use a real socket.

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
        CHECK(state->authenticated);
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
        client.send(R"({"id":7,"cmd":5})");
        client.send(R"({"id":"a","cmd":"x","args":3})");
        client.send(R"({"id":[1],"cmd":"x"})");
        client.send("[1, 2]");
        client.send("not json");
        CHECK(fixture.poll().empty());
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
        fixture.server.set_token("secret");
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
        fixture.server.set_token("secret");
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
        fixture.server.set_token("secret");
        CHECK(fixture.server.token() == "secret");
        loopback_client wrong = fixture.connect();
        CHECK_FALSE(fixture.server.client(wrong.client_id())->authenticated);
        wrong.send(R"({"id":1,"cmd":"editor.state"})");
        std::vector<protocol_request> requests = fixture.poll();
        CHECK(requests.empty());
        std::vector<std::string> replies = responses_of(wrong.receive_all());
        REQUIRE(replies.size() == 1);
        CHECK(replies[0].find("UNAUTHORISED") != std::string::npos);
        (void) fixture.poll();
        CHECK_FALSE(wrong.is_connected());

        loopback_client right = fixture.connect();
        right.send(R"({"id":1,"cmd":"session.hello","args":{"token":"secret"}})");
        requests = fixture.poll();
        REQUIRE(requests.size() == 1);
        CHECK(requests[0].command == "session.hello");
        CHECK(fixture.server.client(right.client_id())->authenticated);
        right.send(R"({"id":2,"cmd":"editor.state"})");
        requests = fixture.poll();
        REQUIRE(requests.size() == 1);
        fixture.server.mark_authenticated(right.client_id());
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

        event_broadcaster secondEvents = {};
        protocol_server second(secondEvents);
        editor_options fallback = options;
        fallback.aiPort.reset();
        CONJURE_REQUIRE_OK(second.start(fallback, {}));
        CHECK(second.bound_port() != 0);
        CHECK(second.bound_port() != first.bound_port());

        event_broadcaster thirdEvents = {};
        protocol_server third(thirdEvents);
        editor_options busy = options;
        busy.aiPort = first.bound_port();
        CHECK_FALSE(third.start(busy, {}).has_value());
        second.stop();
        first.stop();
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

        server.set_project_root(projectB);
        const std::filesystem::path portFileB = projectB / ".conjure" / "ai_port";
        CHECK_FALSE(std::filesystem::exists(portFileA));
        CHECK(server.port_file() == portFileB);
        REQUIRE(std::filesystem::is_regular_file(portFileB));
        CHECK(editor_test::read_text_file(portFileB) == expected);

        server.set_project_root({});
        CHECK_FALSE(std::filesystem::exists(portFileB));
        CHECK(server.port_file().empty());
        server.stop();
    }

    TEST_CASE("protocol_server - set_project_root writes no port file without a TCP transport") {
        server_fixture fixture = {};
        testing::temp_directory project = {};
        fixture.server.set_project_root(project.path());
        CHECK(fixture.server.port_file().empty());
        CHECK_FALSE(std::filesystem::exists(project.path() / ".conjure" / "ai_port"));
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
}
