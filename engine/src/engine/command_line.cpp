#include "conjure/engine/command_line.h"

#include "conjure/engine/engine_error.h"

#include "conjure/core/string_utils.h"

#include <cmath>
#include <format>
#include <limits>

using namespace conjure;
using namespace conjure::engine;

namespace {

    constexpr std::string_view HELP_TEXT =
      "Conjure engine options:\n"
      "  --project <dir>                directory containing project.json\n"
      "  --scene <asset path>           startup scene (relative to the assets root, or assets://...)\n"
      "  --headless                     null window, null device, null audio, no scripting, forced frame delta\n"
      "  --frames <n>                   quit after n frames (0 = run until quit)\n"
      "  --rhi d3d12|vulkan|null        graphics backend\n"
      "  --audio miniaudio|null|off     audio backend (off disables audio)\n"
      "  --no-physics                   disable the physics subsystem\n"
      "  --scripting                    enable the scripting subsystem (wins over project.json and --headless)\n"
      "  --no-scripting                 disable the scripting subsystem\n"
      "  --no-render                    disable rendering (no device, no renderer)\n"
      "  --width <n> --height <n>       window size in logical pixels\n"
      "  --fullscreen                   windowed fullscreen on the primary monitor\n"
      "  --no-vsync                     present without vertical sync\n"
      "  --fixed-hz <n>                 fixed-step rate (default 60)\n"
      "  --fixed-frame-delta <seconds>  forced frame delta (0 = wall clock)\n"
      "  --target-hz <n>                frame pacing target when no vsync present happens\n"
      "  --time-scale <f>               simulation time scale\n"
      "  --jobs <n|auto>                worker threads (0 = every job inline; default auto)\n"
      "  --gpu-validation off|debug|gpu graphics validation for this run (gpu: D3D12 GPU-based validation)\n"
      "  --log-level <level>            trace|debug|info|warn|error|fatal|off\n"
      "  --log-file <path>              also log to a file\n"
      "  --help                         print this text\n"
      "Unknown options are passed to the executable (--exec, --export).\n";

    /// <summary>
    /// The BAD_ARGUMENTS error for one flag.
    /// </summary>
    [[nodiscard]] core::error bad_argument(std::string_view flag, std::string_view reason) {
        return make_error(engine_error::BAD_ARGUMENTS, std::format("{}: {}", flag, reason));
    }

    /// <summary>
    /// Fetches the value token of a flag or fails when the arguments end or the next token is another flag
    /// ("--scene --no-physics" lacks its scene; it must not take "--no-physics" as one).
    /// </summary>
    [[nodiscard]] core::result<std::string_view> take_value(
      std::span<const std::string_view> arguments,
      usize &index,
      std::string_view flag
    ) {
        if (index + 1 >= arguments.size() || core::str::starts_with(arguments[index + 1], "--")) {
            return bad_argument(flag, "missing value");
        }
        ++index;
        return arguments[index];
    }

    [[nodiscard]] core::result<u64> parse_count(std::string_view flag, std::string_view text, bool allowZero) {
        const std::optional<u64> parsed = core::str::parse_uint(text);
        if (!parsed.has_value()) {
            return bad_argument(flag, std::format("'{}' is not a non-negative integer", text));
        }
        if (!allowZero && *parsed == 0) {
            return bad_argument(flag, "must be at least 1");
        }
        return *parsed;
    }

    [[nodiscard]] core::result<u32> parse_count32(std::string_view flag, std::string_view text, bool allowZero) {
        CONJURE_TRY_ASSIGN(const u64 value, parse_count(flag, text, allowZero));
        if (value > std::numeric_limits<u32>::max()) {
            return bad_argument(flag, std::format("'{}' is too large", text));
        }
        return static_cast<u32>(value);
    }

    /// <summary>
    /// The --gpu-validation spellings, case-insensitively: off, debug (DEBUG_LAYER) and gpu (GPU_BASED).
    /// </summary>
    [[nodiscard]] core::result<gpu_validation_level> parse_validation(std::string_view flag, std::string_view text) {
        if (core::str::iequals(text, "off")) {
            return gpu_validation_level::OFF;
        }
        if (core::str::iequals(text, "debug")) {
            return gpu_validation_level::DEBUG_LAYER;
        }
        if (core::str::iequals(text, "gpu")) {
            return gpu_validation_level::GPU_BASED;
        }
        return bad_argument(flag, std::format("unknown level '{}' (expected off, debug or gpu)", text));
    }

    [[nodiscard]] core::result<f32> parse_real(std::string_view flag, std::string_view text, bool allowZero) {
        const std::optional<f32> parsed = core::str::parse_float(text);
        if (!parsed.has_value() || !std::isfinite(*parsed) || *parsed < 0.0f || (!allowZero && *parsed == 0.0f)) {
            const std::string_view kind = allowZero ? "non-negative" : "positive";
            return bad_argument(flag, std::format("'{}' is not a {} number", text, kind));
        }
        return *parsed;
    }
}

