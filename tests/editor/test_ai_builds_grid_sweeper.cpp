#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/headless_runner.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/scripting/script_class_registry.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

// An agent builds Grid Sweeper (docs/design/17_demo_games.md 2.1, 04_editor.md 10, 08_testing.md 6.1): the command
// file tests/editor/data/commands/build_grid_sweeper.json runs through headless_runner; the result is an 8 x 8 grid
// of cell prefab instances, a game-state entity with its script, the mines and numbers as component data and an
// orthographic camera. play.start / play.stop leaves the scene byte for byte as it was.

using namespace conjure;
using namespace conjure::editor;

namespace {
    constexpr usize GRID_SIZE = 8;
    constexpr usize MINE_COUNT = 10;

    std::vector<protocol_request> load_requests() {
        const std::string text = editor_test::read_text_file(
          editor_test::fixture_directory() / "build_grid_sweeper.json"
        );
        core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(text);
        REQUIRE_MESSAGE(requests.has_value(), testing::describe_error(requests));
        return std::move(requests).value();
    }
}

TEST_SUITE("editor/ai_builds_grid_sweeper") {

    TEST_CASE("ai_builds_grid_sweeper - the command file runs to the end with exit code 0") {
        editor_test::headless_editor editor = {};
        const std::vector<protocol_request> requests = load_requests();
        std::ostringstream out = {};
        headless_runner runner = {};
        const exec_summary summary = runner.run_requests(editor.application(), requests, out, false);
        CHECK_MESSAGE(summary.failed == 0, out.str());
        CHECK(summary.executed == requests.size());
        CHECK(summary.exitCode == 0);
        CHECK(summary.firstFailedIndex == -1);

        // 1 template cell + 1 board + 64 cells + game state + camera.
        CHECK(editor.scene().world().entity_count() == 2 + GRID_SIZE * GRID_SIZE + 2);
        const core::json::document cells = editor.ok(
          "entity.find",
          core::json::object({{"name", "Cell"}, {"component", "prefab_instance"}})
        );
        CHECK(cells.root()["total"].as_i64() == static_cast<i64>(GRID_SIZE * GRID_SIZE + 1));
        const core::json::document board = editor.ok("entity.get", core::json::object({{"entity", "Board"}}));
        CHECK(board.root()["children"].size() == GRID_SIZE * GRID_SIZE);

        // Every cell under the board is a prefab instance with a sprite and a script holding the mine data.
        usize mines = 0;
        usize numbers = 0;
        for (const core::json::value child : board.root()["children"]) {
            const core::json::document cell = editor.ok("entity.get", core::json::object({{"entity", child}}));
            CHECK(cell.root()["components"].contains("conjure.scene.prefab_instance"));
            CHECK(cell.root()["components"].contains("conjure.renderer.sprite"));
            const core::json::value cellComponents = cell.root()["components"];
            const core::json::value scripts = cellComponents["conjure.scripting.script_component"]["scripts"];
            REQUIRE(scripts.size() == 1);
            // The null host knows no classes: the entry carries the class hash until the assembly arrives.
            CHECK(scripts[0]["hash"].as_u64() == scripting::script_name_hash("Game.CellBehaviour"));
            if (scripts[0]["fields"]["mine"].as_bool(false)) {
                ++mines;
            } else if (scripts[0]["fields"]["adjacent"].as_i64(0) > 0) {
                ++numbers;
            }
        }
        CHECK(mines == MINE_COUNT);
        CHECK(numbers > 0);

        const core::json::document state = editor.ok("entity.get", core::json::object({{"entity", "GameState"}}));
        const core::json::value stateComponents = state.root()["components"];
        const core::json::value stateScripts = stateComponents["conjure.scripting.script_component"]["scripts"];
        REQUIRE(stateScripts.size() == 1);
        CHECK(stateScripts[0]["hash"].as_u64() == scripting::script_name_hash("Game.GameState"));
        CHECK(stateScripts[0]["fields"]["mines"].as_i64() == static_cast<i64>(MINE_COUNT));
        CHECK(stateScripts[0]["fields"]["width"].as_i64() == static_cast<i64>(GRID_SIZE));

        const core::json::document camera = editor.ok(
          "component.get",
          core::json::object({{"entity", "Camera"}, {"path", "camera.cameraData.projection"}})
        );
        CHECK(camera.root()["value"].as_string() == "ORTHOGRAPHIC");
        CHECK(editor.ok("view.mode").root()["mode"].as_string() == "2d");
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "assets" / "prefabs" / "cell.prefab.json"));
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "scripts" / "CellBehaviour.cs"));
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "scripts" / "GameState.cs"));
        CHECK(
          std::filesystem::is_regular_file(editor.project_directory() / "assets" / "scenes" / "grid_sweeper.scene.json")
        );
        CHECK(editor.ok("project.info").root()["startupScene"].as_string() == "scenes/grid_sweeper.scene.json");
    }

    TEST_CASE("ai_builds_grid_sweeper - play.start and play.stop restore the built scene byte for byte") {
        editor_test::headless_editor editor = {};
        const std::vector<protocol_request> requests = load_requests();
        std::ostringstream out = {};
        headless_runner runner = {};
        REQUIRE(runner.run_requests(editor.application(), requests, out, false).failed == 0);
        const editor_test::editor_snapshot built = editor_test::capture_snapshot(editor);
        (void) editor.ok("play.start");
        (void) editor.ok(
          "input.inject",
          core::json::object({{"button", "LEFT"}, {"position", core::json::array({100.0f, 100.0f})}})
        );
        editor.pump(5);
        (void) editor.ok("entity.destroy", core::json::object({{"entity", "Board"}}));
        (void) editor.ok("play.pause");
        (void) editor.ok("play.step", core::json::object({{"fixedSteps", 2}}));
        (void) editor.ok("play.stop");
        const editor_test::editor_snapshot restored = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(built == restored, editor_test::describe_difference(built, restored));
    }

    TEST_CASE(
      "ai_builds_grid_sweeper - the compile step answers NOT_AVAILABLE without dotnet, or a build report with it"
    ) {
        editor_test::headless_editor editor = {};
        const std::vector<protocol_request> requests = load_requests();
        std::ostringstream out = {};
        headless_runner runner = {};
        REQUIRE(runner.run_requests(editor.application(), requests, out, false).failed == 0);
        editor.log().allow_errors();
        if (editor_test::dotnet_available()) {
            editor.context().set_dotnet_executable("dotnet");
        }
        const protocol_response compiled = editor.send("script.compile");
        if (compiled.ok) {
            CHECK(compiled.result.root()["diagnostics"].is_array());
        } else if (editor_test::dotnet_available()) {
            CHECK(compiled.errorCode == protocol_error_code::COMPILE_FAILED);
        } else {
            CHECK(compiled.errorCode == protocol_error_code::NOT_AVAILABLE);
        }
    }

    TEST_CASE("ai_builds_grid_sweeper - running the file twice into one project is rejected cleanly") {
        editor_test::headless_editor editor = {};
        const std::vector<protocol_request> requests = load_requests();
        std::ostringstream out = {};
        headless_runner runner = {};
        REQUIRE(runner.run_requests(editor.application(), requests, out, false).failed == 0);
        std::ostringstream second = {};
        const exec_summary again = runner.run_requests(editor.application(), requests, second, true);
        // The fixed guids and the existing prefab file are refused; nothing crashes and the scene stays coherent.
        CHECK(again.failed > 0);
        CHECK(again.executed == requests.size());
        (void) editor.ok("scene.info");
    }
}
