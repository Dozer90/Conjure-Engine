# Conjure Engine

A production-grade, data-oriented 2D/3D game engine for Windows, Linux and macOS.

- **Core**: C++20, CMake, data-oriented ECS with an object-oriented facade.
- **Rendering**: NVRHI (D3D12 / Vulkan), PBR materials, image-based lighting, cascaded shadow maps,
  SSAO, HDR pipeline with tonemapping, glTF import, 2D sprite batching.
- **Physics**: Jolt Physics, authored through components and driven from scripts.
- **Audio**: miniaudio with 3D spatialisation.
- **Scripting**: C# on .NET 8 with hot reload, batched per-frame dispatch and direct component
  array access for performance-critical code.
- **Editor**: Dear ImGui workspace with gizmos, reflection-driven inspector, undo/redo, and a JSON
  command protocol so AI agents can author entire games unattended.
- **Runtime**: standalone executable that runs exported games without editor code.
- **Modules**: Unreal-style plugins for engine extensions and user content.

## Building

Requirements: CMake 3.28+ (the `windows-vs` preset needs the Visual Studio 2026 bundled CMake or
CMake 4.1+), Ninja, a C++20 compiler (MSVC 19.40+ from VS 2022 17.10 or VS 2026, GCC 13+,
Clang 16+), .NET 8+ SDK (for scripting; optional with `-DCONJURE_WITH_SCRIPTING=OFF`), Python 3.

```bash
python tools/build.py --test
```

See `.claude/skills/build-and-test/SKILL.md` for presets, options and single-test runs.

## Running

The executables land in `build/<preset>/bin` (`.exe` on Windows):

```bash
build/<preset>/bin/conjure_editor --project <project dir>                               # the editor
build/<preset>/bin/conjure_editor --project <project dir> --headless --export <out dir>  # export a game
build/<preset>/bin/conjure_runtime --project <out dir>                                  # run the exported game
```

To try the editor on a ready-made project, copy `tests/assets/projects/minimal` into `build/` (ignored by git)
first, as CI does: the editor writes `.meta` sidecars and a `.conjure/` folder into the project it opens, and
those must never land in the repository. `conjure_editor --help` lists the editor flags;
[docs/api/engine/running-the-engine.md](docs/api/engine/running-the-engine.md) covers the runtime and engine
flags, and [.claude/skills/editor-ai-control/SKILL.md](.claude/skills/editor-ai-control/SKILL.md) drives the
editor from an agent.

## Documentation

| Document | Content |
|---|---|
| [docs/index.md](docs/index.md) | documentation home |
| [docs/api/index.md](docs/api/index.md) | Public API wiki: making games with the engine |
| [docs/handbook/index.md](docs/handbook/index.md) | Engineering handbook: working in the source |
| [docs/handbook/getting-started.md](docs/handbook/getting-started.md) | prerequisites, presets, building and testing |
| [AGENTS.md](AGENTS.md) | rules and workflow for contributors (human or AI) |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | module map, frame flow, wave plan |
| [docs/design/](docs/design/) | detailed per-area designs |
| [docs/CONVENTIONS.md](docs/CONVENTIONS.md) | engineering conventions |
| [docs/CODE_STYLE.cpp](docs/CODE_STYLE.cpp) | code style (authoritative) |
| [docs/DESIGN_DOC.md](docs/DESIGN_DOC.md) | original requirements |

## License

Copyright (c) 2026. All rights reserved.
