# The frame, layers and time

📘 **Public API wiki** · 🛠 Handbook: [Engine module](../../handbook/modules/engine.md)

What happens inside one `step_frame()` and how your code takes part: layers (`conjure/engine/layer.h`,
`conjure/engine/layer_stack.h`, `conjure/engine/game_layer.h`, `conjure/engine/imgui_layer.h`,
`conjure/engine/debug_overlay_layer.h`), subsystems (`conjure/engine/engine_subsystem.h`), time
(`conjure/engine/engine_time.h`, `conjure/engine/fixed_step_clock.h`, `conjure/engine/engine_phase.h`) and the
per-frame statistics and scratch memory (`conjure/engine/frame_stats.h`, `conjure/engine/frame_memory.h`).

## At a glance

| You want to... | Use |
|---|---|
| React to window events, draw ImGui, run code once per frame outside the ECS | derive from `layer`, push it with `application::layers().push_layer()` |
| Add a module object with systems of its own (a plugin's subsystem) | derive from `engine_subsystem`, `application::add_subsystem()` before `initialise()` |
| Read the frame delta, the fixed delta, the frame index | `application::time()` (an `engine_time`), or `system_context::frame()` inside a system |
| Know whether a phase is a fixed one | `is_fixed_phase(phase)`, `FIXED_PHASES`, `to_string(phase)` |
| Profile the last frames | `application::stats().latest()`, `history()`, `average_frame_milliseconds(n)` |
| Allocate scratch memory that dies at the next frame | `context.frameMemory->allocate_span<T>(count)` |
| Quit when the window closes | push a `game_layer` (the runtime does) |

## The frame in order

Every phase is an `ecs::system_phase`; the engine calls `scheduler.run_phase(phase, frame_info)` and the
scheduler flushes command buffers at every stage boundary, so a structural change made in one phase is visible
in the next, never in the same one.

```
begin_frame (engine_time) -> frame_memory.reset -> pump events, layers.on_event, input.end_frame
-> main-thread queue -> subsystems.begin_frame, layers.on_frame_begin
-> FRAME_BEGIN
-> [FIXED_UPDATE, PHYSICS_STEP, FIXED_END] x fixed steps this frame   (skipped while PAUSED)
-> UPDATE -> layers.on_update -> LATE_UPDATE -> TRANSFORM -> SYNC
-> layers.on_imgui, layers.on_imgui_end -> EXTRACT -> render_frame
-> FRAME_END -> subsystems.end_frame, layers.on_frame_end, layers.apply_pending -> pacer -> stats.commit
```

While `simulation_mode::PAUSED` the fixed loop is skipped and UPDATE / LATE_UPDATE run only the systems flagged
`RUNS_WHILE_PAUSED`; TRANSFORM, SYNC and EXTRACT keep running so an editor still renders edits. `STEP_ONCE` runs
one frame and returns to PAUSED; `application::step_fixed(n)` runs exactly `n` fixed steps in the next frame.

## Layers

A `layer` is a named object with virtual hooks: `on_attach` (startup step 19; a failure fails `initialise()`
with `LAYER_ATTACH_FAILED`), `on_detach`, `on_frame_begin`, `on_update` (after UPDATE, also while paused),
`on_imgui` and `on_imgui_end` (editor builds, with the ImGui frame open), `on_frame_end` and `on_event`, which
returns `true` to consume a window or input event before it reaches the input state and the layers below.

`layer_stack` keeps ordinary layers below overlays: `push_layer` and `push_overlay` (a request made inside a
frame is applied at the frame boundary), `pop(layer)`, `dispatch_event` (top-down until consumed), `layers()`,
`overlay_count()`, `pending_count()`. The three built-in layers:

| Layer | What it does |
|---|---|
| `game_layer` | consumes the window-close event and calls `request_quit(OK)`; `close_requests()` counts them |
| `imgui_layer` | editor builds only: owns the one `ImGuiContext`, begins the frame in `on_frame_begin`, ends it in `on_imgui_end` and hands the draw data to the renderer; `context()`, `is_frame_open()`, `stats()`, clipboard helpers |
| `debug_overlay_layer` | outside Distribution: the frame-time overlay; `text()`, `set_visible()`, `set_log_interval_frames()`, `format_sample(context)` |

```cpp
#include "conjure/engine/layer.h"

class fps_layer final : public conjure::engine::layer {
    public:
        fps_layer() : layer("fps") {}

    public:
        void on_frame_end(conjure::engine::engine_context &context) override {
            if (context.time->frameIndex % 60 == 0) {
                const conjure::engine::frame_sample &last = context.stats->latest();
                CONJURE_LOG_INFO(LOG_GAME, "{:.2f} ms/frame", last.frameMilliseconds);
            }
        }
};

app.layers().push_layer(std::make_unique<fps_layer>());    // before initialise(), or at any frame boundary
```

> [!WARNING]
> Layer hooks run on the main thread between phases. Mutating the world from `on_update` is allowed (no system
> is running), mutating it from a job is not.

## Subsystems

`engine_subsystem` is the adapter interface the engine uses for its own modules and offers to plugins and
executables: `name()`, `startup(context)` (create the module object, publish its pointer), `post_startup(context)`
(after every startup), `register_systems(scheduler, context)` (before the scheduler is finalised),
`begin_frame`, `end_frame` and `shutdown(context)`, which must be safe after a failed or skipped startup.
Executables add subsystems with `application::add_subsystem()` before `initialise()`; a module entry adds them
through the `application` service during startup. The bindings module's `scripting_bindings_subsystem` is the
first user (📘 API: [Scripting from C#](../bindings/index.md)).

## Time

`engine_time` is written once per frame and read everywhere: `time` and `unscaledTime` (seconds since the first
frame), `deltaTime` and `unscaledDeltaTime` (clamped to `maxFrameDeltaSeconds`), `fixedDeltaTime`, `fixedTime`,
`timeScale`, `interpolationAlpha` (where the frame sits between the last two fixed steps, valid from UPDATE on),
`frameIndex`, `fixedStepIndex`, `fixedStepsThisFrame`, `inFixedStep` and `droppedFixedTime` (the spiral-of-death
guard fired). The first frame of an application uses the fixed delta whatever the clock says, so every run starts
the same way. `make_frame_info(time)` builds the `ecs::frame_info` a system sees.

`fixed_step_clock` is the accumulator behind the fixed loop: `advance(scaledDelta)` returns how many steps to
run and the new alpha, caps the steps at `maxStepsPerFrame` (dropping the remainder and flagging it),
`set_fixed_delta`, `reset`. `engine_phase.h` adds `to_string(phase)`, `is_fixed_phase(phase)`, `phase_index(phase)`
and the `FIXED_PHASES` span.

> [!NOTE]
> With `time_settings::forcedFrameDeltaSeconds > 0` (the headless default, 1/60) the wall clock is never read,
> so a headless run is repeatable frame for frame; `jobs.workerCount = 0` makes it repeatable across machines too.

## Frame statistics and scratch memory

`frame_stats` is a ring of 240 `frame_sample`s committed at the end of each frame: `frameMilliseconds`,
`phaseMilliseconds[phase]`, `submitMilliseconds`, `pacerSleepMilliseconds`, `fixedSteps`, `jobsExecuted`,
`frameMemoryHighWater`, `droppedFixedTime`. `latest()`, `sample_at(age)`, `history()`,
`average_frame_milliseconds(count)`, `total_committed()` and `total_dropped_fixed_steps()` read it; the editor's
profiler panel and the C# `Diagnostics.Stats` show the same numbers.

`frame_memory` is per-frame scratch over the core frame allocator: `allocate(bytes, alignment)`,
`allocate_span<T>(count)`, `construct<T>(args...)`; everything is released by `reset()` at the start of the next
frame. Jobs of the scheduler may allocate from it; an overflow falls back to the heap and is counted
(`stats().overflowCount`), never fatal.

> [!CAUTION]
> A pointer into frame memory is valid until the next `reset()`, which the engine performs at step 2 of the next
> frame. Keeping one across frames is a use-after-free.

## API summary

| Signature (short) | Purpose |
|---|---|
| `class layer { virtual on_attach / on_detach / on_frame_begin / on_update / on_imgui / on_imgui_end / on_frame_end / on_event }` | the layer hooks |
| `layer_stack::push_layer(unique_ptr<layer>)` / `push_overlay` / `pop(layer &)` / `dispatch_event` | the stack |
| `class engine_subsystem { name, startup, post_startup, register_systems, begin_frame, end_frame, shutdown }` | the adapter interface |
| `struct engine_time` / `make_frame_info(const engine_time &)` | the frame clock |
| `fixed_step_result fixed_step_clock::advance(f32)` | steps and alpha for this frame |
| `bool is_fixed_phase(system_phase)` / `std::string_view to_string(system_phase)` | phase helpers |
| `const frame_sample &frame_stats::latest()` / `span<const frame_sample> history()` | profiling |
| `void *frame_memory::allocate(usize, usize)` / `span<T> allocate_span<T>(usize)` / `T *construct<T>(...)` | scratch memory |

## See also

- 📘 API: [Running the engine](running-the-engine.md), [Queries, views and systems](../ecs/queries-views-and-systems.md), [Jobs and threading](../core/jobs.md)
- 🛠 Handbook: [Engine module](../../handbook/modules/engine.md), [Data-oriented design](../../handbook/data-oriented-design.md)
