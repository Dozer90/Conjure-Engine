# Design Area F - Engine loop, threading and application

Module: `engine` (wave 4). Namespace `conjure::engine`. Target `conjure_engine`, tests `conjure_engine_tests`.
Status: **final**. Implementers follow this document; nothing architectural is left open.

This is the merge of two proposals (performance-first and robustness-first). The backbone is the robustness-first
single-orchestrator loop; the performance-first proposal contributed the worker-participating main thread, the
richer time/stats model, the frame pacer, clock injection and per-module system registration. Third-party facts
were verified against the GLFW, NVRHI, Dear ImGui and Jolt sources on 2026-11-06.

---

## 1. Overview and decisions

The engine is one `application` object that owns every module object, drives one frame per `step_frame()` call
and exposes a non-owning `engine_context` service table to layers, subsystems, plugins and bindings. The main
thread orchestrates; `core::jobs` does the bulk work; the ECS scheduler decides intra-phase ordering.

### D1. No dedicated render thread in v1; the renderer prepares in parallel jobs, main records and submits.

**Decision.** The main thread runs pump -> fixed steps -> update -> transform -> sync -> extract -> submit ->
present every frame. Parallelism comes from `core::jobs` *inside* each stage: chunk-parallel ECS systems, the
Jolt `JPH::JobSystem` adapter, async asset decode, and job-parallel culling, sorting and instance-buffer filling
inside `renderer::renderer::render_frame()`. Command recording itself is one NVRHI command list on the main
thread in v1; parallel recording is the renderer's listed v2 item (`02_renderer.md` R5 and 11; R8). No second
thread pipelines frame N+1 simulation against frame N rendering.

**Why.** GLFW event processing, window creation and callbacks are main-thread only, so a render thread adds a
second owner and a handoff protocol without removing the main thread from the frame. Dear ImGui forbids using one
context from two threads; a render thread requires staging `ImDrawData`. NVRHI gives no documented guarantee for
off-thread resource creation; recording on workers and executing on main stays inside what it documents, and
MoltenVK presentation is most reliable from the main thread. The 10,000-entity target is simulation-bound and
simulation is already job-parallel; the 1-3 ms a render thread would overlap is recovered by job-parallel
renderer preparation with none of the asset-lifetime, device-lost and shutdown-mid-frame hazards, and with one frame less input
latency for editor gizmos. For parallel AI implementers, one owner per object and one thread per subsystem is the
largest available risk reduction.

**Seam kept open (binding on the renderer, area B).** `renderer::render_scene` is an immutable snapshot produced
by EXTRACT; after `end_extract()` the renderer never touches `ecs::world` (it never does: the renderer has no
`ecs` dependency, the engine owns the EXTRACT systems, R7). Submission is one call, `render_frame() ->
core::result<void>`, which renders the read slot of the renderer-owned `render_scene_pair` (`begin_extract()`
returns the write slot, `end_extract()` swaps; R8). A future render thread is a local change in
`frame_loop.cpp` plus ImGui draw-data staging, never a contract change.

### D2. The main thread is a participating worker, never an idle one.

`core::jobs::job_system::wait(counter)` executes queued jobs on the calling thread until the counter reaches zero.
The scheduler and engine use only this wait. Worker count is `hardware_concurrency - 1` clamped to `[1, 16]`.

### D3. Fixed-step simulation: pure accumulator, clamped substeps, render-side interpolation.

Default 60 Hz. Frame delta clamped to `maxFrameDeltaSeconds = 0.1`; steps per frame capped at
`maxFixedStepsPerFrame = 8`; leftover accumulator dropped and flagged (warning at most once per second).
Simulation state is never mutated for rendering: interpolation produces a separate `scene::render_world_transform`
in the TRANSFORM phase, in the *same* hierarchy pass that computes `scene::world_transform`, so children of
interpolated bodies render correctly at no extra traversal cost.

### D4. The phase list is fixed and lives in `ecs`; modules register their own systems into it.

The ordered phase list (2.4) is `ecs::system_phase` in `conjure/ecs/system_phase.h` (area A owns the header, this
document owns the list; R1). Each module that depends on `ecs` (scene, physics, scripting) exposes
`register_systems(ecs::scheduler &, ...)` declaring phase, `system_flags`, read/write sets and ordering labels for
its own systems, because the module author knows its component access best; for the `ecs`-free modules
(renderer, audio) the engine's subsystem adapter registers their component types and owns their systems
(`engine.render_sync`, `engine.render_extract`, `engine.audio_sync`, `engine.audio_finish_one_shots`; R7). The
engine calls each registration in a fixed order from `application.cpp` and calls `scheduler::finalise()` once
(and again after a module load). Structural changes are flushed at every phase boundary.

### D5. The ECS world is mutated only on the main thread outside parallel systems.

Scripting systems are `EXCLUSIVE`. Scene loads, hot-reload application, editor and AI-protocol commands run on
the main thread. Anything arriving from another thread (file watcher, TCP command server, async asset completion)
goes through `application::post_to_main_thread()` and is drained before FRAME_BEGIN.

### D6. Frame memory is one per-thread-block linear arena, reset at FRAME_BEGIN with no global job wait.

Every job launched by the scheduler during frame N is complete when FRAME_END's phase wait returns, so resetting
at the next FRAME_BEGIN is safe *by construction*. The engine never calls `job_system::wait_idle()` per frame: that
would stall on long-running asset decode jobs. Rule: jobs not launched by the scheduler never use frame memory
(asserted in Debug through an allocation epoch check).

### D7. Explicit static startup order with reverse unwind; shutdown idempotent.

The order is written once in `application.cpp` (3.4). Every subsystem `startup` returns `core::result<void>`; on
the first failure already-started subsystems are shut down in reverse and the error returned. No dependency solver.

### D8. Headless is configuration, not a code path.

`engine_config::headless_defaults()` selects `null` platform/rhi/audio, disables rendering and forces a
deterministic frame delta. Tests, CI, servers, `conjure_editor --headless --exec` and the editor's step button all
drive the same public `step_frame()`.

### D9. Configuration precedence: defaults < project.json < command line, validated once after merging.

`application_desc::from_command_line()` performs the whole merge so `application` receives a final, validated
`engine_config`. `project.json` has a versioned schema; unknown sections are preserved verbatim in `extensions`.

### D10. Pause is a simulation mode, not `timeScale == 0`.

`simulation_mode::{RUNNING, PAUSED, STEP_ONCE}` is owned by the application. `timeScale` is a gameplay feature;
`PAUSED` skips the fixed loop and every UPDATE/LATE_UPDATE system without `RUNS_WHILE_PAUSED`, while TRANSFORM,
SYNC and EXTRACT always run so the editor keeps rendering edits.

### D11. Naming of plain data fields.

Classes use `mCamelCase` members. Plain aggregate structs (`engine_time`, settings, `engine_context`) use
`camelCase` fields with no prefix, following "camelCase for variables, `m` prefix for members" in `CODE_STYLE.cpp`
and the brief. Stated here so review does not churn on it.

---

## 2. Data model

### 2.1 `engine_time` (`conjure/engine/engine_time.h`)

Written only by the main thread between phases; read from any thread during a phase.

```cpp
struct engine_time {
    double time = 0.0;                  // Scaled seconds since the first frame.
    double unscaledTime = 0.0;          // Real seconds since the first frame (after frame clamp).
    double fixedTime = 0.0;             // Scaled seconds advanced by fixed steps so far.
    float deltaTime = 0.0f;             // Scaled, clamped frame delta.
    float unscaledDeltaTime = 0.0f;     // Clamped frame delta before time scale.
    float fixedDeltaTime = 1.0f / 60.0f;
    float timeScale = 1.0f;
    float interpolationAlpha = 0.0f;    // accumulator / fixedDeltaTime in [0, 1); valid from UPDATE onwards.
    uint64_t frameIndex = 0;
    uint64_t fixedStepIndex = 0;        // Total fixed steps since startup.
    uint32_t fixedStepsThisFrame = 0;
    bool inFixedStep = false;           // True while FIXED_UPDATE, PHYSICS_STEP or FIXED_END runs.
    bool droppedFixedTime = false;      // Spiral-of-death guard fired this frame.
};
```

Frame 0 uses `unscaledDeltaTime = fixedDeltaTime` so the first frame is deterministic and runs exactly one fixed
step. `timeScale == 0` yields no fixed steps and `deltaTime == 0`; UPDATE still runs (UI reads `unscaledDeltaTime`).

### 2.2 `fixed_step_clock` (`conjure/engine/fixed_step_clock.h`)

Pure value type, no clock, fully unit-testable.

```cpp
struct fixed_step_result {
    uint32_t stepCount = 0;
    float interpolationAlpha = 0.0f;
    bool droppedTime = false;
};

class fixed_step_clock {
    private:
        double mAccumulatorSeconds = 0.0;
        float mFixedDeltaSeconds = 1.0f / 60.0f;
        uint32_t mMaxStepsPerFrame = 8;

    public:
        fixed_step_clock();
        fixed_step_clock(float fixedDeltaSeconds, uint32_t maxStepsPerFrame);
        ~fixed_step_clock();

    public:
        /// <summary>
        /// Adds a scaled frame delta and returns how many fixed steps to run this frame plus the alpha to use
        /// after them. When the step cap is hit the leftover is discarded so a slow frame cannot snowball.
        /// </summary>
        [[nodiscard]] fixed_step_result advance(float scaledDeltaSeconds);

        void reset() noexcept;
        void set_fixed_delta(float fixedDeltaSeconds);
        [[nodiscard]] float fixed_delta() const noexcept { return mFixedDeltaSeconds; }
};
```

