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
// application_desc::from_command_line() orders the whole merge (defaults < --headless defaults < project.json <
// the remaining flags).
//
// Recognised flags: --project <dir>, --scene <asset path>, --headless, --frames <n>, --rhi d3d12|vulkan|null,
// --audio miniaudio|null|off, --no-physics, --scripting, --no-scripting, --no-render, --width <n>,
// --height <n>, --fullscreen, --no-vsync, --fixed-hz <n>, --fixed-frame-delta <seconds>, --target-hz <n>,
// --time-scale <f>, --jobs <n>, --gpu-validation off|debug|gpu, --log-level <level>, --log-file <path>, --help.
// Anything else is returned in unrecognisedArguments for the executable (--exec, --export).
//
// --gpu-validation sets rendering.gpuValidation for one run (code sweep F458): off, debug (DEBUG_LAYER) or gpu
// (GPU_BASED, which only D3D12 tells apart from debug). It wins over the build default and project.json, which
// can only raise the level (project::apply_to); when the flag is given several times the last one wins.
//
// --scripting is the opt-in of a headless run (06 D12, code sweep F075): --headless starts without the
// scripting subsystem whatever project.json says, so that a smoke test or a CI boot needs no .NET runtime;
// --headless --scripting runs the project's scripts and, with scripting.required at its default, fails the
// start with SCRIPT_RUNTIME_NOT_FOUND where no runtime is installed. A build without CONJURE_WITH_SCRIPTING has
// no scripting subsystem: the flag is accepted and the run goes on without scripts; the engine logs a WARN that
// the build has no scripting module for a headless run outside the editor and for any run whose configuration
// names a game assembly.

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
        bool scripting = false;
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
        std::optional<gpu_validation_level> gpuValidation = std::nullopt;
        std::optional<core::log_level> logLevel = std::nullopt;
        std::optional<std::filesystem::path> logFile = std::nullopt;
        bool help = false;
        std::vector<std::string> unrecognisedArguments = {};
    };

    /// <summary>
    /// Parses the arguments (argv[1..], not the program name). Fails with BAD_ARGUMENTS for a flag without
    /// its value (a value token that starts with -- counts as a missing value), a value that does not parse
    /// (a non-number, a zero where a positive count is needed, an unknown backend, validation level or log
    /// level), and for --scripting together with --no-scripting in either order. When --audio or
    /// --gpu-validation is given several times the last one wins. Unknown tokens, and the values that follow
    /// one until the next known flag, land in unrecognisedArguments in order.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<command_line_options> parse_command_line(
      std::span<const std::string_view> arguments
    );

    /// <summary>
    /// Applies every override except `headless` and `projectPath` (which from_command_line handles before the
    /// project is loaded): --scene, --frames, backends, --scripting, the --no-* switches, window, time, pacing,
    /// jobs, the GPU validation level and log settings. --rhi null leaves rendering enabled on the null
    /// device; --audio off disables audio. `scripting` sets scripting.enabled, `noScripting` clears it and is
    /// applied second, so options filled by hand with both (parse_command_line refuses the pair) end with
    /// scripting off.
    /// </summary>
    CONJURE_API void apply_command_line(const command_line_options &options, engine_config &config);

    /// <summary>
    /// The usage text printed for --help.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view command_line_help() noexcept;
}
