# Contributing

🛠 **Engineering handbook**

This chapter is the workflow for landing a change in Conjure, for humans and AI agents alike: read the
design, implement against it, test, review against the checklist, document, commit. It collects the rules
spread over `AGENTS.md`, `docs/CONVENTIONS.md`, the skills under `.claude/skills/` and the definition of done
in `docs/design/08_testing.md` 7.4 and `docs/design/16_documentation.md` 5.

## Contents

- [The workflow](#1-the-workflow)
- [Before you start](#2-before-you-start)
- [Non-negotiable rules](#3-non-negotiable-rules)
- [Working alongside other contributors](#4-working-alongside-other-contributors)
- [The review checklist](#5-the-review-checklist)
- [Commit messages](#6-commit-messages)
- [AGENTS.md and the skills](#7-agentsmd-and-the-skills)
- [Definition of done](#8-definition-of-done)

## 1. The workflow

```
design note  ->  implement  ->  tests  ->  review checklist  ->  docs  ->  commit
```

| Step | What it means | Where |
|---|---|---|
| Design note | the behaviour is specified in `docs/design/<NN>_<area>.md`; a new decision gets a row in the note's decision table (or in `09_cross_area_resolutions.md` when two notes disagree) before code is written | `docs/design/` |
| Implement | public headers in `engine/include/conjure/<module>/`, everything else in `engine/src/<module>/`, files listed explicitly in the module's `CMakeLists.txt` | [Repository layout](repository-layout.md), [Build pipeline](build-pipeline.md) |
| Tests | `tests/<module>/test_<header>.cpp` for every public header, written alongside the code; `python tools/build.py --test` green in Debug with warnings as errors | [Testing](testing.md) |
| Review checklist | walk `.claude/skills/code-review-checklist/SKILL.md` over the full diff; fix, do not waive | section 5 |
| Docs | update `engine/src/<module>/README.md`, the handbook page of the module and the wiki pages of the headers you changed; `python tools/docs.py check` clean | section 8 |
| Commit | `module: imperative summary`; the commit builds and passes tests on its own | section 6 |

## 2. Before you start

1. Read `AGENTS.md` fully, then `docs/ARCHITECTURE.md`, the design note of the module you touch and the
   `README.md` of every module (or core area) you depend on. `docs/design/09_cross_area_resolutions.md`
   decides when two documents disagree; the headers decide when a document and the code disagree.
2. Know your configuration: `python tools/build.py --configure-only` once verifies the toolchain
   ([Getting started](getting-started.md)).
3. If the change needs an API that another module does not have, write the request down in your report
   instead of editing that module (`AGENTS.md` 4).

## 3. Non-negotiable rules

The ten rules of `AGENTS.md` section 2, in one line each:

1. Never read or copy code from outside this repository on the local machine; public web documentation of third-party libraries is fine.
2. Production quality only: no stubs, no to-do marker without an issue reference, no commented-out code, no temporary hacks; say what is unfinished in the report.
3. Follow `docs/CODE_STYLE.cpp` and `docs/CONVENTIONS.md` exactly (snake_case types and functions, camelCase locals, `m` prefix members, ALL_CAPS constants, braces always, early-out, 120 columns, every variable initialised, one declaration per line).
4. Engine code never throws: `conjure::core::result<T>` / `status` (`conjure/core/result.h`) for recoverable failures, `CONJURE_ASSERT` (`conjure/core/assert.h`) for programmer errors.
5. Every module ships headless tests covering every public type, passing under `/W4 /WX` and `-Wall -Wextra -Werror`.
6. Warnings are errors; never disable one, fix the code or include the offending third-party header through `SYSTEM`.
7. Build and run the tests before reporting done; "should compile" is a failure.
8. Review before commit; every commit builds and passes tests on its own.
9. No globbing in CMake; no new third-party dependency without a pinned entry in `third_party/CMakeLists.txt` and a `conjure::thirdparty::` alias.
10. Platform code stays behind backends; no `#ifdef _WIN32` outside `conjure/core/platform_detect.h` and backend implementation files; everything builds on Windows, Linux and macOS.

> [!IMPORTANT]
> Rule 7 is checked mechanically by the reader of your report: list what was built, which presets were
> tested and what was left undone. A report without that list is incomplete.

## 4. Working alongside other contributors

Several agents or people work in parallel, each owning one module directory and its test directory. Never
edit another module's files. Builds in one directory must not overlap, so each contributor uses a private
build directory:

```bash
python tools/build.py --build-dir build/<your-name> --target conjure_<module>_tests --test -R <module>
python tools/build.py --build-dir build/<your-name> --no-configure --target conjure_<module>_tests --test -R <module>
```

Raw equivalents, in a shell where `vcvars64.bat` is imported: `cmake --preset <preset> -B build/<your-name>`,
`cmake --build build/<your-name> --target <target>`, `ctest --test-dir build/<your-name> -R <module>`.
Everything under `build/` is ignored by git. A public header change in a lower wave is a change to that
module with its own tests; higher waves never patch around it (`docs/ARCHITECTURE.md` section 11).

## 5. The review checklist

`.claude/skills/code-review-checklist/SKILL.md` is mandatory before every commit and when reviewing someone
else's work. Procedure: `git diff --cached --stat`, read every changed file in full, walk the sections,
note every violation with `file:line`, fix them all, rebuild, rerun the tests, then commit. The sections:

| Section | Headline items |
|---|---|
| Correctness | builds with warnings as errors in Debug; tests for happy path, failure path and edge cases; no undefined behaviour (uninitialised reads, dangling views, use-after-move, data races); every `result<T>` checked before `value()`; `CONJURE_TRY` return types match; explicit ownership; shared state guarded; error paths log at the right level |
| Style (`docs/CODE_STYLE.cpp`) | 4 spaces, <= 120 columns; naming (snake_case / camelCase / `m` and `s` prefixes / ALL_CAPS / PascalCase template parameters); braces and early-out; one initialised declaration per line; `*` and `&` hug the name; class layout and `.cpp` constructors; wrapped argument lists with two-space indent and the closing parenthesis on its own line; `enum class` with explicit underlying type; `/// <summary>` on public methods |
| Conventions (`docs/CONVENTIONS.md`) | `#pragma once`; include order own, Conjure, third-party, standard; no third-party header in `engine/include/` (glm excepted); no platform ifdefs; no Windows-macro enumerator names; no exceptions; log categories per module; explicit CMake file lists and `conjure::` aliases; README and architecture docs updated |
| Hygiene | no debug output, commented-out code, unreferenced to-do markers or dead code; no build outputs, downloaded assets or IDE files staged; commit subject `module: imperative summary` |

`python tools/check_style.py --tests-mirror-headers` automates the mechanical half; the checklist covers
what a script cannot see.

## 6. Commit messages

- Subject: `<module>: <imperative summary>`, lower-case module, no trailing period. Examples from the log:
  `core: wave 1 - math, containers, strings, memory, time, jobs, json, vfs, system, reflect; test support rework`,
  `tools: build.py --clean removes read-only git pack files on Windows`,
  `docs: R58 records the quaternion storage decision; ignore pycache`.
- Body: why, when it is not obvious from the diff; reference the design decision (`10_core.md C2`, `R58`)
  the change implements.
- Scope: one logical change; the commit builds and passes `ctest` on its own, so a bisect never lands on
  a broken tree.
- Never commit build outputs, IDE files or the documentation virtual environment (`.gitignore` covers
  them) nor downloaded assets (`tests/.downloaded/` is ignored;
  [Repository layout](repository-layout.md#6-generated-and-ignored-files)).
- Prefer a new commit over amending a pushed one.

## 7. AGENTS.md and the skills

`AGENTS.md` is the single source of truth for working in the repository; `CLAUDE.md` only points at it.
Skills under `.claude/skills/` package repeatable procedures so an agent (or a person) follows the same
steps every time:

| Skill | Use when | Covers |
|---|---|---|
| `build-and-test` | before reporting any change as done, when a build fails, when asked how to build | the one command, presets, single-test runs, private build directories, reading failures, the CI matrix |
| `add-engine-module` | adding a subsystem, backend, importer family or gameplay system | module layout, `conjure_add_module` / `conjure_add_test_module` registration, log category, backends with a `null` implementation, README |
| `code-review-checklist` | before every commit, when reviewing another agent's work | section 5 |
| `write-docs` | writing or updating a wiki page or a handbook page, before `python tools/docs.py check` | the page templates, badge and link prefixes, alert meanings, the header coverage rule, the build and check commands ([Documentation](documentation.md)) |
| `add-component` | adding a component type to a module | the `CONJURE_COMPONENT` / `CONJURE_REFLECT_STRUCT` pair, registration, the C# mirror and the test the pair needs |
| `editor-ai-control` | building, editing, inspecting or play-testing a scene through `conjure_editor`, scripting the editor in CI, looking up a command's arguments | starting the editor headless or with a window, the wire format, the command catalogue summarised from `docs/generated/editor_protocol.json` (`editor.schema`), exit codes of `--exec` |

## 8. Definition of done

A task is done when every item holds (`AGENTS.md` 6, `docs/design/08_testing.md` 7.4,
`docs/design/16_documentation.md` 5):

- [ ] Builds in Debug with warnings as errors on the host platform; the Distribution preset builds when the
      change touches assertions or logging.
- [ ] All tests pass (`ctest`); new behaviour has tests; no test is skipped or disabled; every public header
      has its `tests/<module>/test_<header>.cpp`; every `result` error code has a test; `memory_guard` clean
      and no unallowed `ERROR` logged.
- [ ] `python tools/check_style.py --tests-mirror-headers` reports zero findings; the review checklist was walked.
- [ ] Public headers compile alone (`conjure_header_check`, part of the default build), carry `<summary>`
      blocks and include no third-party header (glm excepted); no platform ifdefs outside the allowed files.
- [ ] `engine/src/<module>/README.md` updated: purpose, main types, threading model, how to test.
- [ ] Documentation: the handbook page of the module (`docs/handbook/modules/<module>.md`) and the wiki
      pages of the headers you changed are written or updated, every header is referenced by its include
      path, and `python tools/docs.py check` passes (`docs/design/16_documentation.md` D9 and 5, item 7 of 7.4).
- [ ] Later-wave gates when they apply: a new component type has an entity in `feature_test`, a new binding a
      script call, a new editor command a fixture file, a new feature a `Conjure.Diagnostics.Check`;
      `test_determinism` stays green at 0, 2 and 8 threads; benchmark thresholds pass on the author's machine.
- [ ] The report lists what was built, what was tested and anything left undone.

> [!TIP]
> Write the handbook and wiki text while the header is open: the templates in
> `docs/design/16_documentation.md` section 3 are short, `check_docs.py` tells you which header is still
> unreferenced, and a signature quoted from the editor buffer is rarely wrong.

## Related

- `AGENTS.md` (rules, workflow, definition of done)
- `docs/CONVENTIONS.md` section 8 (git), `docs/CODE_STYLE.cpp`
- design record: `docs/design/08_testing.md` section 7.4 (definition of done for a module)
- design record: `docs/design/16_documentation.md` section 5 (documentation in the development process)
- `.claude/skills/build-and-test/SKILL.md`, `.claude/skills/add-engine-module/SKILL.md`, `.claude/skills/code-review-checklist/SKILL.md`, `.claude/skills/write-docs/SKILL.md`
- [Getting started](getting-started.md), [Testing](testing.md), [Continuous integration](ci.md), [Documentation](documentation.md)