`advance`: `acc += delta; steps = floor(acc / dt); if (steps > max) { steps = max; acc = 0; dropped = true; }
else { acc -= steps * dt; } alpha = acc / dt`.

### 2.3 Configuration (`conjure/engine/engine_config.h`)

Enums: `rhi_backend_choice : uint8_t { AUTO, D3D12, VULKAN, NULL_DEVICE }`,
`audio_backend_choice : uint8_t { AUTO, MINIAUDIO, NULL_DEVICE }`. Settings structs (all fields initialised):

| Struct | Fields (type, default) |
|---|---|
| `window_settings` | `std::string title = "Conjure"`, `uint32_t width = 1600`, `uint32_t height = 900`, `bool fullscreen = false`, `bool resizable = true`, `bool vsync = true` |
| `time_settings` | `float fixedDeltaSeconds = 1/60`, `float maxFrameDeltaSeconds = 0.1f`, `uint32_t maxFixedStepsPerFrame = 8`, `float timeScale = 1`, `float forcedFrameDeltaSeconds = 0` (> 0 replaces the wall clock), `bool interpolatePhysics = true` |
| `pacing_settings` | `uint32_t targetHz = 0` (0 = unpaced; used only when no vsync present happened), `bool sleepWhenUnfocused = true` |
| `rendering_settings` | `bool enabled = true`, `rhi_backend_choice backend = AUTO`, `bool gpuValidation = false` (forced on in Debug by `defaults()`) |
| `physics_settings` | `bool enabled = true`, `math::vec3 gravity = (0, -9.81, 0)`, `uint32_t maxBodies = 65536`. Engine-side summary only: `physics_subsystem` builds the full `physics::physics_settings` with `physics::physics_settings::from_json(project.physics, project.extensions["physics_layers"])`, so every field of `14_physics.md` 2.1 is authorable under `"physics"` (snake_case) without the engine naming it (R53) |
| `audio_settings` | `bool enabled = true`, `audio_backend_choice backend = AUTO`, `uint32_t sampleRate = 48000`, `std::string device = ""` (empty = default), `float masterGain = 1`, `float dopplerFactor = 0`, `float speedOfSound = 343`, `std::vector<audio_group_setting> groups` (`{ std::string name; std::string parent; float gain; }`); mapped one-to-one onto `audio::audio_settings` by `audio_subsystem` (`12_audio.md` 4.4, 8; `NULL_DEVICE` -> `audio_backend_kind::NULL_BACKEND`) (R52) |
| `assets_settings` | `bool hotReload = true` (polling file watcher + `hot_reload_controller`; ignored in Distribution and by cooked projects, off in `headless_defaults()` so a headless frame stays allocation-free, 09 item 15a) |
| `scripting_settings` | `bool enabled = true`, `bool required = true` (false: missing runtime warns and disables), `std::filesystem::path gameAssembly` (relative to project root), `bool hotReload = true` (ignored in Distribution), `script_export_mode exportMode = FRAMEWORK_DEPENDENT` (`FRAMEWORK_DEPENDENT`, `SELF_CONTAINED`), `bool exportDebugSymbols = false` (R39) |
| `modules_settings` | `std::vector<std::filesystem::path> searchPaths = {"modules"}`, `bool enginePlugins = true` (also discover `<engine>/plugins`), `std::vector<std::string> required = {}` (manifest names that must load) (R20) |
| `jobs_settings` | `uint32_t workerCount = AUTO_WORKER_COUNT` (`0xFFFFFFFF` = `hardware_concurrency - 1` clamped to `[1, 16]`; `0` = every job runs inline on the calling thread, `08_testing.md` D4) |
| `memory_settings` | `size_t frameArenaBytes = 32 MiB`, `size_t frameThreadBlockBytes = 256 KiB` |

```cpp
struct engine_config {
    bool headless = false;
    bool editor = false;                        // Set by conjure_editor; enables imgui_layer and editor hooks.
    uint64_t maxFrames = 0;                     // 0 = run until quit.
    std::filesystem::path projectPath = {};     // Directory containing project.json; empty = no project.
    std::filesystem::path startupSceneOverride = {};
    core::log_level logLevel = core::log_level::INFO;
    std::filesystem::path logFile = {};
    window_settings window = {};
    time_settings time = {};
    pacing_settings pacing = {};
    rendering_settings rendering = {};
    physics_settings physics = {};
    audio_settings audio = {};
    scripting_settings scripting = {};
    jobs_settings jobs = {};
    memory_settings memory = {};

    [[nodiscard]] static engine_config defaults();
    [[nodiscard]] static engine_config headless_defaults();
    [[nodiscard]] core::result<void> validate() const;
};
```

`headless_defaults()`: `headless = true`, `window.width = 1280`, `window.height = 720` (the `null` window size area
H assumes, R33), `rendering.enabled = false`, `rendering.backend = NULL_DEVICE`,
`audio.backend = NULL_DEVICE`, `scripting.enabled = false`, `time.forcedFrameDeltaSeconds = 1/60`,
`jobs.workerCount = 2`. Tests opt individual subsystems back in.

`validate()` rejects (`engine_error::INVALID_CONFIG`, message names the field): `fixedDeltaSeconds <= 0`;
`maxFrameDeltaSeconds < fixedDeltaSeconds`; `maxFixedStepsPerFrame == 0`; zero window size when not headless;
`rendering.backend == NULL_DEVICE` when not headless; `headless && rendering.enabled && backend != NULL_DEVICE`;
`frameThreadBlockBytes > frameArenaBytes`; `forcedFrameDeltaSeconds < 0`.

### 2.4 Phases (`conjure/ecs/system_phase.h`; helpers in `conjure/engine/engine_phase.h`)

```cpp
namespace conjure::ecs {
    enum class system_phase : uint8_t {
        FRAME_BEGIN,        // Hot reload apply, script OnCreate, pending scene loads.
        FIXED_UPDATE,       // [fixed loop] C# FixedUpdate, character controller input.
        PHYSICS_STEP,       // [fixed loop] physics.push_transforms then physics.step.
        FIXED_END,          // [fixed loop] physics.pull_transforms (prev/curr poses), contact events.
        UPDATE,             // C# Update, animation, gameplay, plugin systems.
        LATE_UPDATE,        // C# LateUpdate, camera follow.
        TRANSFORM,          // Hierarchy propagation: world_transform and render_world_transform.
        SYNC,               // Mirrors into audio (listener/sources) and renderer (camera/light params).
        EXTRACT,            // Culling + extraction into render_scene, debug draw flush.
        FRAME_END,          // Deferred destroys, OnDestroy, asset unloads, stats commit.
        COUNT
    };
}

namespace conjure::engine {
    [[nodiscard]] std::string_view to_string(ecs::system_phase phase) noexcept;
    [[nodiscard]] constexpr bool is_fixed_phase(ecs::system_phase phase) noexcept;
}
```

Every phase ends with a scheduler command-buffer flush; structural changes become visible at phase boundaries and
never mid-phase. The three fixed phases run `stepCount` times as a group; all others run exactly once.

### 2.5 `engine_context` (`conjure/engine/engine_context.h`)

Non-owning service table, forward declarations only. A null pointer means "compiled out or disabled by
configuration"; every consumer must early-out on it. Pointers are stable from `initialise()` to `shutdown()`.

```cpp
struct engine_context {
    application *app = nullptr;
    const engine_config *config = nullptr;
    const project *project = nullptr;           // nullptr when no project is loaded.
    const engine_time *time = nullptr;
    frame_memory *frameMemory = nullptr;
    core::jobs::job_system *jobs = nullptr;
    core::vfs::file_system *filesystem = nullptr;
    platform::window *window = nullptr;         // Null backend when headless, never nullptr after init.
    platform::input *input = nullptr;
    rhi::device *device = nullptr;              // nullptr when rendering.enabled == false.
    assets::asset_manager *assets = nullptr;    // Always present: cache source (dev) or pack source (cooked) (R22).
    assets::asset_database *assetDatabase = nullptr;   // nullptr in cooked (exported) projects.
    ecs::component_registry *components = nullptr;
    ecs::world *world = nullptr;
    ecs::scheduler *scheduler = nullptr;
    scene::scene_manager *scenes = nullptr;
    renderer::renderer *renderer = nullptr;     // nullptr when rendering disabled or compiled out.
    physics::physics_world *physics = nullptr;
    audio::audio_engine *audio = nullptr;
    scripting::script_runtime *scripting = nullptr;    // nullptr when disabled or the runtime is unavailable (R27).
    modules::module_manager *modules = nullptr;        // registry() for reads; request_* for the editor (R19).
    diagnostics::check_registry *checks = nullptr;     // Self-verifying samples (area H 2.5).
    const frame_stats *stats = nullptr;                // Read by diagnostics_frame_stats (15_bindings.md) and the editor profiler (R51).
};
```

