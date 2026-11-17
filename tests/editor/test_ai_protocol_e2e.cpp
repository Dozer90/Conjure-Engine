#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/testing/test_environment.h"

#include "conjure/core/json/document.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/native_file_system.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/headless_runner.h"
#include "conjure/editor/protocol/access_token.h"
#include "conjure/editor/protocol/loopback_transport.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/protocol/protocol_server.h"
#include "editor/core/net/socket.h"
#include "editor/core/protocol/tcp_transport.h"

#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The AI protocol end to end (docs/design/08_testing.md 6.3 items 1 and 4, 04_editor.md 10): an agent session over
// the loopback transport, through protocol_server and the dispatcher, driving the Grid Sweeper command file and
// reading the events back.

using namespace conjure;
using namespace conjure::editor;

namespace {
    /// <summary>
    /// Sends one request line over a real socket and pumps the editor until a response came back (events on
    /// the way are skipped). nullopt when the server closed the connection or never answered.
    /// </summary>
    [[nodiscard]] std::optional<protocol_response> socket_request(
      editor_test::headless_editor &editor,
      const net::socket_handle &socket,
      std::string_view line
    ) {
        editor_test::send_all(socket, std::string(line) + "\n");
        std::string received = {};
        for (u32 frame = 0; frame < 20000; ++frame) {
            editor.pump(1);
            received += editor_test::receive_some(socket, 1, 4);
            const usize lineEnd = received.rfind('\n');
            if (lineEnd == std::string::npos) {
                continue;
            }
            for (const std::string &complete : editor_test::non_empty_lines(received.substr(0, lineEnd))) {
                core::result<protocol_response> response = parse_response(complete);
                if (response.has_value()) {
                    return std::move(response).value();
                }
            }
            received.erase(0, lineEnd + 1);
        }
        return std::nullopt;
    }
}

