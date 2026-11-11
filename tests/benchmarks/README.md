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
directory `build/perf_integration_dev`), default options (200 iterations, 20 warm-up, 10,000 entities,
`desktop` class, `--threads auto` = 16 workers), the integration of the performance round of 2026-11
(`conjure_benchmarks --class desktop --enforce --out build/perf_final_dev_run.json`, one enforced run of all
20 scenarios on an otherwise idle machine with the round's final code: the module owners' allocation fixes and
the integrator's job-system change, a 50 us spin window before a thread sleeps and a test-and-test-and-set
queue lock that parks after a bounded spin, the form the review of the integration settled on, `core/jobs`
README). Figures are the `median_ms` / `p99_ms` / `allocs` the run printed; the budgets are the `desktop`
column of 08 7.1 for the 17 scenarios the design lists. The three scenarios the design table does not list
take their budgets from elsewhere: `scene.snapshot_capture_restore_10k` from 13_scene.md 3.14 (play/stop
round trip < 50 ms), `renderer.null_frame_10k` from ARCHITECTURE.md 11 (`bench_render_null_frame < 3 ms`,
allocation-free like every renderer row) and `physics.step_2k_settling` as described under deviations
(not a design budget).

| Scenario | items | median_ms | p99_ms | allocs | desktop budget (p50 / p99 / allocs) | status |
|---|---|---|---|---|---|---|
| `core.job_parallel_for_1m` | 1,000,000 | 0.052 | 0.085 | 0 | 0.500 / - / 0 | PASSED |
| `ecs.iterate_100k` | 100,000 | 0.049 | 0.062 | 0 | 0.150 / - / 0 | PASSED |
| `ecs.spawn_destroy_10k` | 2,000 | 0.243 | 0.288 | 0 | 1.000 / - / 2 | PASSED |
| `ecs.query_changed_10k` | 10,000 | 0.004 | 0.004 | 0 | 0.050 / - / 0 | PASSED |
| `scene.transform_propagation_10k` | 10,000 | 0.173 | 0.480 | 0 | 0.400 / - / 0 | PASSED |
| `scene.transform_propagation_10k_sparse` | 10,000 | 0.136 | 0.411 | 0 | 0.100 / - / 0 | FAILED |
| `scene.snapshot_capture_restore_10k` | 10,000 | 14.6 | 16.5 | 99,918 | 50.0 / - / n/a | PASSED |
| `scene.load_10k` | 10,000 | 34.4 | 36.9 | 130,016 | 250.0 / - / n/a | PASSED |
| `physics.step_10k_active` | 10,000 | 3.864 | 4.469 | 0 | 8.000 / - / 0 | PASSED |
| `physics.step_10k_sleeping` | 10,000 | 0.091 | 0.127 | 0 | 0.800 / - / 0 | PASSED |
| `physics.sync_10k` | 10,000 | 0.075 | 0.111 | 0 | 0.300 / - / 0 | PASSED |
| `physics.step_2k_settling` | 2,000 | 1.177 | 1.610 | 0 | 3.000 / - / 0 | PASSED |
| `renderer.extract_10k` | 10,000 | 0.141 | 0.190 | 0 | 1.000 / - / 0 | PASSED |
| `renderer.null_frame_10k` | 10,000 | 0.779 | 1.179 | 0 | 3.000 / - / 0 | PASSED |
| `scripting.update_10k` | 10,000 | 0.189 | 0.229 | 0 | 2.500 / - / 0 | PASSED |
| `scripting.update_10k_dod` | 10,000 | 0.005 | 0.010 | 0 | 0.600 / - / 0 | PASSED |
| `scripting.native_call_100k` | 100,000 | 1.244 | 1.913 | 0 | 10.0 / - / 0 | PASSED |
| `engine.full_frame_10k_transform_render` | 10,000 | 0.470 | 0.552 | 0 | 4.000 / 8.000 / 0 | PASSED |
| `engine.full_frame_10k_physics` | 10,000 | 3.829 | 4.511 | 0 | 12.0 / 16.0 / 0 | PASSED |
| `engine.full_frame_10k_scripts` | 10,000 | 0.119 | 0.217 | 0 | 5.000 / 10.0 / 0 | PASSED |