### 2.6 `frame_memory` (`conjure/engine/frame_memory.h`)

`class frame_memory` wraps one `core::memory::frame_allocator mAllocator` and a `uint64_t mEpoch`. Constructed
from `const memory_settings &`. API: `[[nodiscard]] void *allocate(size_t bytes, size_t alignment)` (thread-safe
bump from the calling thread's block; arena overflow falls back to the heap, counts and logs once per second with
the high-water mark); `template <typename ElementType> std::span<ElementType> allocate_span(size_t count)`;
`template <typename ElementType, typename... Args> ElementType *construct(Args &&...args)` (trivially destructible
types only); `void reset()` (main thread only, at FRAME_BEGIN; asserts no scheduler job is in flight);
`uint64_t epoch() const noexcept`; `frame_memory_stats stats() const noexcept` where `frame_memory_stats` is
`{ size_t highWaterBytes; size_t capacityBytes; uint32_t overflowCount; }`.

Requirements on `core::memory::frame_allocator`: one arena of `frameArenaBytes`; each thread grabs a
`frameThreadBlockBytes` block via one atomic bump and allocates inside its thread-local block lock-free (slot
indexed by `core::jobs::thread_index()`); `reset()` returns all blocks; `overflow_count()`. Anything
frame-allocated is dead at the next FRAME_BEGIN. `render_scene` buffers are persistent renderer storage.

### 2.7 `frame_stats` (`conjure/engine/frame_stats.h`)

```cpp
struct frame_sample {
    float frameMilliseconds = 0.0f;
    std::array<float, static_cast<size_t>(ecs::system_phase::COUNT)> phaseMilliseconds = {};
    float submitMilliseconds = 0.0f;
    float pacerSleepMilliseconds = 0.0f;
    uint32_t fixedSteps = 0;
    uint32_t jobsExecuted = 0;
    size_t frameMemoryHighWater = 0;
    bool droppedFixedTime = false;
};

```

`class frame_stats` keeps a ring of `HISTORY_SIZE = 240` samples (`mSamples`, `mWriteIndex`,
`mTotalDroppedFixedSteps`) with `void commit(const frame_sample &sample)`, `const frame_sample &latest() const
noexcept`, `std::span<const frame_sample> history() const noexcept` and
`float average_frame_milliseconds(size_t frameCount) const noexcept`. The editor profiler panel reads it.

### 2.8 Layers (`conjure/engine/layer.h`, `conjure/engine/layer_stack.h`)

Layers are hooks for executables (editor workspace, ImGui bracket, debug overlay, game bootstrap). Game logic lives
in ECS systems and scripts. Events travel from the top (overlays) downwards and stop at the first layer returning
`true`; updates travel bottom-up.

```cpp
class layer {
    private:
        std::string mName = "";

    public:
        explicit layer(std::string name);
        virtual ~layer();

    public:
        [[nodiscard]] std::string_view name() const noexcept { return mName; }
        [[nodiscard]] virtual core::result<void> on_attach(engine_context &context);
        virtual void on_detach(engine_context &context);
        virtual void on_frame_begin(engine_context &context);   // After input snapshot, before FRAME_BEGIN.
        virtual void on_update(engine_context &context);        // After UPDATE, before LATE_UPDATE.
        virtual void on_imgui(engine_context &context);         // After SYNC, before EXTRACT; ImGui frame open.
        virtual void on_frame_end(engine_context &context);     // After FRAME_END.
        [[nodiscard]] virtual bool on_event(engine_context &context, const platform::event &event);
};
```

`class layer_stack` owns `std::vector<std::unique_ptr<layer>> mLayers` split at `mOverlayBegin`, plus deferred
`mPendingPush` / `mPendingPop` applied while `mIterating` is false. API: `layer &push_layer(std::unique_ptr<layer>)`
(below the overlays), `layer &push_overlay(std::unique_ptr<layer>)`, `void pop(layer &)`,
`bool dispatch_event(engine_context &, const platform::event &)`, `std::span<const std::unique_ptr<layer>>
layers() const noexcept`, `void apply_pending(engine_context &)` (attaches pushes, detaches pops; called by the
application at step 13). Pushes and pops during iteration are deferred to that call.

Shipping layers: `game_layer` (engine; quits on window close), `imgui_layer` (engine, `CONJURE_WITH_RENDERER` and
not Distribution; brackets `ImGui::NewFrame`/`ImGui::Render`, feeds events, hands draw data to the renderer),
`editor_layer` (editor executable), `debug_overlay_layer` (engine, not Distribution; draws `frame_stats`).

### 2.9 Subsystems (`conjure/engine/engine_subsystem.h`)

Adapters between the engine and each module share one interface so unwind is uniform. Public so plugins (area G)
and executables can add their own.

```cpp
class engine_subsystem {
    public:
        engine_subsystem();
        virtual ~engine_subsystem();

    public:
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;
        [[nodiscard]] virtual core::result<void> startup(engine_context &context) = 0;
        [[nodiscard]] virtual core::result<void> post_startup(engine_context &context);  // After all startups.
        virtual void register_systems(ecs::scheduler &scheduler, engine_context &context);
        virtual void begin_frame(engine_context &context);      // Main thread, before FRAME_BEGIN.
        virtual void end_frame(engine_context &context);        // Main thread, after FRAME_END.
        virtual void shutdown(engine_context &context) = 0;     // Must be safe after a failed startup.
};
```

### 2.10 Project (`conjure/engine/project.h`)

Modules are discovered by `module.json` manifests (`07_modules.md` 3.1): under each of `modules.searchPaths`,
under `<engine>/plugins` when `enginePlugins` is true, and at the project root (the single `game` module); the
`modules.required` names must load or startup fails (R20). `struct project_description` fields: `uint32_t
schemaVersion = 1`, `std::string name`, `std::string version = "0.1.0"`, `std::string engineVersion`,
`std::filesystem::path assetsRoot = "assets"`, `std::filesystem::path cookedRoot = "cooked"`,
`std::filesystem::path startupScene` (relative to `assetsRoot`), `modules_settings modules`, the
`window`/`time`/`physics`/`rendering`/`audio`/`scripting` settings structs, `core::json::document extensions` (opaque per-module sections preserved verbatim) and
`bool cooked = false` (true in an exported build: only `cookedRoot` is mounted).

`class project` holds `mRootDirectory` and `mDescription`. API: `static core::result<project> load(const
std::filesystem::path &directory)`, `core::result<void> save() const`, `description()` (const and mutable),
`root_directory()`, `assets_directory()`, `cooked_directory()` (absolute, resolved against the root) and
`void apply_to(engine_config &config) const` (copies the settings sections and `startupScene`).

`project.json` (schema version 1, snake_case keys):

```json
{
  "schema_version": 1, "name": "FeatureTest", "version": "0.1.0", "engine_version": "0.1.0",
  "assets_root": "assets", "cooked_root": "cooked", "startup_scene": "scenes/main.scene.json", "cooked": false,
  "modules": { "search_paths": ["modules"], "engine_plugins": true, "required": ["feature_test_module"] },
  "window": { "title": "Feature Test", "width": 1600, "height": 900, "fullscreen": false, "vsync": true },
  "time": { "fixed_hz": 60, "max_frame_delta_seconds": 0.1, "max_fixed_steps_per_frame": 8 },
  "physics": { "gravity": [0.0, -9.81, 0.0], "max_bodies": 65536 },
  "rendering": { "backend": "auto" },
  "audio": { "backend": "auto", "sample_rate": 48000, "device": "", "master_gain": 1.0, "doppler_factor": 0.0,
             "speed_of_sound": 343.0, "groups": [{ "name": "Music", "parent": "", "gain": 0.8 }] },
  "assets": { "hot_reload": true },
  "scripting": { "assembly": "bin/FeatureTest.Game.dll", "hot_reload": true,
                 "export_mode": "framework-dependent", "debug_symbols": false },
  "extensions": { "physics_layers": { }, "editor": { } }
}
```

Rules: unknown top-level keys log one `WARN` each and are preserved in `extensions` on save. `schema_version`
greater than supported -> `PROJECT_SCHEMA_TOO_NEW`; missing file -> `PROJECT_NOT_FOUND`; parse failure ->
`PROJECT_PARSE_FAILED` with yyjson line/column; missing `name` -> `PROJECT_INVALID`. Relative paths resolve
against the project root. `fixed_hz` is stored for readability and converted to `fixedDeltaSeconds`. The export
pipeline (3.6) writes the same file with `"cooked": true`.

### 2.11 Application descriptor and command line (`application_desc.h`, `command_line.h`)

```cpp
struct application_desc {
    engine_config config = {};
    std::optional<project> project = {};
    core::clock *clockOverride = nullptr;       // Tests inject core::manual_clock; nullptr = steady clock.
    std::vector<std::string> unrecognisedArguments = {};   // Returned to the executable (--exec, --export).
    bool showHelp = false;

    /// <summary>
    /// Performs the whole configuration merge: baseConfig, then project.json (if --project), then command-line
    /// overrides, then validate(). The returned desc is final; application never re-reads configuration.
    /// </summary>
    [[nodiscard]] static core::result<application_desc> from_command_line(
      std::span<const std::string_view> arguments,
      const engine_config &baseConfig
    );
};

[[nodiscard]] std::string_view command_line_help() noexcept;
```

