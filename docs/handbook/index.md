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
| [Build pipeline](build-pipeline.md) | the root `CMakeLists.txt`, `ConjureConfigs`, the `ConjureModule` helpers, the umbrella shared library and game modules, the shader pipeline, `ConjureTesting`, third-party pinning and `SYSTEM` includes, warnings as errors, the generated headers, planned managed and export steps |
| [Coding standards](coding-standards.md) | style, conventions, the review checklist, error model, logging, asserts |
| [Data-oriented design](data-oriented-design.md) | DOD practice in Conjure: archetypes, chunk iteration, SoA, change detection, jobs, frame memory, allocation-free steady state, what to avoid |
| [Testing](testing.md) | the test support library `conjure/testing/*.h`, conventions, labels, preconditions and skipping (`REAL_WINDOW`, `SOFTWARE_GPU_ADAPTER`, `AUDIO_DEVICE`, `DOWNLOADED_ASSETS`), doctest discovery, header, style, export and documentation checks, benchmarks, how to write and run a test |
| [Modules: core](modules/core.md) | the foundation module: purpose, areas, internals, threading, extension points, tests |
| [Modules: ecs](modules/ecs.md) | archetype storage and chunk layout, component ids and masks, queries and change detection, command buffer flush, the stage-packing scheduler, observers, archives and serialisation, threading, extension points (`component_type_desc`, `system_extension_desc`), tests |
| [Modules: platform](modules/platform.md) | the GLFW backend (context, window creation, gamepads, monitors, high-DPI), event double buffering and resize coalescing, the input state machine, the null backend, threading, tests |
| [Modules: rhi](modules/rhi.md) | `device` over the backend interfaces, null / D3D12 / Vulkan backends, swapchains per backend, the shader pipeline (`conjure_add_shaders`, depfiles, `index.json`), threading, testing with and without a GPU |
| [Modules: assets](modules/assets.md) | identity and the slot table, scan and hashing, cache layout and validation, the load pipeline, publish / retire / collect, threading, hot reload and the watcher job, cooker and pack determinism, extension points, tests |
| [Modules: audio](modules/audio.md) | mixer architecture and callback rules, command and event rings, voice tables and lifecycle, the group gain tree, spatialisation, streaming, the null backend's virtual clock, device loss, threading, tests |
| [Modules: modules](modules/modules.md) | the ABI handshake, one load step by step, unload / fault / reload, shadow copies, registration sets and consumers, services, static modules, the umbrella shared library and `conjure_add_game_module()`, tests and fixtures |
| [Modules: scene](modules/scene.md) | hierarchy storage, one structural change per flush, the propagation algorithm, instantiation and the link pass, files, prefabs and snapshots, the scene manager, threading, tests |
| [Modules: renderer](modules/renderer.md) | the frame and the render graph, the resource cache and uploads, assets and importers, threading, deviations from the design, failure model, engine wiring, tests |
| [Modules: physics](modules/physics.md) | Jolt encapsulation, bodies following components, the fixed step and change detection, the job adapter, contacts, commands and queries, characters, collision meshes, settings and build options, tests |
| [Modules: scripting](modules/scripting.md) | the two ABI tables and the handshake, the runtime's re-entrancy rules, the component registration, hot reload, the managed side, nethost on MSVC, threading, tests |
| [Modules: engine](modules/engine.md) | startup and shutdown order, the frame loop, the subsystem adapters, module consumers and services, configuration and `project.json`, the export pipeline, threading, failure handling, tests |
| [Modules: bindings](modules/bindings.md) | filling the native table, the status protocol, structural routing, call counters, the managed facades and mirrors, threading, adding an entry, tests |
| [Modules: editor](modules/editor.md) | `editor_core` and `editor_ui`, how a command runs, undo and play mode, the protocol server and transports, frame integration and headless mode, the workspace, module records, threading, tests |
| [Runtime](modules/runtime.md) | `conjure_runtime`'s `main.cpp`, exit codes, linkage against `conjure_shared`, the Distribution stripping check, tests |
| [Module system](module-system/index.md) | engine modules versus plugin and game modules, how a plugin fits the engine (one engine copy, the C front door, registration as data, consumers), and the two chapters below |
| [Module system: creating a module](module-system/creating-a-module.md) | directory layout, the manifest, the CMake target, the entry point, registering components, systems and importers, building, testing with a static test host |
| [Module system: importing and distributing](module-system/importing-and-distributing.md) | declaring modules in a project, the binary layout `bin/<platform>-<arch>-<config>`, loading at start-up, the export layout, versioning and compatibility, C# assemblies in modules |
| [Continuous integration](ci.md) | the GitHub Actions matrix, what each job proves, sanitizer options, how to reproduce a CI failure locally |
| [Contributing](contributing.md) | the workflow from design note to commit, commit message convention, `AGENTS.md` and the skills, private build directories, the definition of done |
| [Documentation](documentation.md) | how this documentation works for contributors: the two sets, badges, alert boxes, page templates, `tools/docs.py`, `tools/check_docs.py`, the site build and CI |
| [Logo and application icon](brand.md) | the official logo and its construction, the small-size cuts, the editor's icon on each platform, `tools/generate_brand_icons.py` and its checks |
| [Decision records](decision-records.md) | annotated index of `docs/design/00` to `16`: what each record decides and when to read it |

## 3. Chapters still to be written

The site navigation in `mkdocs.yml` reserves the following chapters. Each is written by the wave that
lands the code it describes (`docs/design/16_documentation.md` section 5); until then the design record is
the reference.

| Chapter | Planned file | Reference until written |
|---|---|---|
| Scripting pipeline, renderer pipeline, asset pipeline (end-to-end chapters across modules) | `handbook/<chapter>.md` | `docs/design/03_scripting.md`, `02_renderer.md`, `05_assets.md`; the per-module chapters above cover each module's half |
| Sample projects (`samples/feature_test`, `samples/stress_test`, the demo games) | `handbook/samples.md` | `docs/design/08_testing.md` 7.2, `17_demo_games.md` |

Editor internals, the AI protocol internals and export and distribution, once listed here, are covered by
[Modules: editor](modules/editor.md), [Modules: engine](modules/engine.md) section 7 and
[Runtime](modules/runtime.md).

> [!NOTE]
> `tools/check_docs.py` (run as `python tools/docs.py check`) fails when an engine module under
> `engine/src/` has no `docs/handbook/modules/<module>.md`, so a module page is part of a module's
> definition of done, not an afterthought.

## Related

- design record: `docs/design/16_documentation.md` (the documentation system: two sets, badges, alerts, page templates, `check_docs.py`)
- `docs/ARCHITECTURE.md` (module map, frame, wave plan)
- `AGENTS.md` (rules and workflow for every contributor)