Sub-phases of `physics.sync_10k` in the same run: `physics.pull_transforms` median 0.073 ms (p99 0.110),
`physics.contact_events` 0.000 ms (no listeners, nothing to collect), `SERIAL_PULL` 0.255 ms (p99 0.295), the
serial fallback whose 0.43 ms the first integration run reported as the breach (09 item 15(j)); the frame cost
of the sync is the parallel one. Every allocation gate holds (19 of 20 scenarios PASSED under `--enforce`); the
one red row is the time budget of `scene.transform_propagation_10k_sparse` (p50 0.136 ms against 0.100), whose
arithmetic is under deviations. The owners' own run before the job-system change (`build/perf_engine_dev_run.json`)
had the same 19 green rows with the dispatch-bound scenarios higher: `core.job_parallel_for_1m` 0.099,
`ecs.iterate_100k` 0.113, `scene.transform_propagation_10k` 0.225, `_sparse` 0.150, `physics.sync_10k` 0.109,
`engine.full_frame_10k_transform_render` 0.571 ms. The integrator's run with the ticket lock the round first
used (`build/perf_integration_dev_run.json`) sat between the two: `core.job_parallel_for_1m` 0.042, `_sparse`
0.125, `scene.transform_propagation_10k` 0.172, `physics.sync_10k` 0.068 ms; the parking lock's compare-exchange
and backoff path costs the dispatch-bound rows about 0.01 ms against a FIFO ticket, the price of a waiter that
lost its core no longer stalling the whole pool (`core/jobs` README).

## What each scenario does

- `core.job_parallel_for_1m`: one `job_system::parallel_for` over 1,000,000 `u32` items (an increment each)
  with the default grain of the per-index helpers (`count / (workers * 4)`), the body a `std::function`
  built once: the dispatch and join cost of the job system with next to no work per range.
- `ecs.iterate_100k`: 100,000 `{position, velocity}` entities, one PARALLEL integrating system over chunks
  (the documented `context.view<...>().parallel_for_each_chunk` idiom, whose query keeps its matches in a
  `small_vector<archetype *, QUERY_INLINE_MATCHES>`) run through `scheduler::run_phase(UPDATE)`; stages run
  inline here, and a warmed-up phase allocates nothing in either mode (stage jobs complete on a stack-held
  `core::jobs::job_fence`).
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

Harness-side state after the integration of the performance round of 2026-11 (the fix-4 round corrected the
scenario set, the performance round closed the engine-side allocation breaches of `09_cross_area_resolutions.md`
4 item 15(j) and the integration cut the job system's dispatch cost; its section 3 "Performance round" entry
is the record).

