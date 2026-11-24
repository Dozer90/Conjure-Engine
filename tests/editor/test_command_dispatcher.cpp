#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"

#include "conjure/core/event.h"
#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/core/uuid.h"
#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/command_registry.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/protocol/access_token.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/protocol/protocol_server.h"
#include "conjure/editor/undo_stack.h"

#include <array>
#include <filesystem>
#include <format>
#include <optional>
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

    /// <summary>
    /// Lets INFO lines through for its lifetime and puts the previous global level back.
    /// </summary>
    class info_log_scope {
        private:
            core::log_level mPrevious = core::log::global_level();

        public:
            info_log_scope() {
                core::log::set_global_level(core::log_level::INFO);
            }

            ~info_log_scope() {
                core::log::set_global_level(mPrevious);
            }

            info_log_scope(const info_log_scope &) = delete;
            info_log_scope &operator=(const info_log_scope &) = delete;
    };

    constexpr std::string_view FULL_TOKEN = "tok-7f3a9c41";
    constexpr std::string_view INSPECT_TOKEN = "look-51d0e2b7";

    /// <summary>
    /// Sets both tokens, which gates every transport, and returns the server.
    /// </summary>
    protocol_server &gated_server(editor_test::headless_editor &editor) {
        protocol_server &server = editor.application().server();
        CONJURE_REQUIRE_OK(server.set_token(std::string(FULL_TOKEN)));
        CONJURE_REQUIRE_OK(server.set_inspect_token(std::string(INSPECT_TOKEN)));
        return server;
    }

    core::json::document hello_args(std::string_view token) {
        return core::json::object({{"token", token}, {"clientName", "dispatcher-test"}});
    }

    /// <summary>
    /// The records of the project's audit log, one parsed document per line. The editor is pumped once first:
    /// the records of a frame reach the file with that frame's flush.
    /// </summary>
    std::vector<core::json::document> audit_records(editor_test::headless_editor &editor) {
        editor.pump(1);
        return editor_test::read_json_lines(editor.project_directory() / ".conjure" / "ai_audit.log");
    }

    /// <summary>
    /// Renames the entity with the guid and returns the outcome of that change: the inverse renames it back, the
    /// replay is the request itself.
    /// </summary>
    command_outcome rename_entity(editor_context &context, std::string_view guid, std::string_view name) {
        scene::scene &scene = context.scene();
        const std::optional<core::uuid> parsed = core::uuid::parse(guid);
        REQUIRE(parsed.has_value());
        const ecs::entity entity = scene.find_guid(*parsed);
        REQUIRE_FALSE(entity.is_null());
        const std::string previous = scene.name_of(entity).to_string();
        CONJURE_REQUIRE_OK(scene.set_name(entity, core::string_id(name)));
        command_outcome outcome = {};
        outcome.label = std::format("Rename to '{}'", name);
        outcome.result = core::json::object({{"name", name}});
        outcome.inverse = command_inverse{"entity.rename", core::json::object({{"entity", guid}, {"name", previous}})};
        return outcome;
    }

    /// <summary>
    /// A command the tests register that renames an entity asynchronously ({entity: guid, name}; ASYNC | MUTATES
    /// | SYNC_IN_BATCH plus 'extra'), the way the prefab commands change the scene once their asset has loaded
    /// (code sweep F736). It parks its reply, and answers inside its handler when the dispatcher requires that
    /// (unless set_answers_when_required(false)). finish() makes the change of a parked request and completes it.
    /// </summary>
    class async_rename {
        private:
            struct parked_request {
                async_reply reply = {};
                std::string guid = "";
                std::string name = "";
            };

            std::vector<parked_request> mParked = {};
            bool mAnswersWhenRequired = true;
            bool mFailsAtOnce = false;

        public:
            static constexpr std::string_view NAME = "test.async_rename";

            /// <summary>
            /// false: the handler parks its reply even where the dispatcher needs the answer at once.
            /// </summary>
            void set_answers_when_required(bool answers) noexcept {
                mAnswersWhenRequired = answers;
            }

            /// <summary>
            /// true: the handler fails its reply before returning, as a command refused up front does.
            /// </summary>
            void set_fails_at_once(bool fails) noexcept {
                mFailsAtOnce = fails;
            }

            void install(editor_context &context, command_flags extra = command_flags::NONE) {
                command_descriptor descriptor = {};
                descriptor.name = std::string(NAME);
                descriptor.argsSchema =
                  R"({"type":"object","properties":{"entity":{"type":"string"},"name":{"type":"string"}},)"
                  R"("required":["entity","name"]})";
                descriptor.flags = command_flags::ASYNC | command_flags::MUTATES | command_flags::SYNC_IN_BATCH |
                  command_flags::REQUIRES_SCENE | extra;
                descriptor.handler = [this](command_invocation &invocation) -> core::result<command_outcome> {
                    const std::string guid = std::string(invocation.args["entity"].as_string());
                    const std::string name = std::string(invocation.args["name"].as_string());
                    if (mFailsAtOnce) {
                        invocation.reply.fail(protocol_error_code::IO_ERROR, "the change could not be made");
                        return command_outcome{};
                    }
                    const bool required = invocation.context.dispatcher().requires_synchronous_completion();
                    if (required && mAnswersWhenRequired) {
                        invocation.reply.succeed_mutation(rename_entity(invocation.context, guid, name));
                        return command_outcome{};
                    }
                    mParked.push_back(parked_request{invocation.reply, guid, name});
                    return command_outcome{};
                };
                CONJURE_REQUIRE_OK(context.registry().add(std::move(descriptor)));
            }

            [[nodiscard]] usize parked() const noexcept {
                return mParked.size();
            }

            [[nodiscard]] const async_reply &reply(usize index) const {
                REQUIRE(index < mParked.size());
                return mParked[index].reply;
            }

            void finish(editor_context &context, usize index) {
                REQUIRE(index < mParked.size());
                const parked_request &request = mParked[index];
                REQUIRE(request.reply.is_valid());
                request.reply.succeed_mutation(rename_entity(context, request.guid, request.name));
            }

            void fail(usize index) {
                REQUIRE(index < mParked.size());
                mParked[index].reply.fail(protocol_error_code::IO_ERROR, "the change could not be made");
            }
    };

    core::json::document rename_args(std::string_view guid, std::string_view name) {
        return core::json::object({{"entity", guid}, {"name", name}});
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

    TEST_CASE("regression - code sweep F187 an inspect-only client's command is refused before anything of it is "
              "looked at") {
        editor_test::headless_editor editor = {};
        (void) gated_server(editor);
        editor_test::loopback_session session(editor);
        CHECK(session.ok("session.hello", hello_args(INSPECT_TOKEN)).root()["access"].as_string() == "inspect");
        const u64 revision = editor.context().revision();
        protocol_response response = {};

        SUBCASE("a mutation with good arguments") {
            response = session.request("entity.create", core::json::object({{"name", "Intruder"}}));
        }
        SUBCASE("a mutation with arguments the schema would refuse: UNAUTHORISED, not INVALID_ARGS") {
            response = session.request("entity.create", core::json::object({{"name", 5}}));
        }
        SUBCASE("a command whose play state is wrong: UNAUTHORISED, not WRONG_PLAY_STATE") {
            response = session.request("play.pause");
        }
        SUBCASE("a command that changes no scene but is not read_only") {
            response = session.request("selection.clear");
        }
        SUBCASE("a file written anywhere on disk") {
            response = session.request("view.screenshot", core::json::object({{"path", "shot.png"}}));
        }
        SUBCASE("a batch, even one of read_only children") {
            core::json::document commands = core::json::document::array();
            commands.push_back(child("scene.info", core::json::document::object()));
            response = session.request("editor.batch", core::json::object({{"commands", commands}}));
        }
        REQUIRE_FALSE(response.ok);
        CHECK(response.errorCode == protocol_error_code::UNAUTHORISED);
        CHECK(response.errorDetails.root()["required"].as_string() == "full_control");
        CHECK(response.errorDetails.root()["granted"].as_string() == "inspect");
        CHECK(response.errorMessage.find("read_only") != std::string::npos);
        CHECK(response.errorMessage.find(FULL_TOKEN) == std::string::npos);
        CHECK(response.errorMessage.find(INSPECT_TOKEN) == std::string::npos);
        CHECK(editor.scene().world().entity_count() == 0);
        CHECK(editor.context().undo().size() == 0);
        CHECK(editor.context().revision() == revision);
        CHECK(editor.context().dispatcher().pending_count() == 0);
        CHECK_FALSE(std::filesystem::exists(editor.project_directory() / "shot.png"));
        // The client stays, and still gets what its token opens.
        CHECK(session.client().is_connected());
        CHECK(session.ok("scene.info").root().contains("entityCount"));
    }

    TEST_CASE("command_dispatcher - an inspect-only client learns that a command is unknown, as any client does") {
        editor_test::headless_editor editor = {};
        (void) gated_server(editor);
        editor_test::loopback_session session(editor);
        (void) session.ok("session.hello", hello_args(INSPECT_TOKEN));
        const protocol_response response = session.request("entity.craete");
        CHECK(response.errorCode == protocol_error_code::UNKNOWN_COMMAND);
        CHECK(response.errorDetails.root()["didYouMean"].size() >= 1);
    }

    TEST_CASE("command_dispatcher - an inspect-only client runs the read_only commands, asynchronous ones included") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Watched");
        (void) editor.ok("script.write", editor_test::script_write_args("Watcher.cs", "class Watcher {}"));
        (void) gated_server(editor);
        editor_test::loopback_session session(editor);
        (void) session.ok("session.hello", hello_args(INSPECT_TOKEN));

        CHECK(session.ok("entity.find", core::json::object({{"name", "Watched"}})).root()["total"].as_u64() == 1);
        CHECK(session.ok("entity.get", core::json::object({{"entity", guid}})).root()["name"].as_string() == "Watched");
        CHECK(session.ok("script.read", core::json::object({{"path", "Watcher.cs"}})).root()["source"].as_string()
              == "class Watcher {}");
        CHECK(session.ok("editor.wait_frames", core::json::object({{"frames", 2}})).root().contains("frame"));
        CHECK(session.ok("session.ping").root().contains("revision"));
        // Its own subscriptions are its own business.
        (void) session.ok("session.subscribe", core::json::object({{"events", core::json::document::array()}}));

        SUBCASE("script.read does not reach the token files: its paths end inside scripts/") {
            const protocol_response outside = session.request(
              "script.read",
              core::json::object({{"path", "../.conjure/ai_token"}})
            );
            CHECK(outside.errorCode == protocol_error_code::INVALID_ARGS);
        }
        SUBCASE("the full-control token raises the client, and the inspect token lowers it again") {
            const core::json::document raised = session.ok("session.hello", hello_args(FULL_TOKEN));
            CHECK(raised.root()["access"].as_string() == "full_control");
            (void) session.ok("entity.rename", core::json::object({{"entity", guid}, {"name", "Renamed"}}));
            const core::json::document lowered = session.ok("session.hello", hello_args(INSPECT_TOKEN));
            CHECK(lowered.root()["access"].as_string() == "inspect");
            const protocol_response refused = session.request(
              "entity.rename",
              core::json::object({{"entity", guid}, {"name", "Again"}})
            );
            CHECK(refused.errorCode == protocol_error_code::UNAUTHORISED);
            // A wrong token in between is refused by the handler and changes nothing.
            const protocol_response wrong = session.request("session.hello", hello_args("wrong"));
            CHECK(wrong.errorCode == protocol_error_code::UNAUTHORISED);
            CHECK_FALSE(wrong.errorDetails.root().contains("required"));
            CHECK(session.ok("session.ping").root().contains("frame"));
            CHECK(session.request("history.undo").errorCode == protocol_error_code::UNAUTHORISED);
        }
    }

    TEST_CASE("command_dispatcher - the editor's own clients have full control whatever tokens are set") {
        editor_test::headless_editor editor = {};
        (void) gated_server(editor);
        // The fixture's client has no transport: the UI, an --exec run and an editor.exec_file task are like it.
        CHECK(editor.ok("session.hello").root()["access"].as_string() == "full_control");
        CHECK(editor.ok("session.hello", hello_args(INSPECT_TOKEN)).root()["access"].as_string() == "full_control");
        (void) editor.create_entity("Made in process");
        CHECK(editor.scene().world().entity_count() == 1);
        // The UI's client 0 as well.
        command_request request = {};
        request.name = "entity.create";
        request.args = core::json::object({{"name", "Made by the UI"}});
        CONJURE_REQUIRE_OK(editor.context().dispatcher().execute(request));
        CHECK(editor.scene().world().entity_count() == 2);
    }

    TEST_CASE("regression - code sweep F187 session.hello does not log a client name that holds a token") {
        editor_test::headless_editor editor = {};
        (void) gated_server(editor);
        editor_test::loopback_session session(editor);
        // The line in question is an INFO; the fixture's editor logs from WARN up.
        const info_log_scope infoLogging = {};
        // The token in the wrong argument as well as in the right one.
        (void) session.ok(
          "session.hello",
          core::json::object({{"token", INSPECT_TOKEN}, {"clientName", std::format("agent {}", FULL_TOKEN)}})
        );
        CHECK_FALSE(editor.log().contains(FULL_TOKEN));
        CHECK_FALSE(editor.log().contains(INSPECT_TOKEN));
        CHECK_FALSE(editor.log().contains("says hello"));
        (void) session.ok("session.hello", core::json::object({{"token", INSPECT_TOKEN}, {"clientName", "agent"}}));
        CHECK(editor.log().contains(core::log_level::INFO, "says hello as 'agent' (inspect access)"));
    }

    TEST_CASE("regression - code sweep F187 a command name that holds a token is not passed on in command.executed") {
        editor_test::headless_editor editor = {};
        (void) gated_server(editor);
        // The fixture's own client listens the way another connected client would.
        const u64 everything = default_subscriptions() | event_bit(editor_event::COMMAND_EXECUTED);
        editor.context().events().set_subscriptions(editor.client_id(), everything);
        editor_test::loopback_session session(editor);
        (void) session.ok("session.hello", hello_args(FULL_TOKEN));
        editor.clear_events();

        // A hello sent as one text: the token sits in what the editor takes for the command name.
        const protocol_response slip = session.request(std::format("session.hello {}", INSPECT_TOKEN));
        CHECK(slip.errorCode == protocol_error_code::UNKNOWN_COMMAND);
        (void) session.ok("session.ping");

        std::vector<std::string> names = {};
        for (const protocol_event &event : editor.events()) {
            CAPTURE(event.name);
            CHECK(serialise(event).find(INSPECT_TOKEN) == std::string::npos);
            CHECK(serialise(event).find(FULL_TOKEN) == std::string::npos);
            if (event.name == "command.executed") {
                names.emplace_back(event.data.root()["cmd"].as_string());
            }
        }
        // The slip is still announced, without what was slipped; every other name is passed on as it is.
        REQUIRE(names.size() == 2);
        CHECK(names[0] == audit_log::TOKEN_MARK);
        CHECK(names[1] == "session.ping");
        CHECK_FALSE(editor.log().contains(INSPECT_TOKEN));
    }

    TEST_CASE("regression - code sweep F187 every request of a transport client reaches the audit log, the editor's "
              "own requests do not") {
        editor_test::headless_editor editor = {};
        (void) gated_server(editor);
        const std::filesystem::path log = editor.project_directory() / ".conjure" / "ai_audit.log";
        // The editor's own client is not a protocol client: no record, and not even a file.
        (void) editor.create_entity("Made in process");
        editor.pump(1);
        CHECK_FALSE(std::filesystem::exists(log));

        editor_test::loopback_session session(editor);
        const u32 id = session.client().client_id();
        (void) session.ok("session.hello", hello_args(INSPECT_TOKEN));
        (void) session.ok("scene.info");
        CHECK(session.request("entity.create", core::json::object({{"name", "Refused"}})).errorCode
              == protocol_error_code::UNAUTHORISED);
        (void) session.ok("session.hello", hello_args(FULL_TOKEN));
        const std::string guid = std::string(
          session.ok("entity.create", core::json::object({{"name", "Recorded"}})).root()["guid"].as_string()
        );
        CHECK(session.request("entity.create", core::json::object({{"name", 5}})).errorCode
              == protocol_error_code::INVALID_ARGS);
        CHECK(session.request("entity.craete").errorCode == protocol_error_code::UNKNOWN_COMMAND);
        (void) session.ok("editor.wait_frames", core::json::object({{"frames", 1}}));
        // An atomic batch whose second child fails: both children are recorded, the rollback is not a request.
        core::json::document commands = core::json::document::array();
        commands.push_back(child("entity.create", core::json::object({{"name", "In a batch"}})));
        commands.push_back(child(
          "entity.rename",
          core::json::object({{"entity", "00000000-0000-4000-8000-000000000001"}, {"name", "Nobody"}})
        ));
        CHECK_FALSE(session.request("editor.batch", core::json::object({{"commands", commands}})).ok);
        // history.undo dispatches the recorded inverse: one record, for the request the client sent.
        (void) session.ok("history.undo");
        (void) editor.ok("scene.info");

        const std::vector<core::json::document> records = audit_records(editor);
        struct expected_record {
            std::string_view command = "";
            std::string_view access = "";
            std::string_view error = "";
            bool async = false;
            bool batch = false;
        };
        const std::array<expected_record, 12> expected = {
            expected_record{"session.hello", "inspect", "", false, false},
            expected_record{"scene.info", "inspect", "", false, false},
            expected_record{"entity.create", "inspect", "UNAUTHORISED", false, false},
            expected_record{"session.hello", "full_control", "", false, false},
            expected_record{"entity.create", "full_control", "", false, false},
            expected_record{"entity.create", "full_control", "INVALID_ARGS", false, false},
            expected_record{"entity.craete", "full_control", "UNKNOWN_COMMAND", false, false},
            expected_record{"editor.wait_frames", "full_control", "", true, false},
            expected_record{"entity.create", "full_control", "", false, true},
            expected_record{"entity.rename", "full_control", "ENTITY_NOT_FOUND", false, true},
            expected_record{"editor.batch", "full_control", "ENTITY_NOT_FOUND", false, false},
            expected_record{"history.undo", "full_control", "", false, false},
        };
        REQUIRE(records.size() == expected.size());
        for (usize index = 0; index < expected.size(); ++index) {
            CAPTURE(index);
            const core::json::value record = records[index].root();
            CHECK(record["cmd"].as_string() == expected[index].command);
            CHECK(record["access"].as_string() == expected[index].access);
            CHECK(record["clientId"].as_u64() == id);
            CHECK(record["transport"].as_string() == "loopback");
            CHECK(record["ok"].as_bool(false) == expected[index].error.empty());
            CHECK(record["error"].as_string() == expected[index].error);
            CHECK(record["async"].as_bool(false) == expected[index].async);
            CHECK(record["batch"].as_bool(false) == expected[index].batch);
            CHECK(record["time"].as_string().size() == 20);
        }
        // What was done is on record, the tokens that were presented are not.
        CHECK(records[4].root()["args"]["name"].as_string() == "Recorded");
        CHECK(records[0].root()["args"]["token"].as_string() == audit_log::WITHHELD);
        CHECK(records[0].root()["args"]["clientName"].as_string() == "dispatcher-test");
        const std::string text = editor_test::read_text_file(log);
        CHECK(text.find(FULL_TOKEN) == std::string::npos);
        CHECK(text.find(INSPECT_TOKEN) == std::string::npos);
        CHECK(editor.scene().world().entity_count() == 1);
        CHECK(guid.size() == 36);
    }

    TEST_CASE("regression - code sweep F736 an asynchronous mutation is recorded, dirties the scene and bumps the "
              "revision when it completes, not when it is dispatched") {
        editor_test::headless_editor editor = {};
        async_rename command = {};
        command.install(editor.context());
        const std::string guid = editor.create_entity("Before");
        editor_test::parked_client client(editor);
        (void) editor.ok("scene.save");
        REQUIRE_FALSE(editor.context().scene_dirty());
        const u64 revision = editor.context().revision();
        const usize entries = editor.context().undo().size();
        editor.clear_events();

        CHECK_FALSE(client.dispatch(async_rename::NAME, rename_args(guid, "After")).has_value());
        REQUIRE(command.parked() == 1);
        editor.pump(2);
        // Parked: nothing of the change happened yet, so nothing of its bookkeeping either.
        CHECK(editor.context().revision() == revision);
        CHECK(editor.context().undo().size() == entries);
        CHECK_FALSE(editor.context().scene_dirty());
        CHECK(scene_changed_count(editor) == 0);
        CHECK_FALSE(client.reply().has_value());

        command.finish(editor.context(), 0);
        const protocol_response answered = client.wait();
        REQUIRE(answered.ok);
        CHECK(answered.result.root()["name"].as_string() == "After");
        CHECK(answered.result.root()["revision"].as_u64() == revision + 1);
        CHECK(editor.context().revision() == revision + 1);
        REQUIRE(editor.context().undo().size() == entries + 1);
        CHECK(editor.context().undo().entries().back()->label == "Rename to 'After'");
        CHECK(editor.context().undo().entries().back()->clientId == client.client_id());
        CHECK(editor.context().scene_dirty());
        REQUIRE(scene_changed_count(editor) == 1);
        for (const protocol_event &event : editor.events()) {
            if (event.name == "scene.changed") {
                CHECK(event.data.root()["cmd"].as_string() == async_rename::NAME);
                CHECK(event.data.root()["revision"].as_u64() == revision + 1);
            }
        }

        // The undo entry holds the inverse and, as the replay, the request with its args.
        (void) editor.ok("history.undo");
        CHECK(editor.ok("entity.get", core::json::object({{"entity", guid}})).root()["name"].as_string() == "Before");
        (void) editor.ok("history.redo");
        CHECK(editor.ok("entity.get", core::json::object({{"entity", guid}})).root()["name"].as_string() == "After");
        // The redo replayed the command inside the step, where it answered synchronously.
        CHECK(command.parked() == 1);
    }

    TEST_CASE("command_dispatcher - an asynchronous mutation that fails or is no longer pending records nothing") {
        editor_test::headless_editor editor = {};
        async_rename command = {};
        command.install(editor.context());
        const std::string guid = editor.create_entity("Before");
        editor_test::parked_client client(editor);
        const u64 revision = editor.context().revision();
        const usize entries = editor.context().undo().size();

        SUBCASE("a failed completion") {
            CHECK_FALSE(client.dispatch(async_rename::NAME, rename_args(guid, "After")).has_value());
            command.fail(0);
            const protocol_response answered = client.wait();
            CHECK_FALSE(answered.ok);
            CHECK(answered.errorCode == protocol_error_code::IO_ERROR);
        }
        SUBCASE("a request that timed out") {
            editor.context().dispatcher().set_async_timeout_seconds(0.0);
            CHECK_FALSE(client.dispatch(async_rename::NAME, rename_args(guid, "After")).has_value());
            const protocol_response answered = client.wait();
            CHECK(answered.errorCode == protocol_error_code::TIMEOUT);
            // The handler's reply is dead: completing it is ignored and records nothing.
            CHECK_FALSE(command.reply(0).is_valid());
            command.reply(0).succeed_mutation(command_outcome{});
        }
        SUBCASE("a client that disconnected") {
            CHECK_FALSE(client.dispatch(async_rename::NAME, rename_args(guid, "After")).has_value());
            editor.context().dispatcher().cancel_client(client.client_id());
            CHECK_FALSE(command.reply(0).is_valid());
            command.reply(0).succeed_mutation(command_outcome{});
            editor.pump(1);
            CHECK_FALSE(client.reply().has_value());
        }
        CHECK(editor.context().revision() == revision);
        CHECK(editor.context().undo().size() == entries);
        CHECK(editor.context().dispatcher().pending_count() == 0);
    }

    TEST_CASE("command_dispatcher - an asynchronous mutation completed inside its handler is recorded at once") {
        editor_test::headless_editor editor = {};
        async_rename command = {};
        command.install(editor.context());
        const std::string guid = editor.create_entity("Before");
        const u64 revision = editor.context().revision();
        const usize entries = editor.context().undo().size();
        const core::json::document batch = core::json::object({
          {"label", "Renames"},
          {"commands", core::json::array({
            child(async_rename::NAME, rename_args(guid, "One")),
            child(async_rename::NAME, rename_args(guid, "Two")),
          })},
        });

        SUBCASE("inside a batch: the children join the batch's single entry") {
            const core::json::document result = editor.ok(
              "editor.batch",
              core::json::document::from_value(batch.root())
            );
            CHECK(result.root()["count"].as_u64() == 2);
            CHECK(result.root()["results"][1]["result"]["name"].as_string() == "Two");
            CHECK(command.parked() == 0);
            REQUIRE(editor.context().undo().size() == entries + 1);
            CHECK(editor.context().undo().entries().back()->label == "Renames");
            CHECK(editor.context().undo().entries().back()->undo.size() == 2);
            (void) editor.ok("history.undo");
            CHECK(editor.ok("entity.get", core::json::object({{"entity", guid}})).root()["name"].as_string() ==
              "Before");
        }
        SUBCASE("a batch that rolls back undoes the completed child") {
            core::json::document failing = core::json::document::from_value(batch.root());
            core::json::document commands = core::json::array({
              child(async_rename::NAME, rename_args(guid, "One")),
              child(
                "entity.rename",
                core::json::object({{"entity", core::uuid::generate().to_string()}, {"name", "x"}})
              ),
            });
            failing.set("commands", commands);
            CHECK(editor.fail("editor.batch", std::move(failing)) == protocol_error_code::ENTITY_NOT_FOUND);
            CHECK(editor.ok("entity.get", core::json::object({{"entity", guid}})).root()["name"].as_string() ==
              "Before");
            CHECK(editor.context().undo().size() == entries);
        }
        SUBCASE("a coalesce key merges two completions into one entry") {
            editor_test::parked_client client(editor);
            const std::array<std::string_view, 2> names = {"One", "Two"};
            for (const std::string_view name : names) {
                CAPTURE(name);
                protocol_request request = request_of(
                  async_rename::NAME,
                  rename_args(guid, name),
                  client.client_id()
                );
                request.coalesceKey = 99;
                CHECK_FALSE(editor.context().dispatcher().dispatch(request).has_value());
                command.finish(editor.context(), command.parked() - 1);
                CHECK(client.wait().ok);
            }
            CHECK(editor.context().undo().size() == entries + 1);
            CHECK(editor.context().revision() == revision + 2);
        }
        SUBCASE("an open transaction of the client takes the completion") {
            (void) editor.ok("editor.begin_transaction", core::json::object({{"label", "Grouped"}}));
            protocol_request request = request_of(async_rename::NAME, rename_args(guid, "One"), editor.client_id());
            CHECK_FALSE(editor.context().dispatcher().dispatch(request).has_value());
            command.finish(editor.context(), 0);
            editor.pump(1);
            CHECK(editor.context().undo().size() == entries);
            (void) editor.ok("editor.end_transaction");
            REQUIRE(editor.context().undo().size() == entries + 1);
            CHECK(editor.context().undo().entries().back()->label == "Grouped");
        }
    }

    TEST_CASE("regression - code sweep F736 an asynchronous command that does not answer inside a batch or an undo "
              "step is INTERNAL and its late completion is ignored") {
        editor_test::headless_editor editor = {};
        editor.log().allow_errors();
        async_rename command = {};
        command.set_answers_when_required(false);
        command.install(editor.context());
        const std::string guid = editor.create_entity("Before");
        const usize entries = editor.context().undo().size();
        const core::json::document batch = core::json::object({
          {"commands", core::json::array({child(async_rename::NAME, rename_args(guid, "One"))})},
        });
        CHECK(editor.fail("editor.batch", core::json::document::from_value(batch.root())) ==
          protocol_error_code::INTERNAL);
        REQUIRE(command.parked() == 1);
        CHECK_FALSE(command.reply(0).is_valid());
        CHECK(editor.context().dispatcher().pending_count() == 0);
        command.reply(0).succeed_mutation(command_outcome{});
        CHECK(editor.context().undo().size() == entries);
        CHECK(editor.log().contains(core::log_level::ERR, "did not complete inside its handler"));
    }

    TEST_CASE("regression - code sweep F736 only a parked request counts toward a client's pending cap: a batch "
              "child, a redo and an answer inside the handler pass, a ninth parked request is BUSY") {
        editor_test::headless_editor editor = {};
        async_rename command = {};
        command.install(editor.context());
        const std::string guid = editor.create_entity("Before");
        command_dispatcher &dispatcher = editor.context().dispatcher();
        editor_test::parked_client client(editor);
        // An undo entry of the command, made by this client, for the redo below.
        CHECK_FALSE(client.dispatch(async_rename::NAME, rename_args(guid, "Recorded")).has_value());
        command.finish(editor.context(), 0);
        REQUIRE(client.wait().ok);
        const usize finished = command.parked();
        for (u32 index = 0; index < command_dispatcher::DEFAULT_MAX_PENDING_PER_CLIENT; ++index) {
            CHECK_FALSE(client.dispatch(async_rename::NAME, rename_args(guid, "Parked")).has_value());
        }
        REQUIRE(dispatcher.pending_count(client.client_id()) == command_dispatcher::DEFAULT_MAX_PENDING_PER_CLIENT);

        SUBCASE("a ninth request its handler leaves parked is BUSY and its reply is dropped") {
            const std::optional<protocol_response> ninth =
              client.dispatch(async_rename::NAME, rename_args(guid, "Ninth"));
            REQUIRE(ninth.has_value());
            CHECK(ninth->errorCode == protocol_error_code::BUSY);
            REQUIRE(command.parked() == finished + command_dispatcher::DEFAULT_MAX_PENDING_PER_CLIENT + 1);
            CHECK_FALSE(command.reply(command.parked() - 1).is_valid());
        }
        SUBCASE("a request whose handler answers at once is not refused") {
            command.set_fails_at_once(true);
            const std::optional<protocol_response> answered =
              client.dispatch(async_rename::NAME, rename_args(guid, "AtOnce"));
            REQUIRE(answered.has_value());
            CHECK(answered->errorCode == protocol_error_code::IO_ERROR);
            command.set_fails_at_once(false);
        }
        SUBCASE("a batch child, which must answer inside its handler") {
            const core::json::document batch = core::json::object({
              {"commands", core::json::array({child(async_rename::NAME, rename_args(guid, "InBatch"))})},
            });
            const std::optional<protocol_response> answered =
              client.dispatch("editor.batch", core::json::document::from_value(batch.root()));
            REQUIRE(answered.has_value());
            CHECK_MESSAGE(answered->ok, answered->errorMessage);
            CHECK(editor.ok("entity.get", core::json::object({{"entity", guid}})).root()["name"].as_string() ==
              "InBatch");
        }
        SUBCASE("the redo of the client's entry, which replays the command inside the step") {
            const std::optional<protocol_response> undone = client.dispatch("history.undo");
            REQUIRE(undone.has_value());
            REQUIRE_MESSAGE(undone->ok, undone->errorMessage);
            const std::optional<protocol_response> redone = client.dispatch("history.redo");
            REQUIRE(redone.has_value());
            CHECK_MESSAGE(redone->ok, redone->errorMessage);
            CHECK(editor.ok("entity.get", core::json::object({{"entity", guid}})).root()["name"].as_string() ==
              "Recorded");
        }
        CHECK(dispatcher.pending_count(client.client_id()) == command_dispatcher::DEFAULT_MAX_PENDING_PER_CLIENT);
        dispatcher.cancel_client(client.client_id());
    }

    TEST_CASE("command_dispatcher - an asynchronous mutation completed while playing follows the play rules") {
        editor_test::headless_editor editor = {};
        async_rename command = {};
        SUBCASE("a scene edit is not recorded and warns once") {
            command.install(editor.context());
        }
        SUBCASE("a file edit is kept for play.stop") {
            command.install(editor.context(), command_flags::ON_DISK);
        }
        const std::string guid = editor.create_entity("Before");
        const bool onDisk = has_command_flag(
          *editor.context().registry().find(async_rename::NAME),
          command_flags::ON_DISK
        );
        const usize entries = editor.context().undo().size();
        (void) editor.ok("play.start");
        editor.clear_events();
        editor_test::parked_client client(editor);
        CHECK_FALSE(client.dispatch(async_rename::NAME, rename_args(guid, "Playing")).has_value());
        command.finish(editor.context(), 0);
        CHECK(client.wait().ok);
        CHECK(editor.context().undo().size() == entries);
        CHECK(editor.context().play().kept_file_edits() == (onDisk ? 1u : 0u));
        CHECK(editor.warnings("UNRECORDED_PLAY_EDIT").size() == (onDisk ? 0u : 1u));
        CHECK(editor.warnings("PLAY_FILE_EDIT").size() == (onDisk ? 1u : 0u));
    }

    TEST_CASE("command_dispatcher - complete_mutation of a command that does not mutate asserts and answers plainly") {
        editor_test::headless_editor editor = {};
        async_reply parked = {};
        command_descriptor descriptor = {};
        descriptor.name = "test.async_plain";
        descriptor.flags = command_flags::ASYNC;
        descriptor.handler = [&parked](command_invocation &invocation) -> core::result<command_outcome> {
            parked = invocation.reply;
            return command_outcome{};
        };
        CONJURE_REQUIRE_OK(editor.context().registry().add(std::move(descriptor)));
        editor_test::parked_client client(editor);
        const u64 revision = editor.context().revision();
        CHECK_FALSE(client.dispatch("test.async_plain").has_value());
        command_outcome outcome = {};
        outcome.result = core::json::object({{"value", 3}});
        CONJURE_CHECK_ASSERTS(parked.succeed_mutation(std::move(outcome)));
        const protocol_response answered = client.wait();
        CHECK(answered.ok);
        CHECK(answered.result.root()["value"].as_i64() == 3);
        CHECK(editor.context().revision() == revision);
    }

    TEST_CASE(
      "command_dispatcher - a request of the local UI is logged with its command when it fails later, not at once"
    ) {
        editor_test::headless_editor editor = {};
        async_rename command = {};
        command.install(editor.context());
        const std::string guid = editor.create_entity("Before");
        command_request request = {};
        request.name = std::string(async_rename::NAME);
        request.args = rename_args(guid, "After");
        SUBCASE("a request that fails after its handler returned") {
            const core::result<core::json::document> parked = editor.context().dispatcher().execute(request);
            CONJURE_REQUIRE_OK(parked);
            REQUIRE(command.parked() == 1);
            CHECK_FALSE(editor.log().contains(core::log_level::WARN, "ui: test.async_rename failed"));
            command.fail(0);
            CHECK(editor.log().contains(
              core::log_level::WARN,
              "ui: test.async_rename failed: the change could not be made"
            ));
        }
        SUBCASE("a request that fails inside its handler is left to the caller") {
            // execute() returns that failure, and ui::support::run_command logs it: a second line here would log
            // one failure twice.
            command.set_fails_at_once(true);
            const testing::log_capture failing = {};
            const core::result<core::json::document> failed = editor.context().dispatcher().execute(request);
            CONJURE_CHECK_ERROR(failed, protocol_error_code::IO_ERROR);
            CHECK(command.parked() == 0);
            CHECK_FALSE(failing.contains(core::log_level::WARN, "ui: test.async_rename failed"));
            command.set_fails_at_once(false);
        }
        // Any other client hears the failure in its response instead.
        const testing::log_capture later = {};
        editor_test::parked_client client(editor);
        CHECK_FALSE(client.dispatch(async_rename::NAME, rename_args(guid, "Other")).has_value());
        REQUIRE(command.parked() > 0);
        command.fail(command.parked() - 1);
        CHECK(client.wait().errorCode == protocol_error_code::IO_ERROR);
        CHECK_FALSE(later.contains(core::log_level::WARN, "ui: test.async_rename failed"));
    }
}
