# Design Area H - Test, Benchmark and CI Strategy

Status: final. Author: area H judge. Supersedes both panel proposals.

This document defines everything that proves Conjure works: doctest conventions, the test support
libraries, headless fixtures on the `null` backends, what every module must cover, integration
tests, optional golden-image tests, the 10,000-entity benchmark harness with pass thresholds, the
`feature_test` sample, how the AI protocol is tested, the CI matrix, and the definition of done.
Every Fixed decision in `docs/design/00_brief.md` and every rule in `docs/CONVENTIONS.md` is
respected; where this document is more specific, it wins for test code.

Nothing here is optional for implementers. Where a name used below belongs to another area
(component type names, protocol command names) the mechanism is registry-driven so that renames do
not break the design; section 9 lists every assumption made about other areas.

---

## 1. Overview and decisions

Backbone: Proposal 1 (registry-driven completeness, real job system, allocation gates, two benchmark
machine classes, replayed `feature_test`, skip discipline). Grafted from Proposal 2: per-case process
isolation, support libraries split by wave, `memory_guard` + fail-on-ERROR `log_capture` in every
fixture, recorder-style null backends, `check_registry`, per-command protocol fixtures, header checks.

| # | Decision | Rationale |
|---|---|---|
| D1 | **One CTest entry per `TEST_CASE`** via doctest's `doctest_discover_tests()` (post-build `--list-test-cases`), `TEST_PREFIX "<module>."`. Benchmarks are the only executable registered as a single entry. | A crash, a leaked global or a stuck .NET host in one case cannot hide the result of 300 others; CTest names the exact failing case; `ctest -j` parallelises across processes. Null backends start in microseconds and `headless_engine` construction is budgeted at 5 ms, so per-process cost is noise. Case names must therefore be unique per executable. |
| D2 | **Two test-only static libraries**: `conjure_test_support` (depends on `core` only; wave 1) and `conjure_test_support_engine` (depends on `engine` and, when built, `editor_core`; wave 4). Namespace `conjure::testing`, headers under `tests/support/include/conjure/testing/`. Never installed or shipped. | Wave 1-3 module tests must build and run before `engine` exists. One library with CMake conditions would still force the whole engine to build before `core` tests run. |
| D3 | **One shared doctest `main`** (`tests/support/src/test_main.cpp`, `DOCTEST_CONFIG_IMPLEMENT`) and one global `operator new`/`delete` replacement TU (`tracked_global_new.cpp`) compiled into every test and benchmark executable by `conjure_add_test_module()`. Tests never write their own `main`. | One place for seed, thread count, log sink, assert handler, skip codes and allocation counting. Replacing global `new` only in test executables gives exact allocation counts (including `std::vector` growth) with zero cost in shipped binaries. |
| D4 | **Tests run with a real job system**: `clamp(hardware_concurrency(), 2, 8)` workers by default, overridable with `CONJURE_TEST_THREADS` (`0` = all jobs inline on the calling thread). Seed from `CONJURE_TEST_SEED` (default `0x5EED`), printed at start. | Job parallelism is the main performance lever and must be tested where it runs. Inline mode exists for debugging and for the determinism baseline. A test that depends on the order of parallel side effects is a bug in the test. |
| D5 | **Deterministic engine stepping**: the fixture calls `engine::application::step_frame(deltaSeconds)` with an explicit delta; the engine loop never reads a wall clock in headless mode. One frame at `1/60` equals exactly one fixed update. | Integration assertions become exact (no interpolation ambiguity) and runs are reproducible. |
| D6 | **Every fixture owns a `memory_guard` and a `log_capture`**. At fixture destruction the test fails if live allocation count differs from the baseline or if an `ERROR` was logged without `allow_errors()`. | Leak detection that works on every platform and compiler, and "no error logged" as a universal, free assertion. |
| D7 | **Allocation-free steady state is a tested property**: `allocation_scope` counts heap allocations in a scope; hot-path unit tests and every benchmark carry an exact allocation gate. | Allocation count is the cheapest, most deterministic proxy for frame-time regressions and is stable on noisy CI where timing is not. |
| D8 | **Machine-checked completeness** replaces checklists: (a) every registered, non-internal component type appears in `feature_test`; (b) every native binding function has a non-zero call counter after `feature_test` runs 600 frames; (c) every registered editor command has an apply/undo/redo fixture file; (d) every `check_registry` check reported by `feature_test` scripts passed. All four are CI-failing tests. | "A scene that tests every feature and the entire scripting API" is a hard customer requirement. Checklists drift in a week; registry assertions do not. |
| D9 | **`feature_test` scene is generated by replaying an AI-protocol command file** (`samples/feature_test/commands/build_feature_test.json`). The command file is the authored source; the committed `.scene.json` is regenerated from it and a test asserts the two hash-equal. Stress groups (1,000 particles, 500 sprites) are spawned at runtime by scripts from prefabs, so the command file stays small. | Proves the protocol can author every component type, and keeps sample and protocol in lock step. |
| D10 | **Benchmarks** are a separate executable `conjure_benchmarks` on an in-house harness (fixed warm-up + fixed measured iterations, min/p50/p95/p99, exact allocation count). Thresholds live in `tests/benchmarks/thresholds.json` with two machine classes: `desktop` (the real target; part of the definition of done, run locally) and `ci` (3x `desktop`; nightly gate on GitHub runners). Allocation gates are identical in both classes. | doctest is fixed; a second framework for ~15 scenarios is not worth a dependency. Fixed iterations are reproducible; time-budgeted loops are not. Two classes keep thresholds honest on 4-vCPU shared runners. |
| D11 | **Golden-image tests** exist, are **off by default** (`CONJURE_TEST_GOLDEN_IMAGES=OFF`), run nightly only, render 640x360 RGBA8 through software rasterisers (D3D12 WARP on Windows = the gate; Vulkan lavapipe on Ubuntu, `continue-on-error` until stable). Per-backend references in git LFS, tolerance 3/255 per channel, 0.5 % mismatching pixels, mean absolute error 1.0; SSAO and soft shadows 8/255 and 2 %. These limits accept a small, wholly wrong region: up to 0.5 % of the pixels may differ by 255 in every colour channel (mean absolute error 0.956), about 1,150 pixels or a 33x33 block at 640x360, so reviewing the reference images in PRs (6.2) is the guard against a small missing object; a self-test pins that verdict. | No GPU in CI is a fixed constraint; software rasterisers are the only way to run the real shaders there, and they differ from each other, hence per-backend references. The mismatch fraction absorbs the scattered edge pixels on which the rasterisers differ widely; tightening the limits waits for real references to calibrate against (12). |
| D12 | **Skipped means skipped**: an unmet precondition (no .NET runtime, no software adapter, downloaded assets absent) exits with code 77 and CTest's `SKIP_RETURN_CODE 77` reports *Not Run*. CI fails on any skip whose label is not in `CONJURE_TEST_ALLOWED_SKIPS` and whose case name carries none of the tags the leg allows (`tools/ctest_check_skips.py --allow-tag`): `[window]` and `[downloaded]` on every leg, `[audio_device]` on every leg (hosted runners have no playback endpoint), `[gpu]` on Linux and macOS only (the D3D12 WARP adapter exists on Windows, so a `[gpu]` skip there is a broken set-up); a `[dotnet]` skip is never allowed. The default `ctest` run on a fresh clone without network has no downloaded-asset dependency: those tests live in their own `downloaded`-labelled executable. | Silent passes are worse than failures; a fresh clone must still be green. |
| D13 | **ASan/UBSan job builds with `CONJURE_WITH_SCRIPTING=OFF`**. Scripting is covered by the other PR jobs and the nightly hot-reload job. TSan is nightly, scripting off, on `core`, `ecs`, `scene`, `engine` and integration. | CoreCLR's own signal handling and heap are not sanitizer-clean; chasing its false positives costs more than it finds. Documented trade-off. |
| D14 | **Editor is split** into `conjure_editor_core` (commands, undo stack, protocol codec, command server, headless session; no ImGui) + `conjure_editor_ui` (panels, viewport) + the thin `conjure_editor` executable. Protocol tests run in-process against `editor_core`; one process-level test runs the real `conjure_editor --headless --exec`. | In-process tests are fast and debuggable; the process-level test proves the real entry point. |
| D15 | **CI**: PR jobs `windows-msvc-debug`, `windows-msvc-dev` (+ export smoke), `linux-gcc-debug`, `linux-clang-sanitize` (ASan+UBSan), `macos-debug`, `style-and-headers`. Nightly: `linux-clang-tsan`, `golden-d3d12-warp`, `golden-vulkan-lavapipe`, `benchmarks-ci-class`, `distribution-export`, `full-matrix-nightly`. Preset names are the ones already in `CMakePresets.json`. | Covers the fixed CI matrix (MSVC, GCC + Clang, macOS, sanitizers) in under 40 minutes per PR; expensive or noisy checks run nightly. |
| D16 | **The Distribution configuration runs the whole unit suite nightly** (code sweep F332): after its export round trip, `distribution-export` runs `ctest --preset <dist-preset> --output-junit junit_dist.xml` on the Distribution tree it has already built (the preset keeps the pull-request label filter; the `process` and `runtime` labels run in their own step before the export), then `tools/ctest_check_skips.py` with no label allowed and the tags `[window]`, `[audio_device]` and `[downloaded]` (the job fetches no downloaded assets), plus `[gpu]` on Linux. The step depends on the Distribution build alone, so the suite and the export round trip report independently, and the skip check runs whether the suite passed or failed (ctest writes the report in both cases), so a red night reports its unexpected skips in the same run. No pull-request leg builds Distribution. | Asserts and TRACE/DEBUG logging are compiled out in this configuration only, so code that leans on an assert's side effect, and a test that passes only with asserts, is caught nowhere else. The test executables are part of the build the job already does, so the cost is run time, and a red nightly gates no pull request. |
| D17 | **Formatting gates** (code sweep F784): C++ layout is enforced by `tools/check_style.py` alone. No workflow runs clang-format; `.clang-format` is an optional local aid that approximates `docs/CODE_STYLE.cpp`, and the style guide wins where the two differ. C# layout is enforced by `dotnet format --verify-no-changes` over `Conjure.Managed`, `Conjure.Bootstrap` and `Conjure.TestScripts`, a blocking step of `style-and-headers`; its rules are the `[*.cs]` section of `.editorconfig` (`csharp_indent_case_contents_when_block = false`: the braces of a case body sit under the label, its statements one level in). The step is the last of the job, runs unless the run was cancelled, and lets all three projects report before it fails. Its formatter is the one of the SDK the job's `actions/setup-dotnet` step installed: when the checkout has no `global.json`, the step writes one naming that version (the step's `dotnet-version` output) with `rollForward` `disable`, and it prints `dotnet --version`; a `global.json` already in the checkout is left alone and selects the SDK (the repository tracks none yet; code sweep F778 decided one). | clang-format cannot express the two-space continuation indent of wrapped argument lists, so a clang-format gate would fight the style guide, and an advisory step that is always red is noise. The C# formatter and the managed sources agree, so that step can block; three projects because the formatter only reads the files of the project it is given. Without a `global.json`, `dotnet` runs the newest SDK on the runner, and the image carries newer ones than the job installs, so the gate would be judged by a formatter nobody chose; last and run after a failed step so that it and the header check cannot hide each other. |

