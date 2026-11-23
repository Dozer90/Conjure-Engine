---
name: code-review-checklist
description: The mandatory pre-commit review for Conjure. Use before every commit, when reviewing another agent's work, or when asked to check code quality or style compliance.
---

# Code review checklist

Review the full diff (`git diff` / `git diff --cached`), not just the files you remember
touching. Every item must hold. Fix, do not waive.

## Correctness

- [ ] Builds with warnings as errors in Debug on the host (`python tools/build.py --test`).
- [ ] All tests pass. New behaviour has tests: happy path, failure path, edge cases.
- [ ] No undefined behaviour: no uninitialised reads, no signed overflow, no dangling
      references/`string_view`s/`span`s, no use-after-move, no data races.
- [ ] Every `result<T>` is checked before `value()`. Every `CONJURE_TRY` return type matches.
- [ ] Resource ownership is explicit (`unique_ptr`, handles, RAII). No raw `new`/`delete`.
- [ ] Threading: shared state documented and guarded; nothing blocks the main thread per frame
      that could be a job.
- [ ] Error paths log at the right level and leave objects in a valid state.

## Style (docs/CODE_STYLE.cpp)

- [ ] 4 spaces, no tabs, lines <= 120 (aim for 100).
- [ ] snake_case namespaces/types/functions; camelCase locals and parameters; `m` prefix members,
      `s` prefix statics; ALL_CAPS constants, enumerators and macros; PascalCase template params.
- [ ] Braces on every control statement; early-out instead of nesting.
- [ ] One declaration per line; every variable initialised at declaration.
- [ ] `*` and `&` hug the name (`int *ptr`, `const foo &ref`).
- [ ] Wrapped argument lists: all arguments moved to new lines with two-space indent, closing
      parenthesis on its own line followed by `{`.
- [ ] Class layout: access modifier first; variables, then constructors/destructors, then
      methods, each section starting with its own access modifier; ctor/dtor defined in the .cpp
      even when empty; initialiser lists with the colon on the first line then follow the same rules
      as set for `Wrapped argument lists` above.
- [ ] `enum class` with explicit underlying type for every enumeration, flag sets included
      (`docs/CONVENTIONS.md` section 3 refines the style guide here): flag enumerations end in `_flags`, have a
      `NONE = 0` member and get their operators from `CONJURE_ENUM_FLAGS(type);`.
- [ ] Public methods have `/// <summary>` blocks; comments explain *why*; proper grammar.
- [ ] Heading comment blocks separate sections in long files.
- [ ] C#: a change under `managed/` passes `dotnet format --verify-no-changes
      managed/<project>/<project>.csproj` for `Conjure.Managed`, `Conjure.Bootstrap` and
      `Conjure.TestScripts` (rules in the `[*.cs]` section of `.editorconfig`; a blocking CI step).

## Conventions (docs/CONVENTIONS.md)

- [ ] `#pragma once`; include order own -> conjure -> third-party -> std.
- [ ] No third-party headers in `engine/include/` (glm via `conjure/core/math.h` excepted).
- [ ] No `#ifdef _WIN32` etc. outside `platform_detect.h` and backend files.
- [ ] No enumerator named after a Windows macro (`ERROR`, `DELETE`, `IN`, `OUT`, `TRUE`, ...).
- [ ] No exceptions thrown or caught in engine code.
- [ ] Log categories per module; no `printf`/`std::cout` outside the console sink and tools.
- [ ] CMake: files listed explicitly, deps through `conjure::` / `conjure::thirdparty::` aliases.
- [ ] Module README updated; architecture docs updated if a design decision changed.

## Hygiene

- [ ] No leftover debug output, commented-out code, `TODO` without reference, or dead code.
- [ ] No build outputs, downloaded assets or IDE files staged (`git status`).
- [ ] Commit subject `module: imperative summary`; body explains why when non-obvious.

## How to run a review as an agent

1. `git diff --cached --stat` then read every changed file in full.
2. Walk the four sections above, noting each violation with `file:line`.
3. Fix all violations, rebuild, rerun tests.
4. Only then commit.
