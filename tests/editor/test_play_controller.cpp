#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/undo_stack.h"

#include <string>

// play_controller.h (docs/design/04_editor.md D9, 3.7).

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/play_controller") {

    TEST_CASE("play_state - names") {
        CHECK(to_string(play_state::EDITING) == "editing");
        CHECK(to_string(play_state::PLAYING) == "playing");
        CHECK(to_string(play_state::PAUSED) == "paused");
    }

    TEST_CASE("play.start - snapshots the scene, freezes undo and runs the engine") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Thing");
        editor.clear_events();
        const core::json::document state = editor.ok("play.start");
        CHECK(state.root()["state"].as_string() == "playing");
        CHECK(editor.context().play().state() == play_state::PLAYING);
        CHECK(editor.context().undo().frozen());
        CHECK(editor.saw_event("play.state"));
        const u64 fixedBefore = editor.context().engine().time().fixedStepIndex;
        editor.pump(3);
        CHECK(editor.context().engine().time().fixedStepIndex > fixedBefore);
        CHECK(editor.context().play().simulation_frame() >= 3);
        (void) editor.ok("play.stop");
        CHECK_FALSE(editor.context().undo().frozen());
        CHECK(editor.context().play().simulation_frame() == 0);
    }

    TEST_CASE("play.start - shutting the editor down while playing leaves play first") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Live");
        (void) editor.ok("play.start");
        editor.pump(2);
        CHECK_FALSE(editor.context().play().is_editing());
        // The fixture's destructor shuts the application down mid-play; nothing may crash or assert.
    }

    TEST_CASE("play.stop - restores 10,000 entities byte for byte and discards play-time edits") {
        editor_test::headless_editor editor = {};
        (void) editor.ok(
          "entity.create_many",
          core::json::object({
            {"count", 10000},
            {"components", core::json::object({{"sprite", core::json::document::object()}})},
          })
        );
        const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
        (void) editor.ok("play.start");
        (void) editor.create_entity("transient");
        (void) editor.ok("entity.destroy", core::json::object({{"entity", "Entity_7"}}));
        editor.pump(2);
        CHECK(editor.scene().world().entity_count() == 10000);
        (void) editor.ok("play.stop");
        const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(before == after, editor_test::describe_difference(before, after));
        CHECK(editor.context().undo().size() == 1);
    }

    TEST_CASE("play.stop - the selection is remapped by guid") {
        editor_test::headless_editor editor = {};
        const std::string kept = editor.create_entity("Kept");
        const std::string gone = editor.create_entity("Gone");
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({kept, gone})}}));
        (void) editor.ok("play.start");
        (void) editor.ok("entity.destroy", core::json::object({{"entity", gone}}));
        (void) editor.ok("selection.clear");
        editor.clear_events();
        (void) editor.ok("play.stop");
        const core::json::document selection = editor.ok("selection.get");
        REQUIRE(selection.root()["guids"].size() == 2);
        CHECK(editor.saw_event("selection.changed"));
        (void) editor.ok("entity.get", core::json::object({{"entity", gone}}));
    }

    TEST_CASE("play.pause / resume / step - run exactly n fixed updates") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("play.start");
        (void) editor.ok("play.pause");
        CHECK(editor.context().play().state() == play_state::PAUSED);
        editor.pump(3);
        const u64 paused = editor.context().engine().time().fixedStepIndex;
        editor.pump(2);
        CHECK(editor.context().engine().time().fixedStepIndex == paused);
        const core::json::document stepped = editor.ok("play.step", core::json::object({{"fixedSteps", 4}}));
        CHECK(stepped.root()["state"].as_string() == "paused");
        CHECK(editor.context().engine().time().fixedStepIndex == paused + 4);
        CHECK(stepped.root()["fixedStepIndex"].as_u64() == paused + 4);
        editor.pump(2);
        CHECK(editor.context().engine().time().fixedStepIndex == paused + 4);
        (void) editor.ok("play.resume");
        CHECK(editor.context().play().state() == play_state::PLAYING);
        editor.pump(1);
        CHECK(editor.context().engine().time().fixedStepIndex > paused + 4);
        (void) editor.ok("play.stop");
    }

    TEST_CASE("play.* - illegal transitions are WRONG_PLAY_STATE and a zero step INVALID_ARGS") {
        editor_test::headless_editor editor = {};
        CHECK(editor.fail("play.stop") == protocol_error_code::WRONG_PLAY_STATE);
        CHECK(editor.fail("play.pause") == protocol_error_code::WRONG_PLAY_STATE);
        CHECK(editor.fail("play.resume") == protocol_error_code::WRONG_PLAY_STATE);
        CHECK(editor.fail("play.step") == protocol_error_code::WRONG_PLAY_STATE);
        (void) editor.ok("play.start");
        CHECK(editor.fail("play.start") == protocol_error_code::WRONG_PLAY_STATE);
        CHECK(editor.fail("play.resume") == protocol_error_code::WRONG_PLAY_STATE);
        CHECK(editor.fail("play.step") == protocol_error_code::WRONG_PLAY_STATE);
        (void) editor.ok("play.pause");
        CHECK(editor.fail("play.pause") == protocol_error_code::WRONG_PLAY_STATE);
        CHECK(editor.fail("play.step", core::json::object({{"fixedSteps", 0}})) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail("history.undo") == protocol_error_code::WRONG_PLAY_STATE);
        (void) editor.ok("play.stop");
        CHECK(editor.context().play().is_editing());
    }

    TEST_CASE("play.state - reports frame counters") {
        editor_test::headless_editor editor = {};
        core::json::document state = editor.ok("play.state");
        CHECK(state.root()["state"].as_string() == "editing");
        CHECK(state.root()["simulationFrame"].as_u64() == 0);
        (void) editor.ok("play.start");
        editor.pump(2);
        state = editor.ok("play.state");
        CHECK(state.root()["state"].as_string() == "playing");
        CHECK(state.root()["simulationFrame"].as_u64() >= 2);
        CHECK(state.root()["frame"].as_u64() == editor.context().frame_index());
        CHECK(state.root().contains("simulationTime"));
        (void) editor.ok("play.stop");
    }

    TEST_CASE("play_controller - a second step while one is pending is BUSY") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("play.start");
        (void) editor.ok("play.pause");
        protocol_request first = {};
        first.id = make_id(1);
        first.command = "play.step";
        first.args = core::json::object({{"fixedSteps", 1}});
        first.clientId = editor.client_id();
        CHECK_FALSE(editor.context().dispatcher().dispatch(first).has_value());
        CHECK(editor.context().play().has_pending_step());
        CHECK(editor.fail("play.step") == protocol_error_code::BUSY);
        editor.pump(2);
        CHECK_FALSE(editor.context().play().has_pending_step());
        (void) editor.ok("play.stop");
    }
}