---

## 2. Data model

### 2.1 Repository layout owned by this area

```
cmake/ConjureTesting.cmake      conjure_add_test_module() (moved from ConjureModule.cmake),
                                conjure_add_benchmark_sources(), conjure_add_golden_tests(), conjure_header_check()
cmake/ConjureSanitizers.cmake   gains CONJURE_ENABLE_TSAN; CMakePresets.json gains linux-clang-tsan + filters
tests/support/include/conjure/testing/
                                core lib:   doctest.h test_environment.h approx.h log_capture.h assert_capture.h
                                            memory_guard.h allocation_scope.h temp_directory.h test_assets.h
                                engine lib: headless_engine.h input_script.h world_builder.h protocol_client.h
                                            golden_image.h
tests/support/{src,tests}/      test_main.cpp, tracked_global_new.cpp, one .cpp per header; self-tests (sec. 11)
tests/assets/                   tiny in-repo fixtures: primitives .gltf, two_materials.glb, malformed.gltf, 8x8 PBR
                                textures, 16x8 sky.hdr, sprites.png, beep.wav, tone.ogg, scenes/, prefabs/,
                                projects/minimal/, commands/, protocol_corpus/
tests/<module>/test_<header>.cpp   one test file per public header (section 5)
tests/modules/sample_plugin/    shared library built by the test tree with conjure_module_entry
tests/scripting/managed/Conjure.TestScripts/   C# scripts for scripting, bindings and integration tests
tests/{integration,downloaded}/ conjure_integration_tests (label integration), conjure_downloaded_tests (downloaded)
tests/golden/                   conjure_golden_tests (label golden; option-gated); scenes/, reference/ (git LFS)
tests/benchmarks/               conjure_benchmarks, include/conjure/bench/benchmark.h, thresholds.json, bench_*.cpp
tools/                          test_assets.json, test_assets.lock, download_test_assets.py, check_style.py, ctest_check_skips.py,
                                bench_compare.py;  .github/workflows/: ci.yml, nightly.yml
```

### 2.2 Core support types (`conjure_test_support`)

```cpp
namespace conjure::testing {

    // Exit code understood by CTest's SKIP_RETURN_CODE (D12). Same convention as Autotools and Meson.
    constexpr int SKIP_EXIT_CODE = 77;

    enum class precondition : uint8_t {
        DOTNET_RUNTIME,                  // scripting::host::is_runtime_available()
        DOWNLOADED_ASSETS,               // tests/.assets_ready stamp present
        SOFTWARE_GPU_ADAPTER,            // rhi can create a device with preferSoftwareAdapter
        LOOPBACK_TCP,                    // a 127.0.0.1 socket can be bound
        REAL_WINDOW                      // CONJURE_TEST_WINDOW=1 set by a developer; never in CI
    };

    struct test_environment {
        uint64_t seed = 0x5EED;                        // CONJURE_TEST_SEED / --conjure-seed
        uint32_t jobThreadCount = 2;                   // CONJURE_TEST_THREADS or clamp(hw, 2, 8)
        bool isolatedProcess = false;                  // --conjure-isolated (set by CTest discovery)
        bool goldenEnabled = false;                    // CONJURE_TEST_GOLDEN_IMAGES compile option
        bool updateGoldens = false;                    // --conjure-update-golden
        std::filesystem::path repoAssetRoot = {};      // <repo>/tests/assets (compile definition)
        std::filesystem::path downloadedAssetRoot = {};// <repo>/tests/.downloaded (empty if absent)
        std::filesystem::path outputRoot = {};         // <build>/test_output/<executable>
    };
}
```

Small value structs live next to their owners: `captured_log_entry { level, category, text }`,
`asserted_failure { expression, message, file, line }`, `memory_snapshot { allocationCount, freeCount,
allocatedBytes }` (cumulative; `core::memory::memory_snapshot`). Counters come from `core::memory::counters()`
(the `allocation_counters` struct of three `std::atomic<u64>`, copied by `core::memory::snapshot()`), bumped by
`tracked_global_new.cpp`, by engine allocators' block acquisition,
and by the Jolt hooks the physics module installs (`JPH::Allocate`, `JPH::Reallocate`, `JPH::Free`,
`JPH::AlignedAllocate`, `JPH::AlignedFree`). Live bytes are not tracked (sized `operator delete` is
not guaranteed); counts are what the gates need.

### 2.3 Engine support types (`conjure_test_support_engine`)

```cpp
namespace conjure::testing {

    struct headless_engine_options {
        uint32_t jobThreadCount = UINT32_MAX;          // UINT32_MAX = test_environment value
        double fixedDeltaSeconds = 1.0 / 60.0;
        bool enableRenderer = true;                    // renderer runs on rhi::null_device
        bool enablePhysics = true;
        bool enableAudio = true;                       // audio::null_backend
        bool enableScripting = false;                  // starts the .NET host; opt in per case
        bool allowErrors = false;                      // forwarded to log_capture
        std::filesystem::path projectDirectory = {};   // empty = in-memory project mounting tests/assets
    };
}
```

`hierarchy_shape { rootCount = 100, depth = 3, childrenPerNode = 4, totalEntityCount = 10000 }` drives
`spawn_hierarchy` (the builder pads or trims the last level). `golden_options { uint8_t
channelTolerance = 3; float maxMismatchFraction = 0.005f; float maxMeanAbsoluteError = 1.0f; uint32_t
width = 640, height = 360, settleFrames = 3 }` and `golden_result { passed, referenceMissing,
mismatchPixelCount, mismatchFraction, meanAbsoluteError, actualPath, diffPath }` belong to
`golden_image::compare`.

### 2.4 Benchmark types (`conjure::bench`)

`enum class machine_class : uint8_t { DESKTOP, CI }`. `bench_threshold { double p50Milliseconds
(required), p95Milliseconds, p99Milliseconds (0 = not checked); uint64_t maxAllocationsPerIteration
(exact, same in both classes) }`. `bench_options { warmupIterations = 20, measuredIterations = 200,
entityCount = 10000, machineClass, enforceThresholds, nameFilter = "*", thresholdsFile, outputFile }`
mirrors the command line. `bench_result { name, iterations, jobThreadCount, itemsPerIteration,
min/p50/p95/p99 ms, allocationsPerIteration (max over iterations), threshold, passed }` is one report
row. `thresholds.json`: `{ "version": 1, "scenarios": { "<module>.<scenario>": { "desktop": { "p50_ms":
0.40, "p99_ms": 0.0 }, "ci": { "p50_ms": 1.20 }, "allocs": 0 } } }`.

### 2.5 Check registry (engine module, `conjure/engine/diagnostics/check_registry.h`)

`check_record { std::string name; bool passed; std::string detail; uint64_t frame; }` stored by a
mutex-protected `check_registry` with `report()`, `records()`, `all_passed()`, `failed_count()`.
Exposed to C# as `Conjure.Diagnostics.Check(string name, bool passed, string detail = "")`. This is
what makes `feature_test` self-verifying (D8d).

---

## 3. Algorithms and flow

### 3.1 Test executable start-up (`test_main.cpp`)

1. Strip `--conjure-*` arguments (`--conjure-isolated`, `--conjure-update-golden`,
   `--conjure-threads=N`, `--conjure-seed=N`) into `test_environment`; everything else goes to
   `doctest::Context::applyCommandLine`. Environment variables are the fallback for the same values.
2. Install `log_capture`'s global sink as the only sink (console sink removed) so passing tests print
   nothing; it keeps a 4,096-entry ring buffer that a `doctest::IReporter` listener
   (`REGISTER_LISTENER("conjure", 1, conjure_listener)`) dumps in `test_case_end` on failure.
3. Install the assert handler via `core::set_assert_handler()`: it records an `asserted_failure` and
   returns `core::assert_action::CONTINUE` inside an active `expect_assert` scope on the current
   thread; otherwise it fails the current case and aborts the process.
4. Seed `core::random::default_engine()` with `seed`; print `seed=<n> threads=<n>`; `context.run()`.
   The exit code is doctest's unless a precondition exit happened (3.2).

### 3.2 Preconditions and skipping (D12)

`CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);` is the first statement of any case with a
precondition. If it holds, no-op. If not: in an isolated process (`isolatedProcess`, always under
CTest) it prints a one-line reason and calls `std::exit(SKIP_EXIT_CODE)`; in a direct developer run
it emits `MESSAGE("skipped: ...")` and returns from the case. CTest marks the entry *Not Run*.
`tools/ctest_check_skips.py` reads the JUnit reports the job's `ctest` runs wrote (`--output-junit`, one per
run) and the labels from `ctest --show-only=json-v1`, and fails unless every skipped entry's label is in
`CONJURE_TEST_ALLOWED_SKIPS` (CI: `golden` on PR jobs, empty nightly) or its name carries a tag the leg
allows (D12).

### 3.3 Headless frame stepping (D5)

```
headless_engine::step_frames(count, delta):
    for frame in 0..count:
        mPlatform->inject(mInputScript.events_for(mFrameIndex))   // no OS calls
        mApplication->step_frame(delta)   // accumulate -> fixed updates (physics, FixedUpdate scripts)
                                          // -> variable update (systems, script batch) -> render extract
                                          // -> render graph on the null device -> audio sync
        ++mFrameIndex
```

`delta` defaults to `fixedDeltaSeconds`, so one frame is one fixed update; `step_seconds(t)` is
`step_frames(round(t / fixedDeltaSeconds))`. No render thread exists in any mode (`06_engine_loop.md` D1; R42). The destructor destroys the application, then
`log_capture` and `memory_guard` run their checks against the baseline taken after construction.

