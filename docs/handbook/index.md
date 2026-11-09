# Engineering handbook

🛠 **Engineering handbook** · 📘 API: [Public API wiki](../api/index.md)

The handbook is for people working in the Conjure source: engine programmers, module authors and
contributors (human or AI). It covers the whole codebase in Confluence-style chapters, each with its own
table of contents, and links into the design record under `docs/design/` whenever a decision is explained
there rather than here. Game developers using the engine should read the Public API wiki instead; the
landing page [Conjure Engine documentation](../index.md) explains the two sets.

## Contents

- [How to read the handbook](#1-how-to-read-the-handbook)
- [Chapters](#2-chapters)
- [Chapters still to be written](#3-chapters-still-to-be-written)

## 1. How to read the handbook

Every page opens with the badge `🛠 **Engineering handbook**`; a page that has a user-facing counterpart
in the wiki links to it on the same line as `📘 API: <title>`. Callout boxes follow the GitHub alert
syntax with the fixed meanings listed on the [landing page](../index.md#callout-boxes-used-throughout);
the [Documentation](documentation.md) chapter explains the whole system for people writing pages.

The code is the source of truth for every signature quoted here. When a page and a header disagree,
the header wins and the page gets fixed in the same pull request (`docs/design/16_documentation.md` D7).

## 2. Chapters

One line per chapter, in reading order for a new contributor.

| Chapter | What it covers |
|---|---|
| [Getting started](getting-started.md) | prerequisites, cloning, `tools/build.py`, presets, the three configurations, running tests, private build directories, IDE notes, the documentation toolchain |
| [Architecture](architecture.md) | module map, dependency rules, the frame, threading; the summary of `docs/ARCHITECTURE.md` with links |
| [Repository layout](repository-layout.md) | every top-level directory and what goes where, the anatomy of a module directory, where tests live, which files are generated |
| [Build pipeline](build-pipeline.md) | the root `CMakeLists.txt`, `ConjureConfigs`, the `ConjureModule` helpers, `ConjureTesting`, third-party pinning and `SYSTEM` includes, warnings as errors, the generated `build_info.h`, planned shader, managed and export steps |
| [Coding standards](coding-standards.md) | style, conventions, the review checklist, error model, logging, asserts |
| [Data-oriented design](data-oriented-design.md) | DOD practice in Conjure: archetypes, chunk iteration, SoA, change detection, jobs, frame memory, what to avoid |
| [Testing](testing.md) | the test support library `conjure/testing/*.h`, conventions, labels, preconditions and skipping, doctest discovery, header checks, `check_style.py`, benchmarks, how to write and run a test |
| [Modules: core](modules/core.md) | the foundation module: purpose, areas, internals, threading, extension points, tests |
| [Module system](module-system/index.md) | what a module is today, the plugin design (manifest, ABI boundary, registration as data, lifecycle), create / import / distribute |
| [Continuous integration](ci.md) | the GitHub Actions matrix, what each job proves, sanitizer options, how to reproduce a CI failure locally |
| [Contributing](contributing.md) | the workflow from design note to commit, commit message convention, `AGENTS.md` and the skills, private build directories, the definition of done |
| [Documentation](documentation.md) | how this documentation works for contributors: the two sets, badges, alert boxes, page templates, `tools/docs.py`, `tools/check_docs.py`, the site build and CI |
| [Decision records](decision-records.md) | annotated index of `docs/design/00` to `16`: what each record decides and when to read it |

## 3. Chapters still to be written

The site navigation in `mkdocs.yml` reserves the following chapters. Each is written by the wave that
lands the code it describes (`docs/design/16_documentation.md` section 5); until then the design record is
the reference.

| Chapter | Planned file | Reference until written |
|---|---|---|
| Modules: ecs, platform, rhi, assets, audio, modules, scene, renderer, physics, scripting, engine | `handbook/modules/<module>.md` | the module's design note under `docs/design/` |

> [!NOTE]
> `tools/check_docs.py` (run as `python tools/docs.py check`) fails when an engine module under
> `engine/src/` has no `docs/handbook/modules/<module>.md`, so a module page is part of a module's
> definition of done, not an afterthought.

## Related

- design record: `docs/design/16_documentation.md` (the documentation system: two sets, badges, alerts, page templates, `check_docs.py`)
- `docs/ARCHITECTURE.md` (module map, frame, wave plan)
- `AGENTS.md` (rules and workflow for every contributor)
