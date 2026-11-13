#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/uuid.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/undo_stack.h"

#include <optional>
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
        const u64 simulated = editor.context().play().simulation_frame();
        CHECK(simulated >= 3);
        (void) editor.ok("play.stop");
        CHECK_FALSE(editor.context().undo().frozen());
        CHECK(editor.context().play().simulation_frame() == simulated);
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

    TEST_CASE("regression - F657 play.stop keeps the primary selection of before the play session") {
        editor_test::headless_editor editor = {};
        const std::string firstText = editor.create_entity("First");
        const std::string secondText = editor.create_entity("Second");
        const std::optional<core::uuid> first = core::uuid::parse(firstText);
        const std::optional<core::uuid> second = core::uuid::parse(secondText);
        REQUIRE(first.has_value());
        REQUIRE(second.has_value());
        // Name the lower guid last: the sorted backup alone would hand the primary to the higher one.
        const bool firstIsLower = *first < *second;
        const std::string &lower = firstIsLower ? firstText : secondText;
        const std::string &higher = firstIsLower ? secondText : firstText;
        const core::uuid lowerGuid = firstIsLower ? *first : *second;
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({higher, lower})}}));
        const core::json::document before = editor.ok("selection.get");
        CHECK(core::uuid::parse(before.root()["primary"].as_string()) == lowerGuid);
        (void) editor.ok("play.start");
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({higher})}}));
        (void) editor.ok("play.stop");
        const core::json::document after = editor.ok("selection.get");
        CHECK(after.root()["guids"].size() == 2);
        CHECK(core::uuid::parse(after.root()["primary"].as_string()) == lowerGuid);
    }

    TEST_CASE("play.pause / resume / step - run exactly n fixed updates") {
        editor_test::headless_editor editor = {};
        // Steps before this session: play.step answers with the session's count, not the engine's.
        (void) editor.ok("play.start");
        editor.pump(2);
        (void) editor.ok("play.stop");
        (void) editor.ok("play.start");
        const u64 sessionStart = editor.context().engine().time().fixedStepIndex;
        REQUIRE(sessionStart > 0);
        (void) editor.ok("play.pause");
        CHECK(editor.context().play().state() == play_state::PAUSED);
        editor.pump(3);
        const u64 paused = editor.context().engine().time().fixedStepIndex;
        editor.pump(2);
        CHECK(editor.context().engine().time().fixedStepIndex == paused);
        const core::json::document stepped = editor.ok("play.step", core::json::object({{"fixedSteps", 4}}));
        CHECK(stepped.root()["state"].as_string() == "paused");
        CHECK(editor.context().engine().time().fixedStepIndex == paused + 4);
        CHECK(stepped.root()["fixedStepIndex"].as_u64() == paused + 4 - sessionStart);
        CHECK(editor.context().play().simulation_fixed_steps() == paused + 4 - sessionStart);
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

    TEST_CASE("regression - F730 play.* report the clocks of the play session and keep the ended session's") {
        editor_test::headless_editor editor = {};
        const f64 fixedDelta = static_cast<f64>(editor.context().engine().time().fixedDeltaTime);
        // A first session moves the engine's clocks on, so the second one starts from engine values above 0.
        (void) editor.ok("play.start");
        editor.pump(3);
        (void) editor.ok("play.stop");
        REQUIRE(editor.context().engine().time().fixedStepIndex > 0);

        const core::json::document started = editor.ok("play.start");
        CHECK(started.root()["simulationFrame"].as_u64() == 0);
        CHECK(started.root()["fixedStepIndex"].as_u64() == 0);
        CHECK(started.root()["simulationTime"].as_f64() == 0.0);
        editor.pump(4);
        const core::json::document running = editor.ok("play.state");
        const u64 runningSteps = running.root()["fixedStepIndex"].as_u64();
        CHECK(runningSteps > 0);
        const f64 runningTime = running.root()["simulationTime"].as_f64();
        CHECK(runningTime == doctest::Approx(static_cast<f64>(runningSteps) * fixedDelta));

        // Paused frames still run (UPDATE systems flagged RUNS_WHILE_PAUSED), the session's clocks stand still.
        const core::json::document paused = editor.ok("play.pause");
        editor.pump(3);
        const core::json::document stillPaused = editor.ok("play.state");
        CHECK(stillPaused.root()["fixedStepIndex"].as_u64() == paused.root()["fixedStepIndex"].as_u64());
        CHECK(stillPaused.root()["simulationTime"].as_f64() == paused.root()["simulationTime"].as_f64());
        CHECK(stillPaused.root()["simulationFrame"].as_u64() == paused.root()["simulationFrame"].as_u64());

        // play.stop and the play.state event it emits carry the ended session's clocks, which stay while editing.
        editor.clear_events();
        const core::json::document stopped = editor.ok("play.stop");
        CHECK(stopped.root()["state"].as_string() == "editing");
        CHECK(stopped.root()["simulationFrame"].as_u64() == paused.root()["simulationFrame"].as_u64());
        CHECK(stopped.root()["simulationFrame"].as_u64() >= 4);
        CHECK(stopped.root()["fixedStepIndex"].as_u64() == paused.root()["fixedStepIndex"].as_u64());
        CHECK(stopped.root()["simulationTime"].as_f64() == paused.root()["simulationTime"].as_f64());
        bool sawStopState = false;
        for (const protocol_event &event : editor.events()) {
            if (event.name != "play.state") {
                continue;
            }
            sawStopState = true;
            CHECK(event.data.root()["simulationFrame"].as_u64() == stopped.root()["simulationFrame"].as_u64());
            CHECK(event.data.root()["fixedStepIndex"].as_u64() == stopped.root()["fixedStepIndex"].as_u64());
        }
        CHECK(sawStopState);
        editor.pump(2);
        const core::json::document editing = editor.ok("play.state");
        CHECK(editing.root()["fixedStepIndex"].as_u64() == stopped.root()["fixedStepIndex"].as_u64());
        CHECK(editing.root()["simulationTime"].as_f64() == stopped.root()["simulationTime"].as_f64());
        CHECK(editor.context().play().simulation_fixed_steps() == stopped.root()["fixedStepIndex"].as_u64());
        CHECK(editor.context().play().simulation_time() == stopped.root()["simulationTime"].as_f64());

        // profiler.frame_stats keeps the engine's clocks under the same names.
        const core::json::document stats = editor.ok("profiler.frame_stats");
        CHECK(stats.root()["fixedStepIndex"].as_u64() == editor.context().engine().time().fixedStepIndex);
        CHECK(stats.root()["fixedStepIndex"].as_u64() > stopped.root()["fixedStepIndex"].as_u64());
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