### 3.4 World hash (`ecs::world_hash`, provided by area A, used everywhere here)

Archetypes sorted by component-set hash; within an archetype, entities in chunk order; per entity
FNV-1a 64 over the entity id then each component's bytes via the reflection registry's `hash_bytes`
(trivially copyable) or canonical `serialize`, combined with the component type hash. Floats hash as
raw bits: determinism tests demand bit-identical state. An `ASSET_REF` field hashes its leading uuid
only, never the bound slot pointer (`17_demo_games.md` P1). Allocation-free, ~1 ms for 10,000 entities.

### 3.5 Golden compare

1. `golden_image::render_offscreen(scenePath, options)` creates an `rhi::device` with
   `rhi::device_options::preferSoftwareAdapter = true` (D3D12: `IDXGIFactory4::EnumWarpAdapter`;
   Vulkan: the ICD selected by `VK_DRIVER_FILES`, lavapipe in CI), runs the engine in deterministic
   render mode (fixed exposure, constant SSAO noise seed, no temporal passes) for `settleFrames`, then
   reads back the tonemapped RGBA8 target.
2. Load `reference/<scene>/<backend>.png` via `core::image`. Missing reference: failure, unless
   `updateGoldens`, in which case the actual image becomes the reference and the case passes with WARN.
   Dimension mismatch is an immediate failure; no resizing, ever.
3. One pass computes per-pixel max channel delta, mismatch count and mean absolute error. Pass iff
   `mismatchFraction <= maxMismatchFraction` and `meanAbsoluteError <= maxMeanAbsoluteError`. A
   mismatching pixel counts once however far off it is, so with the defaults up to 0.5 % of the pixels
   may be wholly wrong and still pass (D11: a 33x33 block at 640x360), and reviewing the reference
   images in PRs (6.2) is the guard. On failure write `actual.png` and `diff.png` (delta x 8, clamped)
   into `outputRoot/golden/<scene>/<backend>/`; CI uploads `test_output/` as an artefact.

### 3.6 Benchmark run

```
for each registered scenario matching --filter:
    context = bench_context(options); scenario.function(context)   // fixture built untimed inside
measure(body):
    repeat warmupIterations: body()
    repeat measuredIterations:
        snapshot = allocation_counters(); t0 = steady_clock::now(); body(); t1 = steady_clock::now()
        samples.push(ms(t1 - t0)); allocs = max(allocs, allocation_counters() - snapshot)
sort samples; p = samples[floor(q * (n - 1))] for q in {0.50, 0.95, 0.99}
threshold = thresholds[name][machineClass]; passed = every set metric <= limit and allocs <= maxAllocs
```

A scenario without a threshold entry is reported `UNTHRESHOLDED` and fails under `--enforce`; exit
code 1 iff `--enforce` and any scenario failed. p50 (and p99 where set) rather than min or mean: p50
is robust to scheduler noise without hiding sustained regressions; p99 is the "no frame loss" claim.

### 3.7 Command catalogue completeness (D8c)

`tests/editor/test_command_catalogue.cpp` iterates `editor::command_registry::descriptors()` (R15). For each
command `<name>` it loads `tests/editor/data/commands/<name>.json`
(`{ "setup": [...], "request": {...}, "expect": {...} }`), applies `setup`, records `world_hash` H0,
applies `request`, checks `expect` as a JSON subset match on `scene.query`, undoes and asserts
`world_hash == H0`, redoes and asserts the post-request hash. A final case asserts the set of fixture
files equals the set of registered command names, both directions.

### 3.8 CMake integration (`cmake/ConjureTesting.cmake`)

```cmake
conjure_add_test_module(<module> SOURCES <files> [DEPS <targets>] [ENGINE] [LINK_ENGINE_SHARED]
                        [LABELS <labels>] [TIMEOUT <s>])
# -> conjure_<module>_tests from SOURCES + test_main.cpp + tracked_global_new.cpp, linking conjure::<module>,
#    conjure::test_support (+ conjure::test_support_engine with ENGINE), conjure::platform_defines,
#    conjure::warnings, conjure::sanitizers. Defs CONJURE_TEST_REPO_ASSET_ROOT, CONJURE_TEST_OUTPUT_ROOT.
#    LINK_ENGINE_SHARED links conjure::shared (the umbrella shared library of 07_modules.md D1) instead of the
#    static conjure::<module>; used by modules, engine, editor and integration tests (R16/R17).
#    doctest_discover_tests(<target> TEST_PREFIX "<module>." EXTRA_ARGS --conjure-isolated
#        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/bin
#        PROPERTIES LABELS "unit;<module>;<labels>" TIMEOUT <s, default 120> SKIP_RETURN_CODE 77)
conjure_add_golden_tests(SOURCES ...)                # conjure_golden_tests, label golden; no-op unless option ON
conjure_add_benchmark_sources(SOURCES ... DEPS ...)  # appends to conjure_benchmarks; one add_test, label benchmark
conjure_header_check(<module>)                       # one TU per public header, compiled with the module's flags
```

Options: `CONJURE_TEST_GOLDEN_IMAGES` (OFF), `CONJURE_ENABLE_TSAN` (OFF, exclusive with ASan).
`CMakePresets.json`: the hidden `base-test` preset gains
`"filter": { "exclude": { "label": "benchmark|golden|downloaded|slow|process" } }`; a hidden
`nightly-test` excludes only `benchmark`; `<preset>-golden` presets include label `golden`.
`third_party/CMakeLists.txt` includes `${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake`.

---

## 4. Public API

All signatures follow `docs/CODE_STYLE.cpp`. Key members only; one `.cpp` per header. Classes shown
with only a `public:` section, or described in prose, keep their private state in the `.h` as usual.

### 4.1 `conjure/testing/doctest.h` and `approx.h`

```cpp
#pragma once

// The only place doctest is included. Tests never include <doctest/doctest.h> directly.
#define DOCTEST_CONFIG_SUPER_FAST_ASSERTS
#define DOCTEST_CONFIG_USE_STD_HEADERS
#include "conjure/core/result.h"
#include <doctest/doctest.h>

// Result helpers. CONJURE_REQUIRE_OK stops the case; CONJURE_CHECK_OK continues;
// CONJURE_CHECK_ERROR(expression, errorCode) checks for one specific error code.
#define CONJURE_REQUIRE_OK(expression) \
    REQUIRE_MESSAGE((expression).has_value(), conjure::testing::describe_error((expression)))
#define CONJURE_CHECK_OK(expression) \
    CHECK_MESSAGE((expression).has_value(), conjure::testing::describe_error((expression)))

// Precondition guard; see section 3.2.
#define CONJURE_TEST_REQUIRE(preconditionValue) \
    if (!conjure::testing::require_precondition((preconditionValue), __FILE__, __LINE__)) { return; }

namespace conjure::testing {

    // True when the precondition holds; otherwise exits 77 in an isolated process or returns false.
    [[nodiscard]] bool require_precondition(precondition value, const char *file, uint32_t line);

    [[nodiscard]] const test_environment &environment() noexcept;

    constexpr float EPSILON = 1.0e-5f;

    [[nodiscard]] bool approx_equal(const math::vec3 &left, const math::vec3 &right, float epsilon = EPSILON) noexcept;
    [[nodiscard]] bool approx_equal(const math::quat &left, const math::quat &right, float epsilon = EPSILON) noexcept;
    [[nodiscard]] bool approx_equal(const math::mat4 &left, const math::mat4 &right, float epsilon = EPSILON) noexcept;
    [[nodiscard]] bool approx_equal(const math::aabb &left, const math::aabb &right, float epsilon = EPSILON) noexcept;
}

#define CONJURE_CHECK_APPROX(actual, expected) CHECK(conjure::testing::approx_equal((actual), (expected)))

// doctest::StringMaker specialisations for math::vec3, math::quat, core::uuid, core::error and
// ecs::entity so failed CHECKs print readable values. Quaternions q and -q compare equal.
```

### 4.2 `conjure/testing/log_capture.h` and `assert_capture.h`

```cpp
namespace conjure::testing {

    class log_capture {
        private:
            std::vector<captured_log_entry> mEntries = {};
            std::array<uint32_t, 5> mCountsPerLevel = {};
            mutable std::mutex mMutex = {};              // job threads log too
            std::shared_ptr<core::log_sink> mSink = nullptr;   // log::add_sink / log::remove_sink
            bool mErrorsAllowed = false;

        public:
            log_capture();
            ~log_capture();                              // CHECKs that no ERROR was logged unless allowed

        public:
            [[nodiscard]] bool contains(core::log_level level, std::string_view substring) const;
            [[nodiscard]] uint32_t count(core::log_level level) const noexcept;
            [[nodiscard]] std::vector<captured_log_entry> entries() const;
            void allow_errors() noexcept;                // suppresses the destructor check; then assert contains()
            void clear();
    };

    // Turns a fatal CONJURE_ASSERT into a recorded failure; never unwinds. Code under test must early-out.
    class expect_assert {
        public:
            expect_assert();
            ~expect_assert();                            // restores the previous handler

        public:
            [[nodiscard]] uint32_t count() const noexcept;
            [[nodiscard]] std::span<const asserted_failure> failures() const noexcept;
    };
}

// Expands to nothing in Distribution, where asserts are compiled out, so the test still compiles.
#define CONJURE_CHECK_ASSERTS(statement) \
    do { conjure::testing::expect_assert guard = {}; statement; CHECK(guard.count() == 1); } while (false)
```

### 4.3 `conjure/testing/memory_guard.h`, `allocation_scope.h`, `temp_directory.h`, `test_assets.h`

- `class memory_guard` (members `memory_snapshot mBaseline`, `bool mChecked`): constructor snapshots;
  `void check()` CHECKs that allocations minus frees since the baseline is zero; `void reset_baseline()
  noexcept`; the destructor calls `check()` if it has not been called.
- `class allocation_scope`: `[[nodiscard]] uint64_t allocation_count() const noexcept`,
  `[[nodiscard]] uint64_t allocated_bytes() const noexcept`, `void reset() noexcept`.
- `class temp_directory`: constructor creates `<build>/test_tmp/<uuid>`, destructor removes it
  recursively (WARN on failure); `[[nodiscard]] const std::filesystem::path &path() const noexcept`,
  `[[nodiscard]] std::filesystem::path file(std::string_view name) const`,
  `[[nodiscard]] core::result<void> write_text(std::string_view name, std::string_view text) const`.
