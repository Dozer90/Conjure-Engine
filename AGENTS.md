# Conjure Engine - Agent Guide

Conjure is a production-grade, data-oriented 2D/3D game engine in C++20 with a C# scripting
layer, an AI-controllable editor and a standalone runtime. This file is the entry point for
every AI agent (and human) working in the repository. Read it fully before touching code.

## 1. Where things are

| Need | Read |
|---|---|
| Customer requirements | `docs/DESIGN_DOC.md` |
| Overall architecture, module map, wave plan | `docs/ARCHITECTURE.md` |
| Per-area designs (ECS, renderer, scripting, editor, assets, loop, modules, testing) | `docs/design/*.md` |
| Code style (authoritative) | `docs/CODE_STYLE.cpp` |
| Engineering conventions (naming, headers, errors, tests, CMake, git) | `docs/CONVENTIONS.md` |
| Public API wiki (📘, for people making games with the engine) | `docs/api/`, start at `docs/api/index.md` |
| Engineering handbook (🛠, for people working in the source) | `docs/handbook/`, start at `docs/handbook/index.md` |
| Documentation system (two sets, badges, alert boxes, page templates, checks) | `docs/design/16_documentation.md` |
| How to write and check a wiki or handbook page | `.claude/skills/write-docs/SKILL.md` |
| How to build and test | `.claude/skills/build-and-test/SKILL.md` |
| How to add a module | `.claude/skills/add-engine-module/SKILL.md` |
| How to add a component | `.claude/skills/add-component/SKILL.md` |
| Review checklist before every commit | `.claude/skills/code-review-checklist/SKILL.md` |
| Driving the editor as an agent | `.claude/skills/editor-ai-control/SKILL.md` (the command catalogue itself: `docs/generated/editor_protocol.json`, wiki page `docs/api/editor/ai-protocol.md`) |
| Benchmarks (harness, scenarios, thresholds, how to run them) | `tests/benchmarks/README.md`, `docs/handbook/testing.md` section 11 |
| Module internals | `engine/src/<module>/README.md` |

## 2. Non-negotiable rules

1. **Never read or copy code from outside this repository on the local machine.** Public
   documentation of third-party libraries on the web is fine.
2. **Production quality only.** No stubs left behind, no `TODO` without an issue reference, no
   commented-out code, no "temporary" hacks. If something cannot be finished, say so explicitly in
   your report instead of hiding it.
3. **Follow `docs/CODE_STYLE.cpp` and `docs/CONVENTIONS.md` exactly.** snake_case types and
   functions, camelCase locals, `m` prefix members, ALL_CAPS constants, braces always, early-out,
   120 columns max, every variable initialised, one declaration per line.
4. **Engine code never throws.** Use `conjure::core::result<T>` / `status` and `CONJURE_ASSERT`.
5. **Every module ships tests** (`tests/<module>/`), every public type is covered, tests are
   headless (null backends) and pass under `/W4 /WX` and `-Wall -Wextra -Werror`.
6. **Warnings are errors.** Do not disable a warning to make a build pass; fix the code. If a
   third-party header is at fault, include it through the `SYSTEM` mechanism already in place.
7. **Build and run the tests before reporting done.** `python tools/build.py --test` (add
   `--preset <name>` to pick a configuration). A report that says "should compile" is a failure.
8. **Review before commit.** Run the checklist in `code-review-checklist`. Commits must build
   and pass tests on their own.
9. **No globbing in CMake, no new third-party dependencies** without updating
   `third_party/CMakeLists.txt` or a file it includes (`jxl.cmake`) with a pinned tag, commit or archive SHA-256
   and a `conjure::thirdparty::` alias, and its licence notice (`conjure_third_party_notice()` beside the pin; the
   configure fails without it only for a declaration in `third_party/CMakeLists.txt` itself, so an included file
   needs tests that check its notices, as `tests/third_party` does for `jxl.cmake`; then
   `python tools/third_party_notices.py --build-dir <tree>`; CTest `notices.up_to_date` fails until then).
10. **Platform code stays behind backends.** No `#ifdef _WIN32` outside `platform_detect.h` and
    backend implementation files. Everything must build on Windows, Linux and macOS.

## 3. Repository layout

```
engine/include/conjure/<module>/   public headers           engine/src/<module>/   implementation
editor/                            conjure_editor           runtime/               conjure_runtime
managed/                           C# SDK + templates       tests/<module>/        doctest tests
samples/                           sample projects          tools/                 build.py, scripts
                                   (feature_test: wave 5)
third_party/                       pinned dependencies      cmake/                 build helpers
docs/                              design + conventions     .claude/skills/        agent skills
```

Modules (dependency order): `core -> ecs, platform, rhi, assets, audio, modules -> scene,
renderer, physics, scripting -> engine -> editor, runtime`.

## 4. Working on a module

1. Read `docs/ARCHITECTURE.md`, the module's design doc in `docs/design/`, and the README of
   every module you depend on.
2. Public headers go in `engine/include/conjure/<module>/`; everything else in
   `engine/src/<module>/`. Register files explicitly in the module's `CMakeLists.txt`
   (`conjure_add_module`).
