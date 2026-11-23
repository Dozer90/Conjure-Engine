# Decision records
<!-- doc-template:chapter -->
[Handbook](index.md)

<!-- doc-content:purpose:start -->
`docs/design/` is Conjure's design record: one binding document per area or module, written before the code and
reviewed for consistency. This page is the annotated index: for every record, what it decides and when you
should read it. Authority order when documents disagree: `09_cross_area_resolutions.md`, then the module note
(`10`-`15`), then the area design (`01`-`08`), then `docs/ARCHITECTURE.md`; `00_brief.md` fixes what none of
them may reopen. Every record follows the same shape (overview and decisions, data model, algorithms, public
API, threading, failure handling, headless behaviour, integration points, implementation plan, tests, out of
scope), so section numbers cited elsewhere in the handbook mean the same thing in each.
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
## Contents

- [1. Top-level documents](#1-top-level-documents)
- [2. Area designs (01-08)](#2-area-designs-01-08)
- [3. Consistency review (09)](#3-consistency-review-09)
- [4. Module design notes (10-15)](#4-module-design-notes-10-15)
- [5. Process (16)](#5-process-16)
- [Related](#related)

## 1. Top-level documents

**`docs/DESIGN_DOC.md`** is the customer's brief: a production-grade, data-oriented 2D + 3D engine in C++ and
CMake with module-based extensions, C# scripting, an editor that an AI agent can drive, a 10,000-entity
performance target and full testing. Read it when a design question comes down to "what did the customer
actually ask for".

**[`00_brief.md`](../design/00_brief.md)** is the lead architect's draft that fixed the decisions the panel
could not reopen: name and namespace, C++20 / CMake 3.28 / Ninja, the third-party choices (GLFW, NVRHI, glm,
miniaudio, Jolt, .NET 8 via nethost, cgltf, stb, yyjson, Dear ImGui, doctest), the static-library-per-module
layout, build options and configurations, the code-style summary, the repository layout and the wave plan. It
also lists the open areas A-H that became records 01-08. Read it first; everything else assumes it.

**`docs/ARCHITECTURE.md`** is the binding top-level map: module table, dependency graph, ownership, the frame,
the asset, scripting, editor and export paths, the build matrix and the per-wave definition of done. Its
section 4.1 is the consolidated core contract every module is written against; 4.2-4.6 are pointers to the
notes. Summarised in [Architecture](architecture.md). Read it before touching more than one module.

**`docs/CONVENTIONS.md`** and **`docs/CODE_STYLE.cpp`** are the coding rules (explained in
[Coding standards](coding-standards.md)); the style file is customer-supplied and its struct example is
overridden by R31.

## 2. Area designs (01-08)

**[`01_ecs.md`](../design/01_ecs.md)** decides the entity-component storage: archetypes with fixed 16 KiB SoA
chunks and a dense record table, 8-byte `{index, generation}` entities, dense `u16` component ids assigned per
name (`MAX_COMPONENTS = 1024`), one explicit `component_registry`, chunk-granular change detection by version
stamp, structural changes refused while iterating and routed through `command_buffer` with real ids reserved
up front, a scheduler that packs systems into conflict-free stages from declared masks, `component_ops` for
non-trivial types, `entity_archive` as the prefab and serialisation base, and an error model where stale
handles are recoverable errors. Read it for anything that stores, iterates or serialises components, and
before writing a system (see [Data-oriented design](data-oriented-design.md)).

**[`02_renderer.md`](../design/02_renderer.md)** decides the two rendering modules: `rhi` wraps NVRHI
(device, swapchain, shader library, `null` device) and `renderer` builds a render graph on it with the
`render_scene` extraction contract (`begin_extract` / `end_extract` / `render_frame`), PBR with IBL, cascaded
and local shadows, SSAO, HDR and tonemapping, the 2D batch renderer, the ImGui backend, glTF import and the
shader build (`conjure_add_shaders`). Read it when you touch anything that ends up on screen or in a cooked
renderer asset.

**[`03_scripting.md`](../design/03_scripting.md)** decides the C# host: one CLR per process through `hostfxr`,
a collectible `AssemblyLoadContext` for hot reload, X-macro native and managed call tables validated by a
handshake, a shared frame block for `Time` and `Input`, one native-to-managed call per phase, 4-byte
`script_component`s pointing into a `script_store`, `[EcsComponent]` structs as dynamic ECS components, the
`Span<T>` DOD escape hatch, the managed build and export modes. Read it for the native/managed boundary
mechanism; the *content* of the non-core bindings is `15_bindings.md`.

**[`04_editor.md`](../design/04_editor.md)** decides the editor and the AI control protocol: every mutation is a
`command_request` with an inverse through one `command_dispatcher`, the newline-delimited JSON protocol over
TCP localhost and stdio, the self-describing command catalogue (`editor.schema`), undo frozen while playing,
play/pause/step by scene snapshot, CPU picking, synchronous screenshots, the headless runner
(`--headless --exec`, exit codes 0/1/2/3) and Distribution stripping. Read it when adding an editor feature,
which always means adding a command and its fixture.

**[`05_assets.md`](../design/05_assets.md)** decides the asset pipeline: uuid identity with `.meta` sidecars,
deterministic importers cooking content-addressed blobs into a hash-validated cache, `asset_ref<T>` handles
with an atomic publish, async loading over the job system with dependency ordering, hot reload, packs and the
cooker. Read it when a module registers an asset type or importer, or when anything is loaded from disk.

**[`06_engine_loop.md`](../design/06_engine_loop.md)** decides the application and frame: a single-orchestrator
main thread with no render thread, the exact phase sequence and subsystem hooks, startup and shutdown order,
fixed-step simulation with interpolation, the frame pacer, clock injection, `frame_memory` over the core frame
allocator, `project.json`, the layer stack, `engine_context` and the export pipeline. Read it for frame
ordering questions and before adding a subsystem adapter.

**[`07_modules.md`](../design/07_modules.md)** decides the plugin system: one shared umbrella `conjure_shared`
that hosts and plugins link, a C front door that proves ABI identity field by field before any C++ crosses,
registration as plain data records applied atomically by consumers, per-type ownership, main-thread loading
between frames, reload with consumer-owned snapshots, manifest-only static modules for the built-in libraries,
the single `game` module and the export layout. Summarised in [Module system](module-system/index.md).

**[`08_testing.md`](../design/08_testing.md)** decides how Conjure proves it works: one CTest entry per
`TEST_CASE`, the two test-support libraries, one shared doctest main with tracked global `new`, a real job
system in tests, doctest naming and coverage rules per module, integration and golden-image tests, the
benchmark harness and thresholds, the replayed `feature_test` sample, the CI matrix and the definition of done
(7.4). Read it before writing tests and whenever CI disagrees with you.

## 3. Consistency review (09)

**[`09_cross_area_resolutions.md`](../design/09_cross_area_resolutions.md)** records the 65 contradictions,
duplicated responsibilities, gaps and naming drifts found by reading the records against each other
(`R1`-`R42` across 01-08, `R43`-`R65` across the module notes), the resolution of each, the final owner of
every duplicated responsibility (transform propagation, extraction, ImGui context, uploads, picking, export,
`interpolated_transform`, ...) and the risks that documents cannot resolve. Every record cites the `R<n>` ids at
the edited spots. Read it whenever two documents seem to disagree: this file wins.

## 4. Module design notes (10-15)

**[`10_core.md`](../design/10_core.md)** decides the core module: eleven independently buildable areas with
their public dependency graph, the error and category model, the no-exceptions / confined-allocation /
confined-threading rules, and the contract of every area (math conventions, container semantics, string ids,
allocators, clocks and events, the job system, canonical JSON, the VFS, processes and libraries, reflection
with `layout_hash`). The headers win where this note and the code differ in spelling. Read it with
[Core module](engine/core.md) whenever you use or change core.

**[`11_platform.md`](../design/11_platform.md)** decides windowing and input: one abstract `window` with `glfw`
and `null` backends that pass the same conformance test, trivially copyable event variants, a
backend-independent `input` state machine whose `apply` and `inject` share one path, the immutable
`input_state` mirrored into the scripting frame block, GLFW key numbering (`key::DEL`) and the
`CONJURE_PLATFORM_GLFW` option. Read it for anything that touches a window, the keyboard, mouse or gamepads.

**[`12_audio.md`](../design/12_audio.md)** decides audio: `audio_engine` over `miniaudio` or `null` with a
backend-agnostic mixer fed by lock-free rings, 256 voices, a group gain tree, 3D attenuation and Doppler, the
`conjure.audio.clip` asset type with a CPU-only loader, and the `audio_source` / `audio_listener` components
(including the hidden `playbackRequest` / `isPlaying` fields the engine's sync systems consume) that live in
`audio` without an `ecs` dependency. Read it for playback, mixing or the clip importer.

**[`13_scene.md`](../design/13_scene.md)** decides the scene module: one `ecs::world` per scene, the hierarchy
as `parent` plus transient `children` and `depth_tag<n>` components so propagation runs depth-level parallel in
one pass, the four scene systems, `.scene.json` / `.prefab.json` over the ECS archive format, fully
materialised prefab instances, in-memory snapshots for play/stop and `scene_manager` residency. Read it for
hierarchy, transforms, prefabs and scene files.

**[`14_physics.md`](../design/14_physics.md)** decides physics: Jolt 5.3 fully encapsulated in
`physics_world` with its job system and allocations routed through core, plain reflected components
(`rigid_body`, five colliders, `physics_material`, `character_controller`), bodies created by observers and
every gameplay mutation queued, 32 named layers from `project.json`, allocation-free entity-keyed queries, the
sorted `contact_events()` span and the `conjure.physics.collision_mesh` asset type. Read it for anything that
simulates or queries collision.

**[`15_bindings.md`](../design/15_bindings.md)** decides the content of the scripting boundary: the ten
bindings-owned groups of native entries (106 of them, stubs for compiled-out subsystems), the C# facades, the
`[NativeComponent]` mirrors verified by `layout_hash`, `AssetRef<T>` marshalling, call counters for the
`feature_test` completeness check and `Conjure.Diagnostics`. Read it when adding anything a C# script can
call or see.

## 5. Process (16)

**[`16_documentation.md`](../design/16_documentation.md)** decides this documentation: two sets (the public
API wiki in `docs/api/` and this handbook in `docs/handbook/`), MkDocs + Material as the site, GitHub alert
syntax for callouts with fixed meanings, the badge on every page, the page templates, link conventions,
`tools/check_docs.py` as the CTest entry `docs.check`, and documentation as part of the definition of done.
Read it before writing or restructuring a page.

## Related

- handbook: [Architecture](architecture.md), [Coding standards](coding-standards.md),
  [Data-oriented design](data-oriented-design.md), [Core module](engine/core.md),
  [Module system](module-system/index.md)
- `AGENTS.md` section 1 ("Where things are") for the non-design documents and skills
<!-- doc-content:body:end -->