- Free functions `[[nodiscard]] std::filesystem::path repo_asset(std::string_view relativePath)` and
  `downloaded_asset(std::string_view relativePath)` resolve under `tests/assets` and `tests/.downloaded`.

```cpp
#define CONJURE_CHECK_NO_ALLOCATIONS(statement) \
    do { conjure::testing::allocation_scope scope = {}; statement; CHECK(scope.allocation_count() == 0); } while (false)
```

### 4.4 `conjure/testing/headless_engine.h` (engine library)

```cpp
namespace conjure::testing {

    class headless_engine {
        private:
            headless_engine_options mOptions = {};
            log_capture mLogCapture = {};
            memory_guard mMemoryGuard = {};
            std::unique_ptr<engine::application> mApplication = nullptr;
            platform::null_backend *mPlatform = nullptr;        // owned by the application
            rhi::null_device *mRhiDevice = nullptr;
            audio::null_backend *mAudioBackend = nullptr;
            input_script mInputScript = {};
            uint64_t mFrameIndex = 0;

        public:
            headless_engine();
            explicit headless_engine(const headless_engine_options &options);
            ~headless_engine();

        public:
            // load_scene takes a project-relative path or a uuid; both loaders run zero frames.
            [[nodiscard]] core::result<scene::scene_handle> load_scene(std::string_view pathOrUuid);
            [[nodiscard]] core::result<scene::scene_handle> load_scene_json(std::string_view sceneJson);
            void step_frames(uint32_t frameCount);
            void step_frames(uint32_t frameCount, double deltaSeconds);
            void step_seconds(double seconds);

            [[nodiscard]] engine::application &application() noexcept;
            [[nodiscard]] ecs::world &world() noexcept;
            [[nodiscard]] scene::scene_manager &scenes() noexcept;
            [[nodiscard]] input_script &input() noexcept;
            [[nodiscard]] const rhi::null_device_stats &rhi_stats() const noexcept;
            [[nodiscard]] const audio::null_backend_stats &audio_stats() const noexcept;
            [[nodiscard]] const engine::diagnostics::check_registry &checks() const noexcept;
            [[nodiscard]] log_capture &logs() noexcept;
            [[nodiscard]] uint64_t frame_index() const noexcept;
            [[nodiscard]] uint64_t fixed_update_count() const noexcept;
            [[nodiscard]] uint64_t world_hash() const;
            [[nodiscard]] ecs::entity find_entity(std::string_view name) const;
    };
}
```

### 4.5 `conjure/testing/input_script.h` and `world_builder.h`

```cpp
namespace conjure::testing {

    class input_script {
        public:
            input_script();
            ~input_script();

        public:
            void key(uint64_t frame, platform::key key, platform::button_action action);
            void mouse_move(uint64_t frame, float x, float y);
            void mouse_button(uint64_t frame, platform::mouse_button button, platform::button_action action);
            void gamepad_button(uint64_t frame, uint32_t padIndex, platform::gamepad_button button);
            void gamepad_axis(uint64_t frame, uint32_t padIndex, platform::gamepad_axis axis, float value);
            [[nodiscard]] std::span<const platform::input_event> events_for(uint64_t frame) noexcept;
    };

    /// <summary>
    /// Spawns count entities in one archetype through the command buffer with one flush.
    /// </summary>
    template <typename... ComponentTypes>
    [[nodiscard]] std::vector<ecs::entity> spawn_many(
      ecs::world &world,
      uint32_t count,
      const ComponentTypes &...prototypes
    );
}
```

`world_builder.h` also declares `[[nodiscard]] std::vector<ecs::entity> spawn_hierarchy(scene::scene
&scene, const hierarchy_shape &shape)`, `[[nodiscard]] ecs::entity spawn_box_body(scene::scene &scene,
const math::vec3 &position, physics::motion_type motion)`, `[[nodiscard]] ecs::entity
spawn_script_entity(scene::scene &scene, std::string_view name, std::string_view scriptType)` and
`void mark_dirty_fraction(scene::scene &scene, std::span<const ecs::entity> entities, float fraction,
uint64_t seed)` (deterministic for a seed).

### 4.6 `conjure/testing/protocol_client.h` and `golden_image.h`

```cpp
namespace conjure::testing {

    class protocol_client {
        public:
            explicit protocol_client(std::unique_ptr<editor::protocol_transport> transport);
            ~protocol_client();

        public:
            [[nodiscard]] static core::result<protocol_client> connect_in_process(editor::protocol_server &server);   // loopback_transport
            [[nodiscard]] static core::result<protocol_client> connect_tcp(uint16_t loopbackPort);

            // Blocks until the response for this request arrives; streamed events are buffered.
            [[nodiscard]] core::result<core::json::document> send(
              std::string_view command,
              const core::json::document &params
            );
            [[nodiscard]] core::result<core::json::document> send_raw(std::string_view rawMessage);
            [[nodiscard]] std::span<const core::json::document> events() const noexcept;
            void clear_events();
    };
}
```

`class golden_image` has two static functions: `[[nodiscard]] static core::result<core::image>
render_offscreen(const std::filesystem::path &scenePath, const golden_options &options)` and
`[[nodiscard]] static golden_result compare(std::string_view sceneName, const core::image_view &actual,
const golden_options &options)`; both follow 3.5.

### 4.7 `conjure/bench/benchmark.h`

`class bench_context` (`explicit bench_context(const bench_options &options)`): `void
set_items(uint64_t itemsPerIteration) noexcept`, `[[nodiscard]] uint32_t entity_count() const noexcept`,
`template <typename Body> void measure(Body &&body)` (warm-up untimed, then measured iterations timed
with `steady_clock` and allocation counting), `[[nodiscard]] std::span<const double>
samples_milliseconds() const noexcept`, `[[nodiscard]] uint64_t max_allocations_per_iteration() const
noexcept`. `using bench_function = void (*)(bench_context &context);`. `CONJURE_BENCHMARK(name)`
defines a static `bench_function` and registers a `bench_descriptor { std::string_view name;
bench_function function; }` with the process-wide `bench_registry` (`add`, `descriptors`).
`bench_runner` (`load_thresholds`, `run`, `run_all`, `write_json`, static `summarise(name, samples,
maxAllocations, threshold)`) implements 3.6. Command line:
`conjure_benchmarks [--class desktop|ci] [--enforce] [--filter <glob>] [--out <file>] [--threads N]`.

---

## 5. Doctest conventions and per-module coverage

### 5.1 Conventions (add to CONVENTIONS section 6)

- `tests/<module>/test_<name>.cpp` mirrors `engine/include/conjure/<module>/<name>.h` one-to-one (a
  header without a test file fails `check_style.py --tests-mirror-headers`); each file opens with
  `#include "conjure/testing/doctest.h"` and `TEST_SUITE("<module>/<name>")`.
- `TEST_CASE("<type> - <behaviour in plain English>")`, unique within the executable (D1 needs it),
  e.g. `"slot_map - erase invalidates the handle generation"`. Variants are `SUBCASE`s, never copies.
  Loops use `CAPTURE(index)`.
- Name tags, filterable with `-tce` when running an executable directly: `[slow]` (> 1 s),
  `[parallel]` (asserts on multi-worker behaviour), `[scripting]`, `[window]` (needs a real GLFW
  window; guarded by `CONJURE_TEST_REQUIRE(precondition::REAL_WINDOW)`).
- `REQUIRE` only when continuing would dereference an invalid result; `CHECK` otherwise. Floats use
  `doctest::Approx` or `CONJURE_CHECK_APPROX`. Every fixture is a local in the case body
  (`TEST_CASE_FIXTURE` allowed; fixture inheritance not).
- Forbidden in tests: `sleep_for`, wall-clock reads, `std::thread`, randomness not derived from
  `environment().seed`, file I/O outside `temp_directory`, third-party headers (test through the
  public API), absolute paths, reading environment variables (only `test_main` does).
- `* doctest::skip()` requires a comment with the tracking issue; `[scripting]` cases carry
  `* doctest::timeout(60)`.
- Every public type: construction/destruction, happy path, failure path (each `error_code` a
  `result` function can return), edge cases (empty, max, invalid or stale handle). Every bug fix adds
  `TEST_CASE("regression - <issue id> <summary>")`.

### 5.2 Required coverage per module (file names are mandatory; cases are the minimum)

