#include "conjure/testing/doctest.h"

#include "conjure/core/log.h"
#include "conjure/core/types.h"
#include "conjure/engine/command_line.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"

#include <array>
#include <span>
#include <string_view>
#include <vector>

// command_line.h (docs/design/06_engine_loop.md 2.11): one case per flag, the unknown-flag pass-through and
// the numeric validation.

using namespace conjure;
using namespace conjure::engine;

namespace {

    [[nodiscard]] core::result<command_line_options> parse(std::initializer_list<std::string_view> arguments) {
        const std::vector<std::string_view> list(arguments);
        return parse_command_line(std::span<const std::string_view>(list));
    }
}

TEST_SUITE("engine/command_line") {

    TEST_CASE("command_line - project and scene flags carry paths") {
        const core::result<command_line_options> parsed =
          parse({"--project", "games/demo", "--scene", "levels/one.scene.json"});
        CONJURE_REQUIRE_OK(parsed);
        REQUIRE(parsed.value().projectPath.has_value());
        CHECK(parsed.value().projectPath->generic_string() == "games/demo");
        REQUIRE(parsed.value().scene.has_value());
        CHECK(parsed.value().scene->generic_string() == "levels/one.scene.json");
    }

    TEST_CASE("command_line - headless, frames and the device flags") {
        const core::result<command_line_options> parsed =
          parse({"--headless", "--frames", "10", "--rhi", "null", "--audio", "miniaudio"});
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().headless);
        CHECK(parsed.value().frames == 10u);
        CHECK(parsed.value().rhi == rhi_backend_choice::NULL_DEVICE);
        CHECK(parsed.value().audio == audio_backend_choice::MINIAUDIO);
        CHECK_FALSE(parsed.value().audioOff);
    }

    TEST_CASE("command_line - audio off and the no-* switches") {
        const core::result<command_line_options> parsed =
          parse({"--audio", "off", "--no-physics", "--no-scripting", "--no-render"});
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().audioOff);
        CHECK_FALSE(parsed.value().audio.has_value());
        CHECK(parsed.value().noPhysics);
        CHECK(parsed.value().noScripting);
        CHECK(parsed.value().noRender);
    }

    TEST_CASE("command_line - window flags") {
        const core::result<command_line_options> parsed =
          parse({"--width", "640", "--height", "480", "--fullscreen", "--no-vsync"});
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().width == 640u);
        CHECK(parsed.value().height == 480u);
        CHECK(parsed.value().fullscreen);
        CHECK(parsed.value().noVsync);
    }

    TEST_CASE("command_line - time and pacing flags") {
        const core::result<command_line_options> parsed =
          parse({"--fixed-hz", "30", "--fixed-frame-delta", "0.02", "--target-hz", "120", "--time-scale", "0.5",
                 "--jobs", "3"});
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().fixedHz == 30u);
        CHECK(parsed.value().fixedFrameDelta == 0.02f);
        CHECK(parsed.value().targetHz == 120u);
        CHECK(parsed.value().timeScale == 0.5f);
        CHECK(parsed.value().jobs == 3u);
    }

    TEST_CASE("command_line - log flags and help") {
        const core::result<command_line_options> parsed =
          parse({"--log-level", "debug", "--log-file", "out.log", "--help"});
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().logLevel == core::log_level::DEBUG);
        REQUIRE(parsed.value().logFile.has_value());
        CHECK(parsed.value().logFile->generic_string() == "out.log");
        CHECK(parsed.value().help);
        CHECK_FALSE(command_line_help().empty());
        CHECK(command_line_help().find("--headless") != std::string_view::npos);
    }

    TEST_CASE("command_line - unknown flags and their values are returned unrecognised") {
        const core::result<command_line_options> parsed =
          parse({"--exec", "commands.json", "--headless", "--export", "out", "dir"});
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().headless);
        const std::vector<std::string> expected = {"--exec", "commands.json", "--export", "out", "dir"};
        CHECK(parsed.value().unrecognisedArguments == expected);
    }

    TEST_CASE("command_line - bad numbers and missing values are BAD_ARGUMENTS") {
        CONJURE_CHECK_ERROR(parse({"--frames", "ten"}), engine_error::BAD_ARGUMENTS);
        CONJURE_CHECK_ERROR(parse({"--width", "0"}), engine_error::BAD_ARGUMENTS);
        CONJURE_CHECK_ERROR(parse({"--fixed-hz"}), engine_error::BAD_ARGUMENTS);
        CONJURE_CHECK_ERROR(parse({"--fixed-frame-delta", "-1"}), engine_error::BAD_ARGUMENTS);
        CONJURE_CHECK_ERROR(parse({"--rhi", "metal"}), engine_error::BAD_ARGUMENTS);
        CONJURE_CHECK_ERROR(parse({"--log-level", "loud"}), engine_error::BAD_ARGUMENTS);
    }

    TEST_CASE("command_line - a value that starts with -- is a missing value") {
        CONJURE_CHECK_ERROR(parse({"--scene", "--no-physics"}), engine_error::BAD_ARGUMENTS);
        CONJURE_CHECK_ERROR(parse({"--project", "--headless"}), engine_error::BAD_ARGUMENTS);
        CONJURE_CHECK_ERROR(parse({"--frames", "--headless", "3"}), engine_error::BAD_ARGUMENTS);
        // A single dash is still a value (a negative number is then rejected by the number check).
        CONJURE_CHECK_ERROR(parse({"--time-scale", "-2"}), engine_error::BAD_ARGUMENTS);
        const core::result<command_line_options> parsed = parse({"--scene", "-odd-name.scene.json"});
        CONJURE_REQUIRE_OK(parsed);
        REQUIRE(parsed.value().scene.has_value());
        CHECK(parsed.value().scene->generic_string() == "-odd-name.scene.json");
    }

    TEST_CASE("command_line - the last --audio flag wins") {
        const core::result<command_line_options> enabled = parse({"--audio", "off", "--audio", "null"});
        CONJURE_REQUIRE_OK(enabled);
        CHECK_FALSE(enabled.value().audioOff);
        engine_config config = engine_config::defaults();
        apply_command_line(enabled.value(), config);
        CHECK(config.audio.enabled);
        CHECK(config.audio.backend == audio_backend_choice::NULL_DEVICE);

        const core::result<command_line_options> disabled = parse({"--audio", "null", "--audio", "off"});
        CONJURE_REQUIRE_OK(disabled);
        CHECK_FALSE(disabled.value().audio.has_value());
        engine_config silent = engine_config::defaults();
        apply_command_line(disabled.value(), silent);
        CHECK_FALSE(silent.audio.enabled);
    }

    TEST_CASE("command_line - apply_command_line lays the overrides over a configuration") {
        const core::result<command_line_options> parsed = parse(
          {"--scene", "a.scene.json", "--frames", "5", "--audio", "off", "--no-physics", "--width", "320",
           "--height", "200",
           "--fixed-hz", "50", "--fixed-frame-delta", "0.01", "--target-hz", "90", "--time-scale", "2", "--jobs", "4",
           "--log-level", "error", "--no-vsync", "--fullscreen"}
        );
        CONJURE_REQUIRE_OK(parsed);
        engine_config config = engine_config::defaults();
        apply_command_line(parsed.value(), config);
        CHECK(config.startupSceneOverride.generic_string() == "a.scene.json");
        CHECK(config.maxFrames == 5);
        CHECK_FALSE(config.audio.enabled);
        CHECK_FALSE(config.physics.enabled);
        CHECK(config.window.width == 320);
        CHECK(config.window.height == 200);
        CHECK(config.time.fixedDeltaSeconds == 1.0f / 50.0f);
        CHECK(config.time.forcedFrameDeltaSeconds == 0.01f);
        CHECK(config.pacing.targetHz == 90);
        CHECK(config.time.timeScale == 2.0f);
        CHECK(config.jobs.workerCount == 4);
        CHECK(config.logLevel == core::log_level::ERR);
        CHECK_FALSE(config.window.vsync);
        CHECK(config.window.fullscreen);
    }

    TEST_CASE("command_line - an audio backend re-enables audio") {
        const core::result<command_line_options> parsed = parse({"--audio", "null"});
        CONJURE_REQUIRE_OK(parsed);
        engine_config config = engine_config::defaults();
        config.audio.enabled = false;
        apply_command_line(parsed.value(), config);
        CHECK(config.audio.enabled);
        CHECK(config.audio.backend == audio_backend_choice::NULL_DEVICE);
    }
}
