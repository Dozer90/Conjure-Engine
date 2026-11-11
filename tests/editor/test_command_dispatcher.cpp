#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/event.h"
#include "conjure/core/json/document.h"
#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/command_registry.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/undo_stack.h"

#include <string>
#include <vector>

// command_dispatcher.h: the request pipeline of docs/design/04_editor.md 3.3.

using namespace conjure;
using namespace conjure::editor;

namespace {
    protocol_request request_of(std::string_view command, core::json::document args, u32 clientId, i64 id = 1) {
        protocol_request request = {};
        request.id = make_id(id);
        request.command = std::string(command);
        request.args = std::move(args);
        request.clientId = clientId;
        return request;
    }
}

TEST_SUITE("editor/command_dispatcher") {

    TEST_CASE("command_dispatcher - an unknown command answers UNKNOWN_COMMAND with suggestions") {
        editor_test::headless_editor editor = {};
        const protocol_response response = editor.send("entity.craete", core::json::object({{"name", "x"}}));
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorCode == protocol_error_code::UNKNOWN_COMMAND);
        const core::json::value suggestions = response.errorDetails.root()["didYouMean"];
        REQUIRE(suggestions.is_array());
        REQUIRE(suggestions.size() >= 1);
        CHECK(suggestions[0].as_string() == "entity.create");
        CHECK(response.id.root().as_i64() > 0);
    }

    TEST_CASE("command_dispatcher - schema violations are INVALID_ARGS before the handler runs") {
        editor_test::headless_editor editor = {};
        const u64 before = editor.context().revision();
        const protocol_response response = editor.send("entity.create", core::json::object({{"name", 12}}));
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorCode == protocol_error_code::INVALID_ARGS);
        CHECK(response.errorMessage.find("args.name") != std::string::npos);
        CHECK(editor.context().revision() == before);
        CHECK(editor.scene().world().entity_count() == 0);
    }

    TEST_CASE("command_dispatcher - preconditions answer SCENE_NOT_OPEN, PROJECT_NOT_OPEN and WRONG_PLAY_STATE") {
        editor_test::headless_editor_options options = {};
        options.withScene = false;
        editor_test::headless_editor editor(options);
        (void) editor.ok("scene.new", core::json::object({{"name", "s"}}));
        (void) editor.ok("scene.close", core::json::object({{"discard", true}}));
        CHECK(editor.fail("entity.create", core::json::object({{"name", "x"}})) == protocol_error_code::SCENE_NOT_OPEN);
        (void) editor.ok("scene.new", core::json::object({{"name", "s"}}));
        (void) editor.ok("play.start");
        const protocol_response response = editor.send("scene.new", core::json::object({{"name", "again"}}));
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorCode == protocol_error_code::WRONG_PLAY_STATE);
        CHECK(response.errorDetails.root()["state"].as_string() == "playing");
        CHECK(editor.fail("play.start") == protocol_error_code::WRONG_PLAY_STATE);
        (void) editor.ok("play.stop");
        CHECK(editor.fail("play.stop") == protocol_error_code::WRONG_PLAY_STATE);
    }

    TEST_CASE("command_dispatcher - a mutating command records undo, bumps the revision and emits events") {
        editor_test::headless_editor editor = {};
        editor.clear_events();
        const u64 before = editor.context().revision();
        const core::json::document result = editor.ok("entity.create", core::json::object({{"name", "x"}}));
        CHECK(result.root()["revision"].as_u64() == before + 1);
        CHECK(editor.context().undo().size() == 1);
        CHECK(editor.context().undo().entries().front()->label == "Create 'x'");
        CHECK(editor.saw_event("scene.changed"));
        bool executed = false;
        for (const protocol_event &event : editor.events()) {
            if (event.name == "command.executed") {
                executed = true;
                CHECK(event.data.root()["cmd"].as_string() == "entity.create");
                CHECK(event.data.root()["ok"].as_bool());
                CHECK(event.data.root()["clientId"].as_u64() == editor.client_id());
            }
        }
        CHECK_FALSE(executed);
        editor.context().events().set_subscriptions(
          editor.client_id(),
          default_subscriptions() | event_bit(editor_event::COMMAND_EXECUTED)
        );
        editor.clear_events();
        (void) editor.ok("editor.state");
        CHECK(editor.saw_event("command.executed"));
    }

    TEST_CASE("command_dispatcher - a read-only command does not touch the undo stack or the revision") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("x");
        const u64 before = editor.context().revision();
        const usize entries = editor.context().undo().size();
        (void) editor.ok("scene.info");
        (void) editor.ok("entity.find", core::json::object({{"name", "x"}}));
        CHECK(editor.context().revision() == before);
        CHECK(editor.context().undo().size() == entries);
    }

    TEST_CASE("command_dispatcher - an asynchronous command defers and resolves on a later frame") {
        editor_test::headless_editor editor = {};
        command_dispatcher &dispatcher = editor.context().dispatcher();
        std::optional<protocol_response> captured = std::nullopt;
        const u32 client = editor.context().events().register_client({});
        editor.context().register_response_sink(client, [&captured](const protocol_response &response) {
            captured = response;
        });
        const std::optional<protocol_response> immediate =
          dispatcher.dispatch(request_of("editor.wait_frames", core::json::object({{"frames", 3}}), client, 77));
        CHECK_FALSE(immediate.has_value());
        CHECK(dispatcher.pending_count(client) == 1);
        CHECK(dispatcher.pending_count() == 1);
        editor.pump(1);
        CHECK_FALSE(captured.has_value());
        editor.pump(4);
        REQUIRE(captured.has_value());
        CHECK(captured->ok);
        CHECK(captured->id.root().as_i64() == 77);
        CHECK(captured->result.root()["frame"].as_u64() >= 3);
        CHECK(dispatcher.pending_count(client) == 0);
        editor.context().unregister_response_sink(client);
        editor.context().events().unregister_client(client);
    }

    TEST_CASE("command_dispatcher - the ninth pending request of a client is BUSY") {
        editor_test::headless_editor editor = {};
        command_dispatcher &dispatcher = editor.context().dispatcher();
        const u32 client = editor.context().events().register_client({});
        for (u32 index = 0; index < command_dispatcher::DEFAULT_MAX_PENDING_PER_CLIENT; ++index) {
            CHECK_FALSE(dispatcher.dispatch(request_of(
              "editor.wait_frames",
              core::json::object({{"frames", 1000}}),
              client
            )).has_value());
        }
        const std::optional<protocol_response> ninth =
          dispatcher.dispatch(request_of("editor.wait_frames", core::json::object({{"frames", 1}}), client));
        REQUIRE(ninth.has_value());
        CHECK_FALSE(ninth->ok);
        CHECK(ninth->errorCode == protocol_error_code::BUSY);
        // Another client is unaffected.
        const u32 other = editor.context().events().register_client({});
        CHECK_FALSE(dispatcher.dispatch(request_of(
          "editor.wait_frames",
          core::json::object({{"frames", 1}}),
          other
        )).has_value());
        dispatcher.cancel_client(client);
        dispatcher.cancel_client(other);
        CHECK(dispatcher.pending_count() == 0);
        editor.context().events().unregister_client(client);
        editor.context().events().unregister_client(other);
    }

    TEST_CASE("command_dispatcher - a pending request past the deadline answers TIMEOUT") {
        editor_test::headless_editor editor = {};
        command_dispatcher &dispatcher = editor.context().dispatcher();
        dispatcher.set_async_timeout_seconds(0.0);
        CHECK(dispatcher.async_timeout_seconds() == doctest::Approx(0.0));
        const protocol_response response = editor.send("editor.wait_frames", core::json::object({{"frames", 100000}}));
        CHECK_FALSE(response.ok);
        CHECK(response.errorCode == protocol_error_code::TIMEOUT);
        CHECK(dispatcher.pending_count() == 0);
        dispatcher.set_async_timeout_seconds(30.0);
    }

    TEST_CASE("command_dispatcher - cancel_client drops the client's pending requests without a response") {
        editor_test::headless_editor editor = {};
        command_dispatcher &dispatcher = editor.context().dispatcher();
        int responses = 0;
        const u32 client = editor.context().events().register_client({});
        editor.context().register_response_sink(client, [&responses](const protocol_response &) {
            ++responses;
        });
        CHECK_FALSE(dispatcher.dispatch(request_of(
          "editor.wait_frames",
          core::json::object({{"frames", 2}}),
          client
        )).has_value());
        dispatcher.cancel_client(client);
        CHECK(dispatcher.pending_count(client) == 0);
        editor.pump(5);
        CHECK(responses == 0);
        editor.context().unregister_response_sink(client);
        editor.context().events().unregister_client(client);
    }

    TEST_CASE("regression - a synchronous completion is matched by ticket, not by the client and id") {
        editor_test::headless_editor editor = {};
        command_dispatcher &dispatcher = editor.context().dispatcher();
        // Client 0 always sends a null id: park a play.step, then let play.stop cancel it so a CANCELLED
        // response for (client 0, null id) sits in the completed queue.
        command_request start = {};
        start.name = "play.start";
        CONJURE_REQUIRE_OK(dispatcher.execute(start));
        command_request pause = {};
        pause.name = "play.pause";
        CONJURE_REQUIRE_OK(dispatcher.execute(pause));
        command_request step = {};
        step.name = "play.step";
        step.args = core::json::object({{"fixedSteps", 100000}});
        const core::result<core::json::document> stepped = dispatcher.execute(step);
        CONJURE_REQUIRE_OK(stepped);
        CHECK(dispatcher.pending_count(0) == 1);
        command_request stop = {};
        stop.name = "play.stop";
        CONJURE_REQUIRE_OK(dispatcher.execute(stop));
        CHECK(dispatcher.pending_count(0) == 0);
        // view.screenshot on the null device completes its reply inside the handler: the answer must be its own
        // NOT_AVAILABLE, never the cancelled step's response.
        command_request shot = {};
        shot.name = "view.screenshot";
        shot.args = core::json::object({{"path", (editor.project_directory() / "s.png").generic_string()}});
        CONJURE_CHECK_ERROR(dispatcher.execute(shot), protocol_error_code::NOT_AVAILABLE);
        // The cancelled step's response is still delivered on the next frame as a local response.
        std::vector<protocol_response> local = {};
        core::event_subscription token = dispatcher.on_local_response().subscribe([&local](
          const protocol_response &response
        ) {
            local.push_back(response);
        });
        editor.pump(1);
        REQUIRE(local.size() == 1);
        CHECK(local[0].errorCode == protocol_error_code::CANCELLED);
        token.release();
    }

    TEST_CASE("command_dispatcher - cancel_all completes every pending request with the given code") {
        editor_test::headless_editor editor = {};
        command_dispatcher &dispatcher = editor.context().dispatcher();
        std::vector<protocol_response> received = {};
        const u32 client = editor.context().events().register_client({});
        editor.context().register_response_sink(client, [&received](const protocol_response &response) {
            received.push_back(response);
        });
        CHECK_FALSE(dispatcher.dispatch(request_of(
          "editor.wait_frames",
          core::json::object({{"frames", 1000}}),
          client,
          5
        )).has_value());
        CHECK_FALSE(dispatcher.dispatch(request_of(
          "editor.wait_frames",
          core::json::object({{"frames", 1000}}),
          client,
          6
        )).has_value());
        dispatcher.cancel_all(protocol_error_code::CANCELLED, "test");
        CHECK(dispatcher.pending_count() == 0);
        editor.pump(1);
        REQUIRE(received.size() == 2);
        CHECK(received[0].errorCode == protocol_error_code::CANCELLED);
        CHECK(received[0].id.root().as_i64() == 5);
        CHECK(received[1].id.root().as_i64() == 6);
        CHECK(received[1].errorMessage.find("test") != std::string::npos);
        editor.context().unregister_response_sink(client);
        editor.context().events().unregister_client(client);
    }

    TEST_CASE("command_dispatcher - execute runs a request in process for the local client") {
        editor_test::headless_editor editor = {};
        command_request request = {};
        request.name = "entity.create";
        request.args = core::json::object({{"name", "local"}});
        const core::result<core::json::document> result = editor.context().dispatcher().execute(request);
        CONJURE_REQUIRE_OK(result);
        CHECK_FALSE(result.value().root()["guid"].as_string().empty());
        command_request bad = {};
        bad.name = "entity.create";
        bad.args = core::json::object({{"name", 1}});
        CONJURE_CHECK_ERROR(editor.context().dispatcher().execute(bad), protocol_error_code::INVALID_ARGS);
        CHECK(editor.context().undo().entries().front()->clientId == 0);
    }

    TEST_CASE("command_dispatcher - a failing handler leaves no undo entry and no revision change") {
        editor_test::headless_editor editor = {};
        const u64 before = editor.context().revision();
        CHECK(editor.fail(
          "entity.destroy",
          core::json::object({{"entity", "Missing/Path"}})
        ) == protocol_error_code::ENTITY_NOT_FOUND);
        CHECK(editor.context().revision() == before);
        CHECK(editor.context().undo().size() == 0);
    }

    TEST_CASE("command_dispatcher - a mutation while playing is executed but not recorded, with one warning") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("keep");
        (void) editor.ok("play.start");
        editor.clear_events();
        (void) editor.create_entity("transient");
        (void) editor.create_entity("transient2");
        CHECK(editor.context().undo().frozen());
        CHECK(editor.context().undo().size() == 1);
        int warnings = 0;
        for (const protocol_event &event : editor.events()) {
            if (event.name == "warning" && event.data.root()["code"].as_string() == "UNRECORDED_PLAY_EDIT") {
                ++warnings;
            }
        }
        CHECK(warnings == 1);
        (void) editor.ok("play.stop");
        CHECK(editor.scene().world().entity_count() == 1);
    }
}