| Module | Files in `tests/<module>/` | Must assert (beyond the universal four) |
|---|---|---|
| core | `test_result`, `test_assert`, `test_log`, `test_uuid`, `test_string_utils`, `test_hash`, `test_string_id`, `test_clock`, `test_arena`, `test_pool`, `test_frame_allocator`, `test_slot_map`, `test_stable_vector`, `test_small_vector`, `test_jobs` `[parallel]`, `test_parallel_for` `[parallel]`, `test_delegate`, `test_event_bus`, `test_vfs`, `test_json`, `test_reflection`, `test_math`, `test_memory` | uuid 1M unique + round-trip; `string_id` collision-free over the test corpus; slot_map stale handle rejected, iteration stable under erase; allocators: alignment, reset, overflow -> error, live count returns to baseline; jobs: 10,000 tiny jobs all run exactly once, nested jobs, shutdown with queued jobs, `parallel_for` with 0 and 8 workers gives identical output; json round-trips every value type, malformed input -> error never assert, canonical output sorted and stable; reflection enumerates fields with offsets; math: transform compose/decompose, frustum/aabb, ray. Allocation-free: `parallel_for` dispatch, `slot_map` lookup, `string_id` compare. |
| ecs | `test_entity`, `test_world`, `test_archetype`, `test_component_registry`, `test_query`, `test_command_buffer`, `test_scheduler` `[parallel]`, `test_world_hash` | generation increments on destroy and wraps safely; add/remove moves between archetypes preserving bytes; query over 10,000 visits each once; with/without/changed filters; structural changes during iteration deferred and applied in order; disjoint-write systems run concurrently and give the same `world_hash` as inline over 100 runs; duplicate registration -> error; `world_hash` independent of archetype creation order, one float bit flips it. Allocation-free: query iteration, `get<T>`, command buffer record within capacity. |
| platform | `test_null_backend`, `test_input_state`, `test_window_events`, `test_backend_selection`, `test_backend_conformance` | injected key is `pressed` for exactly one frame then `held`; resize delivered; unknown backend name -> error; `TEST_CASE_TEMPLATE` conformance over backend factories (null always, glfw `[window]`). |
| rhi | `test_null_device`, `test_shader_library`, `test_resource_helpers`, `test_swapchain_null`, `test_backend_selection` | null device counts buffer/texture creations, command list submissions, draws, dispatches and records pass names; shader library reports missing permutation as error. |
| assets | `test_asset_database`, `test_meta_files`, `test_asset_ref`, `test_async_loading`, `test_importer_registry`, `test_file_watcher`, `test_asset_pack` | uuid stable across re-scan; meta create/upgrade; missing file -> error state not crash; async load completes within N job pumps; null watcher with injected events; pack cook + read-back byte-identical. |
| audio | `test_null_backend`, `test_clip`, `test_source_listener`, `test_mix_groups`, `test_spatialisation` | WAV decode from clips generated in-test (`tests/audio/wav_builder.h`); `.ogg/.mp3/.flac` decode from `tests/.downloaded/audio/` behind `precondition::DOWNLOADED_ASSETS` (`12_audio.md` 10, R62); play records clip and gain in stats; attenuation at 0 and max distance; group gain multiplies. |
| modules | `test_module_manifest`, `test_module_version`, `test_module_abi`, `test_module_registrar`, `test_module_resolver`, `test_module_manager_static`, `test_module_manager_shared`, `test_module_reload`, `test_dynamic_library` (names per `07_modules.md` 11) | `sample_plugin` loads, registers `plugin_counter` component + a system; ABI/version mismatch -> error; after unload no component instances remain and `memory_guard` is clean. |
| scene | `test_hierarchy`, `test_transform_propagation`, `test_name_tag_layer`, `test_prefab`, `test_scene_serialisation`, `test_scene_manager` | reparent keeps world transform, cycles rejected; dirty propagation touches only the subtree and is allocation-free at 10,000; prefab instantiate/override/nested; save -> load -> save byte-identical, unknown component -> WARN not failure; additive load keeps existing entities. |
| renderer | `test_render_graph`, `test_render_scene`, `test_mesh_asset`, `test_material`, `test_gltf_import`, `test_camera`, `test_shadows`, `test_light_culling`, `test_ibl`, `test_ssao`, `test_tonemap`, `test_sprite_batch`, `test_debug_draw`, `test_imgui_backend`, `test_null_frame` | graph rejects cycles, orders passes, culls unused; extraction of 10,000 mesh renderers allocation-free with correct cull counts; `cube.gltf` -> 24 vertices / 36 indices + material, `malformed.gltf` -> error; cascade splits monotonic; IBL LUT dimensions and pass names; CPU reference ACES/Reinhard/AgX: f(0)=0, monotonic, known samples; sprite batch count; one frame on null rhi executes the expected pass list and draw count. |
| physics | `test_physics_world`, `test_rigid_body`, `test_colliders`, `test_character_controller`, `test_layers`, `test_queries`, `test_fixed_step`, `test_contact_events`, `test_component_sync` | free fall analytic after 60 steps; box rests within 1 cm; layer matrix prevents contact; raycast/shape hit distances; interpolation alpha 0.5 is the midpoint; enter/stay/exit each emitted exactly once into the `physics_world::contact_events()` span (no ECS event queue, R6/R62), ordered by body id pair; Jolt allocations visible to `allocation_scope`. |
| scripting | `test_host` `[scripting]`, `test_assembly_load`, `test_function_tables`, `test_script_component`, `test_serialised_fields`, `test_hot_reload`, `test_batched_update`, `test_managed_exceptions` | host starts/stops cleanly, missing runtime -> error; `Conjure.TestScripts` loads in a collectible ALC and unloads; every function table slot non-null; hot reload preserves a `[Serialized] int`; a throwing `OnUpdate` logs ERROR per fault, the third consecutive fault disables that callback (`allow_errors()` then assert the count), and the engine continues (`03_scripting.md` D11; R28); 10,000 scripts updated in one managed call. |
| engine | `test_application`, `test_layer_stack`, `test_engine_loop`, `test_project`, `test_export`, `test_headless_mode`, `test_component_sync`, `test_check_registry` | 600 frames at 1/60 -> 600 fixed updates; one frame at 0.1 s -> 6; time scale and pause; `project.json` round-trip; `engine::export_pipeline` into `temp_directory` produces a folder that an `application` built from `--project <dir> --headless --frames 60` boots with exit 0 (R21). |
| bindings | `test_binding_tables`, `test_entity_bindings`, `test_transform_bindings`, `test_input_bindings`, `test_physics_bindings`, `test_audio_bindings`, `test_prefab_scene_bindings`, `test_logging_bindings`, `test_diagnostics_bindings`, `test_dod_span_bindings`, `test_call_counters` | generated C# manifest equals native table (names, arity, hashes); a span over 1,000 transforms stays valid for the whole query scope because structural requests made inside the scope are deferred to `query_end` (R5), and a span escaping the scope is a compile error; every counter increments when its thunk runs. |
| editor | `test_command_protocol`, `test_command_catalogue`, `test_undo_redo`, `test_tcp_transport`, `test_stdin_transport`, `test_event_stream`, `test_headless_runner`, `test_protocol_robustness`, `test_query_schemas`, `test_selection`, `test_inspector_model`, `test_panels_headless`, `test_editor_process` | section 6.3; panels build an ImGui frame on the null device without asserting. |
| runtime | `test_runtime_boot`, `test_missing_project` | boots an exported folder headless for 60 frames, exit 0; missing `project.json` -> exit 2. |

---

## 6. Integration, golden and AI protocol tests

### 6.1 Integration (`tests/integration/`, `conjure_integration_tests`, label `integration`)

Pattern: `headless_engine` -> `load_scene` -> `step_frames(N)` -> assert state. One file per
scenario, one case per file (so one CTest entry each).

| File | Scenario and assertions |
|---|---|
| `test_headless_frames.cpp` | empty project, 600 frames: `frame_index()==600`, `fixed_update_count()==600`, 600 rhi submissions, zero ERROR. |
| `test_scene_roundtrip.cpp` | `assets/scenes/roundtrip.scene.json` load -> save -> byte-identical; programmatic scene -> save -> load -> equal `world_hash`. |
| `test_hierarchy_propagation.cpp` | depth-8 chain, move root, 1 frame: leaf world position offset exactly; 10,000-entity hierarchy frame allocation-free after frame 2. |
| `test_physics_stack.cpp` | 10 stacked boxes over a static plane, 180 frames: settled, top box y within 1 cm of analytic, sleeping bodies > 0. |
| `test_prefab_spawn_destroy.cpp` | spawn `crate.prefab.json` 100x + destroy 100x per frame for 300 frames: entity count stable, handles invalid after destroy, zero per-frame allocations after frame 10. |
| `test_scene_switch.cpp` | load A, additive B, unload A, reload A: A handles invalid, B intact, world empty after both unload, memory clean. |
| `test_module_plugin.cpp` | load `sample_plugin`, entities with `plugin_counter` update 60 frames, unload: component gone, no ERROR. |
| `test_determinism.cpp` | (a) same options, two runs of 300 frames of `assets/scenes/determinism.scene.json` (physics + scripting off) -> identical per-frame hash sequences; (b) threads 0, 2, 8 -> identical sequences with physics disabled; (c) physics enabled, same thread count, two runs -> identical. The scene spawns only from EXCLUSIVE / MAIN_THREAD systems and command buffers flushed in grain order, because entity id *values* reserved inside PARALLEL grains are not stable across worker counts (`01_ecs.md` 3.5; R41). |
| `test_script_lifecycle.cpp` `[scripting]` | `LifecycleProbe` entity, 10 frames, destroy: checks `script.on_create`, `script.update_count==10`, `script.on_destroy`. |
| `test_hot_reload.cpp` `[scripting]` | rebuild `Conjure.TestScripts` variant B into a temp dir, trigger reload: `[Serialized]` field preserved, removed type -> component disabled with WARN. |
| `test_export_roundtrip.cpp` | export in-repo `assets/projects/minimal` to `temp_directory` through `engine::export_pipeline`; an `application` built from `--project <dir> --headless --frames 60` returns 0 (R21); component counts equal the editor load. |
| `test_ai_protocol_e2e.cpp` | `assets/commands/build_grid_sweeper.json` through `editor::headless_runner::run` (R15): a minesweeper-style grid of cell prefab instances, reveal/flag scripts, timer and win/lose state script (docs/design/17_demo_games.md 2.1); saved scene reloads with equal `world_hash`. |

`tests/downloaded/` (`conjure_downloaded_tests`, label `downloaded`, every case starts with
`CONJURE_TEST_REQUIRE(precondition::DOWNLOADED_ASSETS)`):

| File | Scenario |
|---|---|
| `test_feature_test_scene.cpp` `[scripting]` | `samples/feature_test`, 600 frames with scripted input (W at frame 10, mouse delta at 20, gamepad A at 30): zero ERROR; every registered component type without `component_flags::INTERNAL` present at least once (D8a); every binding call counter > 0 except entries in `binding_exclusions.json`, each with a reason (D8b); `checks().all_passed()` with failed records printed (D8d); check count >= 120. |
| `test_feature_test_replay.cpp` | replay `commands/build_feature_test.json` in a headless editor session into a temp project; canonical serialisation equals the committed `feature_test.scene.json` byte-for-byte and `world_hash` matches (D9). |
| `test_gltf_damaged_helmet.cpp` | imports DamagedHelmet: mesh count, material slots, emissive + occlusion textures bound. |
| `test_hdri_import.cpp` | 1k HDRI -> cubemap -> irradiance + prefiltered mips: pass names and dimensions on the null device. |

### 6.2 Golden (`tests/golden/`, `conjure_golden_tests`, label `golden`, off by default)

Scenes in `tests/golden/scenes/`: `pbr_spheres`, `ibl_only`, `shadows_directional_cascades`,
`shadows_spot_point_soft`, `ssao_only` (AO debug view), `hdr_bloom_tonemap_aces`,
`tonemap_reinhard`, `tonemap_agx`, `sprites_2d_ortho`, `debug_draw`, `imgui_frame`,
`ai_screenshot` (the `view.screenshot` command output with `--offscreen`). Defaults from `golden_options`;
`ssao_only` and `shadows_*` use `channelTolerance = 8`, `maxMismatchFraction = 0.02`. References are
regenerated only with `--conjure-update-golden` on the matching backend and reviewed as PR images. That
review is the guard against a small missing or wrong object, which the limits accept (D11): up to 0.5 %
wholly wrong pixels with the defaults, and about 0.52 % in the 2 % scenes, where the unchanged mean
absolute error limit of 1.0 binds first.

