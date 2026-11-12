#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/headless_runner.h"
#include "conjure/editor/protocol/loopback_transport.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/protocol/protocol_server.h"

#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <vector>

// The AI protocol end to end (docs/design/08_testing.md 6.3 items 1 and 4, 04_editor.md 10): an agent session over
// the loopback transport, through protocol_server and the dispatcher, driving the Grid Sweeper command file and
// reading the events back.

using namespace conjure;
using namespace conjure::editor;

namespace {
    /// <summary>
    /// A client on the loopback transport: sends a line, pumps frames until the response with the id arrives,
    /// and keeps the events seen on the way.
    /// </summary>
    class loopback_session {
        private:
            editor_test::headless_editor *mEditor = nullptr;
            loopback_client mClient = {};
            std::vector<protocol_event> mEvents = {};
            i64 mNextId = 1;

        public:
            explicit loopback_session(editor_test::headless_editor &editor) :
                mEditor(&editor) {
                std::unique_ptr<loopback_transport> transport = std::make_unique<loopback_transport>();
                mClient = transport->connect();
                const core::result<void> added = editor.application().server().add_transport(std::move(transport));
                REQUIRE_MESSAGE(added.has_value(), testing::describe_error(added));
                editor.pump(1);
                REQUIRE(mClient.is_connected());
            }

        public:
            [[nodiscard]] protocol_response request(
              std::string_view command,
              core::json::document args = core::json::document::object()
            ) {
                protocol_request request = {};
                request.id = make_id(mNextId++);
                request.command = std::string(command);
                request.args = std::move(args);
                return send_line(serialise(request), request.id.root().as_i64());
            }

            [[nodiscard]] protocol_response send_line(const std::string &line, i64 expectedId) {
                mClient.send(line);
                for (u32 frame = 0; frame < 20000; ++frame) {
                    mEditor->pump(1);
                    for (std::string &received : mClient.receive_all()) {
                        const core::result<protocol_event> event = parse_event(received);
                        if (event.has_value()) {
                            mEvents.push_back(event.value());
                            continue;
                        }
                        core::result<protocol_response> response = parse_response(received);
                        REQUIRE_MESSAGE(response.has_value(), received);
                        if (response.value().id.root().as_i64(-1) == expectedId) {
                            return std::move(response).value();
                        }
                    }
                }
                FAIL("no response to " << line);
                return {};
            }

            [[nodiscard]] core::json::document ok(
              std::string_view command,
              core::json::document args = core::json::document::object()
            ) {
                const protocol_response response = request(command, std::move(args));
                REQUIRE_MESSAGE(
                  response.ok,
                  std::format("{}: {} {}", command, to_string(response.errorCode), response.errorMessage)
                );
                return response.result;
            }

            [[nodiscard]] const std::vector<protocol_event> &events() const noexcept {
                return mEvents;
            }

            [[nodiscard]] usize count_events(std::string_view name) const {
                usize count = 0;
                for (const protocol_event &event : mEvents) {
                    count += event.name == name ? 1 : 0;
                }
                return count;
            }

            [[nodiscard]] loopback_client &client() noexcept {
                return mClient;
            }
    };
}

TEST_SUITE("editor/ai_protocol_e2e") {

    TEST_CASE("ai_protocol_e2e - hello, schema, a mutation with its events, undo and a bad request over loopback") {
        editor_test::headless_editor editor = {};
        loopback_session session(editor);
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
        loopback_session session(editor);
        editor.context().set_token("secret");
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
        loopback_session session(editor);
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
}
