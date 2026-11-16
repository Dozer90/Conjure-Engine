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

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
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

    usize scene_changed_count(const editor_test::headless_editor &editor) {
        usize count = 0;
        for (const protocol_event &event : editor.events()) {
            if (event.name == "scene.changed") {
                ++count;
            }
        }
        return count;
    }

    std::filesystem::path script_file(const editor_test::headless_editor &editor, std::string_view name) {
        return editor.project_directory() / "scripts" / std::filesystem::path(std::string(name));
    }

    core::json::document child(std::string_view command, core::json::document args) {
        return core::json::object({{"cmd", command}, {"args", args}});
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

    TEST_CASE("regression - F102 an unknown command with a 1 MiB name answers at once with a truncated message") {
        editor_test::headless_editor editor = {};
        // U+65E5 is three bytes in UTF-8, so byte 128, where the echo is cut, falls inside a character.
        const std::string character = "\xE6\x97\xA5";
        std::string name = {};
        name.reserve(1024 * 1024 + character.size());
        while (name.size() < 1024 * 1024) {
            name += character;
        }
        const protocol_response response = editor.send(name, core::json::document::object());
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorCode == protocol_error_code::UNKNOWN_COMMAND);
        const core::json::value suggestions = response.errorDetails.root()["didYouMean"];
        REQUIRE(suggestions.is_array());
        CHECK(suggestions.size() == 0);
        CHECK(response.errorMessage.size() < 200);
        CHECK(response.errorMessage.find(character + character) != std::string::npos);
        CHECK(response.errorMessage.find("...") != std::string::npos);
        // The cut lands on a character boundary: the response is still valid UTF-8 on the wire.
        const core::result<protocol_response> parsed = parse_response(serialise(response));
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().errorMessage == response.errorMessage);
    }

    TEST_CASE("command_dispatcher - an unknown child of editor.batch is named with a bounded echo") {
        editor_test::headless_editor editor = {};
        const std::string name(4096, 'z');
        const protocol_response response = editor.send(
          "editor.batch",
          core::json::object({{"commands", core::json::array({core::json::object({{"cmd", name}})})}})
        );
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorCode == protocol_error_code::UNKNOWN_COMMAND);
        CHECK(response.errorMessage.size() < 250);
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

    TEST_CASE("regression - F508 one history step bumps the revision once and emits one scene.changed, also for "
      "an editor.batch inverse") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("entity.create_many", core::json::object({{"count", 3}}));
        u64 before = editor.context().revision();
        editor.clear_events();
        const core::json::document undone = editor.ok("history.undo");
        CHECK(editor.scene().world().entity_count() == 0);
        CHECK(editor.context().revision() == before + 1);
        CHECK(undone.root()["revision"].as_u64() == before + 1);
        CHECK(scene_changed_count(editor) == 1);
        for (const protocol_event &event : editor.events()) {
            if (event.name == "scene.changed") {
                CHECK(event.data.root()["label"].as_string() == "Undo Create 3 entities");
                CHECK(event.data.root()["revision"].as_u64() == before + 1);
            }
        }
        editor.clear_events();
        (void) editor.ok("history.redo");
        CHECK(editor.scene().world().entity_count() == 3);
        CHECK(editor.context().revision() == before + 2);
        CHECK(scene_changed_count(editor) == 1);

        // entity.destroy_many's inverse is an editor.batch of restores: still one step, one revision, one event.
        const std::string first = editor.create_entity("first");
        const std::string second = editor.create_entity("second");
        (void) editor.ok("entity.destroy_many", core::json::object({{"entities", core::json::array({first, second})}}));
        before = editor.context().revision();
        editor.clear_events();
        (void) editor.ok("history.undo");
        CHECK(editor.scene().world().entity_count() == 5);
        CHECK(editor.context().revision() == before + 1);
        CHECK(scene_changed_count(editor) == 1);
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

    TEST_CASE("regression - code sweep F089 a file command run while playing is kept and undoable after play.stop") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("keep");
        const usize historyBefore = editor.context().undo().size();
        const std::filesystem::path file = script_file(editor, "Live.cs");
        (void) editor.ok("play.start");
        editor.clear_events();
        (void) editor.ok("script.write", editor_test::script_write_args("Live.cs", "// written while playing"));

        // Neither on the frozen stack nor dropped: the play controller holds it until play.stop.
        CHECK(editor.context().undo().frozen());
        CHECK(editor.context().undo().size() == historyBefore);
        CHECK(editor.context().play().kept_file_edits() == 1);
        CHECK(editor.fail("history.undo") == protocol_error_code::WRONG_PLAY_STATE);
        REQUIRE(std::filesystem::is_regular_file(file));
        // The warning names the edit that persists, and nothing claims that play.stop discards it.
        const std::vector<protocol_event> kept = editor.warnings("PLAY_FILE_EDIT");
        REQUIRE(kept.size() == 1);
        CHECK(kept[0].data.root()["cmd"].as_string() == "script.write");
        CHECK(kept[0].data.root()["message"].as_string().find("File changes during play") != std::string_view::npos);
        CHECK(editor.warnings("UNRECORDED_PLAY_EDIT").empty());

        const core::json::document stopped = editor.ok("play.stop");
        CHECK(stopped.root()["fileChanges"].as_u64() == 1);
        CHECK(stopped.root()["state"].as_string() == "editing");
        // play.stop restores the scene, not the file.
        CHECK(std::filesystem::is_regular_file(file));
        CHECK(editor.context().play().kept_file_edits() == 0);
        REQUIRE(editor.context().undo().size() == historyBefore + 1);
        const undo_entry *entry = editor.context().undo().entries().back();
        CHECK(entry->label == "File changes during play");
        CHECK(entry->label == play_controller::FILE_EDITS_LABEL);
        CHECK(entry->undo.size() == 1);
        CHECK(entry->redo.size() == 1);
        CHECK(entry->clientId == editor.client_id());

        const core::json::document undone = editor.ok("history.undo");
        CHECK(undone.root()["labels"][0].as_string() == "File changes during play");
        CHECK_FALSE(std::filesystem::exists(file));
        (void) editor.ok("history.redo");
        CHECK(editor_test::read_text_file(file) == "// written while playing");
        CHECK(editor.scene().world().entity_count() == 1);
    }

    TEST_CASE("regression - code sweep F089 a batch run while playing keeps its file commands, not its scene edits") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path first = script_file(editor, "First.cs");
        const std::filesystem::path second = script_file(editor, "Second.cs");
        const std::filesystem::path folder = editor.project_directory() / "assets" / "from_batch";
        (void) editor.ok("play.start");
        editor.clear_events();
        (void) editor.ok(
          "editor.batch",
          core::json::object({
            {"commands",
             core::json::array({
               child("entity.create", core::json::object({{"name", "InBatch"}})),
               child("script.write", editor_test::script_write_args("First.cs", "// first")),
               child("asset.create_folder", core::json::object({{"path", "from_batch"}})),
             })},
          })
        );
        CHECK(editor.context().undo().size() == 0);
        CHECK(editor.context().play().kept_file_edits() == 2);
        // One warning for the file edits, naming the first, and one for the scene edit that will be discarded.
        const std::vector<protocol_event> kept = editor.warnings("PLAY_FILE_EDIT");
        REQUIRE(kept.size() == 1);
        CHECK(kept[0].data.root()["cmd"].as_string() == "script.write");
        CHECK(editor.warnings("UNRECORDED_PLAY_EDIT").size() == 1);
        // A command outside the batch joins the same entry, and the session warns only once.
        (void) editor.ok("script.write", editor_test::script_write_args("Second.cs", "// second"));
        CHECK(editor.warnings("PLAY_FILE_EDIT").size() == 1);

        CHECK(editor.ok("play.stop").root()["fileChanges"].as_u64() == 3);
        CHECK(editor.scene().world().entity_count() == 0);
        REQUIRE(editor.context().undo().size() == 1);
        const undo_entry *entry = editor.context().undo().entries().back();
        CHECK(entry->label == play_controller::FILE_EDITS_LABEL);
        REQUIRE(entry->redo.size() == 3);
        CHECK(entry->redo[0].name == "script.write");
        CHECK(entry->redo[1].name == "asset.create_folder");
        CHECK(entry->redo[2].name == "script.write");
        CHECK(std::filesystem::is_regular_file(first));
        CHECK(std::filesystem::is_regular_file(second));
        CHECK(std::filesystem::is_directory(folder));

        // One step takes all three back, newest first, and one step brings them back.
        CHECK(editor.ok("history.undo").root()["applied"].as_u64() == 1);
        CHECK_FALSE(std::filesystem::exists(first));
        CHECK_FALSE(std::filesystem::exists(second));
        CHECK_FALSE(std::filesystem::exists(folder));
        (void) editor.ok("history.redo");
        CHECK(editor_test::read_text_file(first) == "// first");
        CHECK(editor_test::read_text_file(second) == "// second");
        CHECK(std::filesystem::is_directory(folder));
    }

    TEST_CASE("regression - code sweep F089 a batch that rolls back while playing keeps no file edit") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path rolledBack = script_file(editor, "RolledBack.cs");
        const std::filesystem::path survivor = script_file(editor, "Survivor.cs");
        const core::json::document missing = child(
          "entity.rename",
          core::json::object({{"entity", "Missing/Path"}, {"name", "x"}})
        );
        (void) editor.ok("play.start");
        editor.clear_events();
        const protocol_response atomic = editor.send(
          "editor.batch",
          core::json::object({
            {"commands",
             core::json::array({
               child("script.write", editor_test::script_write_args("RolledBack.cs", "// gone again")),
               missing,
             })},
          })
        );
        REQUIRE_FALSE(atomic.ok);
        CHECK(atomic.errorDetails.root()["failedIndex"].as_i64() == 1);
        // The rollback undid the write through its inverse, so there is nothing left to undo later.
        CHECK_FALSE(std::filesystem::exists(rolledBack));
        CHECK(editor.context().play().kept_file_edits() == 0);
        CHECK(editor.warnings("PLAY_FILE_EDIT").empty());

        // Without atomic the child that succeeded stands, and so does its inverse.
        (void) editor.ok(
          "editor.batch",
          core::json::object({
            {"atomic", false},
            {"commands",
             core::json::array({
               child("script.write", editor_test::script_write_args("Survivor.cs", "// stays")),
               missing,
             })},
          })
        );
        CHECK(std::filesystem::is_regular_file(survivor));
        CHECK(editor.context().play().kept_file_edits() == 1);

        CHECK(editor.ok("play.stop").root()["fileChanges"].as_u64() == 1);
        REQUIRE(editor.context().undo().size() == 1);
        (void) editor.ok("history.undo");
        CHECK_FALSE(std::filesystem::exists(survivor));
    }

    TEST_CASE("regression - code sweep F089 the warning of a scene edit while playing says which edits persist") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("play.start");
        editor.clear_events();
        (void) editor.create_entity("transient");
        const std::vector<protocol_event> warnings = editor.warnings("UNRECORDED_PLAY_EDIT");
        REQUIRE(warnings.size() == 1);
        const std::string_view message = warnings[0].data.root()["message"].as_string();
        CHECK(message.find("scene edit") != std::string_view::npos);
        CHECK(message.find("on_disk") != std::string_view::npos);
        CHECK(message.find("File changes during play") != std::string_view::npos);
        // It names the commands whose edits persist, read from the registry, and only those.
        const std::array<std::string_view, 4> keptCommands = {
            "asset.delete", "prefab.apply", "project.set_settings", "script.write",
        };
        for (const std::string_view kept : keptCommands) {
            CHECK_MESSAGE(message.find(kept) != std::string_view::npos, kept);
        }
        const std::array<std::string_view, 4> otherCommands = {
            "entity.create", "prefab.create", "scene.save", "asset.list",
        };
        for (const std::string_view other : otherCommands) {
            CHECK_MESSAGE(message.find(other) == std::string_view::npos, other);
        }
        // A scene edit is not a file edit: nothing is kept for play.stop, and nothing is pushed there.
        CHECK(editor.warnings("PLAY_FILE_EDIT").empty());
        CHECK(editor.context().play().kept_file_edits() == 0);
        CHECK(editor.ok("play.stop").root()["fileChanges"].as_u64() == 0);
        CHECK(editor.context().undo().size() == 0);
        CHECK(editor.scene().world().entity_count() == 0);
    }

    TEST_CASE("command_dispatcher - a file command that records nothing is not kept while playing") {
        editor_test::headless_editor editor = {};
        // Registering a file that already lives under assets:// has nothing to undo (skipRecording).
        const std::filesystem::path inPlace = editor_test::write_tiny_png(
          editor.project_directory() / "assets" / "textures" / "in_place.png"
        );
        (void) editor.ok("play.start");
        editor.clear_events();
        const core::json::document imported = editor.ok(
          "asset.import",
          core::json::object({{"source", "textures/in_place.png"}})
        );
        CHECK_FALSE(imported.root()["copied"].as_bool());
        CHECK(editor.context().play().kept_file_edits() == 0);
        CHECK(editor.warnings("PLAY_FILE_EDIT").empty());
        CHECK(editor.warnings("UNRECORDED_PLAY_EDIT").empty());
        CHECK(editor.ok("play.stop").root()["fileChanges"].as_u64() == 0);
        CHECK(editor.context().undo().size() == 0);
        CHECK(std::filesystem::is_regular_file(inPlace));
    }
}
