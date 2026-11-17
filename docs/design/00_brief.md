# Conjure Engine - Architecture Brief (draft for design review)

This is the lead architect's draft. It fixes the decisions that are already made and lists the
areas that the design panel must refine. Everything in "Fixed decisions" is settled; everything in
"Open design areas" is where independent proposals are wanted.

## Fixed decisions

| Topic | Decision |
|---|---|
| Name / namespace | **Conjure**, root namespace `conjure`, module namespaces `conjure::core`, `conjure::ecs`, ... |
| Language / standard | C++20, CMake >= 3.28, Ninja. MSVC 14.44+, GCC 13+, Clang 16+ |
| Platforms | Windows 11 (primary dev target), Ubuntu 24.04+, macOS 14+ (Vulkan via MoltenVK) |
| Windowing / input | GLFW 3.4 behind a `platform` backend abstraction (`glfw` backend + `null` headless backend) |
| GPU abstraction | NVRHI behind `rhi` module. D3D12 default on Windows, Vulkan on Linux/macOS, Vulkan optional on Windows. A `null` RHI device exists for headless tests |
| Shaders | HLSL source, compiled at build time. DXIL via `dxc` (Windows SDK / fetched DXC release), SPIR-V via glslang (FetchContent). CMake function `conjure_add_shaders()` |
| Math | glm, wrapped in `conjure::math` type aliases (`vec3`, `quat`, `mat4`, `transform`, `aabb`, `frustum`, `ray`) |
| Audio | miniaudio behind `audio` backend abstraction (`miniaudio` + `null`) |
| Physics | Jolt Physics behind `physics` module |
| Scripting | C# on .NET 8+ hosted via `nethost`/`hostfxr`. Managed SDK `Conjure.Managed`, user game assembly loaded in a collectible `AssemblyLoadContext` for hot reload |
| glTF | cgltf; images via stb_image (LDR + .hdr) and libjxl (JPEG XL), screenshots via stb_image_write (PNG) or libjxl (lossless JPEG XL) |
| JSON | yyjson (C, no exceptions) wrapped in `conjure::core::json` |
| Editor UI | Dear ImGui (docking branch) + ImGuizmo, rendered through a custom NVRHI ImGui backend |
| Tests | doctest. Every module ships `tests/<module>/test_*.cpp`, registered with CTest. Headless (`null` backends) so they run in CI |
| CI | GitHub Actions: windows-latest (MSVC), ubuntu-24.04 (GCC + Clang), macos-14. Build + ctest |
| Exceptions | Enabled at compiler level (third-party needs them) but **engine code never throws**. Errors use `conjure::core::result<T>` / `conjure::core::error`. Asserts (`CONJURE_ASSERT`) are fatal in debug, compiled out in distribution |
| Library layout | Each engine module is a static library `conjure_<module>` created with `conjure_add_module()`. Executables: `conjure_editor`, `conjure_runtime`. Game modules/plugins are shared libraries |
| Build options | `CONJURE_WITH_RENDERER`, `CONJURE_WITH_PHYSICS`, `CONJURE_WITH_AUDIO`, `CONJURE_WITH_SCRIPTING`, `CONJURE_BUILD_EDITOR`, `CONJURE_BUILD_RUNTIME`, `CONJURE_BUILD_TESTS`, `CONJURE_BUILD_SAMPLES`, `CONJURE_RHI_D3D12`, `CONJURE_RHI_VULKAN`. All default ON (D3D12 only on Windows) |
| Build configs | `Debug`, `Development` (optimised + asserts + editor + profiling), `Distribution` (no asserts, no editor, no dev tooling). `CONJURE_DISTRIBUTION` macro |
| Code style | `docs/CODE_STYLE.cpp` is law. snake_case types/functions/namespaces, camelCase variables, `m` prefix for members, ALL_CAPS constants/enum values, `enum class` with explicit underlying type, braces always, early-out, 4 spaces, 120 col max (~100 recommended), `*`/`&` hug the name, every variable initialised, one declaration per line |

## Repository layout

```
CMakeLists.txt                 root: options, configs, warnings, add_subdirectory order
CMakePresets.json              windows-msvc-{debug,dev,dist}, linux-{gcc,clang}-*, macos-*
cmake/                         ConjureModule.cmake, CompilerWarnings.cmake, Dependencies.cmake,
                               ConjureShaders.cmake, ConjureManaged.cmake, ConjureExport.cmake
third_party/                   FetchContent declarations + any vendored single-header libs
engine/
  include/conjure/<module>/    public headers (install interface)
  src/<module>/                private headers + .cpp
  shaders/                     HLSL (+ shared .hlsli)
editor/                        conjure_editor executable
runtime/                       conjure_runtime executable
managed/
  Conjure.Managed/             C# SDK (net8.0), built with dotnet when found
  Conjure.Templates/           project template for game scripts
tests/<module>/                doctest unit tests; tests/integration/ for multi-module tests
samples/feature_test/          the "everything" scene + its project
tools/                         scripts (python): download_test_assets.py, etc.
docs/                          DESIGN_DOC.md, CODE_STYLE.cpp, ARCHITECTURE.md, CONVENTIONS.md, design/
.claude/skills/                build-test, add-module, add-component, editor-ai-control, style-review
AGENTS.md, CLAUDE.md -> AGENTS.md
```

## Engine modules and dependency order (waves)