### 6.3 AI protocol (`tests/editor/`, in-process via `conjure_editor_core` unless stated)

1. `test_command_protocol.cpp`: request id echoed; unknown command -> `UNKNOWN_COMMAND` (+ `didYouMean`);
   missing or wrong-typed arg -> `INVALID_ARGS` naming the path; a response never lacks `id` (names per
   `04_editor.md` 2.3, R15).
2. `test_command_catalogue.cpp`: algorithm 3.7 over every registered command (D8c); the catalogue
   itself is area D's. `test_undo_redo.cpp`: 50 seeded random commands, undo all -> initial
   `world_hash`, redo all -> final hash; stack depth limit respected.
3. `test_tcp_transport.cpp`: `protocol_server` on `127.0.0.1:0` reporting `bound_port()`; two concurrent
   clients; two messages in one packet and one message split across packets; message above
   `editor::protocol::MAX_MESSAGE_BYTES` (16 MiB) -> `MESSAGE_TOO_LARGE` response, then the client is
   disconnected (`04_editor.md` 6); client disconnect mid-request leaves the server alive with no
   transaction open. `test_stdin_transport.cpp`: `stdio_transport` over injected `std::istringstream` /
   `std::ostringstream`, same framing cases.
4. `test_event_stream.cpp`: subscribe -> `entity.create` emits `scene.changed {revision, label}` and the
   response carries the guid; `scene.loaded` and `progress` events; unsubscribe stops delivery.
5. `test_headless_runner.cpp`: `editor::headless_runner::load_command_file(file)`, then
   `run(application, options, requests)` (`--exec`); exit codes 0 all ok, 1 a command failed (NDJSON
   responses identify the failing index), 2 startup/project failure, 3 bad arguments (for a command file that
   cannot be read or parsed, `editor_application::initialise` fails with `BAD_ARGUMENTS` before the engine
   starts, `04_editor.md` D22); a startup scene that did not load is the first output line (the event
   `warning {code: "STARTUP_SCENE_FAILED", message}`); `--keep-going` runs all; `view.screenshot` on the null
   device answers `NOT_AVAILABLE` (R15).
6. `test_protocol_robustness.cpp`: fixed 60-entry corpus in `tests/assets/protocol_corpus/`
   (truncated JSON, 10,000-deep nesting, `1e400`, invalid UTF-8, null bytes, duplicate keys, 1 MiB
   string, empty line): every entry yields an error response or is ignored, never a crash or assert.
7. `test_query_schemas.cpp`: `scene.query`, `entity.get`, `reflection.components` responses validate
   against the JSON schemas in `engine/src/editor/core/protocol/schema/` (installed next to the editor
   for agents; R15).
8. `test_editor_process.cpp` (label `process`, excluded from the default preset, run in CI): spawns the
   real `conjure_editor --headless --exec tests/assets/commands/build_grid_sweeper.json --project <tmp>` via
   `core::process`, checks exit 0 and that the saved scene hash equals the in-process result.

---

## 7. Benchmarks, `feature_test`, CI and definition of done

### 7.1 Benchmark scenarios (`tests/benchmarks/bench_<module>.cpp`, Development config)

| Scenario | Setup | Iteration | desktop p50 | ci p50 | allocs |
|---|---|---|---|---|---|
| `core.job_parallel_for_1m` | 1,000,000 trivial items | `parallel_for` | 0.50 ms | 1.50 ms | 0 |
| `ecs.iterate_100k` | 100,000 {position, velocity} | one integrating system | 0.15 ms | 0.45 ms | 0 |
| `ecs.spawn_destroy_10k` | 10,000 live | spawn 1,000 + destroy 1,000 via command buffer, flush | 1.00 ms | 3.00 ms | <= 2 |
| `ecs.query_changed_10k` | 10,000, 5 % modified | changed-filter query | 0.05 ms | 0.15 ms | 0 |
| `scene.transform_propagation_10k` | 10,000, depth 3, all roots dirty | transform system | 0.40 ms | 1.20 ms | 0 |
| `scene.transform_propagation_10k_sparse` | same, 10 % dirty | same | 0.10 ms | 0.30 ms | 0 |
| `scene.load_10k` | json scene with 10,000 entities | load + unload | 250 ms | 750 ms | n/a |
| `assets.first_scan_100` | project folder on disk (`native_file_system`) with 100 sources that have no `.meta` | first `asset_database::scan()` | 1,000 ms | 3,000 ms | n/a |
| `physics.step_10k_active` | 10,000 dynamic boxes falling onto a plane | one fixed step | 8.00 ms | 24.0 ms | 0 |
| `physics.step_10k_sleeping` | same after 600 settle steps | one fixed step | 0.80 ms | 2.40 ms | 0 |
| `physics.sync_10k` | 10,000 bodies | Jolt -> transform sync | 0.30 ms | 0.90 ms | 0 |
| `renderer.extract_10k` | 10,000 mesh renderers, 50 % visible | extraction + culling | 1.00 ms | 3.00 ms | 0 |
| `scripting.update_10k` `[scripting]` | 10,000 `BenchMover.cs` | batched managed update | 2.50 ms | 7.50 ms | 0 (native) |
| `scripting.update_10k_dod` `[scripting]` | same via `Span<T>` | batched update | 0.60 ms | 1.80 ms | 0 |
| `scripting.native_call_100k` `[scripting]` | 100,000 `Entity.IsValid` | one script | 10.0 ms | 30.0 ms | 0 |
| `engine.full_frame_10k_transform_render` | headless, 10,000 moving mesh renderers | `step_frames(1)` | 4.00 ms (p99 8) | 12.0 ms | 0 |
| `engine.full_frame_10k_physics` | headless, 10,000 active bodies | `step_frames(1)` | 12.0 ms (p99 16) | 36.0 ms (p99 48) | 0 |
| `engine.full_frame_10k_scripts` `[scripting]` | headless, 10,000 scripted movers | `step_frames(1)` | 5.00 ms (p99 10) | 15.0 ms | 0 |

Budget rationale: 60 Hz is 16.67 ms. Each customer scenario (transform, physics body, script update
on 10,000 entities) fits with headroom on `desktop`; `engine.full_frame_10k_physics` is the tightest
and its p99 gate is the "no frame loss" claim. Allocation gates are steady-state (after warm-up).
`assets.first_scan_100` (code sweep F243) is a disk benchmark: the scan writes one `.meta` per source
through `write_file_atomic`, which flushes each file to the storage device (`10_core.md` C11), so its
time follows the drive, not the processor. Its budget is a regression guard of 10 ms per source, not a
measured design figure (`tests/benchmarks/README.md`).
`samples/stress_test` is the visual counterpart with the profiler overlay and is not in ctest.

### 7.2 The `feature_test` sample (`samples/feature_test/`)

Layout: `project.json`, `commands/build_feature_test.json` (source, D9), `scenes/feature_test.scene.json`
(generated), `scenes/sub_scene.scene.json`, `prefabs/{crate,coin,particle,sprite_cell}.prefab.json`,
`scripts/FeatureTest.csproj` + `*.cs`, `assets/` (in-repo primitives, `sprites.png`, `ambient.wav`,
`beep.wav`; downloaded: Khronos `DamagedHelmet`, `MetalRoughSpheres`, `TextureCoordinateTest`,
polyhaven 1k `kloofendal_48d_partly_cloudy_puresky.hdr`). The project lists `sample_plugin` in its
modules so a module-provided component is exercised. Every script reports checks through
`Conjure.Diagnostics.Check`. Entity names are exact; component names follow areas A/B/E/G.

| Group | Entity | Components | Script (API areas and checks) |
|---|---|---|---|
| Cameras | `MainCamera` | name, tag `MainCamera`, transform, camera (perspective 60 deg), audio_listener, post_process (ACES, bloom, fixed exposure, SSAO on) | `CameraOrbit`: Input keyboard/mouse/gamepad, Transform get/set, `Time.DeltaTime` |
| | `OrthoCamera2D` | transform, camera (orthographic, layer mask `UI2D`), disabled | `CameraToggle`: enable/disable, `Entity.Find` |
| Lights | `Sun`; `PointLight_Shadowed`, `PointLight_Plain`, `SpotLight_Soft`, `SpotLight_Hard`; `Sky` | directional_light (4 cascades, soft PCF); point_light / spot_light with shadow settings; sky_light (HDRI IBL asset ref) | `DayCycle` on `Sun`: native component field get/set |
| Geometry | `Helmet`; `SphereGrid` + 25 `Sphere_<m>_<r>`; `TexCoordTest`; `DeepChain_0..7` | mesh_renderer (DamagedHelmet) + material override; hierarchy + metal/rough material sweep; mesh_renderer; hierarchy depth 8 | `Spinner`; `ChainWiggle`: Parent/Children, local vs world, reparent |
| Physics | `Ground`; `Stack_0..9`; `BouncySphere`, `Capsule`, `ConvexProp`, `Terrain` | rigid_body static + box_collider + physics_material; dynamic box_collider + mesh_renderer; dynamic sphere / capsule / convex colliders, static mesh_collider | `ForceApplier` on `BouncySphere`: force, impulse, velocity |
| | `KinematicPlatform`; `Player`; `Raycaster` | rigid_body kinematic + box_collider; character_controller + capsule_collider + mesh_renderer + audio_source; transform | `PlatformMover`: kinematic move; `PlayerController`: input, move, jump, ground raycast, `OnCollisionEnter` plays `beep.wav`; `RaycastProbe`: raycast, sphere cast, overlap box, layer masks |
| | `TriggerVolume`; `LayerA_Body`, `LayerB_Body`, `LayerC_Body` | box_collider (trigger); rigid_body + sphere_collider, layers with A-B filtered | `TriggerLogger`: `OnTriggerEnter/Exit`; `LayerAsserter`: collision events, layer API |
| Audio | `MusicLoop`; `OneShotEmitter` | audio_source (loop, 3D, group `Music`); audio_source (one-shot) | `AudioParams`: volume, pitch, group gain, pause/resume; `OneShotTrigger`: Play/Stop/IsPlaying, timers |
| 2D | `SpriteGridRoot`; `AnimatedSprite` | transform, layer `UI2D`; sprite_renderer + sprite_animation | `SpriteGridSpawner`: spawns 500 `sprite_cell` prefabs; `SpriteFlipper`: sprite API, frame index |
| Prefabs | `CoinSpawner`; `NestedPrefabInstance` | transform; prefab_instance (crate nested in coin) | `CoinSpawner`: spawn 50 `coin`, parent, override field, destroy after 2 s, `IsValid` |
| Stress | `ParticleRoot`; `Churner` | transform | `ParticleSpawner`: 1,000 `particle` prefabs + `DodSpanMover`: `Span<TransformData>` moves all per frame; `Churner`: spawn 100 + destroy 100 entities per frame |
| Module | `PluginUser` | plugin_counter (from `sample_plugin`) | `PluginProbe`: reads the module component through generic `GetComponent` |
| Scripting | `FieldZoo` | script | `FieldZoo`: every serialised field type (bool, int, float, string, Vector2/3/4, Quaternion, Color, Entity, AssetRef<T>, enum, List<T>) round-trip |
| | `LifecycleProbe`; `ComponentAccessor`; `QueryUser` | script; transform; transform | `LifecycleProbe`: OnCreate/OnEnable/OnStart/OnUpdate/OnFixedUpdate/OnLateUpdate/OnDisable/OnDestroy order; `ComponentApi`: Has/Get/Add/Remove native and script components; `QueryApi`: `World.Query<Transform, Tag>()`, count by tag, find by name |
| | `SceneLoader`; `Logger`; `TimeProbe` | script | `SceneLoader`: additive load/unload of `sub_scene`, async load, `IsLoaded`; `LogProbe`: Trace/Debug/Info/Warn; `TimeProbe`: DeltaTime, FixedDeltaTime, FrameCount, Elapsed, TimeScale |
| | `EventBus`; `DebugDrawer`; `HotReloadProbe`; `ChecklistReporter` | script; transform; script; script | `EventBus`: custom events send/receive; `DebugDrawProbe`: DrawLine/Box/Sphere/Text; `HotReloadProbe`: `[Serialized]` counter used by `test_hot_reload`; `FeatureChecklist`: reports `feature_test.complete` on frame 600 |
| Misc | `DisabledEntity`; `TaggedOnly` | transform + mesh_renderer, disabled; name, tag `Pickup`, layer `Gameplay` | `MustNotRun`: reports failure if ever updated |