Recognised flags: `--project <dir>`, `--scene <asset path>`, `--headless`, `--frames <n>`,
`--rhi d3d12|vulkan|null`, `--audio miniaudio|null|off`, `--no-physics`, `--no-scripting`, `--no-render`,
`--width <n>`, `--height <n>`, `--fullscreen`, `--no-vsync`, `--fixed-hz <n>`, `--fixed-frame-delta <seconds>`,
`--target-hz <n>`, `--time-scale <f>`, `--jobs <n>`, `--log-level <level>`, `--log-file <path>`, `--help`.
`--headless` applies `headless_defaults()` *before* the project and remaining flags. Bad numeric values return
`BAD_ARGUMENTS`.

### 2.12 Errors and exit codes (`engine_error.h`, `exit_code.h`)

```cpp
enum class engine_error : uint16_t {
    INVALID_CONFIG, BAD_ARGUMENTS, PROJECT_NOT_FOUND, PROJECT_PARSE_FAILED, PROJECT_INVALID,
    PROJECT_SCHEMA_TOO_NEW, JOBS_START_FAILED, WINDOW_CREATE_FAILED, DEVICE_CREATE_FAILED,
    ASSET_DATABASE_FAILED, RENDERER_INIT_FAILED, PHYSICS_INIT_FAILED, SCRIPT_RUNTIME_NOT_FOUND,
    SCRIPT_ASSEMBLY_LOAD_FAILED, MODULE_LOAD_FAILED, SCHEDULER_CONFLICT, LAYER_ATTACH_FAILED,
    STARTUP_SCENE_FAILED, ALREADY_INITIALISED, NOT_INITIALISED, DEVICE_LOST
};
[[nodiscard]] core::error make_error(engine_error code, std::string message);

enum class exit_code : int { OK = 0, INIT_FAILURE = 1, RUNTIME_FAILURE = 2, BAD_ARGUMENTS = 3, DEVICE_LOST = 4 };
```

---

## 3. Algorithms and flow

### 3.1 Frame sequence (`engine/src/engine/frame_loop.cpp`, invoked by `application::step_frame()`)

```
step_frame():
  1  time.begin_frame(clock | forced delta)       clamp to maxFrameDelta, apply timeScale, frameIndex++
  2  frameMemory.reset()                          safe: all scheduler jobs of frame N-1 completed at step 12
  3  window.pump_events()                         GLFW, main thread only
       for each event: if layers.dispatch_event() -> consumed, else input.apply(event)
       close requested             -> request_quit(exit_code::OK)
       framebuffer resized (w,h>0) -> swapchain.resize(w, h); renderer.on_swapchain_resized()
       focus lost/gained           -> pacer.set_unfocused(flag)
     input.end_frame()                            publishes the immutable input snapshot for this frame
  4  mainThreadQueue.drain()                      tasks posted from workers (core::jobs::post_to_main_thread)
  5  subsystems.begin_frame(); layers.on_frame_begin()
       assets_subsystem:    asset_manager.begin_frame(frameIndex); asset_manager.update();
                            hot_reload_controller.update()  (editor/dev builds; R23)
       modules_subsystem:   module_manager.apply_pending()  (load/unload/reload requests; R19)
       scripting_subsystem: writes conjure_frame_block (time + input snapshot)
       imgui_layer:         ImGui::NewFrame()
  6  run_phase(FRAME_BEGIN)
  7  if simulationMode != PAUSED:
       steps = fixedClock.advance(time.deltaTime); time.fixedStepsThisFrame = steps.stepCount
       time.inFixedStep = true
       repeat steps.stepCount times:
         run_phase(FIXED_UPDATE); run_phase(PHYSICS_STEP); run_phase(FIXED_END)
         time.fixedTime += fixedDeltaTime; time.fixedStepIndex++
       time.inFixedStep = false; time.interpolationAlpha = steps.interpolationAlpha
       time.droppedFixedTime = steps.droppedTime   (rate-limited WARN, stats counter)
       run_phase(UPDATE); layers.on_update(); run_phase(LATE_UPDATE)
     else:
       run_phase_filtered(UPDATE, RUNS_WHILE_PAUSED); layers.on_update()
       run_phase_filtered(LATE_UPDATE, RUNS_WHILE_PAUSED)
     if simulationMode == STEP_ONCE: simulationMode = PAUSED
  8  run_phase(TRANSFORM)                         always: scene.apply_hierarchy_changes (EXCLUSIVE repair of raw
                                                  parent edits, 13_scene.md 3.7, R46) then scene.propagate_transforms:
                                                  world_transform + render_world_transform
  9  run_phase(SYNC)                              always: audio/renderer component mirrors
 10  if renderer:
       scene = renderer.begin_extract()           clears and returns the write render_scene (R8)
       layers.on_imgui()                          editor panels, gizmos; imgui_layer ends with ImGui::Render()
                                                  and renderer.set_imgui_draw_data(ImGui::GetDrawData()) (R9)
       run_phase(EXTRACT)                         engine.render_extract: culling + instances + lights + sprites
                                                  + debug draw into `scene` (R7)
       renderer.end_extract()                     swaps the pair; snapshot immutable from here
 11  if renderer and not window.is_minimised():
       result = renderer.render_frame()           job-parallel cull/sort/fill, one command list, execute + present
       on error: handle_render_failure(result)    see section 6
       device.run_garbage_collection()            NVRHI requires at least once per frame
 12  run_phase(FRAME_END)                         deferred destroys, OnDestroy, final structural flush
 13  subsystems.end_frame() (assets_subsystem: asset_manager.end_frame()); layers.on_frame_end(); layers.apply_pending()
 14  pacer.end_frame(presentHappened)             sleeps only when there was no vsync present (3.3)
 15  stats.commit(sample)
 16  if maxFrames != 0 and frameIndex >= maxFrames: request_quit(exit_code::OK)
```

`run_phase(p)` = `scheduler.run_phase(p, frameInfo)` (the scheduler was constructed over the world and the job
system, `01_ecs.md` 4.7, R2); the scheduler flushes command buffers at the end of every stage and therefore of
the phase; the engine only asks for the phase and times it. The main thread executes jobs while waiting for
each phase's counter (D2). `frameInfo` is `ecs::frame_info` filled from `engine_time` before each phase
(`deltaTime`, `fixedDeltaTime`, `interpolationAlpha`, `time`, `frameIndex`, `inFixedStep`).

`run()` is `while (!mQuitRequested) { const auto result = step_frame(); if (!result) { log; mExitCode =
RUNTIME_FAILURE; break; } }` then `shutdown()`; returns `static_cast<int>(mExitCode)`.

### 3.2 Physics interpolation (scene-owned TRANSFORM pass)

- `scene` defines and registers `interpolated_transform { math::transform previous; math::transform current; }`
  (world-space pose pair, TRANSIENT) and `render_world_transform { math::mat4 matrix; }`. Physics is the only
  writer and the only module that adds or removes the component: the `ADDED` observer of a DYNAMIC/KINEMATIC
  `rigid_body` or a `character_controller` enqueues `add<interpolated_transform>` through the deferred buffer,
  the first pull writes `previous = current = pose`, and body destruction on a surviving entity removes it again
  (`14_physics.md` 3.3, R47).
- `physics.pull_transforms` (FIXED_END, PARALLEL), for every dynamic body entity: `previous = current;
  current = jolt pose; local_transform = parentWorldInverse * current`. Kinematic and teleported entities are
  pushed by `physics.push_transforms` (PHYSICS_STEP, ordered `before("physics.step")`).
- `scene.propagate_transforms` (TRANSFORM, PARALLEL by hierarchy depth level, `RUNS_WHILE_PAUSED`) computes in one
  pass `world_transform = parentWorld * local` and `render_world_transform = parentRender * R`, where `R` is
  `lerp/slerp(previous, current, alpha)` expressed in parent-render space when the entity carries
  `interpolated_transform` and `time.interpolatePhysics`, otherwise `local`. Roots without a pose get
  `render == world`. This system is the single owner of transform propagation (R3): dirty tracking uses the ECS
  `changed<local_transform>` chunk filter plus scene's own `transform_dirty` tag and applies to
  `world_transform`; `render_world_transform` is recomputed for dirty entities and for every entity with an
  `interpolated_transform` in its ancestry (one "interpolated subtree" bit). Parallelism is per hierarchy depth
  level (`parallel_for_chunks` over the entities of one level, then the next), so wide trees under a single root
  still fan out.
- Renderer extraction reads `render_world_transform` only. Editor gizmos read `world_transform` (simulation truth).
  Result: one fixed step of visual latency, correct children of bodies, zero mutation of simulation state.

### 3.3 Frame pacing (`engine/src/engine/frame_pacer.{h,cpp}`)

