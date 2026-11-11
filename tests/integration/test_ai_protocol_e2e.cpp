#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "integration_engine_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/types.h"
#include "conjure/ecs/world.h"
#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/editor_application.h"
#include "conjure/editor/editor_context.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/editor_options.h"
#include "conjure/editor/headless_runner.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/scene/scene.h"

#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// docs/design/08_testing.md 6.1 test_ai_protocol_e2e (R15; 17_demo_games.md 2.1): the Grid Sweeper command file
// (tests/editor/data/commands/build_grid_sweeper.json, the editor owner's copy of the samples/ command file)
// runs through editor::headless_runner against a headless editor over a scratch project; the saved scene reopens
// with an equal canonical serialisation.
//
// The design asks for an equal world_hash. ecs::world_hash folds the entity ids (index and generation) in, and a
// REPLACE reload into the same world reuses the freed slots with bumped generations, so the hash of the reopened
// scene can never equal the built one; the canonical scene text (which carries guids, not ids) is the comparison
// that holds. An id-independent world_hash variant is requested in 09_cross_area_resolutions.md 4 item 15e.

using namespace conjure;
using namespace conjure::testing;

namespace {
    constexpr std::string_view COMMAND_FILE = "build_grid_sweeper.json";
    constexpr std::string_view SCENE_PATH = "scenes/grid_sweeper.scene.json";
    constexpr usize GRID_SIZE = 8;

    /// <summary>
    /// A headless editor over a scratch project (null platform, rhi and audio; scripting on the null host).
    /// </summary>
    class scratch_editor {
        private:
            temp_directory mDirectory;
            editor::editor_application mApplication = {};

        public:
            scratch_editor() :
                mDirectory("integration_ai_protocol") {
                integration_test::write_file(
                  mDirectory.path() / "project.json",
                  integration_test::project_json("grid_sweeper")
                );
                editor::editor_options options = {};
                options.headless = true;
                options.aiPort = u16{0};
                options.timeoutSeconds = 30.0;
                options.projectPath = mDirectory.path();
                options.engineArguments = {"--log-level", "warn"};
                CONJURE_REQUIRE_OK(mApplication.initialise(options, {}));
                // script.compile is not part of the file; a missing dotnet keeps any build attempt a clean failure.
                mApplication.context().set_dotnet_executable("conjure-integration-tests-no-dotnet-here");
            }

            ~scratch_editor() {
                mApplication.shutdown();
            }

            scratch_editor(const scratch_editor &) = delete;
            scratch_editor &operator=(const scratch_editor &) = delete;

        public:
            [[nodiscard]] editor::editor_application &application() noexcept {
                return mApplication;
            }

            [[nodiscard]] const std::filesystem::path &project_directory() const noexcept {
                return mDirectory.path();
            }

            /// <summary>
            /// Dispatches one synchronous request in process (the asynchronous ones answer through a sink the
            /// headless runner owns; this helper is for scene.open and the other immediate commands).
            /// </summary>
            [[nodiscard]] editor::protocol_response send(std::string_view command, core::json::document args) {
                editor::protocol_request request = {};
                request.id = editor::make_id(i64{1});
                request.command = std::string(command);
                request.args = std::move(args);
                std::optional<editor::protocol_response> response =
                  mApplication.context().dispatcher().dispatch(request);
                REQUIRE_MESSAGE(response.has_value(), command << " answered asynchronously");
                return *response;
            }
    };

    [[nodiscard]] std::vector<editor::protocol_request> load_requests() {
        const std::filesystem::path file = integration_test::editor_command_directory() / COMMAND_FILE;
        const std::string text = integration_test::read_file(file);
        core::result<std::vector<editor::protocol_request>> requests =
          editor::headless_runner::parse_command_file(text);
        CONJURE_REQUIRE_OK(requests);
        return std::move(requests).value();
    }
}

TEST_SUITE("integration/ai_protocol_e2e") {

    TEST_CASE("ai_protocol_e2e - the Grid Sweeper command file builds the game and the saved scene reloads equal") {
        log_capture logs = {};
        scratch_editor editor = {};
        const std::vector<editor::protocol_request> requests = load_requests();
        REQUIRE_FALSE(requests.empty());

        std::ostringstream out = {};
        editor::headless_runner runner = {};
        const editor::exec_summary summary = runner.run_requests(editor.application(), requests, out, false);
        CHECK_MESSAGE(summary.failed == 0, out.str());
        CHECK(summary.executed == requests.size());
        CHECK(summary.succeeded == requests.size());
        CHECK(summary.exitCode == 0);
        CHECK(summary.firstFailedIndex == -1);

        // The board, its 64 cells, the template cell, the game state and the camera.
        editor::editor_context &context = editor.application().context();
        const scene::scene &built = context.scene();
        CHECK(built.world().entity_count() == 2 + GRID_SIZE * GRID_SIZE + 2);
        CHECK(std::filesystem::is_regular_file(editor.project_directory() / "assets" / SCENE_PATH));
        const std::string builtText = integration_test::serialise_scene(built);

        // Reopening the saved file yields the same canonical scene (see the header on why not world_hash).
        const editor::protocol_response reopened = editor.send(
          "scene.open",
          core::json::object({{"path", std::string(SCENE_PATH)}})
        );
        REQUIRE_MESSAGE(reopened.ok, editor::to_string(reopened.errorCode) << " " << reopened.errorMessage);
        CHECK(reopened.result.root()["entityCount"].as_i64() == static_cast<i64>(2 + GRID_SIZE * GRID_SIZE + 2));
        const std::string reopenedText = integration_test::serialise_scene(context.scene());
        CHECK(reopenedText == builtText);
    }
}
