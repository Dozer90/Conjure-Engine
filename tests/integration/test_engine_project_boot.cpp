#include "conjure/testing/doctest.h"

#include "conjure/testing/headless_engine.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "integration_engine_support.h"

#include "conjure/core/types.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/exit_code.h"
#include "conjure/engine/project.h"
#include "conjure/scene/scene.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

// docs/design/06_engine_loop.md 10 test_engine_project_boot: a project directory boots headless, loads its startup
// scene, runs 60 frames and quits clean with no leaks. Two cases: the headless_engine fixture (its memory check is
// the leak gate; a frame at a time) and the executables' own path, an application built from the command line
// `--project <dir> --headless --frames 60` whose run() stops at the frame limit and returns exit_code::OK. The
// design names samples/feature_test, which wave 5 delivers (09_cross_area_resolutions.md 4 item 15c); until then
// the in-repo project tests/assets/projects/minimal (the export round trip's source) is the project that boots
// here, and the expected entity count is read from its startup scene rather than written down twice.

using namespace conjure;
using namespace conjure::testing;

namespace {
    constexpr u32 RUN_FRAMES = 60;
    constexpr std::string_view NAME_COMPONENT_KEY = "\"conjure.scene.name\"";

    /// <summary>
    /// Entities the fixture's startup scene declares: every serialised entity carries a name component.
    /// </summary>
    [[nodiscard]] u32 fixture_entity_count(const std::filesystem::path &projectDirectory) {
        const std::string scene =
          integration_test::read_file(projectDirectory / "assets" / "scenes" / "main.scene.json");
        u32 count = 0;
        usize offset = scene.find(NAME_COMPONENT_KEY);
        while (offset != std::string::npos) {
            ++count;
            offset = scene.find(NAME_COMPONENT_KEY, offset + NAME_COMPONENT_KEY.size());
        }
        REQUIRE(count > 0);
        return count;
    }
}

TEST_SUITE("integration/engine_project_boot") {

    TEST_CASE("engine_project_boot - the minimal project boots headless, loads its startup scene and runs 60 frames") {
        temp_directory scratch("integration_project_boot");
        const std::filesystem::path projectDirectory = integration_test::copy_minimal_project(scratch.path());
        const u32 expectedEntities = fixture_entity_count(projectDirectory);

        headless_engine_options options = integration_test::quiet_options();
        options.projectDirectory = &projectDirectory;
        headless_engine engine(options);

        REQUIRE(engine.context().project != nullptr);
        CHECK(engine.context().project->description().name == "minimal");
        CHECK(engine.context().assetDatabase != nullptr);   // a development project scans its sources
        CHECK(engine.world().entity_count() == expectedEntities);
        CHECK(engine.active_scene().name() == "main");
        CHECK_FALSE(engine.find_entity("Root/Child").is_null());

        engine.step_frames(RUN_FRAMES);
        CHECK(engine.frame_index() == RUN_FRAMES);
        CHECK(engine.fixed_update_count() == RUN_FRAMES);
        CHECK(engine.world().entity_count() == expectedEntities);
        CHECK_FALSE(engine.application().is_quit_requested());
        CHECK(engine.logs().count(core::log_level::ERR) == 0);
    }

    TEST_CASE("engine_project_boot - an application from the command line runs its 60 frames and exits OK") {
        log_capture logs = {};
        temp_directory scratch("integration_project_boot_run");
        const std::filesystem::path projectDirectory = integration_test::copy_minimal_project(scratch.path());
        const u32 expectedEntities = fixture_entity_count(projectDirectory);

        // The runtime's boot path (runtime/src/main.cpp): the merge of defaults, --headless, project.json and the
        // remaining flags, then initialise() and run() until the frame limit requests the quit.
        const std::string projectArgument = projectDirectory.generic_string();
        const std::string framesArgument = std::to_string(RUN_FRAMES);
        const std::array<std::string_view, 7> arguments = {
            "--project", projectArgument, "--headless", "--frames", framesArgument, "--log-level", "warn"
        };
        core::result<engine::application_desc> parsed =
          engine::application_desc::from_command_line(arguments, engine::engine_config::defaults());
        CONJURE_REQUIRE_OK(parsed);
        REQUIRE(parsed->project.has_value());
        CHECK(parsed->unrecognisedArguments.empty());
        CHECK(parsed->config.maxFrames == RUN_FRAMES);

        engine::application app(std::move(parsed).value());
        CONJURE_REQUIRE_OK(app.initialise());
        CHECK(app.context().world->entity_count() == expectedEntities);
        CHECK(app.context().scenes->active().name() == "main");

        const int exitCode = app.run();
        CHECK(exitCode == static_cast<int>(engine::exit_code::OK));
        CHECK(app.current_exit_code() == engine::exit_code::OK);
        CHECK(app.is_quit_requested());
        CHECK(app.time().frameIndex == RUN_FRAMES);
        CHECK(app.time().fixedStepIndex == RUN_FRAMES);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }
}