Active for a frame iff `forcedFrameDeltaSeconds == 0`, no vsync present happened this frame (headless, rendering
disabled, minimised, or `vsync == false`) and the effective target is non-zero. Effective target:
`pacing.targetHz` when non-zero, else `1 / fixedDeltaSeconds` when headless (servers tick at simulation rate by
default), else unpaced. `sleepWhenUnfocused` on an unfocused window forces 30 Hz. Implementation: hybrid sleep
(`core::clock::sleep_until(deadline - 2 ms)` then spin), accurate to 0.1 ms, never sleeps when behind. With vsync
the present inside `render_frame` is the back-pressure.

### 3.4 Startup order (`application::initialise()`)

| # | Step | Failure error |
|---|---|---|
| 1 | `core::log` sinks (console + `logFile`), `core::platform_detect` (idempotent) | never fails |
| 2 | `config.validate()`; assert state `CREATED` | `INVALID_CONFIG` / `ALREADY_INITIALISED` |
| 3 | `core::jobs::job_system::start(job_system_desc{.workerCount = workerCount})` | `JOBS_START_FAILED` |
| 4 | `frame_memory` | `OUT_OF_MEMORY` (core) |
| 5 | `core::vfs` mounts: `project://`, `assets://`, `engine://` (engine built-in content), `cooked://`; cooked projects mount `engine://` and `cooked://` only (R24; module content mounts as `modules://<name>/` at step 15) | `PROJECT_NOT_FOUND` |
| 6 | `platform_subsystem`: window + input (`glfw` or `null`) | `WINDOW_CREATE_FAILED` |
| 7 | `rhi_subsystem`: `rhi::device::create(device_desc)` + `rhi::swapchain::create`; `AUTO` tries the platform default then the other compiled backend once; skipped when `rendering.enabled == false` | `DEVICE_CREATE_FAILED` |
| 8 | `assets_subsystem`: `asset_manager` (`cache_asset_source` or, when `cooked`, `pack_asset_source`), `asset_database` + importer registry + polling file watcher (non-cooked only, and only when `assets.hotReload`); `scan()` waits for `post_startup` so every importer is registered first (R33) | `ASSET_DATABASE_FAILED` |
| 9 | `ecs_subsystem`: `component_registry`, `world`, `scheduler(world, jobs)` | never fails |
| 10 | `scene_subsystem`: `scene_manager(world, assets)`; scene components registered (`scene::register_components`) | never fails |
| 11 | `audio_subsystem`: `audio_engine` (`miniaudio` or `null`); device failure falls back to `null` with `WARN`; registers audio component types into the ECS and audio asset types (R7) | never fails |
| 12 | `physics_subsystem`: `physics::physics_settings::from_json(project.physics, extensions.physics_layers)` + `validate()` (R53), `physics_world(settings, jobs)` (Jolt global init is reference counted inside, `14_physics.md` P4), `attach(world)`; `physics::register_components`; `physics::register_asset_types` (`conjure.physics.collision_mesh`, R44); forwards asset `RELOADED` to `on_asset_reloaded` | `PHYSICS_INIT_FAILED` |
| 13 | `renderer_subsystem`: `renderer::create(renderer_desc)`; `renderer::register_asset_types`; registers renderer component types into the ECS; subscribes to asset events for `upload_*` / `release_*` (R7/R10) | `RENDERER_INIT_FAILED` |
| 14 | `scripting_subsystem`: constructs `script_runtime` and registers `script_component`; no `initialize` yet because the bindings fill the native table at step 16 (R27) | never fails |
| 15 | `modules_subsystem`: `module_manager` + consumers (`ecs`, `assets`, `scripting`, and `editor` when the executable registered one), services (`engine_context`, `application`, `ecs::world`, `assets::asset_database`, `scene::scene_manager`), manifest-only static modules for the built-in libraries, `discover` + `add_manifest` + `load_all` (R18/R19); a `modules.required` failure is fatal, others warn; entries reach `application::add_subsystem` through the `application` service during this step only | `MODULE_LOAD_FAILED` |
| 16 | user-added subsystems (e.g. `bindings::scripting_bindings_subsystem` added by the executables, which fills the non-core `conjure_native_api` entries) in push order | their error |
| 17 | every subsystem `post_startup` in order: `assets_subsystem` runs `scan()` + `import_stale()` (R33); `scripting_subsystem` calls `script_runtime::initialize` (handshake; missing runtime is an error when `scripting.required`, else `WARN` + null context pointer) then `load_game_assembly` | `SCRIPT_RUNTIME_NOT_FOUND`, `SCRIPT_ASSEMBLY_LOAD_FAILED` |
| 18 | every subsystem `register_systems(scheduler)` in order; `scheduler.finalise()` validates access sets and labels (R2) | `SCHEDULER_CONFLICT` |
| 19 | layers `on_attach` in stack order | `LAYER_ATTACH_FAILED` |
| 20 | startup scene (`startupSceneOverride` or project `startupScene`) via synchronous `scene_manager::load`; fatal in runtime builds, `WARN` when `config.editor` | `STARTUP_SCENE_FAILED` |

State becomes `RUNNING`. Steps 11-14 compile only under their `CONJURE_WITH_*` option; the context pointer stays
null otherwise.

### 3.5 Shutdown order (`application::shutdown()`)

Idempotent; called by the destructor if the owner forgot. Each step is guarded by "was started".

1. `jobs.wait_idle()` (shutdown is the one place a global wait is correct), then `device.wait_for_idle()`.
2. `scene_manager.unload_all()`: fires `on_before_unload` (the scripting subsystem runs `destroy_all(true)` so
   `OnDestroy` sees live entities, R29), destroys every entity and flushes commands so component destructors
   release physics bodies, audio sources and GPU handles while their owners are alive, then `asset_manager.collect()`.
3. Layers `on_detach` in reverse; pending pushes discarded.
4. Subsystems `shutdown` in reverse of startup (16 .. 6): user subsystems, modules unload, scripting (unload ALC),
   renderer, physics, audio, scene, ecs, assets (watcher stopped first), rhi (final garbage collection), platform.
5. vfs unmount, `frame_memory` destroyed, `jobs.stop()`, log flush. State `STOPPED`.

### 3.6 Export pipeline (`conjure/engine/export.h`, `engine/src/engine/export_pipeline.cpp`; R21)

One owner for `conjure_editor --export <dir>` and the editor's `project.export` command:
`export_pipeline::run(const export_settings &settings, progress_fn progress) -> core::result<export_report>`,
synchronous, main thread, outside a frame (the editor runs it from `editor_application::run` before frames, or
pumps `progress` into protocol events when invoked as a command). Steps, each all-or-nothing and logged:

1. If the project has a native game module (`<project>/module.json` with a `binary`), build it in Distribution
   through `core::process` (CMake preset named in `project.json` `extensions.build`), else skip.
2. `assets::asset_cooker::cook` with roots = startup scene + `always_include` -> `<dir>/cooked/content.cpak`
   + `manifest.json` (`05_assets.md` 3.8).
3. When scripting is enabled: `dotnet publish managed/Conjure.Bootstrap -c Release -r <rid>
   --self-contained <export_mode == self-contained>` and `dotnet build <project>/Scripts -c Release` into
   `<dir>/managed` (`03_scripting.md` 3.11).
4. `modules::module_manager::export_layout(<dir>, build_config::DISTRIBUTION)` (`07_modules.md` 3.7).
5. Copy the Distribution `conjure_runtime` executable, the `conjure_shared` library and `shaders/` (index.json
   + blobs) next to it; on MSVC also `nethost.dll`, which `conjure_shared` imports (the output of
   `conjure_scripting_nethost`, `engine/src/scripting/CMakeLists.txt`).
6. Write `<dir>/project.json` with `"cooked": true` and `cooked_root = "cooked"`.

The exported folder runs standalone (`conjure_runtime --project <dir>`); `tests/integration/test_export_roundtrip.cpp`
exports `tests/assets/projects/minimal` and boots the result headless for 60 frames through `application`.

---

## 4. Public API

### 4.1 Header list (`engine/include/conjure/engine/`)

| Header | Main types |
|---|---|
| `application.h` | `application`, `application_state`, `simulation_mode` |
| `application_desc.h` | `application_desc`, `command_line_help()` |
| `command_line.h` | `parse_command_line()` (used by `application_desc::from_command_line`) |
| `engine_config.h` | `engine_config`, all `*_settings` structs, backend choice enums |
| `engine_context.h` | `engine_context` |
| `engine_phase.h` | `to_string(ecs::system_phase)`, `is_fixed_phase`, `FIXED_PHASES` span |
| `engine_time.h` | `engine_time` |
| `fixed_step_clock.h` | `fixed_step_clock`, `fixed_step_result` |
| `frame_memory.h` | `frame_memory`, `frame_memory_stats` |
| `frame_stats.h` | `frame_stats`, `frame_sample` |
| `engine_subsystem.h` | `engine_subsystem` |
| `layer.h` / `layer_stack.h` | `layer` / `layer_stack` |
| `game_layer.h` | `game_layer` |
| `imgui_layer.h` | `imgui_layer` (`CONJURE_WITH_RENDERER` and not Distribution) |
| `debug_overlay_layer.h` | `debug_overlay_layer` (not Distribution) |
| `project.h` | `project`, `project_description`, `project_module_entry` |
| `engine_error.h` / `exit_code.h` | `engine_error`, `make_error` / `exit_code` |
| `engine_version.h` | `ENGINE_VERSION_MAJOR/MINOR/PATCH`, `engine_version_string()` |
| `export.h` | `export_settings`, `export_report`, `export_pipeline` (3.6; R21) |
| `diagnostics/check_registry.h` | `check_record`, `check_registry` (thread-safe `report`, `records`, `all_passed`, `failed_count`; the `Conjure.Diagnostics.Check` binding writes here; area H 2.5) |
| `log_category.h` | `CONJURE_DECLARE_LOG_CATEGORY(ENGINE)` |

