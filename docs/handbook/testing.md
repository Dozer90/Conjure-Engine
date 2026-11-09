# Testing

🛠 **Engineering handbook**

Every module ships tests, every public header has a test file, and every test is headless and
deterministic. This chapter documents the machinery that makes that cheap: the test support library under
`tests/support/include/conjure/testing/`, the shared `main`, CMake registration, the naming and content
conventions of `docs/design/08_testing.md` 5.1, labels, preconditions and skipping, the header and style
checks, and how to write and run one test.

## Contents

- [Overview](#1-overview)
- [Registering tests with CMake](#2-registering-tests-with-cmake)
- [Conventions](#3-conventions)
- [The support library](#4-the-support-library)
- [Test executable start-up](#5-test-executable-start-up)
- [Preconditions and skipping](#6-preconditions-and-skipping)
- [Writing a test](#7-writing-a-test)
- [Running tests](#8-running-tests)
- [Header, style, export and documentation checks](#9-header-style-export-and-documentation-checks)
- [Labels](#10-labels)
- [Benchmarks](#11-benchmarks)

## 1. Overview

- doctest 2.4.11, included only through `conjure/testing/doctest.h` (`DOCTEST_CONFIG_SUPER_FAST_ASSERTS`,
  `DOCTEST_CONFIG_USE_STD_HEADERS`). `tools/check_style.py` rejects a direct `<doctest/doctest.h>` include.
- One CTest entry per `TEST_CASE`, named `<module>.<case>`, run as its own process from
  `build/<preset>/bin` with `--conjure-isolated`. A crash takes down exactly one entry.
- One static support library `conjure_test_support` (`conjure::test_support`, namespace `conjure::testing`,
  depends on `conjure::core` only). `tests/support/src/test_main.cpp` and `tracked_global_new.cpp` are
  compiled into every test executable, not into the library, so every heap allocation of a test process is
  counted and the shared `main` owns seed, thread count, log sink and assert handler.
- Tests run against a real job system (default `clamp(hardware_concurrency(), 2, 8)` workers, `0` = inline)
  and the `null` backends of `platform`, `rhi` and `audio`; nothing may need a GPU, window, audio device or
  network. The few cases that exercise real hardware guard themselves with a precondition (section 6).

## 2. Registering tests with CMake

```cmake
conjure_add_test_module(<module>
    SOURCES <files...>              # test_<header>.cpp files and helper headers next to them
    [DEPS <targets...>]             # extra link targets (test groups without a library of their own)
    [LABELS <labels...>]            # appended to "unit;<module>"
    [TIMEOUT <seconds>]             # default 120
    [LINK_ENGINE_SHARED]            # link the umbrella conjure::shared instead of the static libraries (below)
    [ENGINE]                        # accepted, no effect until wave 4 adds conjure::test_support_engine
)
conjure_header_check(conjure_<module>)
```

`conjure_add_test_module()` (`cmake/ConjureTesting.cmake`) builds `conjure_<module>_tests` from the sources
plus the two shared translation units, links `conjure::<module>` when that target exists, `conjure::test_support`,
`conjure::platform_defines`, `conjure::warnings` and `conjure::sanitizers`, adds `engine/src` as a private
include directory (tests may reach private module headers), and defines per executable
`CONJURE_TEST_EXECUTABLE_NAME`, `CONJURE_TEST_REPO_ASSET_ROOT` (`tests/assets`), `CONJURE_TEST_DOWNLOADED_ASSET_ROOT`
(`tests/.downloaded`), `CONJURE_TEST_OUTPUT_ROOT` (`build/<preset>/test_output/<target>`) and
`CONJURE_TEST_TEMP_ROOT` (`build/<preset>/test_tmp/<target>`). Discovery is `doctest_discover_tests()` with
`TEST_PREFIX "<module>."`, `EXTRA_ARGS --conjure-isolated`, `WORKING_DIRECTORY bin`, properties `TIMEOUT` and
`SKIP_RETURN_CODE 77`; the label list is applied by a generated `<target>_labels.cmake` because doctest's
script flattens nested lists.

`conjure_add_test_executable(<name> SOURCES ... [DEPS ...] [FOLDER ...])` is the executable half without
CTest registration, used for helper programs such as `conjure_support_child`. `tests/CMakeLists.txt` adds
every `tests/<module>/` that has a `CMakeLists.txt`; `tests/core/CMakeLists.txt` adds an area directory only
when `TARGET conjure::core_<area>` exists (`conjure_add_test_module(core_<area> ...)` in
`tests/core/<area>/CMakeLists.txt`). Every module test directory guards itself the same way, so disabling a
subsystem disables its tests: `tests/{core,ecs,platform,rhi,assets,audio,modules}` are in the tree.

`LINK_ENGINE_SHARED` (used by `tests/modules/`) links the umbrella `conjure::shared` and
`conjure::test_support_shared` (the support library compiled against it) instead of the static libraries, so
the executable and the fixture plugins it loads share one engine copy. Such an executable keeps the C
runtime's allocation functions: `memory_guard` and `allocation_scope` count nothing in it, and the suite asserts
"no growth" through the registry instead. The option needs `conjure_finalize_engine()` to have run, which the
root `CMakeLists.txt` does before `tests/` ([Build pipeline](build-pipeline.md#44-the-umbrella-shared-library-and-game-modules)).

> [!WARNING]
> Case names become CMake list items and CTest names: they must be unique within the executable and must
> not contain `;` or unbalanced `[` / `]`. `check_style.py` (`test-case-name`) rejects `;` and unbalanced
> brackets; a duplicate name is not caught until `doctest_discover_tests()` registers the second entry, so
> grep the file before adding a case.

## 3. Conventions

The binding text is `docs/design/08_testing.md` 5.1, summarised in `docs/CONVENTIONS.md` 6. In short:

| Rule | Detail |
|---|---|
| One file per header | `tests/<module>/test_<name>.cpp` mirrors `engine/include/conjure/<module>/<name>.h` (core: `tests/core/<area>/test_<name>.cpp`); `check_style.py --tests-mirror-headers` enforces it, exceptions with reasons in `tools/style_exceptions.json` |
| File opening | `#include "conjure/testing/doctest.h"` first, then `TEST_SUITE("<module>/<name>")` |
| Case names | `TEST_CASE("<type> - <behaviour in plain English>")`, e.g. `"slot_map - erase invalidates the handle generation"`; bug fixes add `"regression - <issue id> <summary>"` |
| Variants | `SUBCASE`, never copied cases; loops `CAPTURE(index)` |
| Tags in the name | `[slow]` (> 1 s), `[parallel]` (asserts on multi-worker behaviour), `[scripting]` (+ `* doctest::timeout(60)`), `[window]` (needs a real window; guarded by `CONJURE_TEST_REQUIRE(precondition::REAL_WINDOW)`) |
| Assertions | `REQUIRE` only when continuing would dereference an invalid result, `CHECK` otherwise; floats through `doctest::Approx` or `CONJURE_CHECK_APPROX`; results through `CONJURE_REQUIRE_OK`, `CONJURE_CHECK_OK`, `CONJURE_CHECK_ERROR` |
| Fixtures | locals in the case body; `TEST_CASE_FIXTURE` allowed, fixture inheritance not |
| Coverage per public type | construction/destruction, happy path, failure path (every `error_code` a `result` function can return), edge cases (empty, max, invalid or stale handle) |
| Forbidden, checked by `check_style.py` (`forbidden-in-tests`) | `std::thread` / `std::jthread` (use `core::jobs`), `sleep_for`, wall-clock reads (`steady_clock` / `system_clock` / `high_resolution_clock::now`), `getenv` (only `test_main` reads variables); `doctest-include` catches a direct `<doctest/doctest.h>` |
| Forbidden, checked in review | randomness not derived from `environment().seed`, file I/O outside `temp_directory`, other third-party headers, absolute paths |
| Skipping | `* doctest::skip()` needs a comment with the tracking issue; machine preconditions use `CONJURE_TEST_REQUIRE` (section 6) |

The first group is mechanical: `check_style.py` reports `forbidden-in-tests`; a test file that the
design allows to break a rule (core areas below `jobs` testing their thread-safety contract, the process
tests' wall-clock timeouts) is listed under `forbidden_in_tests` in `tools/style_exceptions.json` with its reason.
The second group has no script behind it; the review checklist (section 5 of [Contributing](contributing.md))
is where it is caught.

## 4. The support library

All headers are under `tests/support/include/conjure/testing/`, namespace `conjure::testing`; every one has
a self-test in `tests/support/tests/` (`conjure_support_tests`, label `support`).

| Header | Provides | Use it for |
|---|---|---|
| `conjure/testing/doctest.h` | `CONJURE_REQUIRE_OK(expr)`, `CONJURE_CHECK_OK(expr)`, `CONJURE_CHECK_ERROR(expr, errorCode)`, `CONJURE_TEST_REQUIRE(precondition)`, `describe_error(result)`, `has_error(result, code)`, `doctest::StringMaker` for `core::uuid` and `core::error`, `toString(core::error_code)` | every test file; the result macros evaluate their expression exactly once and print `category.CODE: message` on failure |
| `conjure/testing/test_environment.h` | `enum class precondition : u8 { DOTNET_RUNTIME, DOWNLOADED_ASSETS, SOFTWARE_GPU_ADAPTER, LOOPBACK_TCP, REAL_WINDOW, AUDIO_DEVICE, COUNT }`, `struct test_environment` (seed, `jobThreadCount`, `isolatedProcess`, `goldenEnabled`, `updateGoldens`, `realWindowAllowed`, `continueOnAssert`, the four paths `repoAssetRoot`, `downloadedAssetRoot`, `outputRoot`, `temporaryRoot`, `executableName`), `environment()`, `precondition_holds()`, `require_precondition()`, `set_precondition_probe()`, `to_string(precondition)`, `SKIP_EXIT_CODE = 77` | reading the seed and thread count instead of environment variables; installing a probe for a module-dependent precondition |
| `conjure/testing/approx.h` | `approx_equal(left, right, epsilon = EPSILON)` for `f32`, `vec2/3/4`, `quat` (q and -q equal), `mat3/4`, `aabb`, `transform`; `EPSILON = 1.0e-5f`; `CONJURE_CHECK_APPROX(actual, expected)`; `StringMaker` for every math type | comparing results of whole computations |
| `conjure/testing/log_capture.h` | `log_capture`: `contains(level, substring)`, `contains(substring)`, `count(level)`, `count()`, `entries()`, `messages()`, `messages(level)`, `allow_errors()`, `errors_allowed()`, `clear()`; `captured_log_entry { level, category, text }` | asserting on log output; the destructor fails the case when an `ERR` or `FATAL` was logged without `allow_errors()` |
| `conjure/testing/assert_capture.h` | `expect_assert`: `count()`, `failures()` (span of `asserted_failure`, at most `CAPACITY = 8`), `last_message()`, `reset()`; `CONJURE_CHECK_ASSERTS(statement)` | code that must assert; scopes nest, record asserts from helper threads, and never allocate |
| `conjure/testing/memory_guard.h` | `memory_guard`: `check()`, `reset_baseline()`, `live_delta()`, `baseline()` | leak detection: the destructor fails the case when live allocations differ from the baseline |
| `conjure/testing/allocation_scope.h` | `allocation_scope`: `allocation_count()`, `free_count()`, `allocated_bytes()`, `reset()`; `CONJURE_CHECK_NO_ALLOCATIONS(statement)` | allocation gates on hot paths (`parallel_for` dispatch, `slot_map` lookup) |
| `conjure/testing/temp_directory.h` | `temp_directory` (optional name prefix): `path()`, `file(name)`, `write_text(name, text) -> result<void>`, `write_file(path, contents)` | the only place tests may do file I/O; unique per instance under `environment().temporaryRoot`, removed on destruction |
| `conjure/testing/test_assets.h` | `repo_asset(relativePath)`, `downloaded_asset(relativePath)` | fixtures under `tests/assets` (always present) and `tests/.downloaded` (behind `precondition::DOWNLOADED_ASSETS`) |

> [!WARNING]
> `memory_guard` sees doctest's own bookkeeping. Create the guard after the `SUBCASE` selection (or call
> `reset_baseline()` once the variant is chosen) and never enclose a `SUBCASE` boundary or a message macro
> on a worker thread; `test_main` pre-grows doctest's INFO stack so the `CONJURE_*` macros are safe inside a
> guard on the test thread only.

> [!CAUTION]
> Outside an `expect_assert` scope a failed `CONJURE_ASSERT` fails the current case, prints the captured
> log and aborts the process, in a direct run as much as under CTest. Code under test must early-out after
> its assert so that a scope which records and continues cannot corrupt state.

## 5. Test executable start-up

`tests/support/src/test_main.cpp` (`DOCTEST_CONFIG_IMPLEMENT`) does, in order: parse the `--conjure-*`
arguments (environment variables are the fallback) into `test_environment` and hand everything else to
doctest; replace the console sink by a 4,096-entry ring buffer that a doctest listener dumps to stderr when a
case fails; install `detail::testing_assert_handler`; seed `core::random` and designate the main thread
(`core::jobs::set_main_thread()`); print `seed=<n> threads=<n>` to stderr; run. An unknown `--conjure-*`
option exits with code 3.

| Argument | Variable | Default | Effect |
|---|---|---|---|
| `--conjure-seed=N` (decimal or `0x` hex) | `CONJURE_TEST_SEED` | `0x5EED` | seeds `core::random::default_engine()`; read back through `environment().seed` |
| `--conjure-threads=N` (<= 1024) | `CONJURE_TEST_THREADS` | `clamp(hardware_concurrency(), 2, 8)` | `environment().jobThreadCount`; `0` runs every job inline |
| `--conjure-isolated` | `CONJURE_TEST_ISOLATED` | off (CTest passes it) | unmet preconditions exit 77 instead of returning |
| `--conjure-update-golden` | `CONJURE_TEST_UPDATE_GOLDEN` | off | golden tests of later waves overwrite their references |
| `--conjure-continue-on-assert` | `CONJURE_TEST_CONTINUE_ON_ASSERT` | off | an unexpected assert on the test thread fails the case and continues instead of aborting |
| - | `CONJURE_TEST_WINDOW=1` | off | `precondition::REAL_WINDOW` holds; never in CI |
| - | `CONJURE_TEST_DOWNLOADED_ASSET_ROOT` | compile definition | the one variable that overrides a compiled-in path, for testing the precondition itself |

## 6. Preconditions and skipping

`CONJURE_TEST_REQUIRE(precondition::X);` is the first statement of a case that needs something from the
machine. When the precondition holds it is a no-op. Otherwise, under CTest (`isolatedProcess`) it prints
`skipped: precondition X not met (file:line)` and exits with `SKIP_EXIT_CODE` (77), which `SKIP_RETURN_CODE 77`
turns into *Not Run*; in a direct run it emits a doctest `MESSAGE` and returns from the case.

| Precondition | Holds when |
|---|---|
| `DOWNLOADED_ASSETS` | `tests/.downloaded/` exists and the stamp file `.assets_ready` exists in its parent directory, i.e. `tests/.assets_ready` (`precondition_holds()` in `tests/support/src/test_environment.cpp`, as `docs/design/08_testing.md` 2.2 specifies) |
| `REAL_WINDOW` | `CONJURE_TEST_WINDOW=1`; used by `tests/platform/test_backend_conformance.cpp` to run the GLFW backend through the same cases as the null backend |
| `SOFTWARE_GPU_ADAPTER` | the probe `tests/rhi/rhi_test_support.cpp` installs with `set_precondition_probe()` can create a device with `preferSoftwareAdapter` (WARP on Windows); guards the D3D12 and Vulkan cases of `test_device`, `test_swapchain` and `test_shader_library` ([RHI module](modules/rhi.md#9-tests)) |
| `AUDIO_DEVICE` | the probe `tests/audio/audio_test_support.cpp` installs finds a miniaudio playback device; guards `test_miniaudio_backend` ([Audio module](modules/audio.md#13-tests)) |
| `DOTNET_RUNTIME`, `LOOPBACK_TCP` | a probe installed by the scripting or editor test libraries (wave 3-4) returns true; without a probe they never hold |

The default test presets exclude the labels `benchmark|golden|downloaded|slow|process`, so a fresh clone
without network is green. `docs/design/08_testing.md` D12 adds `tools/ctest_check_skips.py`, which will fail
CI on any skip outside an allow-list; it is not in the tree yet.

## 7. Writing a test

A minimal file for a header `conjure/core/<area>/<name>.h`, following `tests/core/foundation/test_result.cpp`:

```cpp
#include "conjure/testing/doctest.h"

#include "conjure/core/result.h"

using namespace conjure;
using namespace conjure::core;

namespace {
    result<int> divide(int numerator, int denominator) {
        if (denominator == 0) {
            return make_error(error_code::INVALID_ARGUMENT, "division by zero ({} / {})", numerator, denominator);
        }
        return numerator / denominator;
    }
}

TEST_SUITE("core/result") {

    TEST_CASE("result - divide returns the quotient") {
        const result<int> value = divide(10, 2);
        CONJURE_REQUIRE_OK(value);
        CHECK(value.value() == 5);
    }

    TEST_CASE("result - divide by zero is INVALID_ARGUMENT") {
        CONJURE_CHECK_ERROR(divide(1, 0), error_code::INVALID_ARGUMENT);
    }
}
```

A case with the two fixtures every module test should own (`log_capture`, `memory_guard`), plus an
allocation gate and an expected assert:

```cpp
#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"

#include "conjure/core/containers/slot_map.h"

using namespace conjure;
using namespace conjure::core;
using namespace conjure::testing;

TEST_SUITE("core/slot_map") {

    TEST_CASE("slot_map - lookup is allocation-free and erasing end asserts") {
        log_capture logs = {};      // its destructor fails the case if an ERROR or FATAL was logged
        memory_guard guard = {};    // its destructor fails the case if an allocation leaks

        slot_map<int> map = {};
        const slot_key key = map.insert(7);
        const int *found = nullptr;
        CONJURE_CHECK_NO_ALLOCATIONS(found = map.find(key));   // lookup is O(1) and allocation-free
        CHECK(found != nullptr);

        CHECK(map.erase(key));
        CHECK_FALSE(map.erase(key));                  // a stale key is rejected, not asserted
        CONJURE_CHECK_ASSERTS(map.erase(map.cend()));  // erasing end() asserts once and leaves the map unchanged
    }
}
```

File I/O goes through `temp_directory` (section 4); its `write_text` returns `result<void>`, so
`CONJURE_REQUIRE_OK(scratch.write_text("scene.json", "{}"))` wraps it.

Register the file in `tests/<module>/CMakeLists.txt` (`SOURCES`), build, and run `tools/check_style.py
--tests-mirror-headers`: a new public header without its `test_<name>.cpp` is a style finding.

> [!TIP]
> Copy patterns from the existing suites: `tests/core/foundation/test_result.cpp` (result macros),
> `tests/core/jobs/` (`CONJURE_JOBS_WORKER_SUBCASES` runs every behavioural case with 0 and 8 workers),
> `tests/core/containers/test_slot_map.cpp` (property tests against the standard container seeded with
> `0x5EED`), `tests/support/tests/` (self-tests of every helper, including a spawned child for exit code 77).

## 8. Running tests

```bash
python tools/build.py --test                                                     # everything, default preset
python tools/build.py --target conjure_core_foundation_tests --test -R core_foundation
ctest --preset windows-msvc-debug -R "core_foundation.result - value result"     # one case through CTest
ctest --preset windows-msvc-debug -L support                                     # by label
build/windows-msvc-debug/bin/conjure_core_foundation_tests -tc="result - *"      # doctest filter, full report
build/windows-msvc-debug/bin/conjure_core_jobs_tests -ts="core/job_system" -s    # one suite, successful asserts too
build/windows-msvc-debug/bin/conjure_core_jobs_tests --conjure-threads=0 --conjure-seed=0x1234
```

Reading failures: `***Failed` in ctest output names the exact case; rerun the executable with `-tc` for
doctest's report and the ring-buffer log of that case. `log_capture: unexpected ERROR logged by [...]`
means the case needs `allow_errors()` or has a real bug; `memory_guard: N allocation(s) still live` is a
leak; an unexpected `CONJURE_ASSERT` prints its source location and aborts the process.

## 9. Header, style, export and documentation checks

- **Header check.** `conjure_header_check(<target>)` compiles every header listed in the target's
  `CONJURE_PUBLIC_HEADERS` in its own translation unit with the project warning set; the aggregate target
  `conjure_header_check` is part of `ALL`, so a header that does not include what it uses breaks the default
  build. `tests/core/CMakeLists.txt` calls it for every core area.
- **Style check.** `python tools/check_style.py --tests-mirror-headers` (CTest `style.check`, label `style`)
  scans `engine/`, `editor/`, `runtime/`, `tests/`, `samples/` and reports `path:line: rule: message` for:
  `tabs`, `line-length` (120), `crlf`, `trailing-whitespace`, `pragma-once`, `using-namespace` (headers),
  `windows-macro` (enumerators or constants named `ERROR`, `DELETE`, `IN`, `min`, ...), `enum-class`
  (plain enums only for `*_flags`), `include-order` (own module, other Conjure, third-party, standard
  library, blank line between groups, quotes for Conjure headers), `third-party-include` (public headers),
  `doctest-include`, `test-case-name`, `forbidden-in-tests`, `file-name` (snake_case) and
  `tests-mirror-headers`. Exit code 0 clean, 1 findings, 2 usage error.
- **Export check.** `python tools/check_exports.py [module ...]` (default `modules`; `--all` for every module)
  reports public classes with out-of-line members, namespace-scope function prototypes and `extern` variables
  that lack `CONJURE_API` (`docs/design/07_modules.md` 5 and 9). Output is `path:line: kind: message`; exit
  code 0 clean, 1 findings, 2 usage error. Not a CTest entry yet; see [Modules module](modules/modules.md) 7.2.
- **Documentation checks.** `python -I tools/check_docs.py` (CTest `docs.check`, label `docs`) checks badges,
  links, page length, markers and header coverage of both documentation sets; `tools/tests/test_check_docs.py`
  (CTest `docs.check_tool`) is the checker's self-test over the fixtures in `tools/tests/check_docs/`. The
  [Documentation](documentation.md#4-tooling) chapter explains both.

## 10. Labels

| Label | Applied to | Default presets |
|---|---|---|
| `unit`, `<module>` | every entry from `conjure_add_test_module()` (`core_math`, `support`, ...) | run |
| extra `LABELS` | e.g. `support` on the self-tests | run |
| `style` | `style.check` | run |
| `docs` | `docs.check`, `docs.check_tool` (`docs/design/16_documentation.md` D9 and section 6) | run |
| `benchmark`, `golden`, `downloaded`, `slow`, `process` | later waves' executables and process-level tests | excluded; `nightly-test` excludes only `benchmark` |

Filter with `ctest -L <label>` or `python tools/build.py --test -L <label>`; `-R` filters by entry name.

> [!NOTE]
> The wave-2 cases that need hardware carry no extra label: they are ordinary `unit;<module>` entries that
> exit 77 when their precondition (section 6) does not hold, so a default run reports them as *Not Run* rather
> than skipping them silently.

## 11. Benchmarks

`CONJURE_BUILD_BENCHMARKS` (ON) and the `tests/benchmarks/` hook in `tests/CMakeLists.txt` exist; the
harness does not yet.

> [!NOTE]
> Planned by `docs/design/08_testing.md` D10 and 7.1: one executable `conjure_benchmarks` on an in-house
> harness (`conjure/bench/benchmark.h`, `CONJURE_BENCHMARK(name)`, fixed warm-up and measured iterations,
> min/p50/p95/p99 and an exact allocation count), thresholds in `tests/benchmarks/thresholds.json` for two
> machine classes (`desktop`, part of the definition of done; `ci`, 3x looser, nightly), registered as a
> single CTest entry with label `benchmark`. Until it lands, allocation-free steady state is asserted in
> unit tests (`CONJURE_CHECK_NO_ALLOCATIONS`, `job_system::stats().poolGrowths`).

## Related

- design record: `docs/design/08_testing.md` (the binding test strategy: D1-D15, 2.2, 3.1-3.2, 3.8, 5.1, 7.4, 11)
- design record: `docs/design/10_core.md` section 3 (test support rework)
- `docs/CONVENTIONS.md` section 6 (summary of the conventions)
- `tests/support/CMakeLists.txt`, `cmake/ConjureTesting.cmake`, `tools/check_style.py`, `tools/check_exports.py`,
  `tools/check_docs.py`, `tools/download_test_assets.py`
- module test suites: [ECS](modules/ecs.md#8-tests), [Platform](modules/platform.md#10-tests), [RHI](modules/rhi.md#9-tests),
  [Assets](modules/assets.md#12-tests), [Audio](modules/audio.md#13-tests), [Modules](modules/modules.md#9-tests-and-fixtures)
- `.claude/skills/build-and-test/SKILL.md` (reading failures, single-test runs)
- [Getting started](getting-started.md), [Build pipeline](build-pipeline.md), [Continuous integration](ci.md)