core::result<command_line_options> conjure::engine::parse_command_line(std::span<const std::string_view> arguments) {
    command_line_options options = {};
    bool collectingUnknown = false;

    for (usize index = 0; index < arguments.size(); ++index) {
        const std::string_view token = arguments[index];
        const bool isFlag = core::str::starts_with(token, "--");

        if (collectingUnknown && !isFlag) {
            options.unrecognisedArguments.emplace_back(token);
            continue;
        }
        collectingUnknown = false;

        if (token == "--project") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            options.projectPath = std::filesystem::path(value);
        } else if (token == "--scene") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            options.scene = std::filesystem::path(value);
        } else if (token == "--headless") {
            options.headless = true;
        } else if (token == "--frames") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            CONJURE_TRY_ASSIGN(const u64 frames, parse_count(token, value, true));
            options.frames = frames;
        } else if (token == "--rhi") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            CONJURE_TRY_ASSIGN(const rhi_backend_choice backend, parse_rhi_backend_choice(value));
            options.rhi = backend;
        } else if (token == "--audio") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            // The last --audio wins: "off" and a backend clear each other.
            if (core::str::iequals(value, "off")) {
                options.audioOff = true;
                options.audio = std::nullopt;
            } else {
                CONJURE_TRY_ASSIGN(const audio_backend_choice backend, parse_audio_backend_choice(value));
                options.audio = backend;
                options.audioOff = false;
            }
        } else if (token == "--no-physics") {
            options.noPhysics = true;
        } else if (token == "--scripting") {
            options.scripting = true;
        } else if (token == "--no-scripting") {
            options.noScripting = true;
        } else if (token == "--no-render") {
            options.noRender = true;
        } else if (token == "--width") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            CONJURE_TRY_ASSIGN(const u32 width, parse_count32(token, value, false));
            options.width = width;
        } else if (token == "--height") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            CONJURE_TRY_ASSIGN(const u32 height, parse_count32(token, value, false));
            options.height = height;
        } else if (token == "--fullscreen") {
            options.fullscreen = true;
        } else if (token == "--no-vsync") {
            options.noVsync = true;
        } else if (token == "--fixed-hz") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            CONJURE_TRY_ASSIGN(const u32 fixedHz, parse_count32(token, value, false));
            options.fixedHz = fixedHz;
        } else if (token == "--fixed-frame-delta") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            CONJURE_TRY_ASSIGN(const f32 delta, parse_real(token, value, true));
            options.fixedFrameDelta = delta;
        } else if (token == "--target-hz") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            CONJURE_TRY_ASSIGN(const u32 targetHz, parse_count32(token, value, true));
            options.targetHz = targetHz;
        } else if (token == "--time-scale") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            CONJURE_TRY_ASSIGN(const f32 scale, parse_real(token, value, true));
            options.timeScale = scale;
        } else if (token == "--jobs") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            if (core::str::iequals(value, "auto")) {
                options.jobs = AUTO_WORKER_COUNT;
            } else {
                CONJURE_TRY_ASSIGN(const u32 jobs, parse_count32(token, value, true));
                options.jobs = jobs;
            }
        } else if (token == "--gpu-validation") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            CONJURE_TRY_ASSIGN(const gpu_validation_level level, parse_validation(token, value));
            options.gpuValidation = level;
        } else if (token == "--log-level") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            const std::optional<core::log_level> level = core::parse_log_level(value);
            if (!level.has_value()) {
                return bad_argument(token, std::format("unknown log level '{}'", value));
            }
            options.logLevel = *level;
        } else if (token == "--log-file") {
            CONJURE_TRY_ASSIGN(const std::string_view value, take_value(arguments, index, token));
            options.logFile = std::filesystem::path(value);
        } else if (token == "--help" || token == "-h") {
            options.help = true;
        } else {
            options.unrecognisedArguments.emplace_back(token);
            collectingUnknown = isFlag;
        }
    }
    if (options.scripting && options.noScripting) {
        return bad_argument("--scripting", "cannot be combined with --no-scripting");
    }
    return options;
}

void conjure::engine::apply_command_line(const command_line_options &options, engine_config &config) {
    if (options.scene.has_value()) {
        config.startupSceneOverride = *options.scene;
    }
    if (options.frames.has_value()) {
        config.maxFrames = *options.frames;
    }
    if (options.rhi.has_value()) {
        config.rendering.backend = *options.rhi;
    }
    if (options.audio.has_value()) {
        config.audio.backend = *options.audio;
        config.audio.enabled = true;
    }
    if (options.audioOff) {
        config.audio.enabled = false;
    }
    if (options.noPhysics) {
        config.physics.enabled = false;
    }
    if (options.scripting) {
        config.scripting.enabled = true;
    }
    if (options.noScripting) {
        config.scripting.enabled = false;
    }
    if (options.noRender) {
        config.rendering.enabled = false;
    }
    if (options.width.has_value()) {
        config.window.width = *options.width;
    }
    if (options.height.has_value()) {
        config.window.height = *options.height;
    }
    if (options.fullscreen) {
        config.window.fullscreen = true;
    }
    if (options.noVsync) {
        config.window.vsync = false;
    }
    if (options.fixedHz.has_value()) {
        config.time.fixedDeltaSeconds = 1.0f / static_cast<f32>(*options.fixedHz);
    }
    if (options.fixedFrameDelta.has_value()) {
        config.time.forcedFrameDeltaSeconds = *options.fixedFrameDelta;
    }
    if (options.targetHz.has_value()) {
        config.pacing.targetHz = *options.targetHz;
    }
    if (options.timeScale.has_value()) {
        config.time.timeScale = *options.timeScale;
    }
    if (options.jobs.has_value()) {
        config.jobs.workerCount = *options.jobs;
    }
    if (options.gpuValidation.has_value()) {
        config.rendering.gpuValidation = *options.gpuValidation;
    }
    if (options.logLevel.has_value()) {
        config.logLevel = *options.logLevel;
    }
    if (options.logFile.has_value()) {
        config.logFile = *options.logFile;
    }
}

std::string_view conjure::engine::command_line_help() noexcept {
    return HELP_TEXT;
}
