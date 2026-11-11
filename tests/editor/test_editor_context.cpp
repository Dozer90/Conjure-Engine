#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/builtin_commands.h"
#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/editor_context.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/log_category.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/protocol/protocol_message.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// editor_context.h: the service owner every command runs against.

using namespace conjure;
using namespace conjure::editor;

namespace {
    class counting_task final : public frame_task {
        private:
            int *mTicks = nullptr;
            int mRemaining = 0;

        public:
            counting_task(int *ticks, int remaining) :
                mTicks(ticks),
                mRemaining(remaining) {}

            ~counting_task() override = default;

        public:
            bool tick(editor_context &) override {
                ++*mTicks;
                --mRemaining;
                return mRemaining <= 0;
            }
    };
}

TEST_SUITE("editor/editor_context") {

    TEST_CASE("editor_context - binds the engine services and reports the project and scene state") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        CHECK(context.headless());
        CHECK_FALSE(context.offscreen());
        CHECK(context.has_project());
        REQUIRE(context.project() != nullptr);
        CHECK(context.project()->description().name == "editor_test");
        CHECK(context.has_scene());
        CHECK(context.scene_path() == "assets://scenes/test.scene.json");
        CHECK_FALSE(context.scene_dirty());
        CHECK(context.components().types().size() > 0);
        CHECK(context.input() != nullptr);
        CHECK(context.modules() != nullptr);
        CHECK(context.asset_database() != nullptr);
        CHECK(context.assets() != nullptr);
        CHECK(context.host() != nullptr);
        CHECK(context.server() != nullptr);
        CHECK(context.registry().size() == builtin_command_count());
        CHECK(context.play().is_editing());
        CHECK(context.frame_index() == context.engine().time().frameIndex);
    }

    TEST_CASE("editor_context - the revision counts mutations and marks the scene dirty") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        const u64 before = context.revision();
        (void) editor.create_entity("a");
        CHECK(context.revision() == before + 1);
        CHECK(context.scene_dirty());
        context.set_scene_dirty(false);
        CHECK_FALSE(context.scene_dirty());
        const u64 bumped = context.bump_revision();
        CHECK(bumped == before + 2);
        // The revision alone is not a scene edit: scene.save keeps the flag clear after its own bump.
        CHECK_FALSE(context.scene_dirty());
        (void) editor.ok("history.undo");
        CHECK(context.scene_dirty());
    }

    TEST_CASE("editor_context - normalise_scene_path adds the mount and the extension") {
        CHECK(editor_context::normalise_scene_path("scenes/level") == "assets://scenes/level.scene.json");
        CHECK(editor_context::normalise_scene_path("/scenes/level.json") == "assets://scenes/level.scene.json");
        CHECK(editor_context::normalise_scene_path("assets://a.scene.json") == "assets://a.scene.json");
        CHECK(editor_context::normalise_scene_path("project://x/y") == "project://x/y.scene.json");
        CHECK(editor_context::normalise_scene_path("  ").empty());
    }

    TEST_CASE("editor_context - describe_state carries project, scene, play state, selection and clients") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("picked");
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({guid})}}));
        const core::json::document state = editor.context().describe_state();
        CHECK(state.root()["project"]["name"].as_string() == "editor_test");
        CHECK(state.root()["scene"]["open"].as_bool());
        CHECK(state.root()["playState"].as_string() == "editing");
        CHECK(state.root()["selection"].size() == 1);
        CHECK(state.root()["headless"].as_bool());
        CHECK(state.root().contains("port"));
        CHECK(state.root().contains("frame"));
        CHECK(state.root()["revision"].as_u64() == editor.context().revision());
    }

    TEST_CASE("editor_context - frame tasks tick until they finish") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        int ticks = 0;
        context.add_frame_task(std::make_unique<counting_task>(&ticks, 3));
        context.add_frame_task(nullptr);
        CHECK(context.frame_task_count() == 1);
        editor.pump(1);
        CHECK(ticks == 1);
        editor.pump(5);
        CHECK(ticks == 3);
        CHECK(context.frame_task_count() == 0);
    }

    TEST_CASE("regression - editor.exec_file whose file opens a project answers CANCELLED and leaves no sink") {
        editor_test::headless_editor editor = {};
        const std::filesystem::path other = editor.project_directory() / "second";
        (void) editor.ok("project.create", core::json::object({{"path", other.generic_string()}, {"name", "Second"}}));
        // project.open restarts the engine at the next frame boundary; the wait_frames that follows is still
        // parked when that happens.
        const std::filesystem::path file = editor.project_directory() / "open_then_wait.json";
        {
            std::ofstream output(file, std::ios::binary | std::ios::trunc);
            REQUIRE(output.is_open());
            output << std::format(
              R"([{{"cmd":"project.open","args":{{"path":"{}"}}}},)"
              R"({{"cmd":"editor.wait_frames","args":{{"frames":5}}}}])",
              other.generic_string()
            );
        }
        const usize clientsBefore = editor.context().events().client_count();
        const protocol_response response = editor.send(
          "editor.exec_file",
          core::json::object({{"path", file.generic_string()}})
        );
        CHECK_FALSE(response.ok);
        CHECK(response.errorCode == protocol_error_code::CANCELLED);
        REQUIRE(editor.context().project() != nullptr);
        CHECK(editor.context().project()->description().name == "Second");
        // Nothing of the old task survives: no pending ticket, no frame task, no exec client, and pumping well
        // past where a stale ticket would have timed out delivers nothing to a destroyed sink.
        CHECK(editor.context().dispatcher().pending_count() == 0);
        CHECK(editor.context().frame_task_count() == 0);
        CHECK(editor.context().events().client_count() == clientsBefore);
        editor.context().dispatcher().set_async_timeout_seconds(0.0);
        editor.pump(5);
        editor.context().dispatcher().set_async_timeout_seconds(30.0);
        CHECK(editor.ok("project.info").root()["name"].as_string() == "Second");
    }

    TEST_CASE("regression - rebind cancels the pending asynchronous requests instead of letting them time out") {
        editor_test::headless_editor editor = {};
        command_dispatcher &dispatcher = editor.context().dispatcher();
        std::optional<protocol_response> captured = std::nullopt;
        const u32 client = editor.context().events().register_client({});
        editor.context().register_response_sink(client, [&captured](const protocol_response &response) {
            captured = response;
        });
        protocol_request request = {};
        request.id = make_id(9);
        request.command = "editor.wait_frames";
        request.args = core::json::object({{"frames", 100000}});
        request.clientId = client;
        CHECK_FALSE(dispatcher.dispatch(request).has_value());
        CHECK(dispatcher.pending_count(client) == 1);
        const std::filesystem::path other = editor.project_directory() / "second";
        (void) editor.ok("project.create", core::json::object({{"path", other.generic_string()}, {"name", "Second"}}));
        (void) editor.ok("project.open", core::json::object({{"path", other.generic_string()}}));
        editor.pump(2);
        REQUIRE(captured.has_value());
        CHECK_FALSE(captured->ok);
        CHECK(captured->errorCode == protocol_error_code::CANCELLED);
        CHECK(captured->id.root().as_i64() == 9);
        CHECK(dispatcher.pending_count() == 0);
        CHECK(editor.context().frame_task_count() == 0);
        editor.context().unregister_response_sink(client);
        editor.context().events().unregister_client(client);
    }

    TEST_CASE("editor_context - response sinks receive the responses of their client") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        std::vector<protocol_response> received = {};
        const u32 client = context.events().register_client({});
        context.register_response_sink(client, [&received](const protocol_response &response) {
            received.push_back(response);
        });
        protocol_response response = make_ok_response(make_id(4).root(), core::json::document::object());
        response.clientId = client;
        context.deliver_response(response);
        REQUIRE(received.size() == 1);
        CHECK(received[0].id.root().as_i64() == 4);
        context.unregister_response_sink(client);
        context.deliver_response(response);
        CHECK(received.size() == 1);
        context.events().unregister_client(client);
    }

    TEST_CASE("editor_context - console records are forwarded as log events to subscribed clients") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        context.events().set_subscriptions(editor.client_id(), default_subscriptions() | event_bit(editor_event::LOG));
        context.events().set_min_log_level(editor.client_id(), core::log_level::WARN);
        editor.clear_events();
        CONJURE_LOG_WARN(LOG_EDITOR, "context forwarding probe");
        editor.pump(1);
        bool found = false;
        for (const protocol_event &event : editor.events()) {
            if (
              event.name == "log" && event.data.root()["message"].as_string().find(
                "forwarding probe"
              ) != std::string::npos
            ) {
                found = true;
                CHECK(event.data.root()["level"].as_string() == core::to_string(core::log_level::WARN));
                CHECK(event.data.root()["category"].as_string() == "editor");
            }
        }
        CHECK(found);
    }

    TEST_CASE("editor_context - the selection emits selection.changed") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("sel");
        editor.clear_events();
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({guid})}}));
        CHECK(editor.saw_event("selection.changed"));
    }

    TEST_CASE("editor_context - without a project the context still works and commands report PROJECT_NOT_OPEN") {
        editor_test::headless_editor_options options = {};
        options.withProject = false;
        options.withScene = false;
        editor_test::headless_editor editor(options);
        CHECK_FALSE(editor.context().has_project());
        CHECK(editor.context().project() == nullptr);
        CHECK(editor.fail("project.info") == protocol_error_code::PROJECT_NOT_OPEN);
        CHECK(editor.fail("entity.create", core::json::object({{"name", "x"}})) == protocol_error_code::SCENE_NOT_OPEN);
        const core::json::document state = editor.ok("editor.state");
        CHECK(state.root()["project"].is_null());
    }
}
