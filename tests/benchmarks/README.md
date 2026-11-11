# benchmarks

The 10,000-entity benchmark harness of `docs/design/08_testing.md` (D10, 2.4, 3.6, 4.7, 7.1) and the
`tests/benchmarks` directory `09_cross_area_resolutions.md` section 4 items 13(c) and 14(d) owed since wave
2: an in-house measure loop (fixed warm-up, fixed measured iterations, min / p50 / p95 / p99, the exact
allocation count of the worst iteration), `thresholds.json` with the `desktop` and `ci` machine classes, one
scenario file per module and one CTest entry per scenario under the label `benchmark`. Benchmarks are not
part of the default `ctest` run (the test presets exclude the label); they run with `-L benchmark` or by
hand.

## Layout

| Path | Contents |
|---|---|
| `include/conjure/bench/benchmark.h` | `machine_class`, `bench_threshold`, `bench_options`, `AUTO_JOB_THREAD_COUNT`, `bench_statistics`, `bench_result`, `bench_context` (`measure`, `set_items`, `add_phase`, `skip`, `fail`, `jobs`), `bench_descriptor`, `bench_registry`, `CONJURE_BENCHMARK(name)`, `percentile`, `compute_statistics`, `resolve_job_thread_count` |
| `include/conjure/bench/bench_runner.h` | `parse_command_line`, `matches_filter`, `parse_machine_class`, `scenario_thresholds`, `bench_runner` (`load_thresholds`, `run`, `run_all`, `exit_code`, `write_json`, `summarise`, `format_line`), the exit codes |
| `src/benchmark.cpp`, `src/bench_runner.cpp` | the harness library `conjure::bench_harness` (depends on `conjure::core` and `conjure::test_support`) |
| `src/bench_main.cpp` | the executable's entry point: command line, main thread, WARN console, seed, run, JSON output |
| `bench_support.{h,cpp}` | build-time paths (shader index, managed SDK, test scripts), the .NET probe, `scratch_directory`, `random_transform`, `spawn_hierarchy` |
| `bench_core.cpp`, `bench_ecs.cpp`, `bench_scene.cpp`, `bench_physics.cpp`, `bench_render_null_frame.cpp`, `bench_scripts_update_phase.cpp`, `bench_full_frame_10k.cpp` | the scenarios, one file per module (the renderer file holds both renderer rows) |
| `thresholds.json` | `{"version": 1, "scenarios": {"<module>.<scenario>": {"desktop": {"p50_ms", "p95_ms", "p99_ms"}, "ci": {...}, "allocs": n}}}`; a missing `allocs` means "not checked", a time of 0 means "not checked" |
| `tests/` | `conjure_benchmarks_tests` (label `unit;benchmarks`): `test_benchmark.cpp`, `test_bench_runner.cpp`, the self-tests of 08 section 11 |

The executable links `tests/support/src/tracked_global_new.cpp` like every test executable (08 D3), so
`conjure::testing::allocation_scope` counts every heap allocation of the process, std containers included.
Jolt's allocations arrive through the physics module's `core::memory` hooks, so the physics gates see them too.

## Running

```
python tools/build.py --preset windows-msvc-dev --build-dir build/mod_bench --target conjure_benchmarks --test -L benchmark
build/mod_bench/bin/conjure_benchmarks --list
build/mod_bench/bin/conjure_benchmarks --filter "scene.*" --threads 8
build/mod_bench/bin/conjure_benchmarks --class ci --enforce --out bench.json
```

Command line: `[--class desktop|ci] [--enforce|--no-enforce] [--filter <glob>] [--out <file>]
[--threads N|auto] [--thresholds <file>] [--iterations N] [--warmup N] [--entities N] [--seed N] [--list]
[--help]`. Defaults: 200 measured iterations after 20 warm-up iterations, 10,000 entities, `desktop`, seed
`0x5EED`, `--threads auto`, the `thresholds.json` next to the sources.

`--threads` follows the engine's `jobs_settings` (`06_engine_loop.md`, `08_testing.md` D4 `--jobs`): `auto`
(the default, `AUTO_JOB_THREAD_COUNT`) is `hardware_concurrency - 1` clamped to 1..16, the worker count the
engine starts with; `0` starts the benchmark job system, and `engine::application` in the full-frame
scenarios, with **no workers**, so every job and every PARALLEL system runs inline on the main thread, the
mode the determinism tests use; any other N is that many workers (at most 256). The report line's
`threads=` carries the resolved value, so `threads=0` means inline.