3. Write the tests alongside the code in `tests/<module>/test_<header>.cpp`, one file per public
   header, registered with `conjure_add_test_module(<module> SOURCES ...)` from
   `cmake/ConjureTesting.cmake`. Every file opens with `#include "conjure/testing/doctest.h"` and
   `TEST_SUITE("<module>/<header>")`; cases are named `"<type> - <behaviour>"` and are unique per
   executable (each one is its own CTest entry). The support library (`tests/support`, namespace
   `conjure::testing`) provides `log_capture`, `expect_assert` / `CONJURE_CHECK_ASSERTS`,
   `memory_guard`, `allocation_scope` / `CONJURE_CHECK_NO_ALLOCATIONS`, `temp_directory`,
   `CONJURE_REQUIRE_OK` / `CONJURE_CHECK_ERROR`, `CONJURE_CHECK_APPROX` and
   `CONJURE_TEST_REQUIRE(precondition::...)`; see `docs/CONVENTIONS.md` section 6. Aim for
   behaviour coverage: construction, happy path, failure path, edge cases, threading where relevant.
4. Build the Debug preset and run the tests. Fix every warning. Run `python tools/check_style.py
   --tests-mirror-headers` (also the CTest entry `style.check`); it must report zero findings.
5. Update `engine/src/<module>/README.md`.
6. Run the review checklist, then commit with a `module: summary` subject.

When several agents work in parallel, each owns one module directory and its test directory.
Never edit another module's files; if you need an API change there, write it down in your report.

Concurrent builds must use separate build directories: pass `--build-dir build/<your-name>` to
`tools/build.py` (for example `python tools/build.py --build-dir build/core_math --target
conjure_core_math_tests --test -R core_math`; add `--no-configure` on later runs).

Private build directories are temporary: a full Debug tree is 11 to 13 GB, and stale ones once filled 360 GB.
Reuse the same names across rounds of parallel work instead of inventing new ones. Whoever creates a private
directory removes it once no further round will use it (an agent that starts other agents removes theirs too):
`python tools/prune_builds.py <name> ... --delete`. `python tools/prune_builds.py` on its own lists every build
tree with its size, its last change and what `--delete` would do. Use `--older-than <days>` or `--all` only when
no one else is building: on Linux and macOS the tool cannot tell that a tree is in use (on Windows it leaves a
tree untouched while a file in it is open or a program runs from it). It removes only directories directly under
`build/` that hold the `CMakeCache.txt` of an out-of-source build configured on this host, never follows a link,
and keeps the presets' binary directories unless `--include-presets` is given.

## 5. Build configurations

| Configuration | Purpose |
|---|---|
| `Debug` | no optimisation, asserts, editor |
| `Development` | optimised + debug info, asserts, editor, profiling |
| `Distribution` | shipping: no asserts, no editor, no dev tooling |

`python tools/build.py` builds the host's Debug preset by default (`windows-msvc-debug`, `linux-gcc-debug`,
`macos-debug`); a configure without a preset falls back to Development (`cmake/ConjureConfigs.cmake`).

Options: `CONJURE_WITH_RENDERER/PHYSICS/AUDIO/SCRIPTING`, `CONJURE_BUILD_EDITOR/RUNTIME/TESTS/
BENCHMARKS/SAMPLES/MANAGED`, `CONJURE_RHI_D3D12/VULKAN`, `CONJURE_ENABLE_ASAN/UBSAN/TSAN`,
`CONJURE_WARNINGS_AS_ERRORS`, `CONJURE_PLATFORM_GLFW`, `CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC`,
`CONJURE_TEST_GOLDEN_IMAGES`.
Presets live in `CMakePresets.json`; `tools/build.py` wraps them and sets up MSVC on Windows.

## 6. Definition of done (per task)

- [ ] Builds in Debug with warnings as errors on the host platform.
- [ ] All tests pass (`ctest`), new behaviour has tests, no test is skipped or disabled; every
      public header has its `tests/<module>/test_<header>.cpp`.
- [ ] Code style and conventions followed (run the checklist); `python tools/check_style.py
      --tests-mirror-headers` is clean (tabs, 120 columns, LF, `#pragma once`, no `using namespace`
      in headers, no Windows-macro enumerator names, no third-party includes in public headers,
      test naming, tests mirror headers). A change under `managed/` also passes
      `dotnet format --verify-no-changes managed/<project>/<project>.csproj` for `Conjure.Managed`,
      `Conjure.Bootstrap` and `Conjure.TestScripts` (a blocking CI step).
- [ ] Public headers compile on their own (`conjure_header_check`, part of the default build) and
      are documented with `<summary>` blocks; module README updated.
- [ ] No third-party header leaks into public headers (glm excepted).
- [ ] No platform ifdefs outside the allowed files.
- [ ] The public API wiki and handbook pages for the change are written or updated
      (`.claude/skills/write-docs/SKILL.md`; every public header referenced by include path) and
      `python tools/docs.py check` passes.
- [ ] Report lists what was built, what was tested, and anything left undone.