Private (`engine/src/engine/`): `application.cpp`, `frame_loop.{h,cpp}`, `frame_pacer.{h,cpp}`,
`main_thread_queue.{h,cpp}`, `engine_services.{h,cpp}` (owns every module object), `fixed_step_clock.cpp`,
`engine_time.cpp`, `engine_config.cpp`, `command_line.cpp`, `project.cpp`, `layer.cpp`, `layer_stack.cpp`,
`game_layer.cpp`, `imgui_layer.cpp`, `debug_overlay_layer.cpp`, `frame_memory.cpp`, `frame_stats.cpp`,
`subsystems/{platform,rhi,assets,ecs,scene,audio,physics,renderer,scripting,modules}_subsystem.{h,cpp}`,
`README.md`.

### 4.2 `application` (`conjure/engine/application.h`)

```cpp
namespace conjure::engine {

    enum class application_state : uint8_t { CREATED, STARTING, RUNNING, STOPPING, STOPPED };
    enum class simulation_mode : uint8_t { RUNNING, PAUSED, STEP_ONCE };

    class application {
        private:
            // mDesc, mContext, mTime, mFixedClock, mLayers, mStats, mMainThreadQueue,
            // mSubsystems (startup order), mServices (owns every module object), mFrameLoop,
            // mOwnedClock / mClock, mState, mSimulationMode, mExitCode, mStartedSubsystemCount, mQuitRequested.

        public:
            explicit application(application_desc desc);
            ~application();
            application(const application &) = delete;
            application &operator=(const application &) = delete;

        public:
            /// <summary>
            /// Starts every subsystem in the documented order. On the first failure the already-started
            /// subsystems are shut down in reverse and the error is returned; the object is then reusable.
            /// </summary>
            [[nodiscard]] core::result<void> initialise();

            /// <summary>
            /// Runs exactly one frame. Public so tests, the editor step button and the AI command server drive
            /// the same code as run(). Returns an error only for unrecoverable runtime failures.
            /// </summary>
            [[nodiscard]] core::result<void> step_frame();

            /// <summary>As step_frame(), but deltaSeconds replaces the clock / forced delta for this frame (tests, R13).</summary>
            [[nodiscard]] core::result<void> step_frame(double deltaSeconds);

            /// <summary>
            /// Editor play.step: the next frame runs exactly fixedSteps fixed steps regardless of the clock and
            /// leaves the mode PAUSED afterwards (R13).
            /// </summary>
            void step_fixed(uint32_t fixedSteps) noexcept;

            /// <summary>Frames until quit or maxFrames, then shutdown(). Returns the process exit code.</summary>
            [[nodiscard]] int run();

            void shutdown();
            void request_quit(exit_code code = exit_code::OK) noexcept;
            [[nodiscard]] bool is_quit_requested() const noexcept { return mQuitRequested; }

            /// <summary>Thread-safe. The task runs on the main thread before the next FRAME_BEGIN.</summary>
            void post_to_main_thread(std::move_only_function<void()> task);

            /// <summary>Executables and plugins add subsystems before initialise() only.</summary>
            void add_subsystem(std::unique_ptr<engine_subsystem> subsystem);

            void set_time_scale(float timeScale) noexcept;
            void set_simulation_mode(simulation_mode mode) noexcept;
            [[nodiscard]] simulation_mode current_simulation_mode() const noexcept { return mSimulationMode; }

            [[nodiscard]] engine_context &context() noexcept { return mContext; }
            [[nodiscard]] const engine_time &time() const noexcept { return mTime; }
            [[nodiscard]] const frame_stats &stats() const noexcept { return mStats; }
            [[nodiscard]] layer_stack &layers() noexcept { return mLayers; }
            [[nodiscard]] application_state state() const noexcept { return mState; }

        private:
            [[nodiscard]] core::result<void> start_subsystems();
            void stop_subsystems();
            void handle_render_failure(const core::error &error);
    };
}
```

`runtime/main.cpp` is: `application_desc::from_command_line(arguments, engine_config::defaults())` (on error
return `BAD_ARGUMENTS`; on `showHelp` print `command_line_help()`), construct `application`, `add_subsystem(
bindings::scripting_bindings_subsystem)`, `layers().push_layer(game_layer)`, `initialise()` (on error return
`INIT_FAILURE`), `return app.run()`. `conjure_editor` does the same with `config.editor = true`, pushes
`imgui_layer` and `editor_layer` as overlays and consumes `unrecognisedArguments` for `--exec` / `--export`.

`game_layer final : public layer` overrides `on_attach` (no-op; scene load is startup step 20) and `on_event`
(window close -> `request_quit`).

---

## 5. Threading and ownership

| Thread | Owner | Responsibilities | Talks to others via |
|---|---|---|---|
| Main | `application` | event pump, phase orchestration, all structural world changes, scripting host calls, `MAIN_THREAD`/`EXCLUSIVE` systems, ImGui, layer callbacks, `executeCommandList` + present, swapchain resize, device resource creation | direct calls; executes jobs while waiting |
| Job workers (`workerCount`) | `core::jobs::job_system` | `PARALLEL` systems over chunks, Jolt step jobs (adapter implements `JPH::JobSystem`: `GetMaxConcurrency`, `CreateJob(s)`, `QueueJob(s)`, `FreeJob`, barriers via `JPH::JobSystemWithBarrier`), parallel NVRHI command-list recording, async asset IO/decode | `job_system` counters only |
| Audio callback | miniaudio inside the `audio` backend | mixing | lock-free command queue owned by `audio` |
| (no thread) File watcher | `assets` | the polling stat pass runs as a job; `hot_reload_controller::update` polls its result on main (R38) | job completion |
| (no thread) AI command server | `editor` | non-blocking sockets polled on main in `protocol_server::poll`; only `--ai-stdio` owns a `core::jobs::dedicated_thread` reader (R38) | mutex-guarded line queue |

Rules enforced or documented by the engine:
- No `std::thread` outside `core::jobs` and backend implementation files (CONVENTIONS 3).
- System thread modes: `PARALLEL` (chunk-parallel, access sets declared), `MAIN_THREAD` (main thread, may overlap
  with non-conflicting `PARALLEL` systems), `EXCLUSIVE` (alone; may mutate the world directly). Scripting systems
  are `EXCLUSIVE`; this makes direct `world` mutation and the `Span<T>` escape hatch safe for C#.
- `engine_time` and `frame_stats` are written only between phases on the main thread.
- Frame memory: any scheduler job may allocate; reset only on the main thread at FRAME_BEGIN; non-scheduler jobs
  never allocate from it (Debug epoch assert).
- GPU resource creation happens on the main thread only (asset finalisation posts to main; the renderer's upload
  queue is drained at the start of `render_frame`). Command lists are recorded on workers, executed on main.
- The `render_scene` write buffer is written only during EXTRACT; after `end_extract()` it is read-only.
- Ownership: `application` owns `engine_services` (every module object as `std::unique_ptr`), the subsystem
  adapters, the layer stack, the frame loop and the clock (when not injected). `engine_context` holds raw
  non-owning pointers only.

---

## 6. Failure handling

| Situation | Behaviour |
|---|---|
| Configuration invalid | `initialise()` returns `INVALID_CONFIG` naming the field; nothing created |
| Subsystem startup failure | reverse unwind; `initialise()` returns the error; state back to `CREATED` |
| `initialise()` twice | `ALREADY_INITIALISED` |
| `step_frame()` before `initialise()` / after `shutdown()` | `CONJURE_ASSERT` in Debug; returns `NOT_INITIALISED` |
| Window close | `request_quit(OK)`; the current frame completes |
| Window minimised | simulate normally; skip submit (step 11); pacer takes over |
| Swapchain resize failure | retry once next frame; second failure -> `request_quit(RUNTIME_FAILURE)` |
| `render_frame` reports device lost | log `ERROR`, `renderer.on_device_lost()`, `request_quit(DEVICE_LOST)`; no recreate attempt in v1 |
| `render_frame` other error | log once per distinct message, continue; three consecutive failures -> `request_quit(RUNTIME_FAILURE)` |
| Spiral of death | leftover dropped, `droppedFixedTime` set, counter incremented, `WARN` at most once per second |
| Frame arena overflow | heap fallback, `overflowCount++`, `WARN` once per second with high-water mark; never fatal |
| Script exception in a frame | caught on the managed side, surfaced as `scripting::script_error` ECS event; loop continues |
| Missing .NET runtime | `SCRIPT_RUNTIME_NOT_FOUND` when `scripting.required` (runtime), else `WARN` and disabled (editor) |
| Required module missing | fatal (`MODULE_LOAD_FAILED`); optional module: `WARN` and continue |
| Audio device failure | fall back to `null`, `WARN`; Development builds also show it in the debug overlay |
| Startup scene missing | fatal in runtime, `WARN` + empty world in editor |
| `post_to_main_thread` during drain | task deferred to the next frame (no re-entrancy) |

