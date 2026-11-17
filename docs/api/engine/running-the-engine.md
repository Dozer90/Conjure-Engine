# Running the engine

📘 **Public API wiki** · 🛠 Handbook: [Engine module](../../handbook/modules/engine.md)

How an `application` comes to life: the configuration in `conjure/engine/engine_config.h`, the command line in
`conjure/engine/command_line.h`, `project.json` in `conjure/engine/project.h`, the merged descriptor in
`conjure/engine/application_desc.h`, the application itself in `conjure/engine/application.h`, the service table
in `conjure/engine/engine_context.h` and the outcomes in `conjure/engine/exit_code.h`,
`conjure/engine/engine_error.h` and `conjure/engine/engine_version.h`.

## At a glance

| You want to... | Use |
|---|---|
| The desktop defaults (window, real backends) | `engine_config::defaults()` |
| Run without a window, device or audio (tests, servers) | `engine_config::headless_defaults()` |
| Run the game's C# scripts in a headless run | `--headless --scripting`, or `config.scripting.enabled = true` |
| Parse `--project`, `--headless`, `--frames`, `--jobs` ... and merge everything | `application_desc::from_command_line(arguments, baseConfig)` |
| Load and query a project file | `project::load(directory)`, `description()`, `assets_directory()`, `module_manifests()` |
| Start, step, run, stop | `application::initialise()`, `step_frame()`, `run()`, `shutdown()` |
| Reach the world, the scenes, the assets, the renderer | `application::context()` (an `engine_context`) |
| Stop from inside a frame | `application::request_quit(exit_code)` |
| Pause, resume, step the simulation | `set_simulation_mode(PAUSED / RUNNING / STEP_ONCE)`, `step_fixed(n)` |
| Run jobs inline for a deterministic run | `config.jobs.workerCount = 0` (`AUTO_WORKER_COUNT` picks the machine's count) |

## Quick example

```cpp
#include "conjure/engine/application.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/exit_code.h"

using namespace conjure;

int run_game(std::span<const std::string_view> arguments) {
    core::result<engine::application_desc> parsed =
      engine::application_desc::from_command_line(arguments, engine::engine_config::defaults());
    if (!parsed.has_value()) {
        return static_cast<int>(engine::exit_code::BAD_ARGUMENTS);
    }
    if (parsed->showHelp) {
        std::puts(engine::command_line_help().data());
        return 0;
    }
    engine::application app(std::move(parsed).value());
    if (!app.initialise().has_value()) {
        return static_cast<int>(engine::exit_code::INIT_FAILURE);   // the error was logged with its step
    }
    return app.run();                                              // frames until quit or --frames
}
```

## Configuration

`engine_config` is a plain struct of plain structs; every field has a default and `validate()` checks the
combination once (`INVALID_CONFIG` names the field). The merge order is fixed: defaults, then
`headless_defaults()` when `--headless` is given, then `project.json`, then the command line, then `validate()`.

| Section | Fields you set most |
|---|---|
| `window` | `title`, `width`, `height`, `fullscreen`, `resizable`, `vsync` |
| `time` | `fixedDeltaSeconds` (1/60), `maxFrameDeltaSeconds`, `maxFixedStepsPerFrame`, `timeScale`, `forcedFrameDeltaSeconds` (> 0 replaces the clock; at most `maxFrameDeltaSeconds`), `interpolatePhysics` |
| `pacing` | `targetHz` (0 = unpaced), `sleepWhenUnfocused` |
| `rendering` | `enabled`, `backend` (`AUTO`, `D3D12`, `VULKAN`, `NULL_DEVICE`), `gpuValidation`, `shaderDirectory` |
| `physics` | `enabled`, `gravity`, `maxBodies`; the rest of the physics settings come from the project's `"physics"` section |
| `audio` | `enabled`, `backend` (`AUTO`, `MINIAUDIO`, `NULL_DEVICE`), `sampleRate`, `deviceName` (the `project.json` key `device`), `masterGain`, `dopplerFactor`, `speedOfSound`, `groups` |
| `assets` | `hotReload`: the file watcher that re-imports changed sources while a development project runs (on by default, off in `headless_defaults()`, ignored in Distribution and by cooked projects) |
| `scripting` | `enabled`, `required`, `gameAssembly`, `managedDirectory`, `hotReload`, `exportMode`, `exportDebugSymbols`, `useNullHost` |
| `modules` | `searchPaths`, `enginePlugins`, `required` |
| `jobs` | `workerCount`: `AUTO_WORKER_COUNT` (default) resolves to `hardware_concurrency - 1` clamped to `[1, 16]`; `0` runs every job on the calling thread |
| `memory` | `frameArenaBytes`, `frameThreadBlockBytes` |

The section types are named after their member (`window_settings`, `time_settings` and so on) with two
exceptions: `physics` is a `physics_config` and `audio` an `audio_config`. Those two feed a module's own settings
struct (`physics::physics_settings`, `audio::audio_settings`), and the different name keeps the engine section and
the module struct apart in a file that uses both namespaces.

`headless_defaults()` sets a 1280 x 720 null window, rendering off on the null device, the null audio backend,
scripting off, asset hot reload off, a forced 1/60 frame delta and two workers; `--headless` applies the same
fields over the base configuration, and a project cannot switch the devices, scripting or hot reload back on
under it (a project whose `max_frame_delta_seconds` is below 1/60 lowers the forced delta to that clamp). Tests
opt subsystems back in field by field, and the command line opts scripting back in with `--scripting`.

> [!IMPORTANT]
> `validate()` rejects rendering on a real backend while `headless` is true. Headless rendering means the null
> device: it exercises extraction and the render graph without pixels. Off-screen rendering to a texture is not
> available yet.

## The command line

`parse_command_line()` understands `--project <dir>`, `--scene <path>`, `--headless`, `--frames <n>`, `--rhi
auto|d3d12|vulkan|null`, `--audio auto|miniaudio|null|off`, `--no-physics`, `--scripting`, `--no-scripting`,
`--no-render`, `--width`, `--height`, `--fullscreen`, `--no-vsync`, `--fixed-hz`, `--fixed-frame-delta`,
`--target-hz`, `--time-scale`, `--jobs <n|auto>`, `--log-level`, `--log-file` and `--help`. Anything else lands in
`unrecognisedArguments` for the executable (the editor reads `--exec` and `--export` from there; the runtime
refuses unknown options). `command_line_help()` is the text the executables print.

A flag that takes a value never takes another flag as it: `--scene --no-physics` is `BAD_ARGUMENTS` (missing value), not
a scene named `--no-physics`. When `--audio` is given several times the last one wins, so `--audio off --audio null`
runs on the null backend. `--rhi d3d12` or `--rhi vulkan` together with `--headless` logs a warning and has no effect: a
headless run renders on the null device only.

`--headless` leaves scripting off whatever `project.json` says, so a smoke test or a CI boot of a scripted game needs
neither a .NET runtime nor the game assembly. `--scripting` turns scripting on: it is applied after the project, so it
wins over the headless default and over a project whose `scripting.enabled` is `false`. `--headless --scripting` runs
the project's C# scripts, which is what a dedicated server wants. `--scripting` together with `--no-scripting` is
`BAD_ARGUMENTS`, in either order.

> [!IMPORTANT]
> With `--scripting` the start can fail where a plain `--headless` run would not: `scripting.required` is `true` by
> default, so a machine without a .NET runtime answers `SCRIPT_RUNTIME_NOT_FOUND` and a missing game assembly
> `SCRIPT_ASSEMBLY_LOAD_FAILED`. An engine build configured without `CONJURE_WITH_SCRIPTING` has no scripting
> subsystem: the flag is accepted and the run goes on without scripts; the engine logs a warning that the build has
> no scripting module for a headless run outside the editor and for any run whose configuration names a game
> assembly.

When a headless run leaves the scripts of a project off, the engine log says so once at startup (`scripting: disabled;
a headless run leaves the project's scripts off unless --scripting is given`). The line is for a project that has
scripts to run, one that enables scripting and names a game assembly (`"scripting": { "assembly": ... }`); a project
without one, a `project.json` without a `scripting` section included, gets `scripting: disabled by configuration`.

## project.json

`project::load(directory)` reads `<directory>/project.json` (schema version 1, snake_case keys): `name`,
`version`, `assets_root`, `cooked_root`, `startup_scene`, the `window`, `time`, `physics`, `rendering`, `audio`,
`assets` (`hot_reload`), `scripting` and `modules` sections, `layers` (the 32 scene layer names) and `cooked`.
Unknown top-level keys are kept in `extensions` and written back by `save()`; when `extensions` already has a member of
the same name, that member is kept and the top-level key is dropped with a warning. Schema 1 reserves `"assets"`, so a
file that used that key for its own data is parsed as the section and its unknown members are dropped on `save()` like
those of every other section. `apply_to(config)` copies the sections into a configuration without touching the headless,
frame-count, jobs and log settings, so a headless test keeps its null backends (and its disabled scripting and asset hot
reload) while reading the project's scenes and physics layers.

```cpp
core::result<engine::project> loaded = engine::project::load("C:/games/rolling_course");
CONJURE_TRY(loaded);
engine::application_desc desc = {};
desc.config = engine::engine_config::headless_defaults();
desc.config.projectPath = loaded->root_directory();
loaded->apply_to(desc.config);
desc.project = std::move(loaded).value();
```

## Startup, frames and shutdown

`initialise()` runs the startup order of the design in one call: the log sinks, the job system, frame memory,
the virtual file system mounts (`project://`, `assets://`, `engine://`, `cooked://`), then one subsystem after
another, every `post_startup` (the asset scan happens here), every `register_systems` followed by
`scheduler::finalise()`, the layers' `on_attach` and the startup scene. Afterwards `state()` is `RUNNING`. A
startup scene that is not a known asset, fails to load or whose source no longer imports is
`STARTUP_SCENE_FAILED` for the runtime, which never runs a stale cached copy. The editor goes on: it warns about a
scene it cannot load and opens the last cached copy of a scene that no longer imports, so the scene can be fixed.

The failure is kept. `startup_scene_failure()` returns the `STARTUP_SCENE_FAILED` error of the last
`initialise()`. It is empty when the scene loaded, when no startup scene was named and when startup failed
before the scene step. A runtime whose `initialise()` failed on the scene holds the error that call returned.
The value survives `shutdown()`, and the next `initialise()` clears it.

- `step_frame()` runs one frame; `step_frame(deltaSeconds)` forces this frame's delta (tests); the first frame
  always runs exactly `fixedDeltaSeconds`.
- `set_time_scale(scale)` takes effect from the next frame; a negative or non-finite scale (NaN, infinity) is
  stored as zero, which stops time while frames keep running.
- `run()` loops `step_frame()` until `is_quit_requested()` or `engine_config::maxFrames`, then calls
  `shutdown()` and returns `current_exit_code()` as an `int`.
- `request_quit(code)` stops `run()` after the current frame; the worst code wins (`OK` < `INIT_FAILURE` <
  `RUNTIME_FAILURE` < `BAD_ARGUMENTS` < `DEVICE_LOST`).
- `shutdown()` is idempotent and runs the reverse order: jobs idle, device idle, every scene unloaded, assets
  collected, layers detached, subsystems stopped, core torn down.

> [!WARNING]
> `add_subsystem()` and the layer stack's `push_layer()` are for *before* `initialise()`: a subsystem added later
> asserts and is ignored, a layer pushed later attaches at the next frame boundary. Plugins add subsystems from
> their module entry during startup instead.

## The service table

`engine_context` is a struct of non-owning pointers, stable from `initialise()` to `shutdown()`: `app`, `config`,
`project`, `time`, `frameMemory`, `jobs`, `filesystem`, `window`, `input`, `device`, `assets`, `assetDatabase`,
`components`, `world`, `scheduler`, `scenes`, `renderer`, `physics`, `audio`, `scripting`, `modules`, `checks` and
`stats`. A pointer is null when the module is compiled out, disabled by configuration or absent for this kind of
project (`assetDatabase` is null in a cooked export; `renderer` and `device` are null with rendering off). Plugins
receive the same struct as a module service (`context.require_service<engine::engine_context>()`).

> [!WARNING]
> Every consumer early-outs on a null pointer. Dereferencing `context().physics` in a `--no-physics` run is a
> crash, not an error result.

## Outcomes

| `exit_code` | Meaning |
|---|---|
| `OK` (0) | `run()` finished: `--frames` reached, the window closed, `--help` printed |
| `INIT_FAILURE` (1) | `initialise()` failed; the log names the step |
| `RUNTIME_FAILURE` (2) | a frame failed, three render failures in a row, or a resize failed twice |
| `BAD_ARGUMENTS` (3) | `from_command_line` failed or the merged configuration did not validate |
| `DEVICE_LOST` (4) | the graphics device was lost; no recovery in v1 |

`engine_error` (category `engine`) carries the recoverable failures: `INVALID_CONFIG`, `BAD_ARGUMENTS`, the four
project errors (`PROJECT_NOT_FOUND`, `PROJECT_PARSE_FAILED`, `PROJECT_INVALID`, `PROJECT_SCHEMA_TOO_NEW`), one
`*_FAILED` per startup step (`JOBS_START_FAILED`, `WINDOW_CREATE_FAILED`, `DEVICE_CREATE_FAILED`,
`ASSET_DATABASE_FAILED`, `RENDERER_INIT_FAILED`, `PHYSICS_INIT_FAILED`, `SCRIPT_RUNTIME_NOT_FOUND`,
`SCRIPT_ASSEMBLY_LOAD_FAILED`, `SCRIPT_RUNTIME_INIT_FAILED`, `MODULE_LOAD_FAILED`, `SCHEDULER_CONFLICT`,
`LAYER_ATTACH_FAILED`, `STARTUP_SCENE_FAILED`), `ALREADY_INITIALISED`, `NOT_INITIALISED`, `DEVICE_LOST` and
`EXPORT_FAILED`. `engine_version_string()` and the `ENGINE_VERSION_*` constants identify the build.

## API summary

| Signature (short) | Purpose |
|---|---|
| `static engine_config engine_config::defaults()` / `headless_defaults()` | the two starting points |
| `result<void> engine_config::validate() const` | one check of the whole configuration |
| `u32 resolve_worker_count(const jobs_settings &)` | what `workerCount` resolves to on this machine |
| `static result<application_desc> application_desc::from_command_line(span<const string_view>, const engine_config &)` | the whole merge |
| `static result<project> project::load(const path &)` / `parse(text, root)` | read `project.json` |
| `void project::apply_to(engine_config &) const` | copy the project's settings into a configuration |
| `explicit application(application_desc)` | takes the descriptor; nothing starts yet |
| `result<void> application::initialise()` / `step_frame()` / `step_frame(f64)` / `int run()` / `void shutdown()` | the lifecycle |
| `const optional<core::error> &application::startup_scene_failure() const` | the `STARTUP_SCENE_FAILED` error of the last `initialise()`, empty when the startup scene loaded or none was named |
| `void request_quit(exit_code = OK)` / `bool is_quit_requested()` / `exit_code current_exit_code()` | stopping |
| `void set_simulation_mode(simulation_mode)` / `step_fixed(u32)` / `cancel_fixed_steps()` / `set_time_scale(f32)` | editor play, pause and step (`cancel_fixed_steps` drops a step the next frame has not run yet) |
| `engine_context &context()` / `const engine_time &time()` / `const frame_stats &stats()` / `layer_stack &layers()` | what the application exposes |
| `std::filesystem::path engine_directory() const` / `managed_directory() const` | the running executable's directory, where `shaders/`, `managed/` and the plugins are found; and the managed SDK directory the script host uses, always absolute (`scripting.managedDirectory` when set, else `engine_directory() / "managed"`). A tool that builds game scripts passes it to MSBuild as `ConjureManagedDir` |
| `void post_to_main_thread(std::function<void()>)` | thread-safe hand-off to the main thread before the next frame |
| `std::string_view to_string(exit_code)` / `engine_error_name(engine_error)` | names for logs |

## See also

- 📘 API: [The frame, layers and time](frame-layers-and-time.md), [Export and diagnostics](export-and-diagnostics.md)
- 📘 API: [The runtime](../runtime/index.md) (the executable built from this page), [Modules and plugins](../modules/index.md)
- 🛠 Handbook: [Engine module](../../handbook/modules/engine.md)
