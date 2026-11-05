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
python tools/build.py --target conjure_core_tests --test -R core # one module
python tools/build.py --no-configure                             # skip the configure step
python tools/build.py --clean                                    # wipe build/<preset> first
python tools/build.py --configure-only -- -DCONJURE_WITH_PHYSICS=OFF
```

Build output lands in `build/<preset>/bin` (executables, shaders, managed assemblies) and
`build/<preset>/lib`.

## Running a single test executable

```bash
build/windows-msvc-debug/bin/conjure_core_tests.exe -ts="core.result"   # one suite
build/windows-msvc-debug/bin/conjure_core_tests.exe -tc="*uuid*"        # test cases by pattern
build/windows-msvc-debug/bin/conjure_core_tests.exe -s                  # show successful asserts
```

doctest flags: `-ts` suite filter, `-tc` test case filter, `-s` success output, `-d` durations.

## Parallel agents

Builds in the same directory must not overlap. If another agent may be building, use a private
build directory from a developer shell:

```bash
cmake --preset windows-msvc-debug -B build/<my-name>
cmake --build build/<my-name> --target conjure_<module>_tests
ctest --test-dir build/<my-name> -R <module>
```

(On Windows run these inside a shell where `vcvars64.bat` has been imported, or prefix with
`python tools/build.py --configure-only` once to verify the environment works.)

## Reading failures

- `error C2xxx` / `error:` lines are compiler errors. Fix the code; never weaken warnings.
- `ninja: build stopped` after errors is normal. Scroll up for the first error.
- `Could not find the .NET host pack` means the .NET 8+ SDK is missing; install it or configure
  with `-DCONJURE_WITH_SCRIPTING=OFF`.
- ctest output `***Failed` names the executable. Run it directly for doctest's detailed report.
- Unexpected `CONJURE_ASSERT` failures inside tests are reported as doctest failures with the
  source location of the assert. Use `conjure::test::assert_capture` when a test expects one.

## CI

`.github/workflows/ci.yml` runs: Windows MSVC Debug and Development (tests), Windows MSVC
Distribution (build only), Ubuntu 24.04 GCC Debug (tests), Ubuntu 24.04 Clang Debug with ASan +
UBSan (tests, sanitizer failures are fatal), macOS Debug (tests). Keep every leg green.
