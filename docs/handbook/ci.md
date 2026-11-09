# Continuous integration

🛠 **Engineering handbook**

`.github/workflows/ci.yml` builds and tests every push to `main` and every pull request on Windows, Linux
and macOS with the same presets developers use locally. This chapter lists the matrix, what each leg
proves, the sanitizer settings, and how to reproduce a red leg on your machine.

## Contents

- [Workflow](#1-workflow)
- [The matrix](#2-the-matrix)
- [Steps of every job](#3-steps-of-every-job)
- [Sanitizer options](#4-sanitizer-options)
- [Reproducing a failure locally](#5-reproducing-a-failure-locally)
- [Planned jobs](#6-planned-jobs)

## 1. Workflow

- Triggers: `push` to `main` and `pull_request`.
- `concurrency: ci-${{ github.ref }}` with `cancel-in-progress: true`: a new push cancels the previous run of
  the same branch.
- One job `build-test` with a matrix and `fail-fast: false`, so every leg reports even when one fails.
- Secrets, deployments and artefact uploads are not used today.

## 2. The matrix

| Leg | Runner | Preset | Tests | Proves |
|---|---|---|---|---|
| `windows-msvc-debug` | `windows-latest` | `windows-msvc-debug` | yes | the default developer configuration on MSVC: asserts on, `/W4 /WX`, every unit test |
| `windows-msvc-dev` | `windows-latest` | `windows-msvc-dev` | yes | the optimised Development configuration: nothing depends on unoptimised code order or uninitialised memory that Debug hides |
| `linux-gcc-debug` | `ubuntu-24.04` | `linux-gcc-debug` | yes | GCC 13 with `-Wall -Wextra -Wpedantic ... -Werror`, libstdc++, POSIX backends (`*_posix.cpp`) |
| `linux-clang-sanitize` | `ubuntu-24.04` | `linux-clang-sanitize` | yes | Clang with AddressSanitizer and UndefinedBehaviorSanitizer; leak detection on; any report fails the test |
| `macos-debug` | `macos-14` | `macos-debug` | yes | AppleClang, libc++, Darwin-specific paths (`@loader_path` in the dynamic-library tests) |
| `windows-msvc-dist` | `windows-latest` | `windows-msvc-dist` | build only | the Distribution configuration compiles with asserts and TRACE/DEBUG logging compiled out (`CONJURE_DISTRIBUTION=1`); tests are not run on this leg |

Together the five test legs cover the fixed matrix of `docs/design/08_testing.md` D15: MSVC, GCC, Clang,
macOS and sanitizers. The style and documentation checks run inside every test leg because
`tests/CMakeLists.txt` registers `style.check`, `docs.check` and `docs.check_tool` as CTest entries, and the
header check runs inside every build because `conjure_header_check` is part of `ALL`. The wave-2 modules keep
every leg headless: the `rhi`, `audio` and `platform` cases that need a software GPU adapter, an audio device or
a real window guard themselves with `CONJURE_TEST_REQUIRE` and report *Not Run* on the runners
([Testing](testing.md#6-preconditions-and-skipping)).

## 3. Steps of every job

| Step | Action | Notes |
|---|---|---|
| Checkout | `actions/checkout@v4` | |
| .NET | `actions/setup-dotnet@v4` with `8.0.x` | needed by `cmake/FindNetHost.cmake` while `CONJURE_WITH_SCRIPTING` is on |
| Linux packages | `sudo apt-get install -y ninja-build xorg-dev libwayland-dev libxkbcommon-dev libgl1-mesa-dev libasound2-dev libpulse-dev` | GLFW (X11 + Wayland) and miniaudio build dependencies |
| macOS packages | `brew install ninja` | |
| MSVC | `ilammy/msvc-dev-cmd@v1` with `arch: x64` | the equivalent of `vcvars64.bat`, which `tools/build.py` does locally |
| Dependency cache | `actions/cache@v4` on `build/<preset>/_deps`, key `deps-<leg>-<hash of third_party/CMakeLists.txt>` | changing a pin invalidates the cache; nothing else does |
| Configure | `cmake --preset <preset>` | |
| Build | `cmake --build --preset <preset> --parallel` | warnings are errors |
| Test | `ctest --preset <preset>` unless `build-only` | the preset's label filter excludes `benchmark|golden|downloaded|slow|process`; output on failure |

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
> CoreCLR is not sanitizer-clean. The preset does not pass it yet; the scripting module does not exist, so
> nothing is compiled under the sanitizers that would need it. Add the flag to the preset when the module
> lands. The same note names suppression files under `tests/sanitizers/`; none exist yet.

Other sanitizer presets: `windows-msvc-asan` (MSVC `/fsanitize=address`, not in CI) and `linux-clang-tsan`
(ThreadSanitizer, scripting off, restricted to the labels `core|ecs|scene|engine|integration`; nightly by
design, no workflow yet).

## 5. Reproducing a failure locally

1. **Match the leg.** Use the leg's preset name; `tools/build.py` accepts it on any host where the preset's
   condition holds. Run from a clean tree if the leg configured from scratch (`--clean`).

   | Leg | Local command |
   |---|---|
   | `windows-msvc-debug` | `python tools/build.py --clean --test` |
   | `windows-msvc-dev` | `python tools/build.py --preset windows-msvc-dev --clean --test` |
   | `windows-msvc-dist` | `python tools/build.py --preset windows-msvc-dist --clean` (then `--test` with the same preset to go beyond CI) |
   | `linux-gcc-debug` | `python tools/build.py --preset linux-gcc-debug --clean --test` |
   | `linux-clang-sanitize` | `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 python tools/build.py --preset linux-clang-sanitize --clean --test` |
   | `macos-debug` | `python tools/build.py --preset macos-debug --clean --test` |

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
   a public header does not compile alone: add the include it is missing.

4. **Sanitizer reports.** The report names the allocation and the access with stack traces; symbolisation
   works out of the box on the runners. Locally, `-fno-omit-frame-pointer` is already set; install `llvm-symbolizer`
   if frames show as addresses. A leak reported at exit in a `memory_guard`-clean test usually comes from a
   process-wide singleton created by the test (a log sink, a job system) that is never destroyed.

5. **Not Run.** An entry marked *Not Run* exited with 77: a precondition was not met. On the PR legs no
   precondition is expected to hold, so the guarded `rhi` (`SOFTWARE_GPU_ADAPTER`), `audio` (`AUDIO_DEVICE`,
   `DOWNLOADED_ASSETS`) and `platform` (`REAL_WINDOW`) cases are *Not Run* by design; any other new *Not Run*
   means a test was added without the matching `downloaded`/`slow` label or its executable.

## 6. Planned jobs

Everything in `.github/workflows/ci.yml` today is the `build-test` matrix above. The design record adds:

| Job | Where | Purpose |
|---|---|---|
| `style-and-headers` | PR, `ubuntu-24.04` | `check_style.py`, `clang-format-18 --dry-run -Werror`, `cmake --build --target conjure_header_check`, `dotnet format --verify-no-changes managed/` ([08_testing.md 7.3](../design/08_testing.md#73-ci-githubworkflowsciyml-push-to-main-and-pull-requests-fail-fast-false-60-min)) |
| `docs` | PR | `mkdocs build --strict` ([16_documentation.md section 4](../design/16_documentation.md#4-tooling)); `docs.check` already runs as a CTest entry in every test leg |
| `ctest_check_skips.py` step | every test leg | fail on any *Not Run* whose label is not in `CONJURE_TEST_ALLOWED_SKIPS` (D12) |
| JUnit and `test_output` upload | every test leg | `ctest --output-junit junit.xml`, golden diffs on failure |
| `nightly.yml` | cron 03:00 UTC | `full-matrix-nightly` (`*-nightly` presets, `slow` included), `linux-clang-tsan`, `distribution-export`, `golden-d3d12-warp`, `golden-vulkan-lavapipe`, `benchmarks-ci-class` |

> [!NOTE]
> Planned, not implemented. Keep this table in step with the workflow files when they land.

## Related

- design record: `docs/design/08_testing.md` sections 7.3 (CI), 3.2 (skip discipline), D13 (sanitizers and scripting)
- `docs/ARCHITECTURE.md` section 9 (CI row of the build matrix)
- `.github/workflows/ci.yml`, `CMakePresets.json`, `cmake/ConjureSanitizers.cmake`
- [Getting started](getting-started.md), [Testing](testing.md), [Build pipeline](build-pipeline.md)