Thresholds are **enforced in Development and Distribution** (a breached or missing entry exits 1) and
**reported only in Debug** (the compile definition `CONJURE_BENCH_ENFORCE_DEFAULT`); `--enforce` and
`--no-enforce` override either. A scenario whose machine precondition is unmet (no .NET runtime, no managed
SDK, no shader index) is reported `SKIPPED` and the run exits 77 when every selected scenario skipped, which
CTest shows as *Not Run*. A scenario is `FAILED` with a `reason` whatever its figures say when it returned
without calling either `measure()` or `skip()` (a forgotten skip would otherwise pass every threshold over
zero samples, 08 D12) or when it called `bench_context::fail(reason)` (the full-frame scenarios do when a
measured `step_frame()` returned an error, the application requested quit, or the fixture's own sample
vectors grew inside the measured loop). `benchmark.<name>` entries carry the labels `benchmark` and
`<module>`, the `[dotnet]` ones also `scripting` and `dotnet`.

Every scenario prints one machine-readable line, and the full-frame and physics sync scenarios one more
per reported sub-phase:

```
benchmark name=<name> phase=total status=PASSED|FAILED|UNTHRESHOLDED|SKIPPED iterations=<n> items=<n>
    threads=<n> min_ms=<f> median_ms=<f> p95_ms=<f> p99_ms=<f> max_ms=<f> allocs=<n>
    [threshold_p50_ms=<f> threshold_p95_ms=<f> threshold_p99_ms=<f> threshold_allocs=<n>] [reason="..."]
benchmark name=<name> phase=<PHASE> status=... min_ms=... median_ms=... p95_ms=... p99_ms=... max_ms=...
```

