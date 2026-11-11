#pragma once

#include "conjure/engine/engine_config.h"

#include "conjure/core/api.h"
#include "conjure/core/log.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The command line (docs/design/06_engine_loop.md 2.11): parse_command_line() turns the arguments into
// a set of optional overrides, apply_command_line() lays them over an engine_config, and
// application_desc::from_command_line() orders the whole merge (defaults < project.json < command line).
//
// Recognised flags: --project <dir>, --scene <asset path>, --headless, --frames <n>, --rhi d3d12|vulkan|null,
// --audio miniaudio|null|off, --no-physics, --no-scripting, --no-render, --width <n>, --height <n>,
// --fullscreen, --no-vsync, --fixed-hz <n>, --fixed-frame-delta <seconds>, --target-hz <n>,
// --time-scale <f>, --jobs <n>, --log-level <level>, --log-file <path>, --help. Anything else is returned
// in unrecognisedArguments for the executable (--exec, --export).

namespace conjure::engine {

    struct command_line_options {
        std::optional<std::filesystem::path> projectPath = std::nullopt;
        std::optional<std::filesystem::path> scene = std::nullopt;
        bool headless = false;
        std::optional<u64> frames = std::nullopt;
        std::optional<rhi_backend_choice> rhi = std::nullopt;
        std::optional<audio_backend_choice> audio = std::nullopt;
        bool audioOff = false;
        bool noPhysics = false;
        bool noScripting = false;
        bool noRender = false;
        std::optional<u32> width = std::nullopt;
        std::optional<u32> height = std::nullopt;
        bool fullscreen = false;
        bool noVsync = false;
        std::optional<u32> fixedHz = std::nullopt;
        std::optional<f32> fixedFrameDelta = std::nullopt;
        std::optional<u32> targetHz = std::nullopt;
        std::optional<f32> timeScale = std::nullopt;
        std::optional<u32> jobs = std::nullopt;
        std::optional<core::log_level> logLevel = std::nullopt;
        std::optional<std::filesystem::path> logFile = std::nullopt;
        bool help = false;
        std::vector<std::string> unrecognisedArguments = {};
    };

    /// <summary>
    /// Parses the arguments (argv[1..], not the program name). Fails with BAD_ARGUMENTS for a flag without
    /// its value, a value that does not parse (a non-number, a zero where a positive count is needed, an
    /// unknown backend or log level). Unknown tokens, and the values that follow one until the next known
    /// flag, land in unrecognisedArguments in order.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<command_line_options> parse_command_line(
      std::span<const std::string_view> arguments
    );

    /// <summary>
    /// Applies every override except `headless` and `projectPath` (which from_command_line handles before the
    /// project is loaded): --scene, --frames, backends, the --no-* switches, window, time, pacing, jobs and log
    /// settings. --rhi null leaves rendering enabled on the null device; --audio off disables audio.
    /// </summary>
    CONJURE_API void apply_command_line(const command_line_options &options, engine_config &config);

    /// <summary>
    /// The usage text printed for --help.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view command_line_help() noexcept;
}