---

## 7. Headless and null behaviour

- `null` platform (`platform::null_backend`, `11_platform.md` 3.5/7): window of `window_settings` size (1280x720 under `headless_defaults()`, R33),
  `pump_events()` returns an empty span unless a test queued events (`push_event`, `simulate_resize/focus/minimise/close/drop_files`),
  `is_minimised() == false`, `should_close()` false until `request_close()` / `simulate_close()` (the engine's `request_quit` does not touch the window) (R60);
  `input` reports nothing but accepts `platform::input::inject(event)`; clipboard and cursor mode round-trip in memory.
- `rendering.enabled == false` (headless default): no `rhi` device, no `renderer`, EXTRACT has no systems, steps
  10-11 skipped, `context.renderer == nullptr` and `context.device == nullptr`.
- Tests needing extraction set `rendering.enabled = true, backend = NULL_DEVICE`: the renderer builds its render
  graph on the `null` device; `render_frame` records into null command lists and returns success, so extraction
  cost is measurable headless.
- `null` audio keeps logical source/listener state so `is_playing`, volume and 3D parameters are assertable.
- Physics and scripting are fully functional headless. Scripting tests are skipped (not failed) on
  `SCRIPT_RUNTIME_NOT_FOUND`; the official CI matrix installs .NET 8 and fails if the skip occurs there.
- Determinism: `forcedFrameDeltaSeconds > 0` or an injected `core::manual_clock` makes the whole `engine_time`
  sequence identical across runs; `workerCount` does not affect results because systems are data-parallel over
  disjoint chunks and structural changes are applied in command-buffer order.
- Servers: `--headless --audio off --fixed-frame-delta 0` (real clock); the pacer ticks at `1 / fixedDeltaSeconds`
  by default or `--target-hz`. `--frames 0` runs until `request_quit`.
- The editor's `--headless --exec commands.json` runs the same `application`; the command server layer calls
  `step_frame()` between commands.

---

## 8. Integration points

**Needs from `core` (wave 1):** `clock` interface (`now_seconds()`, `sleep_until()`), `steady_clock`,
`manual_clock::advance()`; `jobs::job_system` with `start(const job_system_desc &)` (`workerCount`, 0 = inline),
`stop()`, `wait(const job_handle &)` executing jobs on the caller, `wait_idle()`, `parallel_for`, `thread_index()`,
`worker_count()`; `memory::frame_allocator` with
per-thread blocks and `overflow_count()`; `vfs` mounts; `json::document` preserving unknown objects;
`result<T>`/`error` with category + code + message; `log_level`; reflection registry for component bootstrap.

**Needs from `ecs` (wave 2, area A):** `ecs::system_phase` exactly as in 2.4 (R1); `scheduler::add_system(system_desc)`
with phase, `system_flags` (`MAIN_THREAD`, `EXCLUSIVE`; neither bit = PARALLEL; `RUNS_WHILE_PAUSED`), read/write
component masks and `after`/`before` labels; `scheduler::finalise() -> result<void>`; `scheduler(world &,
job_system *)` with `run_phase(system_phase, const frame_info &)` and `run_phase_filtered(system_phase, const
frame_info &, system_flags)` (R2); command-buffer flush at the end of every stage; `ecs::frame_info` in
`system_phase.h`; `world` constructible without renderer/physics; `ecs::world_hash` for tests (R32).

**Needs from `platform` / `rhi`:** `window::pump_events() -> std::span<const event>`, `should_close`,
`framebuffer_size`, `is_minimised`, `is_focused`; `input::apply(event)`, `input::end_frame()`,
`input::inject(event)`, `input::snapshot()`; `null` backend. `rhi::device::create(device_desc) ->
result<unique_ptr<device>>` (`window` of kind `NONE` for `null`/headless), `rhi::swapchain::create` / `resize`,
`device::wait_idle()`, `run_garbage_collection()`, `is_lost()` (names per `02_renderer.md` 4.1; R30). As implemented
(wave 2 integration, `09_cross_area_resolutions.md` 3): `device_desc::window` and `swapchain::create` take an
`rhi::native_window`, which the `rhi_subsystem` fills by copying `window.native_handle()` field for field (kind
`WINDOWS` / `COCOA` / `X11` / `WAYLAND`; the rhi never includes platform) and `swapchain_desc` takes
`window.framebuffer_size()`; on macOS the Vulkan backend still needs a `CAMetalLayer` (COCOA is `INVALID_DESC` on
Vulkan until the handle or `platform::window::create_vulkan_surface` provides one). After `window::set_cursor_mode()`
step 3 calls `input::skip_next_mouse_delta()` so the first cursor report after the mode change is not a jump; the
`window_settings -> window_desc` mapping is the one `tests/platform/test_backend_selection.cpp` shows (headless ->
`NULL_BACKEND`, 1280x720, R33).

**Needs from `scene` (wave 3):** `local_transform`, `world_transform`, `render_world_transform`,
`interpolated_transform`, parent/children; `scene::register_systems(scheduler, scene_manager &)` registering
`scene.apply_pending_loads` (FRAME_BEGIN), `scene.apply_hierarchy_changes` (TRANSFORM, EXCLUSIVE, before
propagation; R46), `scene.propagate_transforms` (TRANSFORM; the single owner of transform propagation, R3) and
`scene.destroy_pending` (FRAME_END) - the four systems of `13_scene.md` 4.7; `scene_manager(world &, asset_manager &)` with
synchronous `load` for startup, deferred `request_load/unload`, the `on_before_unload` event (R29) and the
post-load asset_ref bind pass (R25).

**Needs from `renderer` (wave 3, area B):** `renderer::create(renderer_desc)`, the renderer-owned
`render_scene_pair` with `begin_extract() -> render_scene &` / `end_extract()`, `render_frame() -> result<void>`
(job-parallel preparation, one command list, present), `on_swapchain_resized`, `on_device_lost`,
`set_imgui_draw_data`, `register_asset_types`, `upload_*` / `release_*` / `find_*`, tolerance of the `null`
device (R8). The renderer does not depend on `ecs`: `renderer_subsystem` registers the renderer components and
owns `engine.render_sync` (SYNC, `RUNS_WHILE_PAUSED`) and `engine.render_extract` (EXTRACT, `RUNS_WHILE_PAUSED`)
and drives GPU uploads from asset events (R7/R10).

**Needs from `physics` (wave 3, `14_physics.md`):** `physics_settings::from_json` + `validate()` (R53),
`physics_world(settings, jobs &)` with a `JPH::JobSystem` adapter over `core::jobs`, `attach(world)`;
`physics::register_components`; `physics::register_asset_types` (R44); `physics::register_systems(scheduler, physics_world &)` registering
`physics.push_transforms` (PHYSICS_STEP, before step), `physics.step` (PHYSICS_STEP, `MAIN_THREAD`),
`physics.pull_transforms` and `physics.contact_events` (FIXED_END; the latter fills
`physics_world::contact_events()`, read by `scripting.collision_dispatch` in the next step's FIXED_UPDATE, R6);
reads `frame_info.fixedDeltaTime`.

**Needs from `audio` (wave 2, no `ecs` dependency):** `audio_engine(settings)` with logical source/listener
state on the `null` backend and `audio::register_asset_types`; the engine's `audio_subsystem` registers the audio
component types and owns `engine.audio_sync` (SYNC, `RUNS_WHILE_PAUSED`) and `engine.audio_finish_one_shots`
(FRAME_END) (R7). **Needs from `scripting` (wave 3, area C):** `script_runtime::initialize(config) -> result`,
`load_game_assembly(path) -> result`, `set_mode`, `destroy_all`, `hot_reload_controller`,
`register_script_systems(scheduler, script_runtime &)` registering the `EXCLUSIVE` systems `scripting.frame_begin`,
`scripting.collision_dispatch` + `scripting.fixed_update` (FIXED_UPDATE), `scripting.update`,
`scripting.late_update`, `scripting.frame_end` (R27); `script_error::RUNTIME_NOT_FOUND` (mapped to
`SCRIPT_RUNTIME_NOT_FOUND`) when hostfxr is absent.

**Needs from `modules` (wave 2, area G):** `module_manager` with `module_consumer`s and a `service_table` (R19);
entry points receive `modules::module_context &` and reach `engine_context` / `application` as services
(`add_subsystem` only during startup step 15); manifest-only static modules for the built-in libraries, which
register nothing through the module system (R18); `export_layout` for 3.6.

**Needs from `bindings` (wave 4):** `bindings::scripting_bindings_subsystem : engine_subsystem` whose `startup`
installs native tables into `context.scripting` (step 16, before the game assembly loads in step 17).

**Offers to others:** `engine_context` (the only way modules reach services), `system_phase` helpers,
`engine_time`, `frame_stats` (editor profiler), `post_to_main_thread`, `simulation_mode` + `step_frame` (editor
play/pause/step), `layer` (editor UI, ImGui), `engine_subsystem` (plugins), `project` + `cookedRoot` (export,
area E), `headless_defaults()` + `maxFrames` + `forcedFrameDeltaSeconds` (every module's tests),
`application_desc::unrecognisedArguments` (editor flags `--exec`, `--export`).

**Build:** `conjure_add_module(engine SOURCES ... PUBLIC_DEPS conjure_core conjure_ecs conjure_platform
conjure_assets conjure_scene PRIVATE_DEPS conjure_rhi conjure_modules
$<$<BOOL:${CONJURE_WITH_RENDERER}>:conjure_renderer> $<$<BOOL:${CONJURE_WITH_PHYSICS}>:conjure_physics>
$<$<BOOL:${CONJURE_WITH_AUDIO}>:conjure_audio> $<$<BOOL:${CONJURE_WITH_SCRIPTING}>:conjure_scripting>)`.
`imgui_layer.cpp` is listed only when `CONJURE_WITH_RENDERER` and not `CONJURE_DISTRIBUTION`.

Performance budget at 10,000 entities (8 threads, mid-range desktop): phase waits 10 x ~10 us; transform pass
~0.3 ms (both matrices); physics pull/push ~0.15 ms; extraction ~0.4 ms; parallel command-list recording ~1 ms
wall; leaves >12 ms of the 16.6 ms frame for Jolt and scripts. `bench_engine_frame` guards this.

---

## 9. Implementation plan

1. `engine_error.h`, `exit_code.h`, `engine_version.h`, `log_category.h`; `engine_time`, `fixed_step_clock` with
   tests. Depends on `core` only.
2. `engine_config` (`defaults`, `headless_defaults`, `validate`) and `command_line` parsing; tests.
3. `project` load/save with `extensions` preservation and `apply_to`; tests against fixture JSON in
   `tests/engine/fixtures/`.
4. `application_desc::from_command_line` (merge + validate); precedence tests.
5. `frame_memory` facade and `frame_stats` ring; tests (the overlap test uses eight `core::jobs` workers).
6. `layer`, `layer_stack` (deferred push/pop); tests without an application.
7. `main_thread_queue` (mutex-guarded vector swap, drained once per frame); tests.
8. `engine_subsystem`; `application` skeleton with `initialise`/`shutdown` unwind over probe subsystems only (no
   module objects yet); `test_startup_order`.
9. `frame_loop` with the exact sequence of 3.1 against probe systems on a bare `ecs::world`/`scheduler`;
   `test_phase_order`, `test_engine_time`, pause-mode cases.
10. `frame_pacer`; tests with `core::manual_clock`.
11. Built-in subsystem adapters in startup order: platform, rhi, assets, ecs, scene, audio, physics, renderer,
    scripting, modules. Each adapter constructs its module object into `engine_services`, publishes the context
    pointer and forwards `register_systems` to the module's function. `test_application_headless` grows with each.
12. `game_layer`, `imgui_layer`, `debug_overlay_layer`.
13. Integration tests (`tests/integration/`) and `bench_engine_frame`.
14. `engine/src/engine/README.md`: purpose, types, threading model, how to test.

---

## 10. Tests (`tests/engine/`, target `conjure_engine_tests`)

| File | Cases |
|---|---|
| `test_fixed_step_clock.cpp` | `zero_delta_gives_zero_steps`, `exact_delta_gives_one_step_alpha_zero`, `two_half_deltas_give_one_step_on_second`, `two_and_half_steps_gives_two_steps_alpha_half`, `alpha_stays_in_unit_interval`, `step_cap_drops_remainder_and_flags`, `flag_clears_next_advance`, `set_fixed_delta_keeps_accumulator`, `reset_zeroes_accumulator`, `random_deltas_conserve_time` |
| `test_engine_time.cpp` | `first_frame_delta_equals_fixed_delta`, `frame_delta_clamped_to_max`, `scaled_and_unscaled_accumulate_under_half_time_scale`, `time_scale_zero_runs_update_without_fixed_steps`, `fixed_time_advances_per_step`, `frame_index_monotonic`, `forced_delta_sequences_identical_across_two_apps`, `in_fixed_step_true_only_inside_fixed_phases` |
| `test_engine_config.cpp` | `defaults_validate`, `headless_defaults_validate`, one case per `validate()` rule naming the field, `debug_forces_gpu_validation`, `worker_count_auto_resolution_clamped` |
| `test_command_line.cpp` | one case per flag, `precedence_cli_over_project_over_defaults`, `unknown_flags_returned_unrecognised`, `bad_numeric_returns_bad_arguments`, `headless_applies_before_others`, `help_sets_flag` |
| `test_project.cpp` | `load_valid`, `missing_file_not_found`, `malformed_reports_line_column`, `unknown_key_preserved_in_extensions_after_save`, `schema_too_new_rejected`, `missing_name_invalid`, `relative_paths_resolve_against_root`, `fixed_hz_round_trips`, `apply_to_then_cli_wins`, `cooked_flag_mounts` |
| `test_frame_memory.cpp` | `alignment_honoured_1_to_256`, `eight_jobs_allocate_without_overlap`, `reset_reclaims_everything`, `overflow_falls_back_and_counts`, `epoch_increments_per_reset`, `stats_high_water_correct` |
| `test_frame_stats.cpp` | `ring_wraps_at_240`, `latest_is_last_committed`, `average_over_n` |
| `test_layer_stack.cpp` | `push_order_and_overlay_ordering`, `dispatch_top_down_stops_at_consumer`, `overlays_receive_events_first`, `update_bottom_up`, `pop_during_dispatch_deferred_once`, `destruction_reverse_order`, `attach_failure_returns_error` |
| `test_main_thread_queue.cpp` | `tasks_from_workers_run_on_main_in_post_order`, `post_during_drain_deferred` |
| `test_startup_order.cpp` | `probe_subsystems_record_documented_order`, `third_probe_failure_unwinds_two_in_reverse`, `post_startup_after_all_startups`, `register_systems_after_post_startup`, `add_subsystem_after_initialise_asserts`, `shutdown_idempotent`, `destructor_shuts_down`, `double_initialise_returns_error` |
| `test_phase_order.cpp` | `probe_systems_record_exact_sequence_with_two_fixed_steps_at_1_over_30`, `entity_created_in_phase_visible_next_phase_not_same`, `paused_skips_update_but_runs_flagged`, `step_once_runs_one_frame_then_pauses`, `transform_and_extract_run_while_paused` |
| `test_frame_pacer.cpp` | `sleeps_to_target_with_manual_clock`, `never_negative`, `skips_when_behind`, `inactive_with_forced_delta`, `inactive_after_vsync_present`, `headless_defaults_to_fixed_rate`, `unfocused_drops_to_30hz` |
| `test_application_headless.cpp` | `initialise_and_shutdown_leak_free`, `run_with_max_frames_10_returns_ok`, `step_frame_ten_times_equals_run_ten`, `request_quit_from_layer_returns_code`, `step_before_initialise_errors`, `context_pointers_null_for_disabled_modules`, `context_pointers_stable_across_frames`, `render_enabled_null_device_runs_extract` |
| `test_render_transform.cpp` (scene + engine) | `interpolated_entity_between_previous_and_current_at_half`, `child_inherits_interpolated_parent`, `interpolate_physics_off_yields_current`, `root_without_pose_equals_world` |
| `tests/integration/test_engine_physics_interpolation.cpp` | falling body, 300 frames at forced 1/30; rendered height monotonic and within one fixed step of simulation |
| `tests/integration/test_engine_project_boot.cpp` | `samples/feature_test` boots headless, loads its startup scene, runs 60 frames, quits clean, no leaks |
| `tests/integration/test_engine_render_failure.cpp` | instrumented null renderer: three consecutive failures quit with `RUNTIME_FAILURE`; device lost quits with `DEVICE_LOST` |
| `tests/benchmarks/bench_engine_frame.cpp` | empty-system frame overhead; 10,000 entities with transform + trivial `PARALLEL` system; 10,000 transform + rigid body headless; microseconds per frame and per phase; reference CI runner fails if mean frame > 12 ms; not in the default ctest run |

---

## 11. Out of scope / future

- Dedicated render thread: deferred until `bench_engine_frame` plus a renderer submit benchmark show the main
  thread submit path above 4 ms at the target scene. The seam in D1 (immutable `render_scene`, single
  `render_frame`, renderer-owned ping-pong) is binding so the change stays local to `frame_loop.cpp` plus ImGui
  draw-data staging.
- Device-lost recovery (device recreation and GPU resource reload). v1 quits with `DEVICE_LOST`.
- Runtime resizing of the worker pool (frame memory blocks are indexed by a fixed `thread_index()`).
- Lockstep-deterministic servers that must never drop fixed time (set `maxFixedStepsPerFrame` very high and accept
  slowdown; a documented project.json choice, no engine change).
- Multiple windows / swapchains; hot reload of `project.json` while running.
- Interpolation of non-physics transforms (networking) can reuse `interpolated_transform` unchanged; writing it is
  the networking module's job.