Binding call counters: the `bindings` module keeps a `uint32_t` per function id, incremented in the
native thunk when `!CONJURE_DISTRIBUTION`; `bindings::call_counters()` returns the span.
`tests/binding_exclusions.json` (authored, not downloaded; schema in `15_bindings.md` 10, R62) lists functions
reachable only on error paths, each with a one-line reason.

### 7.3 CI (`.github/workflows/ci.yml`: push to `main` and pull requests; `fail-fast: false`, 60 min)

| Job | Runner | Preset | Notes |
|---|---|---|---|
| `windows-msvc-debug` | windows-latest | `windows-msvc-debug` | full unit + integration incl. `[scripting]`; .NET 8 via `actions/setup-dotnet@v4` |
| `windows-msvc-dev` | windows-latest | `windows-msvc-dev` | Development config; plus `conjure_editor --export` of `feature_test` and `conjure_runtime --headless --frames 120` on the result |
| `linux-gcc-debug` | ubuntu-24.04 | `linux-gcc-debug` | apt: `ninja-build xorg-dev libwayland-dev libxkbcommon-dev libasound2-dev libvulkan-dev` |
| `linux-clang-sanitize` | ubuntu-24.04 | `linux-clang-sanitize` + `-DCONJURE_WITH_SCRIPTING=OFF` (D13) | `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; no suppression files (none are needed) |
| `macos-debug` | macos-14 | `macos-debug` | Vulkan backend compiles against Vulkan-Headers; MoltenVK not needed for null tests |
| `style-and-headers` | ubuntu-24.04 | - | `python tools/check_style.py` (tabs, > 120 cols, missing `#pragma once`, file naming, `enum` without `class` outside `_flags`, third-party includes in public headers, test-mirrors-header), `cmake --build --target conjure_header_check`, then as the last step, run after a failed step too, `dotnet format --verify-no-changes` on `Conjure.Managed`, `Conjure.Bootstrap` and `Conjure.TestScripts` with the SDK `actions/setup-dotnet` installed, selected through a `global.json` (blocking; C++ has no formatter step, D17) |

Common steps: `actions/checkout@v4` (`lfs: true`), `actions/setup-dotnet@v4` (8.0.x), `actions/cache`
on `build/<preset>/_deps` keyed by the hash of `third_party/CMakeLists.txt`, cache on
`tests/.downloaded` keyed by the hash of `tools/test_assets.json`, `python tools/download_test_assets.py`
(hashes verified against the committed `tools/test_assets.lock`), configure, build,
`ctest --preset <preset> --output-junit junit.xml` followed by
`ctest --test-dir build/<preset> -L "downloaded|process" --no-tests=error --output-junit junit_labels.xml`
(the preset's filter excludes those labels, so the label run goes through `--test-dir`; a label that matches
nothing fails the step), `python tools/ctest_check_skips.py` over both reports with
`CONJURE_TEST_ALLOWED_SKIPS=golden` and the leg's `--allow-tag` list (D12), upload `junit.xml`,
`junit_labels.xml` and `build/<preset>/test_output` on failure. Both workflows run with
`permissions: contents: read`; `ci.yml` cancels a superseded run for pull requests only.

`.github/workflows/nightly.yml` (cron 03:00 UTC + manual):

| Job | Purpose |
|---|---|
| `full-matrix-nightly` | all five build jobs with `*-nightly` test presets (adds `slow`, scripting hot reload on all three OSes) |
| `linux-clang-tsan` | `CONJURE_ENABLE_TSAN=ON`, scripting off; labels `core`, `ecs`, `scene`, `engine`, `integration`; `TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1`, no suppression files |
| `distribution-export` | `windows-msvc-dist` and `linux-gcc-dist`: `ctest -L "process|runtime" --no-tests=error` on the Distribution tree, export `feature_test`, run the exported runtime 300 frames headless, assert exit 0 and no editor symbols in the binary; then the whole unit suite in Distribution (D16): `ctest --preset <dist-preset> --output-junit junit_dist.xml` and `ctest_check_skips.py` with `--allow-tag window`, `audio_device` and `downloaded` (plus `gpu` on Linux), `junit_dist.xml` and `test_output` uploaded on failure |
| `golden-d3d12-warp` | windows-latest, `CONJURE_TEST_GOLDEN_IMAGES=ON`, `ctest --test-dir build/windows-msvc-dev -L golden --no-tests=error` (red with "No tests were found" until `tests/golden` registers its cases, never green with zero comparisons), upload `test_output` on failure (the gate) |
| `golden-vulkan-lavapipe` | ubuntu-24.04, apt `mesa-vulkan-drivers`, `VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json`, the same `ctest -L golden --no-tests=error`; `continue-on-error: true` until two weeks stable |
| `benchmarks-ci-class` | windows-latest + ubuntu-24.04, Development, `conjure_benchmarks --class ci --enforce --out bench.json`; the job fails on a breach; results appended to the job summary and uploaded as the `bench-<os>` artefact (the workflow has read permissions only, no history branch is written) |

### 7.4 Definition of done for a module

1. Public headers compile alone (`conjure_header_check`) and contain no third-party includes;
   `engine/src/<module>/README.md` exists: purpose, main types, threading model, how to test.
2. `tests/<module>/` contains every file in 5.2 for the module; every public type has the universal
   four; every `result` error code has a test; all cases pass on all six PR jobs.
3. If the module has backends, a `null` backend with a stats recorder exists and is what tests use;
   the real backend is tagged `[window]` / `[gpu]` and runs locally.
4. Every test leaves `memory_guard` clean and logs no unallowed ERROR.
5. Hot paths have `CONJURE_CHECK_NO_ALLOCATIONS` tests and, where listed in 7.1, a benchmark whose
   `desktop` thresholds pass with `conjure_benchmarks --class desktop --enforce --filter <module>.*`
   on the author's machine (stated in the PR).
6. New component type -> entity in `feature_test` (D8a); new binding -> script call in `feature_test`
   (D8b); new editor command -> `tests/editor/data/commands/<name>.json` (D8c); new feature -> a
   `Conjure.Diagnostics.Check` (D8d). `test_feature_test_scene` and `test_feature_test_replay` pass.
7. The public API wiki and handbook pages for the change are written or updated and `docs.check` passes
   (`16_documentation.md` section 5).
8. If the module adds a system, `test_determinism` still passes with 0, 2 and 8 threads; if it
   participates in the frame loop, at least one `tests/integration` case exercises it.
9. Zero warnings under the project warning set, `check_style.py` clean, `dotnet format --verify-no-changes`
   clean for managed sources (D17), nightly TSan clean for anything touching the job system, no unexpected
   skips.
10. Code review completed, conventional commit subject, commit builds and passes `ctest` locally.

---

## 8. Threading, ownership, failure handling and headless behaviour

### 8.1 Threading

- doctest assertions run only on the test thread. `headless_engine` owns a job system through
  `engine::application`; all workers are joined in the destructor so no job from one case can touch
  another. `[parallel]` cases start workers through `core::jobs` and join before asserting.
- `log_capture` (mutex) and the allocation counters (atomics) are the only support types touched by
  worker threads. `expect_assert` uses a thread-local active-scope pointer; an assert on a worker
  thread outside a scope fails the case and aborts the process, as it would in the engine.
- Null backend stats are mutex-protected. CTest runs cases in parallel processes; the only shared
  files are read-only `tests/assets` and `tests/.downloaded`; everything written goes to
  `temp_directory` or `outputRoot/<executable>/<case>`. Benchmarks use the real worker count.

### 8.2 Ownership

Fixtures own; tests borrow. `headless_engine` holds the `unique_ptr<application>`; accessors return
non-owning references valid until the fixture dies. doctest re-enters the case body per `SUBCASE`
and reconstructs the fixture, so construction is budgeted at <= 5 ms with null backends (tested).
Code ownership: `tests/support`, `cmake/ConjureTesting.cmake`, workflows, `thresholds.json`, `tools/`
-> test infrastructure; `tests/<module>` -> module owner; `tests/integration`, `tests/downloaded`,
`tests/golden` -> engine owner; `samples/feature_test` -> samples owner, with the rule that whoever
adds a component, binding or command updates it in the same PR.

### 8.3 Failure handling

- Fixture construction failure (project missing, backend creation error) is a `REQUIRE` failure
  with the `core::error` message, never a crash.