- **Scenario set.** All 17 rows of 08_testing.md 7.1 have a scenario and a `thresholds.json` entry with the
  design's figures. `core.job_parallel_for_1m`, `renderer.extract_10k` and `scripting.native_call_100k` were
  missing until the fix-4 round (the first integration run's "8 of 17" counted the three unlisted scenarios below
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
  1.23 ms.
- **`scripting.native_call_100k` spelling.** The design row says `Entity.IsValid`; the managed API has no
  such member, its native liveness check is `Entity.IsAlive` (`docs/api/bindings/csharp-api.md`), which
  the probe calls. One bridge call per iteration, as the row intends.
- **`physics.sync_10k` measurement.** Until the fix-4 round the scenario timed `pull_transforms()` without a
  `system_context`, the serial fallback the engine never takes for this system, and reported the 0.43 ms
  breach of 09 item 15(j). It runs the engine's FIXED_END through a scheduler with `physics::register_systems`
  (parallel chunks): 0.11 ms and 0 allocations after the performance round (the pull fans its persistent query
  out through `system_context::parallel_for_chunks`, resolves each body once with one `TryGetBody` and finds
  rows through a dense entity-index table; the serial fallback went from 0.44 ms to 0.27 ms).
- **Allocation gates: all green.** The performance round of 2026-11 closed every allocation breach of 09 item
  15(j): `ecs.iterate_100k` 1 -> 0 and `ecs.spawn_destroy_10k` 33 -> 0 (inline query matches, the world's
  member flush plan, recycled command-buffer arenas, parked spare chunks), `renderer.extract_10k` 21 -> 0 and
  `renderer.null_frame_10k` 306 -> 0 (persistent view states, `cull_scratch`, pooled graph entries,
  binding-desc scratch, pre-created timer queries, an allocation-free null device whose marker names are
  recorded only after `reset_null_stats()`), `scripting.update_10k_dod` 3 -> 0 (a cached `ecs::query` per
  component set, reusable scope states), `physics.sync_10k` 2 -> 0, and the three engine frames:
  `engine.full_frame_10k_transform_render` 12 -> 0, `engine.full_frame_10k_physics` 42 -> 0,
  `engine.full_frame_10k_scripts` 1 -> 0 (the scheduler's stack-held stage fence, the asset hot-reload
  watcher now opt-in through `assets_settings::hotReload` and off headless, the extraction's member camera
  list, the main-thread queues drained only when non-empty). The `headless_engine` empty frame the full-frame
  rows include is 0 blocks inline and with workers in Development. The integration repeated the three
  contention-sensitive gates (`physics.physics_world - 600 empty steps allocate nothing after the first`,
  `integration.hierarchy_propagation - a 10,000-entity hierarchy adds nothing to the frame's allocations once
  warm`, `integration.prefab_spawn_destroy - 100 crates spawned and 100 destroyed per frame keep the count
  stable`) with `ctest --repeat until-fail:20` in the Debug and the Development tree without a failure.
- **Time budget still red: `scene.transform_propagation_10k_sparse`** (p50 0.136 ms against 0.100; 0.125
  with the ticket lock the round first used, 0.150 before the integration's job-system change, 0.17 before the scene owner's pass rewrite; the dense variant
  passes at 0.172 of 0.400). The threshold is unchanged. The scene owner's arithmetic: the scenario moves 10 %
  of the roots, but the 100 roots share one archetype whose chunk holds 67 of them, so the chunk-granular
  `local_transform` stamp (13_scene.md S4) dirties 67 roots and their subtrees: 6,770 of the 10,000 rows are
  recomputed, a 68 % scenario by the design's own detection (08_testing.md 7.1's "10 % dirty" is 10 % of the
  roots, which with S4 is 68 % of the rows). The rows cost about 25 ns each (0.122 ms with inline jobs), so the
  work fits the budget over four workers and more; what does not fit at 16 is the dispatch. The integration
  measured the pass against the worker count (Development, idle machine, `--threads N`): 0.122 ms inline, 0.094
  with 2 workers, 0.070 with 4, 0.092 with 8, 0.125 with 16 (ticket lock; 0.136 with the parking lock), and
  `core.job_parallel_for_1m` (`workers * 4`
  ranges of next to no work, 8 to 64) 0.015 / 0.016 / 0.032 / 0.044 ms with 2 / 4 / 8 / 16 workers (the
  ticket lock's enforced run gave 0.042 ms, the parking lock's 0.052 in the table): every job popped from
  the single shared queue is a serialisation point costing about 0.6 us (ticket lock) to 0.8 us (parking lock)
  with 16 workers contending (the lane head, the
  queued count, the block and the free list all move between cores on every pop), and the pass dispatches
  about 41 jobs per iteration (9 for the 1,072-row level, 34 for the 4,288-row level) plus two joins. Before
  the integration's job-system change the same pop cost 1.7 us (a parked `std::mutex` waiter is a kernel
  wake-up per job; `core.job_parallel_for_1m` was 0.100 ms with 16 workers), which is what the owner's note
  measured as "80-100 us of wake-up and join". What was tried and rejected: one range per thread instead of
  one per 128-row job (dense 0.199, sparse 0.097 at 4 workers: load imbalance costs more than the pops it
  saves); dropping the pass's per-chunk relaxed counters (no change). Reaching 0.100 ms with 16 workers needs
  per-worker queues with stealing, or a `parallel_for` that queues min(ranges, workers) claim blocks and hands
  out ranges with one atomic per range; both change what the core tests pin (`test_parallel_for.cpp`:
  `poolCapacity >= range_count - 1` after one dispatch, the queue-full fallback per range) and are the core
  owner's decision. Alternatively the scenario could use `--threads 4` for this row, or 7.1's "10 % dirty"
  could be read as 10 % of the rows. Under `--enforce` (Development, Distribution, the nightly
  `benchmarks-ci-class`) the run exits 1 on this row alone.
- **`renderer.null_frame_10k` visibility.** Its perspective camera sees 2,725 of the 10,000 instances (the
  pyramid narrows towards the camera), not "half" as the first README said; the scenario and its
  ARCHITECTURE.md budget are unchanged. `renderer.extract_10k` uses the orthographic view described above
  so that exactly the front half (5,100 instances, the straddling row included) passes.
- **Scenario fixtures count towards the gate.** `bench_context::measure` counts every allocation of the
  process, so the fixtures reserve what they touch before the loop (`frame_fixture::reserve` counts from the
  current size, since the mesh-load frames may exceed the settle frames) and the full-frame scenarios fail
  the row if their own vectors grew, a frame returned an error or the application requested quit, instead of
  timing frames the engine cut short.
- **Allocation gates need an idle machine.** `allocation_scope` counts every thread of the process and the
  gate is the maximum over the measured iterations, so a job-system block-pool growth under CPU contention
  (another build on the same machine) shows up as a few blocks in a frame that is allocation-free when idle
  (`engine.full_frame_10k_scripts` reported 4 under a concurrent compile and 0 in every idle run; 09 item
  14(f) records the same suspect for `physics.physics_world`). Run the enforced benchmarks alone.

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
