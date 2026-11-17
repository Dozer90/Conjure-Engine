#include "conjure/testing/doctest.h"

#include "conjure/core/log.h"
#include "conjure/core/types.h"
#include "conjure/engine/command_line.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"

#include <array>
#include <format>
#include <span>
#include <string>
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

    TEST_CASE("command_line - --scripting is parsed and turns scripting on over a configuration that has it off") {
        const core::result<command_line_options> parsed = parse({"--headless", "--scripting"});
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().headless);
        CHECK(parsed.value().scripting);
        CHECK_FALSE(parsed.value().noScripting);
        CHECK(parsed.value().unrecognisedArguments.empty());

        // Only scripting.enabled changes: every other headless default stays.
        engine_config config = engine_config::headless_defaults();
        REQUIRE_FALSE(config.scripting.enabled);
        apply_command_line(parsed.value(), config);
        engine_config expected = engine_config::headless_defaults();
        expected.scripting.enabled = true;
        CHECK(config == expected);

        // Without the flag the configuration keeps what it had, on or off.
        const core::result<command_line_options> absent = parse({"--frames", "1"});
        CONJURE_REQUIRE_OK(absent);
        CHECK_FALSE(absent.value().scripting);
        engine_config off = engine_config::headless_defaults();
        apply_command_line(absent.value(), off);
        CHECK_FALSE(off.scripting.enabled);
        engine_config on = engine_config::defaults();
        apply_command_line(absent.value(), on);
        CHECK(on.scripting.enabled);
    }

    TEST_CASE("command_line - --scripting with --no-scripting is BAD_ARGUMENTS in either order") {
        struct ordering {
            std::string_view first = "";
            std::string_view second = "";
        };
        constexpr std::array<ordering, 2> ORDERINGS = {{
          {"--scripting", "--no-scripting"},
          {"--no-scripting", "--scripting"},
        }};
        for (const ordering &order : ORDERINGS) {
            CAPTURE(order.first);
            const core::result<command_line_options> parsed = parse({order.first, "--headless", order.second});
            CONJURE_CHECK_ERROR(parsed, engine_error::BAD_ARGUMENTS);
            if (!parsed.has_value()) {
                const std::string &message = parsed.get_error().message();
                CHECK_MESSAGE(message.find("--scripting") != std::string::npos, message);
                CHECK_MESSAGE(message.find("--no-scripting") != std::string::npos, message);
            }
        }

        // Each flag alone, and each one repeated, is fine.
        CONJURE_CHECK_OK(parse({"--scripting", "--scripting"}));
        CONJURE_CHECK_OK(parse({"--no-scripting", "--no-scripting"}));
    }

    TEST_CASE("command_line - options filled by hand with scripting and noScripting end with scripting off") {
        // parse_command_line never returns the pair; apply_command_line still has one answer for it, the same
        // the editor gives when its own --no-scripting meets a forwarded --scripting.
        command_line_options options = {};
        options.scripting = true;
        options.noScripting = true;
        engine_config config = engine_config::defaults();
        apply_command_line(options, config);
        CHECK_FALSE(config.scripting.enabled);
    }

    TEST_CASE("command_line - the help text has one line for --scripting and one for --no-scripting") {
        const std::string_view help = command_line_help();
        for (const std::string_view flag : {"--scripting", "--no-scripting"}) {
            CAPTURE(flag);
            const std::string line = std::format("\n  {} ", flag);
            const usize first = help.find(line);
            REQUIRE(first != std::string_view::npos);
            CHECK(help.find(line, first + 1) == std::string_view::npos);
        }
        // The --headless line says what the opt-in is for.
        const usize headlessLine = help.find("\n  --headless ");
        REQUIRE(headlessLine != std::string_view::npos);
        const usize headlessEnd = help.find('\n', headlessLine + 1);
        CHECK(help.substr(headlessLine, headlessEnd - headlessLine).find("no scripting") != std::string_view::npos);
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

    TEST_CASE("command_line - --log-level takes every level name in any case, off included") {
        struct expectation {
            std::string_view text = "";
            core::log_level level = core::log_level::INFO;
        };
        constexpr std::array<expectation, 9> EXPECTED = {{
          {"trace", core::log_level::TRACE},
          {"debug", core::log_level::DEBUG},
          {"info", core::log_level::INFO},
          {"warn", core::log_level::WARN},
          {"error", core::log_level::ERR},
          {"fatal", core::log_level::FATAL},
          {"off", core::log_level::OFF},
          {"OFF", core::log_level::OFF},
          {"Warn", core::log_level::WARN},
        }};
        for (const expectation &expected : EXPECTED) {
            CAPTURE(expected.text);
            const core::result<command_line_options> parsed = parse({"--log-level", expected.text});
            CONJURE_REQUIRE_OK(parsed);
            REQUIRE(parsed.value().logLevel.has_value());
            CHECK(*parsed.value().logLevel == expected.level);
        }

        // "off" is a request for silence, not a parse failure: it reaches the configuration as OFF.
        const core::result<command_line_options> silenced = parse({"--log-level", "off"});
        CONJURE_REQUIRE_OK(silenced);
        engine_config config = engine_config::defaults();
        apply_command_line(silenced.value(), config);
        CHECK(config.logLevel == core::log_level::OFF);
    }

    TEST_CASE("command_line - an unknown --log-level is BAD_ARGUMENTS and names the value") {
        constexpr std::array<std::string_view, 7> UNKNOWN = {"loud", "of", "offf", "err", "warning", "3", ""};
        for (const std::string_view name : UNKNOWN) {
            CAPTURE(name);
            const core::result<command_line_options> parsed = parse({"--log-level", name});
            CONJURE_CHECK_ERROR(parsed, engine_error::BAD_ARGUMENTS);
            if (!parsed.has_value()) {
                const std::string &message = parsed.get_error().message();
                CHECK_MESSAGE(message.find("--log-level") != std::string::npos, message);
                CHECK_MESSAGE(message.find(std::format("unknown log level '{}'", name)) != std::string::npos, message);
            }
        }

        // Without --log-level nothing is set, so the configuration keeps its own level.
        const core::result<command_line_options> absent = parse({"--headless"});
        CONJURE_REQUIRE_OK(absent);
        CHECK_FALSE(absent.value().logLevel.has_value());
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