- Unmet precondition: exit 77 (3.2); CI fails on unlisted skips. Missing in-repo asset: failure.
  Missing downloaded asset inside `conjure_downloaded_tests`: skip 77 with the download command in
  the message; CI's allowlist does not include `downloaded`, so it is red there.
- Golden mismatch or missing reference outside update mode: failure with `actual.png` / `diff.png`.
  Benchmark over threshold under `--enforce` or missing threshold entry: table printed, exit 1.
- Unallowed ERROR log or live-allocation delta: failure at fixture destruction naming the first
  offending entry or the delta. A crash takes down exactly one CTest entry (D1).
- Timeouts: 120 s per case by default, 300 s for integration and downloaded, 600 s for golden.

### 8.4 Headless / null behaviour required of the engine

- `platform::null_backend`: 1280x720 virtual window, consumes injected `input_event`s, no OS calls,
  edge/level key state identical to the glfw backend.
- `rhi::null_device`: resources record their descriptors and return valid handles; command lists
  record pass names, draw and dispatch counts (`null_device_stats`); readback returns zero-filled
  images; the full render graph compiles and executes so renderer tests assert on the stream.
- `audio::null_backend`: virtual clock, voice states, per-group gains, listener position
  (`null_backend_stats`); decoding runs for real. Scripting has no null path: real .NET or skip.
  Golden tests are the one exception to "no GPU": software adapter, off by default.

---

## 9. Integration points

**Needs from other areas**

- core (A/F): `core::set_assert_handler()` returning `assert_action`; `log::add_sink` / `log::remove_sink`
  with `std::shared_ptr<log_sink>`; `core::memory::counters()` (the three atomics) and `memory::snapshot()` plus
  `tracked_allocate` / `tracked_free` used by engine allocators; seedable `core::random::default_engine()`;
  `core::json::document` canonical serialisation (sorted keys, shortest round-trip floats); `core::image`
  PNG load/save over stb; `core::process` (spawn, wait, exit code); `core::jobs` where worker count `0`
  means inline.
- ecs (A): `ecs::world_hash(const world &)` as in 3.4 (`conjure/ecs/world_hash.h`); `component_registry::types()`
  with `component_flags::EDITOR_HIDDEN` (the "internal" flag, R32); stable chunk iteration order; bulk spawn
  through the command buffer.
- platform / rhi / audio (B/F): null backends as in 8.4; `rhi::device_desc::preferSoftwareAdapter`;
  `renderer::read_back(render_target_id)` to RGBA8 (R30). renderer (B): deterministic render mode (fixed
  exposure, constant SSAO noise seed, no temporal passes); offscreen `render_target` without a swapchain;
  queryable pass names (`render_stats::passNames`, `rhi::null_device_stats`); CPU reference tonemap functions
  in a public header.
- physics (B/F): Jolt allocation hooks routed to `core::memory`; contact events sorted by body id pair
  in `physics_world::contact_events()` (R6); sync iterates engine component arrays, not `GetActiveBodies()`;
  sleeping query.
- scripting / bindings (C): `scripting::is_runtime_available()` (R28); `Conjure.TestScripts` built by
  `conjure_add_managed_project()` from `ConjureManaged.cmake` when tests are on; programmatic hot
  reload trigger (`hot_reload_controller::request(true)`); per-function call counters in the native thunks
  (`bindings::call_counters()`); structural requests inside a `Span<T>` query scope are deferred to `query_end`
  (R5); `Conjure.Diagnostics.Check` binding writing to `engine::diagnostics::check_registry`.
- engine (F): synchronous `application::step_frame(double)` (R13); `engine_config` with backend selection (no
  render thread exists, R42); `diagnostics::check_registry`; export as a library call (`engine::export_pipeline`,
  R21); headless boot of an exported folder through `application` (`--project`, `--headless`, `--frames`).
- editor (D): the `editor_core` / `editor_ui` / executable split (D14); `editor::protocol_transport` with
  `tcp_transport`, `stdio_transport` (injectable streams) and `loopback_transport`;
  `command_registry::descriptors()`; `headless_runner` with exit codes 0/1/2/3 and `--keep-going`;
  `protocol_server` on port 0 reporting `bound_port()`; `protocol::MAX_MESSAGE_BYTES`; JSON schemas for
  `scene.query` / `entity.get` / `reflection.components` responses (R15).
- assets (E): download stamp `tests/.assets_ready`; in-memory VFS mount of `tests/assets` for the
  default project. modules (G): `tests/modules/sample_plugin` built with `conjure_module_entry`,
  registering `plugin_counter` and a system.

**Offers to other areas**

- `conjure_test_support` (wave 1) and `conjure_test_support_engine` (wave 4), `conjure_add_test_module()`
  and the other CMake functions, labels and presets.
- `headless_engine`, `world_builder`, `input_script`, `protocol_client` as the shared basis for unit,
  integration and benchmark code.
- Machine-checked completeness (D8) as the gate the customer asked for.
- CI artefacts (JUnit, golden diffs, benchmark history) and the skip discipline that keeps them honest.

---

## 10. Implementation plan

1. Move `conjure_add_test_module()` from `cmake/ConjureModule.cmake` into `cmake/ConjureTesting.cmake`
   with the signature in 3.8; include doctest's `doctest.cmake` in `third_party/CMakeLists.txt`; add
   `CONJURE_ENABLE_TSAN` and `CONJURE_TEST_GOLDEN_IMAGES`; update `CMakePresets.json` (filters,
   `linux-clang-tsan`).
2. Create `tests/CMakeLists.txt` and `conjure_test_support` (every core-library header in 2.1 plus
   `test_main.cpp` and `tracked_global_new.cpp`); depends only on `core`.
3. Write `tests/support/tests/` (section 11) and `tests/core/` skeleton files so wave 1 implementers
   have the pattern to copy; add `tools/check_style.py` and `tools/ctest_check_skips.py`.
4. Author `tests/assets/`, `tools/test_assets.json` and `tools/download_test_assets.py` (sha256, stamp).
5. Write `.github/workflows/ci.yml` with the six PR jobs so wave 1 lands green.
6. Wave 2: `tests/modules/sample_plugin`, null-backend tests, `world_hash` tests (with area A).
7. Wave 4: `conjure_test_support_engine` and its self-tests; `tests/integration/` scenarios in 6.1
   order; `tests/editor/data/commands/` fixtures and the catalogue test; `tests/benchmarks/` harness,
   `thresholds.json`, scenarios in 7.1 order, `tools/bench_compare.py`.
8. Wave 5: `samples/feature_test` command file and scripts, `tests/downloaded/`, `binding_exclusions.json`.
9. Golden: `tests/golden/` scenes and `conjure_golden_tests`; generate WARP references, then lavapipe;
   `nightly.yml` with all six jobs. `docs/CONVENTIONS.md` section 6 gains a pointer to 5.1 and 7.4.

---

## 11. Tests of this area's own code (`tests/support/tests/`, `conjure_support_tests`)

Case names use the `"<type> - <behaviour>"` convention; the type prefix is elided after the first.

- `test_log_capture.cpp`: `"log_capture - counts per level"`, captures from job threads, ring buffer wraps
  at capacity, destructor fails on ERROR unless allowed, restores previous sinks.
- `test_assert_capture.cpp`: `"expect_assert - records exactly one assert"`, restores handler, nested
  scopes; `"CONJURE_CHECK_ASSERTS - compiles to nothing in distribution"`.
- `test_memory_guard.cpp`: `"memory_guard - clean scope passes"`, leaked block fails check;
  `"allocation_scope - counts std::vector growth"`, zero for arena allocations, counts job threads.
- `test_temp_directory.cpp`: unique per instance, removed on destruction, `write_text` round trip.
- `test_approx.cpp`: vec3 inside and outside tolerance, quaternion sign equivalence, mat4;
  `"StringMaker - prints vec3 and uuid"`.
- `test_test_environment.cpp`: seed and threads from arguments; `"require_precondition - unmet exits 77
  in isolated mode"` (runs a tiny child executable built by the test tree), unmet returns false in
  direct mode.
- `test_headless_engine.cpp` (engine library): constructs in under 5 ms, 60 frames give 60 fixed
  updates, one frame at 0.1 s gives 6, `load_scene_json` error on bad json, destructor joins job
  threads, rhi stats count submissions.
- `test_input_script.cpp`: events delivered on exact frame, out-of-order insertion sorted, key pressed
  then held. `test_world_builder.cpp`: `spawn_many` count in one archetype, `spawn_hierarchy` total and
  depth match shape, `mark_dirty_fraction` deterministic for seed.
- `test_protocol_client.cpp`: in-process ping, tcp ping on ephemeral port, buffers events between
  sends, `send_raw` garbage returns error document, closed port returns error.
- `test_golden_image.cpp`: identical passes, one pixel off by 5 passes, 1 percent pixels off fails,
  a wholly wrong 33x33 block at 640x360 passes the defaults (the D11 blind spot) while a 34x34 one fails,
  size mismatch fails, writes actual and diff on failure, missing reference reported.
- `test_benchmark.cpp` (in `tests/benchmarks/tests/`): `summarise` exact p50/p95/p99 on known samples,
  allocation gate exact, `load_thresholds` missing scenario is unthresholded, `--enforce` fails on
  breach, `write_json` round trip.
- `test_check_registry.cpp` (in `tests/engine/`): report from two threads, `all_passed` and `failed_count`.

---

## 12. Out of scope / future

- Thread-count invariance of Jolt results is not asserted (Jolt guarantees determinism for identical
  inputs on the same binary and documents that callback order is thread dependent); the determinism
  test varies thread count only with physics disabled. Revisit if Jolt documents the guarantee.
- A self-hosted desktop-class runner to gate `desktop` thresholds in CI; MoltenVK golden images on
  macOS; Vulkan lavapipe stays `continue-on-error` until stable.
- Tighter golden limits once off-screen rendering produces the first WARP and lavapipe references to
  calibrate against: a connected-region criterion (fail when the mismatching pixels form one region
  larger than, say, 256 pixels, which tells a missing object from edge noise) or a lower mean absolute
  error limit. Until then the D11 limits stand; the 33x33-block self-test flips once the limits reject
  that block (a mean absolute error limit below 0.904, or a default region limit below 1,089 pixels).
- A pure-managed `Conjure.Managed.Tests` project (xunit); libFuzzer fuzzing of the protocol codec and
  scene loader; coverage reporting (llvm-cov) nightly; the sanitizer job with scripting enabled once a
  CoreCLR-under-ASan recipe is proven.