TEST_SUITE("editor/ai_protocol_e2e") {

    TEST_CASE("ai_protocol_e2e - hello, schema, a mutation with its events, undo and a bad request over loopback") {
        editor_test::headless_editor editor = {};
        editor_test::loopback_session session(editor);
        const core::json::document hello = session.ok("session.hello", core::json::object({{"clientName", "e2e"}}));
        CHECK(hello.root()["clientId"].as_u64() == session.client().client_id());
        const core::json::document schema = session.ok("editor.schema");
        CHECK(schema.root()["schemaHash"].as_string() == hello.root()["schemaHash"].as_string());

        const core::json::document created = session.ok("entity.create", core::json::object({{"name", "Remote"}}));
        const std::string guid = std::string(created.root()["guid"].as_string());
        CHECK(created.root()["revision"].as_u64() >= 1);
        CHECK(session.count_events("scene.changed") >= 1);
        bool revisionInEvent = false;
        for (const protocol_event &event : session.events()) {
            if (event.name == "scene.changed") {
                revisionInEvent = event.data.root()["revision"].as_u64() == created.root()["revision"].as_u64();
                CHECK_FALSE(event.data.root()["label"].as_string().empty());
            }
        }
        CHECK(revisionInEvent);
        (void) session.ok("selection.set", core::json::object({{"entities", core::json::array({guid})}}));
        CHECK(session.count_events("selection.changed") >= 1);

        // An unknown command and bad arguments answer with the echoed id.
        protocol_response unknown = session.request("entity.craete", core::json::object({{"name", "x"}}));
        CHECK_FALSE(unknown.ok);
        CHECK(unknown.errorCode == protocol_error_code::UNKNOWN_COMMAND);
        CHECK(unknown.errorDetails.root()["didYouMean"].size() >= 1);
        protocol_response invalid = session.request("entity.create", core::json::object({{"name", 5}}));
        CHECK(invalid.errorCode == protocol_error_code::INVALID_ARGS);
        CHECK(invalid.errorMessage.find("args.name") != std::string::npos);

        // Unsubscribing stops delivery.
        (void) session.ok("session.subscribe", core::json::object({{"events", core::json::document::array()}}));
        const usize before = session.count_events("scene.changed");
        (void) session.ok("history.undo");
        CHECK(session.count_events("scene.changed") == before);
        CHECK(editor.scene().world().entity_count() == 0);

        // Asynchronous replies arrive through the same transport.
        const core::json::document waited = session.ok("editor.wait_frames", core::json::object({{"frames", 2}}));
        CHECK(waited.root().contains("frame"));
    }

    TEST_CASE("regression - F477 a loopback client's session.hello with a wrong token is UNAUTHORISED") {
        editor_test::headless_editor editor = {};
        // Connected before the token exists, so the server's handshake let the client in: only the handler checks.
        editor_test::loopback_session session(editor);
        CONJURE_REQUIRE_OK(editor.application().server().set_token("secret"));
        const protocol_response wrong = session.request("session.hello", core::json::object({{"token", "wrong"}}));
        CHECK_FALSE(wrong.ok);
        CHECK(wrong.errorCode == protocol_error_code::UNAUTHORISED);
        const core::json::document hello = session.ok("session.hello", core::json::object({{"token", "secret"}}));
        CHECK(hello.root()["clientId"].as_u64() == session.client().client_id());
        // The in-process client of the same editor is trusted whatever token it sends.
        (void) editor.ok("session.hello", core::json::object({{"token", "wrong"}}));
    }

    TEST_CASE(
      "ai_protocol_e2e - the Grid Sweeper command file builds the game over loopback and the saved scene reloads"
    ) {
        editor_test::headless_editor editor = {};
        editor_test::loopback_session session(editor);
        const std::string text = editor_test::read_text_file(
          editor_test::fixture_directory() / "build_grid_sweeper.json"
        );
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(text);
        CONJURE_REQUIRE_OK(requests);
        REQUIRE(requests.value().size() > 10);
        i64 id = 1000;
        for (protocol_request request : requests.value()) {
            headless_runner::expand_placeholders(request, editor.project_directory());
            request.id = make_id(id);
            const protocol_response response = session.send_line(serialise(request), id);
            CHECK_MESSAGE(
              response.ok,
              std::format("{}: {} {}", request.command, to_string(response.errorCode), response.errorMessage)
            );
            ++id;
        }
        CHECK(session.count_events("play.state") >= 2);
        CHECK(session.count_events("scene.loaded") >= 1);
        CHECK(session.count_events("asset.imported") >= 1);

        const core::json::document info = session.ok("scene.info");
        CHECK(info.root()["entityCount"].as_i64() == 68);
        const u64 hash = editor.world_hash();
        const std::string path = std::string(info.root()["path"].as_string());
        CHECK(
          std::filesystem::is_regular_file(editor.project_directory() / "assets" / "scenes" / "grid_sweeper.scene.json")
        );
        (void) session.ok("scene.new", core::json::object({{"name", "scratch"}}));
        CHECK(editor.scene().world().entity_count() == 0);
        (void) session.ok("scene.open", core::json::object({{"path", path}}));
        CHECK(editor.world_hash() == hash);
        CHECK(editor.scene().world().entity_count() == 68);
    }

    TEST_CASE("regression - code sweep F740 a TCP client reads the port and the token from the project, says hello "
      "and drives the editor") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        editor_test::headless_editor editor = {};
        // Everything the editor says while the client works is kept, to look for the token in it afterwards.
        const u64 everything = default_subscriptions() | event_bit(editor_event::LOG)
                               | event_bit(editor_event::COMMAND_EXECUTED);
        editor.context().events().set_subscriptions(editor.client_id(), everything);
        protocol_server &server = editor.application().server();
        CONJURE_REQUIRE_OK(server.add_transport(std::make_unique<tcp_transport>(u16{0}, u16{0})));
        server.set_project_root(editor.project_directory());
        if (server.token_file().empty()) {
            REQUIRE(editor.log().contains(core::log_level::WARN, "cannot be restricted to its owner"));
            MESSAGE("this volume keeps no owner-only files: the case did not run");
            return;
        }

        // What a client does: both files from <project>/.conjure, nothing else known in advance.
        const std::filesystem::path sessionDirectory = editor.project_directory() / ".conjure";
        const std::string portText = editor_test::read_text_file(sessionDirectory / "ai_port");
        const std::optional<u64> port = core::str::parse_uint(core::str::trim(portText));
        REQUIRE(port.has_value());
        REQUIRE(*port == server.bound_port());
        const std::string token = std::string(
          core::str::trim(editor_test::read_text_file(sessionDirectory / "ai_token"))
        );
        CHECK(core::vfs::is_private_file(sessionDirectory / "ai_token").value_or(false));
        CONJURE_REQUIRE_OK(net::initialise());
        const core::result<net::socket_handle> socket = net::connect_loopback(static_cast<u16>(*port));
        CONJURE_REQUIRE_OK(socket);

        const std::optional<protocol_response> hello = socket_request(
          editor,
          socket.value(),
          std::format(R"({{"id":1,"cmd":"session.hello","args":{{"clientName":"tcp-e2e","token":"{}"}}}})", token)
        );
        REQUIRE(hello.has_value());
        REQUIRE_MESSAGE(hello->ok, hello->errorMessage);
        CHECK(hello->result.root()["protocolVersion"].as_i64() == PROTOCOL_VERSION);
        const std::optional<protocol_response> created = socket_request(
          editor,
          socket.value(),
          R"({"id":2,"cmd":"entity.create","args":{"name":"Made over TCP"}})"
        );
        REQUIRE(created.has_value());
        REQUIRE_MESSAGE(created->ok, created->errorMessage);
        CHECK(editor.scene().world().entity_count() == 1);
        // A second hello with a wrong token is refused by the handler, and the session goes on.
        const std::optional<protocol_response> wrong = socket_request(
          editor,
          socket.value(),
          R"({"id":3,"cmd":"session.hello","args":{"token":"wrong"}})"
        );
        REQUIRE(wrong.has_value());
        CHECK_FALSE(wrong->ok);
        CHECK(wrong->errorCode == protocol_error_code::UNAUTHORISED);
        CHECK(wrong->errorMessage.find(token) == std::string::npos);
        const std::optional<protocol_response> ping = socket_request(
          editor,
          socket.value(),
          R"({"id":4,"cmd":"session.ping"})"
        );
        REQUIRE(ping.has_value());
        CHECK(ping->ok);

        // No log line and no event carries the token: not the hello's command.executed, not the client's name line.
        CHECK_FALSE(editor.log().contains(token));
        CHECK(editor.saw_event("command.executed"));
        for (const protocol_event &event : editor.events()) {
            CAPTURE(event.name);
            CHECK(serialise(event).find(token) == std::string::npos);
        }
        net::shutdown_sockets();
    }

    TEST_CASE("regression - code sweep F187 an inspect-only TCP client reads its token from the project, looks, is "
              "refused every change, and all of it is on record") {
        CONJURE_TEST_REQUIRE(testing::precondition::LOOPBACK_TCP);
        editor_test::headless_editor editor = {};
        const std::string watched = editor.create_entity("Watched");
        const u64 everything = default_subscriptions() | event_bit(editor_event::LOG)
                               | event_bit(editor_event::COMMAND_EXECUTED);
        editor.context().events().set_subscriptions(editor.client_id(), everything);
        protocol_server &server = editor.application().server();
        CONJURE_REQUIRE_OK(server.add_transport(std::make_unique<tcp_transport>(u16{0}, u16{0})));
        server.set_project_root(editor.project_directory());
        if (server.inspect_token_file().empty()) {
            REQUIRE(editor.log().contains(core::log_level::WARN, "cannot be restricted to its owner"));
            MESSAGE("this volume keeps no owner-only files: the case did not run");
            return;
        }

        // What an inspect-only client does: the port and its own token file, nothing else known in advance.
        const std::filesystem::path sessionDirectory = editor.project_directory() / ".conjure";
        const std::optional<u64> port = core::str::parse_uint(
          core::str::trim(editor_test::read_text_file(sessionDirectory / "ai_port"))
        );
        REQUIRE(port.has_value());
        const std::string inspect = std::string(
          core::str::trim(editor_test::read_text_file(sessionDirectory / "ai_token_inspect"))
        );
        const std::string full = std::string(
          core::str::trim(editor_test::read_text_file(sessionDirectory / "ai_token"))
        );
        REQUIRE(inspect.size() == ACCESS_TOKEN_LENGTH);
        REQUIRE(inspect != full);
        CHECK(core::vfs::is_private_file(sessionDirectory / "ai_token_inspect").value_or(false));
        CONJURE_REQUIRE_OK(net::initialise());
        const core::result<net::socket_handle> socket = net::connect_loopback(static_cast<u16>(*port));
        CONJURE_REQUIRE_OK(socket);

        const std::optional<protocol_response> hello = socket_request(
          editor,
          socket.value(),
          std::format(R"({{"id":1,"cmd":"session.hello","args":{{"clientName":"watcher","token":"{}"}}}})", inspect)
        );
        REQUIRE(hello.has_value());
        REQUIRE_MESSAGE(hello->ok, hello->errorMessage);
        CHECK(hello->result.root()["access"].as_string() == "inspect");
        const std::optional<protocol_response> found = socket_request(
          editor,
          socket.value(),
          R"({"id":2,"cmd":"entity.find","args":{"name":"Watched"}})"
        );
        REQUIRE(found.has_value());
        REQUIRE_MESSAGE(found->ok, found->errorMessage);
        CHECK(found->result.root()["total"].as_u64() == 1);

        const std::optional<protocol_response> destroyed = socket_request(
          editor,
          socket.value(),
          std::format(R"({{"id":3,"cmd":"entity.destroy","args":{{"entity":"{}"}}}})", watched)
        );
        REQUIRE(destroyed.has_value());
        CHECK_FALSE(destroyed->ok);
        CHECK(destroyed->errorCode == protocol_error_code::UNAUTHORISED);
        CHECK(destroyed->errorMessage.find(inspect) == std::string::npos);
        CHECK(destroyed->errorMessage.find(full) == std::string::npos);
        const std::optional<protocol_response> quit = socket_request(
          editor,
          socket.value(),
          R"({"id":4,"cmd":"editor.quit"})"
        );
        REQUIRE(quit.has_value());
        CHECK(quit->errorCode == protocol_error_code::UNAUTHORISED);
        CHECK(editor.scene().world().entity_count() == 1);
        CHECK_FALSE(editor.application().quit_requested());

        // The inspect token does not lead to the other one: the one command that reads project files ends in
        // scripts/, and the refusal is not an oracle for the file's content.
        const std::optional<protocol_response> peek = socket_request(
          editor,
          socket.value(),
          R"({"id":5,"cmd":"script.read","args":{"path":"../.conjure/ai_token"}})"
        );
        REQUIRE(peek.has_value());
        CHECK_FALSE(peek->ok);
        CHECK(serialise(*peek).find(full) == std::string::npos);

        // With the other token the same connection may change things.
        const std::optional<protocol_response> raised = socket_request(
          editor,
          socket.value(),
          std::format(R"({{"id":6,"cmd":"session.hello","args":{{"token":"{}"}}}})", full)
        );
        REQUIRE(raised.has_value());
        REQUIRE_MESSAGE(raised->ok, raised->errorMessage);
        CHECK(raised->result.root()["access"].as_string() == "full_control");
        const std::optional<protocol_response> renamed = socket_request(
          editor,
          socket.value(),
          std::format(R"({{"id":7,"cmd":"entity.rename","args":{{"entity":"{}","name":"Renamed"}}}})", watched)
        );
        REQUIRE(renamed.has_value());
        CHECK_MESSAGE(renamed->ok, renamed->errorMessage);

        // On record: who, over what, with which access, what, and how it ended. Not on record: either token.
        editor.pump(1);
        const std::string audit = editor_test::read_text_file(sessionDirectory / "ai_audit.log");
        CHECK(audit.find(inspect) == std::string::npos);
        CHECK(audit.find(full) == std::string::npos);
        const std::vector<std::string> lines = editor_test::non_empty_lines(audit);
        REQUIRE(lines.size() == 7);
        CHECK(lines[0].find(R"("transport":"tcp","access":"inspect","cmd":"session.hello")") != std::string::npos);
        CHECK(lines[0].find(R"("clientName":"watcher")") != std::string::npos);
        CHECK(lines[2].find(R"("cmd":"entity.destroy")") != std::string::npos);
        CHECK(lines[2].find(R"("ok":false,"error":"UNAUTHORISED")") != std::string::npos);
        CHECK(lines[2].find(watched) != std::string::npos);
        CHECK(lines[6].find(R"("access":"full_control","cmd":"entity.rename")") != std::string::npos);
        CHECK(lines[6].find(R"("ok":true)") != std::string::npos);
        CHECK(core::vfs::is_private_file(sessionDirectory / "ai_audit.log").value_or(false));

        // No log line and no event carries a token either.
        CHECK_FALSE(editor.log().contains(inspect));
        CHECK_FALSE(editor.log().contains(full));
        for (const protocol_event &event : editor.events()) {
            CAPTURE(event.name);
            const std::string text = serialise(event);
            CHECK(text.find(inspect) == std::string::npos);
            CHECK(text.find(full) == std::string::npos);
        }
        net::shutdown_sockets();
    }
}
