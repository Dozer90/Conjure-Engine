#pragma once

#include "conjure/engine/engine_error.h"

#include "conjure/core/api.h"
#include "conjure/core/log.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <string>
#include <vector>

// The engine configuration (docs/design/06_engine_loop.md 2.3, D8, D9): plain section structs with
// camelCase fields (R31) merged from defaults(), project.json and the command line by
// application_desc::from_command_line and validated once. headless_defaults() is what every test and
// server starts from: null platform / rhi / audio backends, rendering off, a forced frame delta.
//
// Nine sections are named *_settings. The two that feed a module's own settings struct are named *_config
// (06 D13, code sweep F626) so that engine::physics_config and engine::audio_config never share a name with
// physics::physics_settings and audio::audio_settings, which have other fields.

namespace conjure::engine {

    /// <summary>
    /// Which graphics backend the rhi device uses. AUTO tries the platform default, then the other
    /// compiled backend once.
    /// </summary>
    enum class rhi_backend_choice : u8 {
        AUTO,
        D3D12,
        VULKAN,
        NULL_DEVICE
    };

    /// <summary>
    /// How much the graphics device validates (rendering.gpuValidation; 06_engine_loop.md 2.3, code sweep F458).
    /// The levels are ordered: a later one validates more. OFF creates the device without validation.
    /// DEBUG_LAYER turns on the D3D12 debug layer or the Vulkan validation layers, plus NVRHI's own validation
    /// layer (rhi::device_desc::enableDebugLayer). GPU_BASED adds D3D12 GPU-based validation
    /// (rhi::device_desc::enableGpuValidation); the Vulkan and null backends ignore it, so there it validates
    /// as DEBUG_LAYER does. engine_config::defaults() picks DEBUG_LAYER in a Debug build and OFF in the others.
    /// </summary>
    enum class gpu_validation_level : u8 {
        OFF,
        DEBUG_LAYER,
        GPU_BASED
    };

    /// <summary>
    /// Which audio backend the audio engine uses. AUTO picks miniaudio and falls back to the null backend.
    /// </summary>
    enum class audio_backend_choice : u8 {
        AUTO,
        MINIAUDIO,
        NULL_DEVICE
    };

    /// <summary>
    /// How the exported managed runtime is published (R39).
    /// </summary>
    enum class script_export_mode : u8 {
        FRAMEWORK_DEPENDENT,
        SELF_CONTAINED
    };

    struct window_settings {
        std::string title = "Conjure";
        u32 width = 1600;
        u32 height = 900;
        bool fullscreen = false;
        bool resizable = true;
        bool vsync = true;

        [[nodiscard]] bool operator==(const window_settings &) const = default;
    };

    struct time_settings {
        f32 fixedDeltaSeconds = 1.0f / 60.0f;
        f32 maxFrameDeltaSeconds = 0.1f;
        u32 maxFixedStepsPerFrame = 8;
        f32 timeScale = 1.0f;
        f32 forcedFrameDeltaSeconds = 0.0f;     // > 0 replaces the wall clock (headless determinism)
        bool interpolatePhysics = true;

        [[nodiscard]] bool operator==(const time_settings &) const = default;
    };

    struct pacing_settings {
        u32 targetHz = 0;                       // 0 = unpaced; used only when no vsync present happened
        bool sleepWhenUnfocused = true;

        [[nodiscard]] bool operator==(const pacing_settings &) const = default;
    };

    struct rendering_settings {
        bool enabled = true;
        rhi_backend_choice backend = rhi_backend_choice::AUTO;
        gpu_validation_level gpuValidation = gpu_validation_level::OFF;    // DEBUG_LAYER in Debug by defaults()
        std::filesystem::path shaderDirectory = {};     // index.json location; empty = <executable>/shaders

        [[nodiscard]] bool operator==(const rendering_settings &) const = default;
    };

    /// <summary>
    /// The "physics" section of the engine configuration: an engine-side summary only (R53).
    /// physics_subsystem builds the full physics::physics_settings with physics::physics_settings::from_json
    /// over the raw project.json "physics" section and takes gravity and maxBodies from here only when
    /// that section does not name them.
    /// </summary>
    struct physics_config {
        bool enabled = true;
        math::vec3 gravity = math::vec3(0.0f, -9.81f, 0.0f);
        u32 maxBodies = 65536;