```
wave 1  core        log, assert, types, uuid, result/error, string utils, hashing, time/clock,
                    memory (arena, pool, frame allocator), containers (slot_map, fixed/stable vectors,
                    small_vector, string_id), job system (work stealing, parallel_for), events/delegates,
                    filesystem/vfs, json (yyjson wrapper), reflection/type registry, math (glm wrappers)
wave 2  ecs         world, entity (id+generation), archetypes/chunks, component registry (bridges core
                    reflection), queries/views, systems + scheduler (phases), command buffer, prefabs base
        platform    window, input (keyboard/mouse/gamepad), events, backends: glfw, null
        rhi         nvrhi device creation per backend, swapchain, shader library loading, resource
                    helpers, null device; backends d3d12, vulkan, null
        assets      asset database (uuid -> path, meta files), asset_ref<T>, async loading via jobs,
                    importer registry, file watcher + hot reload, cooked asset packs
        audio       engine, clip asset, source/listener, mixing groups, 3D spatialisation; backends
                    miniaudio, null
        modules     plugin system: manifest, load shared lib, `conjure_module_entry`, registration API
wave 3  scene       scene = ecs world + hierarchy (parent/children, local/world transforms, dirty
                    propagation), name/tag/layer components, prefab assets + instantiation, scene
                    serialisation (json), scene manager (load/unload/additive)
        renderer    render graph, frame data extraction (render_scene), mesh/material/texture/shader
                    assets, gltf import, PBR (metal/rough, normal, occlusion, emissive), IBL (HDRI ->
                    cubemap -> irradiance + prefiltered specular + BRDF LUT), directional cascaded shadow
                    maps + spot/point shadows (hard + PCF soft), SSAO (GTAO-style, half-res + blur), HDR
                    (RGBA16F) + bloom + tonemapping (ACES, Reinhard, AgX) + exposure, debug draw, 2D
                    sprite/batch renderer and orthographic camera, ImGui render backend
        physics     Jolt world, rigid body / collider / character controller components, layers &
                    collision filtering, raycast/shape queries, fixed-step simulation with interpolation,
                    contact events -> ecs events
        scripting   .NET host (nethost/hostfxr), Conjure.Managed load, native<->managed function tables,
                    collectible ALC hot reload, script_component, serialised script fields, batched update
wave 4  engine      application + layer stack, engine loop (fixed update / variable update / render),
                    system registration for all modules, component <-> renderer/physics/audio sync,
                    project (project.json), export/cook pipeline, headless mode
        bindings    native binding tables for scripting (entities, components, transform, input,
                    physics, audio, prefab spawn, scene load, logging), generated C# counterparts,
                    DOD escape hatch (Span<T> over component arrays)
        editor      ImGui docking workspace: hierarchy, inspector (reflection driven), asset browser,
                    viewport (gizmos, picking), console, profiler, play/pause/step, undo/redo, export
                    dialog, AI command server (JSON protocol over TCP localhost + stdin, headless --exec)
        runtime     loads an exported project and runs it, no editor code compiled in
wave 5  samples     feature_test scene exercising every component and the whole scripting API;
                    stress test (10,000 dynamic entities)
```

## Key requirements the design must satisfy

1. **DOD internals, OOP facade.** ECS storage is contiguous per archetype; systems run over chunks and
   are job-parallel. The public API offers `entity` / `component<T>` handles and C# `Entity`
   objects for casual users, plus direct `span<T>` access to component arrays and query iteration for
   power users (RTS case).
2. **Performance target.** 10,000 dynamic entities with a transform, a physics body or a script update
   at 60 Hz without frame loss on a mid-range desktop. Benchmarks in `tests/benchmarks`.
3. **AI-controllable editor.** Every editor action is a command (undoable) and every command is
   reachable through a JSON protocol so an agent can author a whole game without a human. Headless
   batch mode: `conjure_editor --headless --exec commands.json --project path --trust-project` (a project
   nobody answered for is not opened, 04 D27).
4. **Everything testable headless.** `null` backends for platform/rhi/audio allow unit tests of every
   system in CI without a GPU or window.
5. **Export.** `conjure_editor --export <dir>` cooks assets, copies runtime + managed assemblies,
   writes `project.json`; the exported folder runs standalone.
6. **Modules.** Engine-level plugins and user content modules use the same mechanism; the editor can
   load/unload them at runtime.
7. **Production quality.** Full test coverage per module, no leaks (RAII, no raw owning pointers),
   no UB, warnings as errors (`/W4 /WX`, `-Wall -Wextra -Wpedantic -Werror`), sanitizers in CI
   (ASan/UBSan on Linux).

## Open design areas for the panel

A. ECS internals: archetype vs sparse-set, entity id layout, chunk size, change detection, structural
   change handling during iteration (command buffers), relationship with scene hierarchy, system
   scheduling and job parallelism, how component types are registered from C++ modules and C#.
B. Renderer: render graph design on top of NVRHI, frame extraction and double-buffered render scene,
   bindless or classic binding, material/shader permutation strategy, shadow cascade scheme, SSAO
   algorithm choice, HDR/tonemap chain, how 2D fits alongside 3D, ImGui integration.
C. Scripting host: exact hosting sequence, native<->managed call tables, hot reload and state
   preservation, how C# scripts become ECS components, per-frame batching, generated vs hand-written
   bindings, how the DOD escape hatch works safely.
D. Editor and AI control protocol: command model, undo/redo, protocol shape (request/response,
   streaming events), command catalogue needed to build a PacMan-class game end to end, headless mode,
   screenshot/query feedback for the agent.
E. Asset pipeline: uuid-based database, meta files, importers, cooking for export, hot reload, how
   renderer/audio/scene assets register, async loading.
F. Engine loop and threading: main thread responsibilities, fixed-step physics with interpolation,
   render thread or not, job system usage, frame memory.
G. Module/plugin system: ABI boundary, registration API, versioning, how a module adds components,
   systems, importers, editor panels, and C# assemblies.
H. Test strategy: unit tests per module, integration tests, benchmark harness, what the feature_test
   scene must contain, CI matrix.