`reason` (SKIPPED and FAILED rows) is double-quoted with `"` and `\` escaped by a backslash, so a Windows
path or a quoted error message keeps the line parseable. `--out` writes the same figures as JSON
(`version`, `class`, `enforce`, `iterations`, `warmup`, `results[]` with `name`, `status`, `iterations`,
`threads`, `items`, the five statistics, `allocs`, `threshold` when one applies, `reason` when one exists,
`phases[]`).

## Scenarios and the measured numbers of the reference machine

Windows 11, MSVC 19.51 (VS 18), 16 hardware threads, Development configuration (`windows-msvc-dev`, build
directory `build/fix4_bench_dev`), default options (200 iterations, 20 warm-up, 10,000 entities, `desktop`
class, `--threads auto` = 16 workers), the fix-4 round of 2026-11 (`conjure_benchmarks --class desktop --out
build/fix4_bench_dev_run.json`; `renderer.extract_10k` from the rerun after its view was corrected in the same
round). Figures are the `median_ms` / `p99_ms` / `allocs` the run printed; the budgets are the `desktop`
column of 08 7.1 for the 17 scenarios the design lists. The three scenarios the design table does not list
take their budgets from elsewhere: `scene.snapshot_capture_restore_10k` from 13_scene.md 3.14 (play/stop
round trip < 50 ms), `renderer.null_frame_10k` from ARCHITECTURE.md 11 (`bench_render_null_frame < 3 ms`,
allocation-free like every renderer row) and `physics.step_2k_settling` as described under deviations
(not a design budget).

| Scenario | items | median_ms | p99_ms | allocs | desktop budget (p50 / p99 / allocs) | status |
|---|---|---|---|---|---|---|
| `core.job_parallel_for_1m` | 1,000,000 | 0.096 | 0.192 | 0 | 0.500 / - / 0 | PASSED |
| `ecs.iterate_100k` | 100,000 | 0.118 | 0.248 | 1 | 0.150 / - / 0 | FAILED |
| `ecs.spawn_destroy_10k` | 2,000 | 0.654 | 1.104 | 33 | 1.000 / - / 2 | FAILED |
| `ecs.query_changed_10k` | 10,000 | 0.004 | 0.004 | 0 | 0.050 / - / 0 | PASSED |
| `scene.transform_propagation_10k` | 10,000 | 0.267 | 0.476 | 0 | 0.400 / - / 0 | PASSED |
| `scene.transform_propagation_10k_sparse` | 10,000 | 0.242 | 0.457 | 0 | 0.100 / - / 0 | FAILED |
| `scene.snapshot_capture_restore_10k` | 10,000 | 26.4 | 33.9 | 103,162 | 50.0 / - / n/a | PASSED |
| `scene.load_10k` | 10,000 | 44.7 | 59.4 | 133,260 | 250.0 / - / n/a | PASSED |
| `physics.step_10k_active` | 10,000 | 4.896 | 6.158 | 0 | 8.000 / - / 0 | PASSED |
| `physics.step_10k_sleeping` | 10,000 | 0.268 | 0.334 | 0 | 0.800 / - / 0 | PASSED |
| `physics.sync_10k` | 10,000 | 0.172 | 0.319 | 2 | 0.300 / - / 0 | FAILED |
| `physics.step_2k_settling` | 2,000 | 1.269 | 1.751 | 0 | 3.000 / - / 0 | PASSED |
| `renderer.extract_10k` | 10,000 | 0.220 | 0.360 | 21 | 1.000 / - / 0 | FAILED |
| `renderer.null_frame_10k` | 10,000 | 0.949 | 1.494 | 306 | 3.000 / - / 0 | FAILED |
| `scripting.update_10k` | 10,000 | 0.075 | 0.090 | 0 | 2.500 / - / 0 | PASSED |
| `scripting.update_10k_dod` | 10,000 | 0.008 | 0.021 | 3 | 0.600 / - / 0 | FAILED |
| `scripting.native_call_100k` | 100,000 | 1.397 | 1.899 | 0 | 10.0 / - / 0 | PASSED |
| `engine.full_frame_10k_transform_render` | 10,000 | 0.550 | 0.937 | 12 | 4.000 / 8.000 / 0 | FAILED |
| `engine.full_frame_10k_physics` | 10,000 | 4.950 | 6.357 | 42 | 12.0 / 16.0 / 0 | FAILED |
| `engine.full_frame_10k_scripts` | 10,000 | 0.132 | 0.246 | 1 | 5.000 / 10.0 / 0 | FAILED |

Sub-phases of `physics.sync_10k` in the same run: `physics.pull_transforms` median 0.170 ms (p99 0.317),
`physics.contact_events` 0.000 ms (no listeners, nothing to collect), `SERIAL_PULL` 0.438 ms (p99 0.607),
which is the figure the first integration run reported as the breach (09 item 15(j)); the frame cost of the
sync is the parallel one. Every time budget holds except `scene.transform_propagation_10k_sparse` (p50
0.242 ms against 0.100); every other `FAILED` row is an allocation gate, listed below.

## What each scenario does

- `core.job_parallel_for_1m`: one `job_system::parallel_for` over 1,000,000 `u32` items (an increment each)
  with the default grain of the per-index helpers (`count / (workers * 4)`), the body a `std::function`
  built once: the dispatch and join cost of the job system with next to no work per range.
- `ecs.iterate_100k`: 100,000 `{position, velocity}` entities, one PARALLEL integrating system over chunks
  (`parallel_for_each_chunk`) run through `scheduler::run_phase(UPDATE)`; stages run inline so the job
  system's per-stage counter stays out of the allocation gate.
- `ecs.spawn_destroy_10k`: 10,000 live entities; every iteration records 1,000 destroys and 1,000 creates
  with two components into one `command_buffer` and flushes it.
- `ecs.query_changed_10k`: 10,000 `{position, marker}` entities; every iteration writes `marker` on the
  first 5 % (contiguous in chunk order) and runs a `changed<marker>` query that reads them into `position`.
- `scene.transform_propagation_10k` / `_sparse`: `spawn_hierarchy` (100 roots, depth 3, four children per
  node, padded to 10,000) with random transforms; every iteration moves every root (or 10 % of them) and runs
  the TRANSFORM phase (`scene.apply_hierarchy_changes` + `scene.propagate_transforms`).
- `scene.snapshot_capture_restore_10k`: `scene::capture` followed by `scene::restore` of the same 10,000.
- `scene.load_10k`: the hierarchy saved once into a `memory_file_system`; every iteration is
  `scene_manager::load_file(REPLACE)` plus `unload`.
- `physics.step_10k_active`: 10,000 dynamic unit boxes resting awake (sleeping disabled) on a static slab,
  one fixed step (push, step, pull, collect) per iteration.
- `physics.step_10k_sleeping`: the same grid created `START_ASLEEP` after 600 settle steps.
- `physics.sync_10k`: the engine's FIXED_END over the awake field through an `ecs::scheduler` with
  `physics::register_systems` (stages inline, chunks parallel), i.e. the PARALLEL `physics.pull_transforms`
  fanned out over the benchmark job system exactly as `engine::application` runs it (14_physics.md 3.7),
  followed by `physics.contact_events`. The scheduler's per-system timings are reported as the phases
  `physics.pull_transforms` and `physics.contact_events`; the phase `SERIAL_PULL` is the single-threaded
  `pull_transforms()` without a `system_context` (the fallback the engine never takes for this system),
  kept for comparison and never thresholded.
- `physics.step_2k_settling`: 1,000 columns of two boxes dropped onto the slab, sleeping disabled; the
  contact-heavy step (box-on-box and box-on-ground manifolds) that `physics.step_10k_active` (one box per
  column, resting) does not cover.
- `renderer.extract_10k`: 10,000 cube instances written into the renderer's own `render_scene` through
  `begin_extract()` / `allocate_meshes` / `end_extract()` (the public extract API, the pair's write slot
  keeps its capacity), then `cull_instances` against the view's frustum on the benchmark job system (the
  culler `render()` runs first for every view, parallel above 2,048 instances); the camera sits in the
  grid's centre so about half the instances are behind it (the scenario skips unless 40-60 % pass).
- `renderer.null_frame_10k`: the same 10,000 instances and camera with a shadow-casting sun, one
  `renderer::render()` on the null device per iteration.
- `scripting.update_10k` `[dotnet]`: 10,000 `Conjure.TestScripts.Mover` instances, one
  `script_runtime::run_phase(UPDATE)` on the real CLR; `scripting.update_10k_dod` `[dotnet]`: one
  `PositionWriter` walking 10,000 `test.scripting.position` rows through a `Span<T>` query scope.
- `scripting.native_call_100k` `[dotnet]`: one `Conjure.TestScripts.NativeCallProbe` whose `OnUpdate` makes
  100,000 `Entity.IsAlive` calls on its own entity (the design row's `Entity.IsValid`; `IsAlive` is the
  managed API's spelling of the native liveness check, one bridge call each), one `run_phase(UPDATE)`.
- `engine.full_frame_10k_transform_render`: `engine::application` headless with the null device, a camera
  and 10,000 mesh renderers moved by a PARALLEL system every frame; one `step_frame()` per iteration.
- `engine.full_frame_10k_physics`: the application booted from a scratch `project.json` whose `physics`
  section disables sleeping, 10,000 awake boxes on a slab; one `step_frame()`.
- `engine.full_frame_10k_scripts` `[dotnet]`: the application with the real CLR and Conjure.TestScripts,
  10,000 `Mover` entities; one `step_frame()`.

## Deviations and open points

Harness-side state after the fix-4 round; the engine-side breaches are the input of the performance round
and are not fixed from here.

- **Scenario set.** All 17 rows of 08_testing.md 7.1 have a scenario and a `thresholds.json` entry with the
  design's figures. `core.job_parallel_for_1m`, `renderer.extract_10k` and `scripting.native_call_100k` were
  missing until this round (the first integration run's "8 of 17" counted the three unlisted scenarios below
  in their place). Three scenarios the design table does not list exist as well: `scene.snapshot_capture_restore_10k`
  (13_scene.md 3.14 budget), `renderer.null_frame_10k` (ARCHITECTURE.md 11 budget) and
  `physics.step_2k_settling`.
- **`physics.step_2k_settling` budget (3.00 ms desktop, 9.00 ms ci, 0 allocations).** The scenario is the
  benchmark counterpart of the 14_physics.md `test_physics_stack.cpp` check ("2,000 settled boxes, one
  `step()` under `CONJURE_CHECK_NO_ALLOCATIONS`"), which fixes the allocation gate at 0; 14_physics.md
  defines no time budget for it (the earlier attribution of the 3.00 ms to that document was wrong). The
  time budget is a harness regression guard, not a design number: `physics.step_10k_active` allows 8.00 ms
  for 10,000 resting bodies (0.8 us per body); the settling columns pay box-on-box as well as box-on-ground
  manifolds, so twice the per-body cost over 2,000 bodies gives 3.2 ms, rounded down to 3.00. Measured:
  1.27 ms.
- **`scripting.native_call_100k` spelling.** The design row says `Entity.IsValid`; the managed API has no
  such member, its native liveness check is `Entity.IsAlive` (`docs/api/bindings/csharp-api.md`), which
  the probe calls. One bridge call per iteration, as the row intends.
- **`physics.sync_10k` measurement.** Until this round the scenario timed `pull_transforms()` without a
  `system_context`, the serial fallback the engine never takes for this system, and reported the 0.43 ms
  breach of 09 item 15(j). It now runs the engine's FIXED_END through a scheduler with
  `physics::register_systems` (parallel chunks): 0.17 ms, inside the 0.30 ms budget. Its 2 allocations per
  iteration are on the engine's parallel path (the same source as `ecs.iterate_100k`'s 1: the PARALLEL
  system dispatch), owed by the performance round.
- **`renderer.extract_10k` allocations (21).** `cull_instances` on the job system builds one
  `std::vector<small_vector<u32, 64>>` per call and each chunk's list outgrows 64 entries at 50 %
  visibility (engine/src/renderer/culling.cpp); the extraction itself (the pair's write slot) is
  allocation-free after warm-up. Engine-side; the time budget holds (0.22 ms of 1.00).
- **Allocation gates still red (engine-side, unchanged from 09 item 15(j)):** `ecs.iterate_100k` 1,
  `ecs.spawn_destroy_10k` 33 (budget 2), `engine.full_frame_10k_physics` 42, `engine.full_frame_10k_scripts`
  1, `engine.full_frame_10k_transform_render` 12, `renderer.null_frame_10k` 306, `scripting.update_10k_dod`
  3. **Time budget still red:** `scene.transform_propagation_10k_sparse` p50 0.242 ms against 0.100 (the
  dense variant passes at 0.267 of 0.400, so the sparse pass does not scale down with the dirty fraction).
  Under `--enforce` (Development, Distribution, the nightly `benchmarks-ci-class`) the run exits 1 until
  these are closed or the module owners revisit the thresholds.
- **`renderer.null_frame_10k` visibility.** Its perspective camera sees 2,725 of the 10,000 instances (the
  pyramid narrows towards the camera), not "half" as the first README said; the scenario and its
  ARCHITECTURE.md budget are unchanged. `renderer.extract_10k` uses the orthographic view described above
  so that exactly the front half (5,100 instances, the straddling row included) passes.
- **Scenario fixtures count towards the gate.** `bench_context::measure` counts every allocation of the
  process, so the fixtures reserve what they touch before the loop (`frame_fixture::reserve` counts from the
  current size, since the mesh-load frames may exceed the settle frames) and the full-frame scenarios fail
  the row if their own vectors grew, a frame returned an error or the application requested quit, instead of
  timing frames the engine cut short.

## Threading

The harness runs scenarios one after another on the main thread (designated with
`core::jobs::set_main_thread()` so the scripting runtime and the engine frame loop accept it). Each
`bench_context` owns one `core::jobs::job_system` started on first use with the resolved worker count; the
engine scenarios let `engine::application` start its own with the same count. Timing uses
`core::ticks_now()` around the body; allocation counts come from the process-wide
`core::memory::counters()` through `conjure::testing::allocation_scope`, so allocations on worker threads
are counted as well.

## Testing

```
python tools/build.py --build-dir build/mod_bench --target conjure_benchmarks_tests --test -R benchmarks
```

`tests/test_benchmark.cpp` covers the percentiles, the measure loop (warm-up untimed, one sample per
measured iteration, exact allocation maximum), `skip`, `fail`, phases, the lazily started job system (with
workers and inline with `--threads 0`), `resolve_job_thread_count` (AUTO, 0, a request), the registry and
the `CONJURE_BENCHMARK` registration; `tests/test_bench_runner.cpp` covers every command-line option and
its failure (`--threads 0|auto|N` included), the glob filter, `thresholds.json` (both classes, the shared
allocation gate, every malformed shape), `summarise` for each metric, `run` / `run_all` with the exit codes
0 / 1 / 2 / 77, the FAILED verdicts of a scenario that neither measured nor skipped (a null function too)
and of one that called `fail`, the escaping of `reason` in the report line and the JSON report round trip.