        [[nodiscard]] bool operator==(const physics_config &) const = default;
    };

    struct audio_group_setting {
        std::string name = {};
        std::string parent = {};
        f32 gain = 1.0f;

        [[nodiscard]] bool operator==(const audio_group_setting &) const = default;
    };

    /// <summary>
    /// The "audio" section of the engine configuration (R52). audio_subsystem maps it field by field onto
    /// audio::audio_settings: backend through audio_backend_choice (NULL_DEVICE and every headless run pick
    /// the null backend), groups by name; `enabled` is the engine's own switch and has no counterpart there.
    /// deviceName is the project.json key "device".
    /// </summary>
    struct audio_config {
        bool enabled = true;
        audio_backend_choice backend = audio_backend_choice::AUTO;
        u32 sampleRate = 48000;
        std::string deviceName = {};            // empty = default device
        f32 masterGain = 1.0f;
        f32 dopplerFactor = 0.0f;
        f32 speedOfSound = 343.0f;              // audio::DEFAULT_SPEED_OF_SOUND (this header builds without audio)
        std::vector<audio_group_setting> groups = {};

        [[nodiscard]] bool operator==(const audio_config &) const = default;
    };

    /// <summary>
    /// The asset pipeline of a development (non-cooked) project. hotReload drives the polling file watcher
    /// and the assets::hot_reload_controller whose stat passes run on job threads every 500 ms; it is
    /// ignored in Distribution (no database there) and off in headless_defaults() so a headless frame
    /// stays allocation-free (09 item 15a). A project's "assets" section sets it for interactive runs.
    /// </summary>
    struct assets_settings {
        bool hotReload = true;                  // ignored in Distribution and by cooked projects

        [[nodiscard]] bool operator==(const assets_settings &) const = default;
    };

    struct scripting_settings {
        bool enabled = true;
        bool required = true;                   // false: a missing runtime warns and disables scripting
        std::filesystem::path gameAssembly = {};        // relative to the project root
        std::filesystem::path managedDirectory = {};    // Conjure.Managed / Conjure.Bootstrap; empty = <exe>/managed
        bool hotReload = true;                  // ignored in Distribution
        script_export_mode exportMode = script_export_mode::FRAMEWORK_DEPENDENT;
        bool exportDebugSymbols = false;
        bool useNullHost = false;               // tests and tools: the C++ fake of the managed side

        [[nodiscard]] bool operator==(const scripting_settings &) const = default;
    };

    struct modules_settings {
        std::vector<std::filesystem::path> searchPaths = {std::filesystem::path("modules")};
        bool enginePlugins = true;              // also discover <engine>/plugins
        std::vector<std::string> required = {}; // manifest names that must load (R20)

        [[nodiscard]] bool operator==(const modules_settings &) const = default;
    };

    /// <summary>
    /// The workerCount that asks resolve_worker_count() for hardware_concurrency - 1 clamped to
    /// [MIN_AUTO_WORKER_COUNT, MAX_AUTO_WORKER_COUNT].
    /// </summary>
    inline constexpr u32 AUTO_WORKER_COUNT = 0xFFFFFFFFu;

    struct jobs_settings {
        u32 workerCount = AUTO_WORKER_COUNT;    // 0 = every job runs inline on the calling thread (08_testing D4)

        [[nodiscard]] bool operator==(const jobs_settings &) const = default;
    };

    struct memory_settings {
        usize frameArenaBytes = 32 * MIB;
        usize frameThreadBlockBytes = 256 * KIB;

        [[nodiscard]] bool operator==(const memory_settings &) const = default;
    };

    /// <summary>
    /// Smallest and largest worker counts resolve_worker_count() yields for an automatic request.
    /// </summary>
    inline constexpr u32 MIN_AUTO_WORKER_COUNT = 1;
    inline constexpr u32 MAX_AUTO_WORKER_COUNT = 16;

