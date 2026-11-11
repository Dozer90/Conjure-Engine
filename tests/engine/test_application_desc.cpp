#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "engine_test_support.h"

#include "conjure/core/types.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/command_line.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"

#include <span>
#include <string_view>
#include <vector>

// application_desc.h (docs/design/06_engine_loop.md 2.11, D9): the merge order defaults < --headless <
// project.json < the remaining flags, validated once at the end.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    [[nodiscard]] core::result<application_desc> from_arguments(
      std::initializer_list<std::string_view> arguments,
      const engine_config &base = engine_config::defaults()
    ) {
        const std::vector<std::string_view> list(arguments);
        return application_desc::from_command_line(std::span<const std::string_view>(list), base);
    }
}

TEST_SUITE("engine/application_desc") {

    TEST_CASE("application_desc - no arguments yields the base configuration") {
        const core::result<application_desc> desc = from_arguments({});
        CONJURE_REQUIRE_OK(desc);
        CHECK(desc.value().config == engine_config::defaults());
        CHECK_FALSE(desc.value().project.has_value());
        CHECK_FALSE(desc.value().showHelp);
        CHECK(desc.value().clockOverride == nullptr);
    }

    TEST_CASE("application_desc - help short-circuits the merge") {
        const core::result<application_desc> desc = from_arguments({"--help", "--rhi", "null"});
        CONJURE_REQUIRE_OK(desc);
        CHECK(desc.value().showHelp);
    }

    TEST_CASE("application_desc - headless applies before the other flags and keeps the null devices") {
        const core::result<application_desc> desc = from_arguments({"--headless", "--frames", "3"});
        CONJURE_REQUIRE_OK(desc);
        const engine_config &config = desc.value().config;
        CHECK(config.headless);
        CHECK(config.rendering.backend == rhi_backend_choice::NULL_DEVICE);
        CHECK(config.audio.backend == audio_backend_choice::NULL_DEVICE);
        CHECK(config.window.width == 1280);
        CHECK(config.maxFrames == 3);
        CHECK(config.time.forcedFrameDeltaSeconds == 1.0f / 60.0f);
        CHECK_FALSE(config.scripting.enabled);
        CHECK_FALSE(config.assets.hotReload);
    }

    TEST_CASE("regression - review 4 --headless turns asset hot reload off even when the project asks for it") {
        // Before the fix the --headless merge copied every headless_defaults() field but assets.hotReload, so a
        // command-line headless run (conjure_runtime --headless, conjure_editor --headless / --offscreen) kept the
        // defaults() watcher and project::apply_to's headless guard protected a value this path never set.
        log_capture logs = {};
        temp_directory directory("engine_desc_hot_reload");
        const project loaded = engine_test::write_project(
          directory.path(),
          "watching",
          "start.scene.json",
          R"("assets": { "hot_reload": true })"
        );
        CHECK(loaded.description().assets.hotReload);

        const std::string projectArgument = directory.path().string();
        const std::vector<std::string_view> arguments = {"--project", projectArgument, "--headless"};
        const core::result<application_desc> desc =
          application_desc::from_command_line(std::span<const std::string_view>(arguments), engine_config::defaults());
        CONJURE_REQUIRE_OK(desc);
        CHECK(desc.value().config.headless);
        CHECK_FALSE(desc.value().config.assets.hotReload);

        // Without --headless the same project switches the watcher on over a base that had it off.
        engine_config base = engine_config::defaults();
        base.assets.hotReload = false;
        const std::vector<std::string_view> interactive = {"--project", projectArgument};
        const core::result<application_desc> windowed =
          application_desc::from_command_line(std::span<const std::string_view>(interactive), base);
        CONJURE_REQUIRE_OK(windowed);
        CHECK(windowed.value().config.assets.hotReload);
    }

    TEST_CASE("application_desc - the command line wins over project.json which wins over defaults") {
        log_capture logs = {};
        temp_directory directory("engine_desc");
        const project loaded = engine_test::write_project(
          directory.path(),
          "desc_project",
          "start.scene.json",
          R"("window": { "width": 800, "height": 600, "title": "From Project" }, "time": { "fixed_hz": 30 })"
        );
        CHECK(loaded.description().window.width == 800);

        const std::string projectArgument = directory.path().string();
        const std::vector<std::string_view> arguments = {"--project", projectArgument, "--width", "1024", "--headless"};
        const core::result<application_desc> desc =
          application_desc::from_command_line(std::span<const std::string_view>(arguments), engine_config::defaults());
        CONJURE_REQUIRE_OK(desc);
        const engine_config &config = desc.value().config;
        REQUIRE(desc.value().project.has_value());
        CHECK(config.projectPath == loaded.root_directory());
        CHECK(config.window.title == "From Project");
        CHECK(config.time.fixedDeltaSeconds == 1.0f / 30.0f);
        CHECK(config.startupSceneOverride.generic_string() == "start.scene.json");
        // Headless keeps its window size and null backends against the project, the flag then wins over both.
        CHECK(config.window.width == 1024);
        CHECK(config.window.height == 720);
        CHECK(config.rendering.backend == rhi_backend_choice::NULL_DEVICE);
    }

    TEST_CASE("application_desc - a missing project directory is PROJECT_NOT_FOUND") {
        temp_directory directory("engine_desc_missing");
        const std::string missing = (directory.path() / "nowhere").string();
        const std::vector<std::string_view> arguments = {"--project", missing};
        CONJURE_CHECK_ERROR(
          application_desc::from_command_line(std::span<const std::string_view>(arguments), engine_config::defaults()),
          engine_error::PROJECT_NOT_FOUND
        );
    }

    TEST_CASE("application_desc - the merged configuration is validated once") {
        CONJURE_CHECK_ERROR(from_arguments({"--rhi", "null"}), engine_error::INVALID_CONFIG);
        CONJURE_CHECK_OK(from_arguments({"--rhi", "null", "--headless"}));
    }

    TEST_CASE("application_desc - unrecognised arguments are handed back to the executable") {
        const core::result<application_desc> desc = from_arguments({"--exec", "run.json", "--headless"});
        CONJURE_REQUIRE_OK(desc);
        const std::vector<std::string> expected = {"--exec", "run.json"};
        CHECK(desc.value().unrecognisedArguments == expected);
        CHECK(desc.value().config.headless);
    }

    TEST_CASE("application_desc - from_options is the same merge without parsing") {
        command_line_options options = {};
        options.headless = true;
        options.frames = 7;
        const core::result<application_desc> desc = application_desc::from_options(options, engine_config::defaults());
        CONJURE_REQUIRE_OK(desc);
        CHECK(desc.value().config.headless);
        CHECK(desc.value().config.maxFrames == 7);
    }
}
