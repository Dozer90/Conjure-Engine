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
| How to build and test | `.claude/skills/build-and-test/SKILL.md` |
| How to add a module | `.claude/skills/add-engine-module/SKILL.md` |
| How to add a component | `.claude/skills/add-component/SKILL.md` |
| Review checklist before every commit | `.claude/skills/code-review-checklist/SKILL.md` |
| Driving the editor as an agent | `.claude/skills/editor-ai-control/SKILL.md` |
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
   `third_party/CMakeLists.txt` with a pinned tag or commit and a `conjure::thirdparty::` alias.
10. **Platform code stays behind backends.** No `#ifdef _WIN32` outside `platform_detect.h` and
    backend implementation files. Everything must build on Windows, Linux and macOS.

## 3. Repository layout

```
engine/include/conjure/<module>/   public headers           engine/src/<module>/   implementation
editor/                            conjure_editor           runtime/               conjure_runtime
managed/                           C# SDK + templates       tests/<module>/        doctest tests
samples/feature_test/              the everything-scene     tools/                 build.py, scripts
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
3. Write the tests alongside the code in `tests/<module>/` (`conjure_add_test_module`). Aim for
   behaviour coverage: construction, happy path, failure path, edge cases, threading where relevant.
4. Build the Debug preset and run the tests. Fix every warning.
5. Update `engine/src/<module>/README.md`.
6. Run the review checklist, then commit with a `module: summary` subject.

When several agents work in parallel, each owns one module directory and its test directory.
Never edit another module's files; if you need an API change there, write it down in your report.

Concurrent builds must use separate build directories. `tools/build.py` always uses
`build/<preset>`; when another agent may be building, configure your own directory instead:
`cmake --preset windows-msvc-debug -B build/<your-name>` (from a shell where the MSVC environment
is loaded), then `cmake --build build/<your-name> --target <your-targets>` and
`ctest --test-dir build/<your-name> -R <module>`.

## 5. Build configurations

| Configuration | Purpose |
|---|---|
| `Debug` | no optimisation, asserts, editor |
| `Development` | optimised + debug info, asserts, editor, profiling (default) |
| `Distribution` | shipping: no asserts, no editor, no dev tooling |

Options: `CONJURE_WITH_RENDERER/PHYSICS/AUDIO/SCRIPTING`, `CONJURE_BUILD_EDITOR/RUNTIME/TESTS/
BENCHMARKS/SAMPLES/MANAGED`, `CONJURE_RHI_D3D12/VULKAN`, `CONJURE_ENABLE_ASAN/UBSAN`.
Presets live in `CMakePresets.json`; `tools/build.py` wraps them and sets up MSVC on Windows.

## 6. Definition of done (per task)

- [ ] Builds in Debug with warnings as errors on the host platform.
- [ ] All tests pass (`ctest`), new behaviour has tests, no test is skipped or disabled.
- [ ] Code style and conventions followed (run the checklist).
- [ ] Public headers documented with `<summary>` blocks; module README updated.
- [ ] No third-party header leaks into public headers (glm excepted).
- [ ] No platform ifdefs outside the allowed files.
- [ ] Report lists what was built, what was tested, and anything left undone.
