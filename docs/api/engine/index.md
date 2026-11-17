# Engine: the application and the frame

📘 **Public API wiki** · 🛠 Handbook: [Engine module](../../handbook/modules/engine.md)

The engine module is the object that runs a game. `conjure::engine::application` owns every other module
(the window, the graphics device, the asset manager, the world, the scene manager, physics, audio, scripting
and the plugin loader), starts them in a fixed order, runs one frame per `step_frame()` in the fixed phase
sequence of the ECS scheduler and shuts them down in reverse. Everything on these pages lives in
`namespace conjure::engine`. You meet it in three places: the standalone runtime (`conjure_runtime`), the
editor (which wraps the same application) and your own tests and tools, which drive it headless on null
backends.

## The mental model in five sentences

1. An `application` is built from an `application_desc`: a validated `engine_config` plus an optional
   `project` (the parsed `project.json`). The descriptor comes from `engine_config::defaults()`,
   `headless_defaults()` or `application_desc::from_command_line()`, which merges defaults, `--headless`,
   `project.json` and the flags in that order.
2. `initialise()` starts the subsystems in the documented order (platform, rhi, assets, ecs, scene, audio,
   physics, renderer, scripting, modules, then yours) and loads the startup scene; a failure unwinds what
   started and leaves the object reusable.
3. `step_frame()` runs exactly one frame: time, input, the fixed-step loop, UPDATE and LATE_UPDATE, the
   transform pass, SYNC, EXTRACT, rendering and FRAME_END. `run()` loops it until `request_quit()` or
   `--frames`, then shuts down and returns an `exit_code`.
4. Everything the frame produced or needs is reachable through `engine_context`: non-owning pointers to the
   world, the scene manager, the asset manager, the renderer and the rest; a null pointer means "compiled
   out or disabled", never "not yet created".
5. Headless is configuration, not a code path: with `headless_defaults()` the same application runs on the
   null window, the null graphics device and the null audio backend, which is how every test and server
   uses it.

## Where to go next

| Page | Read it when you want to... |
|---|---|
| [Running the engine](running-the-engine.md) | build an `application` from a configuration or a command line, read `project.json`, understand the startup order, exit codes and the errors `initialise()` can return |
| [The frame, layers and time](frame-layers-and-time.md) | hook into the frame with a `layer` or an `engine_subsystem`, read `engine_time` and `frame_stats`, use per-frame scratch memory, pause and step the simulation |
| [Export and diagnostics](export-and-diagnostics.md) | export a project into a standalone folder with `export_pipeline`, report self-checks through `check_registry` |

## Headers

| Header | What it holds |
|---|---|
| `conjure/engine/application.h` | `application`, `application_state`, `simulation_mode` |
| `conjure/engine/application_desc.h` | `application_desc` and its `from_command_line` / `from_options` merge |
| `conjure/engine/engine_config.h` | `engine_config` and its section structs (`window_settings`, `time_settings`, `rendering_settings`, `physics_config`, `audio_config`, `scripting_settings`, `modules_settings`, `jobs_settings`, `memory_settings`), the backend choice enumerations, `AUTO_WORKER_COUNT`, `resolve_worker_count()` |
| `conjure/engine/command_line.h` | `command_line_options`, `parse_command_line()`, `apply_command_line()`, `command_line_help()` |
| `conjure/engine/project.h` | `project`, `project_description`, `project_module_entry`, `PROJECT_SCHEMA_VERSION` |
| `conjure/engine/engine_context.h` | `engine_context`, the service table |
| `conjure/engine/engine_subsystem.h` | `engine_subsystem`, the adapter interface plugins and executables implement |
| `conjure/engine/layer.h`, `conjure/engine/layer_stack.h` | `layer`, `layer_stack` |
| `conjure/engine/game_layer.h`, `conjure/engine/imgui_layer.h`, `conjure/engine/debug_overlay_layer.h` | the three built-in layers |
| `conjure/engine/engine_time.h`, `conjure/engine/fixed_step_clock.h`, `conjure/engine/engine_phase.h` | `engine_time`, `make_frame_info()`, `fixed_step_clock`, the phase helpers |
| `conjure/engine/frame_stats.h`, `conjure/engine/frame_memory.h` | `frame_stats`, `frame_sample`, `frame_memory` |
| `conjure/engine/exit_code.h`, `conjure/engine/engine_error.h`, `conjure/engine/engine_version.h` | `exit_code`, `engine_error`, the version constants |
| `conjure/engine/export.h` | `export_pipeline`, `export_settings`, `export_report` |
| `conjure/engine/diagnostics/check_registry.h` | `check_registry`, `check_record` |
| `conjure/engine/log_category.h` | `LOG_ENGINE` |

## Quick tour

```cpp
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"

using namespace conjure;

engine::application_desc desc = {};
desc.config = engine::engine_config::headless_defaults();   // null window, null device, null audio
desc.config.jobs.workerCount = 0;                           // every job inline: deterministic

engine::application app(std::move(desc));
CONJURE_TRY(app.initialise());
for (u32 frame = 0; frame < 60; ++frame) {
    CONJURE_TRY(app.step_frame());                          // one frame at the forced 1/60 delta
}
CONJURE_LOG_INFO(LOG_GAME, "{} fixed steps", app.time().fixedStepIndex);
app.shutdown();
```

> [!TIP]
> In tests, prefer `conjure::testing::headless_engine` (🛠 Handbook: [Testing](../../handbook/testing.md)) over a
> hand-built application: it adds the memory and log checks, scene loading by path and scripted input.

## Local terms

| Term | Meaning here |
|---|---|
| subsystem | an adapter that creates one module object at startup, registers its systems and steps it per frame (`engine_subsystem`) |
| layer | an object stacked on the application that receives events, `on_update`, `on_imgui` and `on_frame_end` (`layer`) |
| fixed step | one `FIXED_UPDATE` / `PHYSICS_STEP` / `FIXED_END` pass at `fixedDeltaSeconds`; a frame runs zero or more |
| forced delta | `time_settings::forcedFrameDeltaSeconds`: replaces the wall clock so runs are repeatable |
| headless | `engine_config::headless`: the null platform, rhi and audio backends, no window |
| cooked project | an exported folder whose `project.json` says `"cooked": true`; only `engine://` and `cooked://` are mounted |

## See also

- 📘 API: [Entities, components and systems](../ecs/index.md), [Scenes, hierarchy and prefabs](../scene/index.md), [Modules and plugins](../modules/index.md)
- 📘 API: [The editor](../editor/index.md), [The runtime](../runtime/index.md), [Scripting from C#](../bindings/index.md)
- 🛠 Handbook: [Engine module](../../handbook/modules/engine.md), [Architecture](../../handbook/architecture.md)