    struct CONJURE_API engine_config {
        bool headless = false;
        bool editor = false;                    // set by conjure_editor; enables imgui_layer and editor hooks
        u64 maxFrames = 0;                      // 0 = run until quit
        std::filesystem::path projectPath = {}; // directory containing project.json; empty = no project
        std::filesystem::path startupSceneOverride = {};
        core::log_level logLevel = core::log_level::INFO;
        std::filesystem::path logFile = {};
        window_settings window = {};
        time_settings time = {};
        pacing_settings pacing = {};
        rendering_settings rendering = {};
        physics_config physics = {};
        audio_config audio = {};
        assets_settings assets = {};
        scripting_settings scripting = {};
        modules_settings modules = {};
        jobs_settings jobs = {};
        memory_settings memory = {};

        /// <summary>
        /// The desktop defaults of 06_engine_loop.md 2.3; rendering.gpuValidation is DEBUG_LAYER in a Debug build
        /// and OFF in the others.
        /// </summary>
        [[nodiscard]] static engine_config defaults();

        /// <summary>
        /// headless = true, a 1280 x 720 null window (R33), rendering disabled on the null device, the
        /// null audio backend, scripting disabled, asset hot reload off (no file watcher stat passes on the
        /// job threads), a forced 1/60 frame delta and two job workers. Tests opt individual subsystems
        /// back in; on the command line --scripting does that for scripting.
        /// </summary>
        [[nodiscard]] static engine_config headless_defaults();

        /// <summary>
        /// Rejects (INVALID_CONFIG, the message names the field): a non-finite or non-positive
        /// fixedDeltaSeconds, a non-finite maxFrameDeltaSeconds or one below fixedDeltaSeconds,
        /// maxFixedStepsPerFrame == 0, a forcedFrameDeltaSeconds that is negative, non-finite or above
        /// maxFrameDeltaSeconds, a negative or non-finite timeScale, a zero window size when not headless,
        /// rendering.backend == NULL_DEVICE when not headless, headless with rendering enabled on a real
        /// backend, frameThreadBlockBytes &gt; frameArenaBytes, a negative or non-finite audio.masterGain,
        /// and jobs.workerCount above core::jobs::MAX_WORKER_COUNT unless it is AUTO_WORKER_COUNT.
        /// </summary>
        [[nodiscard]] core::result<void> validate() const;

        [[nodiscard]] bool operator==(const engine_config &) const = default;
    };

    /// <summary>
    /// The worker count a jobs_settings resolves to: the request itself (0 = every job inline on the
    /// calling thread) unless it is AUTO_WORKER_COUNT, which becomes hardwareConcurrency - 1 clamped to
    /// [MIN_AUTO_WORKER_COUNT, MAX_AUTO_WORKER_COUNT].
    /// </summary>
    [[nodiscard]] CONJURE_API u32 resolve_worker_count(const jobs_settings &settings, u32 hardwareConcurrency) noexcept;

    /// <summary>
    /// resolve_worker_count with the machine's std::thread::hardware_concurrency().
    /// </summary>
    [[nodiscard]] CONJURE_API u32 resolve_worker_count(const jobs_settings &settings) noexcept;

    /// <summary>
    /// The lower-case names used by project.json and the command line ("auto", "d3d12", "vulkan", "null").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(rhi_backend_choice value) noexcept;
    [[nodiscard]] CONJURE_API std::string_view to_string(audio_backend_choice value) noexcept;

    /// <summary>
    /// The lower-case name project.json uses for a level: "off", "debug_layer" or "gpu_based". The command line
    /// spells the levels off, debug and gpu (--gpu-validation, command_line.h).
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(gpu_validation_level value) noexcept;

    /// <summary>
    /// "framework-dependent" or "self-contained" (R39).
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(script_export_mode value) noexcept;

    /// <summary>
    /// Parses the names above case-insensitively. Fails with BAD_ARGUMENTS for anything else.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<rhi_backend_choice> parse_rhi_backend_choice(std::string_view text);
    [[nodiscard]] CONJURE_API core::result<audio_backend_choice> parse_audio_backend_choice(std::string_view text);
    [[nodiscard]] CONJURE_API core::result<script_export_mode> parse_script_export_mode(std::string_view text);

    /// <summary>
    /// Parses the project.json names of a level ("off", "debug_layer", "gpu_based") case-insensitively. Fails
    /// with BAD_ARGUMENTS for anything else, the command-line spellings debug and gpu included.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<gpu_validation_level> parse_gpu_validation_level(std::string_view text);
}
