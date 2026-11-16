# Continuous integration

🛠 **Engineering handbook**

`.github/workflows/ci.yml` builds and tests every push to `main` and every pull request on Windows, Linux
and macOS with the same presets developers use locally, plus one `style-and-headers` job;
`.github/workflows/nightly.yml` runs the expensive and noisy checks once a night. This chapter lists both
matrices, what each job proves, the sanitizer settings, and how to reproduce a red leg on your machine.

## Contents

- [Workflow](#1-workflow)
- [The matrix](#2-the-matrix)
- [Steps of every job](#3-steps-of-every-job)
- [Sanitizer options](#4-sanitizer-options)
- [Reproducing a failure locally](#5-reproducing-a-failure-locally)
- [The nightly workflow](#6-the-nightly-workflow)

## 1. Workflow

- Triggers: `push` to `main` and `pull_request`.
- `permissions: contents: read` at workflow level (nightly too): no job needs more than checkout, so the
  `GITHUB_TOKEN` never carries write access whatever the repository default is.
- `concurrency: ci-${{ github.ref }}` with `cancel-in-progress` for pull requests only
  (`github.event_name == 'pull_request'`): a newer push to a pull request supersedes the running check, while
  every push to `main` keeps its own result.
- One job `build-test` with a matrix and `fail-fast: false`, so every leg reports even when one fails, and
  one job `style-and-headers` on `ubuntu-24.04`.
- `CONJURE_TEST_ALLOWED_SKIPS: golden` at workflow level: the only label a pull request may skip wholesale; the
  case-name tags a leg may skip (`skip-tags` in the matrix) are `window`, `downloaded` and `audio_device` on
  every leg plus `gpu` on Linux and macOS (section 3, "Check skips").
- No secrets or deployments; JUnit reports and `test_output` are uploaded as artefacts when a leg fails.

## 2. The matrix

| Leg | Runner | Preset | Tests | Proves |
|---|---|---|---|---|
| `windows-msvc-debug` | `windows-latest` | `windows-msvc-debug` | yes | the default developer configuration on MSVC: asserts on, `/W4 /WX`, every unit test |
| `windows-msvc-dev` | `windows-latest` | `windows-msvc-dev` | yes, plus the export smoke | the optimised Development configuration: nothing depends on unoptimised code order or uninitialised memory that Debug hides; `conjure_editor --headless --export` of `tests/assets/projects/minimal` booted by `conjure_runtime --headless --frames 120` |
| `linux-gcc-debug` | `ubuntu-24.04` | `linux-gcc-debug` | yes | GCC 13 with `-Wall -Wextra -Wpedantic ... -Werror`, libstdc++, POSIX backends (`*_posix.cpp`) |
| `linux-clang-sanitize` | `ubuntu-24.04` | `linux-clang-sanitize` with `-DCONJURE_WITH_SCRIPTING=OFF` | yes | Clang with AddressSanitizer and UndefinedBehaviorSanitizer; leak detection on; any report fails the test; scripting is off because CoreCLR is not sanitizer-clean (D13) |
| `macos-debug` | `macos-14` | `macos-debug` | yes | AppleClang, libc++, Darwin-specific paths (`@loader_path` in the dynamic-library tests) |
| `style-and-headers` | `ubuntu-24.04` | `linux-gcc-debug` (header check only) | no | `check_style.py --tests-mirror-headers`, `check_exports.py` and its self-test, the `check_distribution_symbols.py` and `ctest_check_skips.py` self-tests, `check_docs.py`, `tools/tests/test_check_docs.py`, `mkdocs build --strict`, the `conjure_header_check` target, `dotnet format --verify-no-changes` on the three managed projects (blocking; the last step, run after a failed step too) |

Formatting has two gates and no advisory step (`docs/design/08_testing.md` D17). C++ layout is checked by
`tools/check_style.py` alone: no workflow runs clang-format, because `.clang-format` cannot express the two-space
continuation indent of `docs/CODE_STYLE.cpp`; the file stays in the repository as an optional local aid. C# layout
is checked by `dotnet format --verify-no-changes` on `Conjure.Managed`, `Conjure.Bootstrap` and
`Conjure.TestScripts`, with the rules of the `[*.cs]` section of `.editorconfig`; a difference fails the job.
The step is the last of `style-and-headers` and runs unless the run was cancelled, so it and the header check
cannot hide each other, and all three projects report before it fails. Its formatter is the one of the SDK that
`actions/setup-dotnet` installed, not the newest SDK of the runner image, which `dotnet` would pick by itself: when
the checkout has no `global.json`, the step writes one that names the version the setup step reports
(`dotnet-version` output, `rollForward` `disable`: that SDK or an error) and prints the file and `dotnet --version`.
A `global.json` already in the checkout is left alone and selects the SDK instead.

Together the five test legs cover the fixed matrix of `docs/design/08_testing.md` D15: MSVC, GCC, Clang,
macOS and sanitizers; the Distribution configuration is built and unit-tested nightly (section 6). The style
and documentation checks also run inside every test leg because `tests/CMakeLists.txt` registers
`style.check`, `docs.check` and `docs.check_tool` as CTest entries, and the header check runs inside every
build because
`conjure_header_check` is part of `ALL`. Every leg stays headless: the `rhi` and `renderer` `[gpu]` cases, the
`audio` `[audio_device]` cases and the `platform` `[window]` cases guard themselves with `CONJURE_TEST_REQUIRE`
and report *Not Run* where the runner lacks the software GPU adapter, a playback endpoint or a display, and the
skip check allows exactly those tags per leg (`[gpu]` on Linux and macOS only: Windows runners have the D3D12
WARP adapter, so the `[gpu]` cases run there); the `[dotnet]` cases run because `actions/setup-dotnet` installs the SDK
([Testing](testing.md#6-preconditions-and-skipping)).

## 3. Steps of every job

| Step | Action | Notes |
|---|---|---|
| Checkout | `actions/checkout@v4` with `lfs: true` | the LFS fixtures under `tests/assets` |
| .NET | `actions/setup-dotnet@v4` with `8.0.x` | `cmake/FindNetHost.cmake`, the managed build and the `[dotnet]` cases; in `style-and-headers` the step has the id `dotnet` and its `dotnet-version` output names the SDK of the `dotnet format` step |
| Python | `actions/setup-python@v5` with `3.12` | the `tools/*.py` checks and `download_test_assets.py` |
| Linux packages | `sudo apt-get install -y ninja-build xorg-dev libwayland-dev libxkbcommon-dev libgl1-mesa-dev libasound2-dev libpulse-dev libvulkan-dev` | GLFW (X11 + Wayland), miniaudio and the Vulkan loader |
| macOS packages | `brew install ninja` | |
| MSVC | `ilammy/msvc-dev-cmd@v1` with `arch: x64` | the equivalent of `vcvars64.bat`, which `tools/build.py` does locally |
| Dependency cache | `actions/cache@v4` on `build/<preset>/_deps`, key `deps-<leg>-<hash of third_party/CMakeLists.txt>` | changing a pin invalidates the cache; nothing else does |
| Test asset cache and download | `actions/cache@v4` on `tests/.downloaded` and `tests/.assets_ready`, key from `tools/test_assets.json`; `python tools/download_test_assets.py` | the `downloaded` label runs on every leg; every file is verified against the committed `tools/test_assets.lock` (a mismatch fails the step; a file the lock does not know yet is pinned and reported, and the lock is committed with the change) |
| Configure | `cmake --preset <preset> <configure-args>` | |
| Build | `cmake --build --preset <preset> --parallel` | warnings are errors |
| Test | `ctest --preset <preset> --output-junit junit.xml` | the preset's label filter excludes `benchmark|golden|downloaded|slow|process`; output on failure |
| Downloaded-asset and process tests | `ctest --test-dir build/<preset> -L "downloaded|process" --no-tests=error --output-junit junit_labels.xml` | the runtime and editor process tests and the fixtures the download step fetched; a label that matches no test fails the step instead of passing with nothing run |
| Check skips | `python tools/ctest_check_skips.py --build-dir ... --junit junit.xml --junit junit_labels.xml <skip-tags>` | reads both reports and fails on any *Not Run* outside `CONJURE_TEST_ALLOWED_SKIPS` and the leg's name tags: `window`, `downloaded`, `audio_device` everywhere, plus `gpu` on Linux and macOS (D12) |
| Export smoke | Development leg only: copy `tests/assets/projects/minimal`, `conjure_editor --headless --export`, `conjure_runtime --headless --frames 120` | the wave-4 exit criterion of `ARCHITECTURE.md` 11; `samples/feature_test` replaces the fixture when wave 5 lands it |
| Upload test reports | `actions/upload-artifact@v4` on failure: `junit.xml`, `junit_labels.xml`, `test_output/` | golden diffs and the JUnit reports of a red leg |

## 4. Sanitizer options

The `linux-clang-sanitize` preset sets `CONJURE_ENABLE_ASAN=ON` and `CONJURE_ENABLE_UBSAN=ON`
(`cmake/ConjureSanitizers.cmake` adds `-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer` to
every Conjure target; third-party libraries are not instrumented, they are built with their own flags and
only share the sanitizer runtime through the final link, so a bug inside Jolt or yyjson is reported only
when it touches instrumented memory). The test step exports:

```
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
```

`halt_on_error=1` makes the first report abort the process, so a sanitizer finding is a failed CTest entry
with the report in the output, never a warning that scrolls past. `detect_leaks=1` runs LeakSanitizer at
exit; because every test process is one case, the leaking case is named.

> [!NOTE]
> `docs/design/08_testing.md` D13 configures the sanitizer leg with `-DCONJURE_WITH_SCRIPTING=OFF` because
> CoreCLR is not sanitizer-clean; the workflow passes the flag as `configure-args` (the preset itself keeps
> scripting on so the same preset is usable with it locally). The same note names suppression files under
> `tests/sanitizers/`; none exist yet.

Other sanitizer presets: `windows-msvc-asan` (MSVC `/fsanitize=address`, not in CI) and `linux-clang-tsan`
(ThreadSanitizer, scripting off, restricted to the labels `core|ecs|scene|engine|integration`), which the
nightly `linux-clang-tsan` job runs with `TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1`.

## 5. Reproducing a failure locally

1. **Match the leg.** Use the leg's preset name; `tools/build.py` accepts it on any host where the preset's
   condition holds. Run from a clean tree if the leg configured from scratch (`--clean`).

   | Leg | Local command |
   |---|---|
   | `windows-msvc-debug` | `python tools/build.py --clean --test` |
   | `windows-msvc-dev` | `python tools/build.py --preset windows-msvc-dev --clean --test` |
   | `linux-gcc-debug` | `python tools/build.py --preset linux-gcc-debug --clean --test` |
   | `linux-clang-sanitize` | `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 python tools/build.py --preset linux-clang-sanitize --clean --test -- -DCONJURE_WITH_SCRIPTING=OFF` |
   | `macos-debug` | `python tools/build.py --preset macos-debug --clean --test` |
   | `style-and-headers` | `python tools/check_style.py --tests-mirror-headers && python tools/check_exports.py && python tools/check_docs.py && python tools/docs.py build`, then `dotnet format --verify-no-changes managed/<project>/<project>.csproj` for `Conjure.Managed`, `Conjure.Bootstrap` and `Conjure.TestScripts` |
   | nightly `distribution-export` | `python tools/build.py --preset windows-msvc-dist --clean --test` (the unit suite without asserts), then `python tools/check_distribution_symbols.py --build-dir build/windows-msvc-dist` |

   Compilers on the runners: MSVC from `windows-latest`, GCC 13 on `ubuntu-24.04` (`g++`), the distribution
   Clang on `ubuntu-24.04` (`clang++`), AppleClang on `macos-14`. A warning that only one compiler emits is
   still an error; read the first `error:` / `error C` line of the failing leg and fix the code.

2. **Find the case.** The CTest summary names the entry: `<module>.<case>`. Rerun it alone:

   ```bash
   ctest --preset linux-clang-sanitize -R "core_jobs.job_system - wait" --output-on-failure
   build/linux-clang-sanitize/bin/conjure_core_jobs_tests -tc="job_system - wait*" -s
   ```

   The direct run prints doctest's full report and the log captured during the failing case. For a
   `[parallel]` case, vary the worker count (`--conjure-threads=0`, `2`, `8`) and the seed printed in the
   CI log (`seed=<n>`) with `--conjure-seed=<n>`.

3. **Style and header failures.** `style.check` output is `path:line: rule: message`; run
   `python tools/check_style.py --tests-mirror-headers` locally. A `<target>_header_check` compile error means
   a public header does not compile alone: add the include it is missing. A `dotnet format` failure names the
   file, line and column of every difference (`error WHITESPACE: Fix whitespace formatting`); the same command
   without `--verify-no-changes` rewrites the files, and the diff it leaves is the fix to review and commit.
   CI runs the formatter of the 8.0 SDK that `actions/setup-dotnet` installed (section 2); the step's log shows the
   `global.json` it used and the output of `dotnet --version`. The repository pins no SDK yet (code sweep F778), so
   a local `dotnet` is the newest SDK installed and may format differently: reproduce a CI-only difference with
   the version the step printed, selected through a `global.json` in a copy of `managed/` and `.editorconfig`
   outside the repository.

4. **Sanitizer reports.** The report names the allocation and the access with stack traces; symbolisation
   works out of the box on the runners. Locally, `-fno-omit-frame-pointer` is already set; install `llvm-symbolizer`
   if frames show as addresses. A leak reported at exit in a `memory_guard`-clean test usually comes from a
   process-wide singleton created by the test (a log sink, a job system) that is never destroyed.

5. **Not Run.** An entry marked *Not Run* exited with 77: a precondition was not met. On the PR legs the
   guarded cases are *Not Run* by design and allowed by their case-name tag: `[gpu]` (`SOFTWARE_GPU_ADAPTER`) on
   Linux and macOS, `[audio_device]` (`AUDIO_DEVICE`) and `[window]` (`REAL_WINDOW`) everywhere, and
   `[downloaded]`; `ctest_check_skips.py` fails the leg on any other *Not Run*, which means a test was added
   without its precondition probe, label or executable, a runner lost something it was set up to have (a
   `[gpu]` skip on Windows: no WARP adapter; any `[dotnet]` skip), or a `[process]` case did not find its
   executable (the label run writes `junit_labels.xml`, which the check reads too). Reproduce with
   `python tools/ctest_check_skips.py --build-dir build/<preset> --junit <file> [--junit <file>]` and the leg's
   `--allow-tag` list from `ci.yml` after `ctest --output-junit`.

## 6. The nightly workflow

`.github/workflows/nightly.yml` runs at 03:00 UTC (`schedule`) and on `workflow_dispatch`, with
`CONJURE_TEST_ALLOWED_SKIPS` empty: a nightly may skip nothing by label. Nothing in it gates a pull request; a
red nightly is a ticket for the module owner ([08_testing.md 7.3](../design/08_testing.md#73-ci-githubworkflowsciyml-push-to-main-and-pull-requests-fail-fast-false-60-min)).

| Job | Runner | What it runs |
|---|---|---|
| `full-matrix-nightly` | the five PR legs | the `*-nightly` test presets where one exists (`windows-msvc-debug-nightly`, `linux-gcc-debug-nightly`), otherwise `ctest -LE benchmark`: the `slow`, `process` and `downloaded` cases included; skips checked with `--allow-tag window --allow-tag audio_device`, plus `--allow-tag gpu` on Linux and macOS (the downloaded cases must run) |
| `linux-clang-tsan` | `ubuntu-24.04` | the `linux-clang-tsan` preset (ThreadSanitizer, scripting off, labels `core|ecs|scene|engine|integration`) |
| `distribution-export` | `windows-latest`, `ubuntu-24.04` | builds `windows-msvc-dist` / `linux-gcc-dist` and the Development editor and runtime, runs the Distribution `process|runtime` tests with `--no-tests=error` (`runtime.distribution_symbols` among them), exports `tests/assets/projects/minimal` with the Development editor, copies the Distribution runtime, `conjure_shared` and (Windows) `nethost.dll` into the folder, boots it for 300 frames and runs `check_distribution_symbols.py --binary` on it; then the whole unit suite in Distribution, `ctest --preset <dist-preset> --output-junit junit_dist.xml`, with its own skip check (below) |
| `golden-d3d12-warp` | `windows-latest` | `windows-msvc-dev` with `-DCONJURE_TEST_GOLDEN_IMAGES=ON`, `ctest -L golden --no-tests=error` on WARP; diffs uploaded on failure |
| `golden-vulkan-lavapipe` | `ubuntu-24.04` | `linux-gcc-dev` with golden images on lavapipe (`VK_DRIVER_FILES`), the same `ctest -L golden --no-tests=error`; `continue-on-error` until the references are stable for two weeks (D11) |
| `benchmarks-ci-class` | `windows-latest`, `ubuntu-24.04` | `conjure_benchmarks --class ci --enforce --out bench.json` on the Development presets; the table goes to the step summary and the JSON to an artefact (D10), uploaded on failure too; a `status=SKIPPED` row fails the job after the summary is written (D12); nothing is pushed anywhere (`permissions: contents: read`) |

The Distribution unit suite (`docs/design/08_testing.md` D16) is the only run of the unit tests with asserts
and TRACE/DEBUG logging compiled out, so it is where code that leans on an assert's side effect fails. The test
executables are part of the Distribution build the job does anyway. The step runs last and depends on that build
alone (`steps.build-dist.outcome == 'success'`): a red suite does not hide the export round trip, and a red
export does not hide the suite. Its skip check runs whether the suite passed or failed (ctest writes
`junit_dist.xml` in both cases), so a red night also reports its unexpected skips. The check allows no label and
the tags `window`, `audio_device` and `downloaded` (the job fetches no downloaded assets, and the preset excludes
the `downloaded` label), plus `gpu` on Linux; `junit_dist.xml` and `test_output` are uploaded as
`distribution-reports-<os>` when the job fails.

The configure presets no workflow uses are for local work only: `windows-msvc-asan`, `windows-vs`,
`linux-clang-debug`, `macos-dev` and `macos-dist`.

> [!NOTE]
> No CTest entry carries the `golden` label yet: `tests/golden` does not exist and off-screen rendering is not
> functional (`09_cross_area_resolutions.md` 4 item 15). The golden steps run with `--no-tests=error`, so until
> the cases land both jobs fail with "No tests were found" rather than passing with zero image comparisons (the
> lavapipe job stays `continue-on-error`). Every `ctest -L` run in both workflows carries the flag for the same
> reason: a label that matches nothing is a broken registration, not a pass.

## Related

- design record: `docs/design/08_testing.md` sections 7.3 (CI), 3.2 (skip discipline), D13 (sanitizers and scripting),
  D16 (Distribution unit suite), D17 (formatting gates)
- `docs/ARCHITECTURE.md` section 9 (CI row of the build matrix)
- `.github/workflows/ci.yml`, `.github/workflows/nightly.yml`, `CMakePresets.json`, `cmake/ConjureSanitizers.cmake`,
  `tools/ctest_check_skips.py`, `tools/check_distribution_symbols.py`
- [Getting started](getting-started.md), [Testing](testing.md), [Build pipeline](build-pipeline.md)
