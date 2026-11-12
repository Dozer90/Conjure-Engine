---
name: build-and-test
description: Configure, build and run the Conjure engine tests on any platform. Use before reporting any change as done, when a build fails, or when asked how to build.
---

# Build and test Conjure

## One command

```bash
python tools/build.py --test
```

Configures the default preset for the host (`windows-msvc-debug`, `linux-gcc-debug`,
`macos-debug`), builds everything and runs `ctest`. On Windows the script imports the MSVC
environment itself, so it works from PowerShell, cmd or Git Bash.

Useful variants:

```bash
python tools/build.py --preset windows-msvc-dev --test          # Development configuration
python tools/build.py --target conjure_core_math_tests --test -R core_math   # one test module
python tools/build.py --no-configure                             # skip the configure step
python tools/build.py --clean                                    # wipe build/<preset> first
python tools/build.py --configure-only -- -DCONJURE_WITH_PHYSICS=OFF
python tools/build.py --test -L style                            # only the style check
```

Every doctest `TEST_CASE` is one CTest entry named `<module>.<case>` (for example
`core_math.transform - to_matrix equals translate * rotate * scale`), so `ctest -R` filters by
module or by case name and `ctest -L <module>` by label. The default test presets exclude the
labels `benchmark|golden|downloaded|slow|process`. An unmet precondition exits 77 and CTest reports
the entry as *Not Run*.

Build output lands in `build/<preset>/bin` (executables, shaders, managed assemblies) and
`build/<preset>/lib`.

## Running a single test executable

```bash
build/windows-msvc-debug/bin/conjure_core_foundation_tests.exe -ts="core/result"   # one suite
build/windows-msvc-debug/bin/conjure_core_foundation_tests.exe -tc="uuid - *"      # cases by pattern
build/windows-msvc-debug/bin/conjure_core_foundation_tests.exe -s                  # show successful asserts
build/windows-msvc-debug/bin/conjure_core_jobs_tests.exe --conjure-threads=0 --conjure-seed=0x1234
```

doctest flags: `-ts` suite filter, `-tc` test case filter, `-s` success output, `-d` durations.
Conjure flags (stripped before doctest sees them): `--conjure-seed=N`, `--conjure-threads=N`,
`--conjure-isolated` (what CTest passes: unmet preconditions exit 77 instead of returning),
`--conjure-update-golden`; the environment variables `CONJURE_TEST_SEED`, `CONJURE_TEST_THREADS`
and `CONJURE_TEST_WINDOW=1` are the fallback. Every run prints `seed=<n> threads=<n>` first. Passing
tests print nothing else: the console log sink is replaced by a ring buffer that is dumped only when
a case fails.

## Parallel agents

Builds in the same directory must not overlap. If another agent may be building, use a private
build directory:

```bash
python tools/build.py --build-dir build/<my-name> --target conjure_<module>_tests --test -R <module>
```

Subsequent runs add `--no-configure` to skip the (slow) configure step. The equivalent raw
commands, for a shell where `vcvars64.bat` is already imported, are `cmake --preset <preset> -B
build/<my-name>`, `cmake --build build/<my-name> --target <target>` and
`ctest --test-dir build/<my-name> -R <module>`.

## Reading failures

- `error C2xxx` / `error:` lines are compiler errors. Fix the code; never weaken warnings.
- `ninja: build stopped` after errors is normal. Scroll up for the first error.
- `Could not find the .NET host pack` means the .NET 8+ SDK is missing; install it or configure
  with `-DCONJURE_WITH_SCRIPTING=OFF`.
- ctest output `***Failed` names the exact case. Run the executable directly with
  `-tc="<case name>"` for doctest's detailed report (and the captured log of the failing case).
- Unexpected `CONJURE_ASSERT` failures inside tests are reported as doctest failures with the
  source location of the assert (and abort the process under CTest or on a worker thread). Use
  `conjure::testing::expect_assert` or `CONJURE_CHECK_ASSERTS(statement)` when a test expects one.
- `log_capture: unexpected ERROR logged` means the case logged an ERROR without calling
  `allow_errors()` on its `log_capture`; `memory_guard: N allocation(s) still live` is a leak.
- `style.check` failures list `path:line: rule: message`; run `python tools/check_style.py
  --tests-mirror-headers` locally. A new public header needs `tests/<module>/test_<name>.cpp`.
- `<target>_header_check` compile errors mean a public header does not compile on its own: add the
  include it is missing.

## CI

`.github/workflows/ci.yml` runs on every push to `main` and every pull request: `windows-msvc-debug` and
`windows-msvc-dev` (tests; the Development leg also runs the export smoke), `linux-gcc-debug` (tests),
`linux-clang-sanitize` (Clang Debug with ASan + UBSan, scripting off; sanitizer failures are fatal),
`macos-debug` (tests) and `style-and-headers` (check_style, check_exports, check_docs, mkdocs, header
check). Every build leg runs its test preset and then a second `ctest -L "downloaded|process"` pass, and
the skip check covers both. The Distribution build, the `slow` cases (the `*-nightly` test presets exclude
only `benchmark`), TSan, golden images and benchmarks run only in `.github/workflows/nightly.yml`
(`distribution-export`, `full-matrix-nightly`, `linux-clang-tsan`, `golden-d3d12-warp` /
`golden-vulkan-lavapipe`, `benchmarks-ci-class`); see `docs/handbook/ci.md`. Keep every leg green.
