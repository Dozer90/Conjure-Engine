---
kind: scope
---

# Engine
<!-- doc-template:scope -->

<!-- doc-content:purpose:start -->
Implementation handbook for the engine libraries and standalone runtime. Use these module pages when changing internals, threading, extension points, or tests. The editor has its own [scope page](editor.md).
<!-- doc-content:purpose:end -->

## Modules

| Module | Description |
|---|---|
| [Assets](engine/assets.md) | Asset references, import, loading, cooking, caches, and packs. |
| [Audio](engine/audio.md) | Clips, decoding, voices, mixing, spatialisation, and audio backends. |
| [Bindings](engine/bindings.md) | The native interface exposed to managed scripts. |
| [Core](engine/core.md) | Foundation types, errors, containers, memory, jobs, files, and reflection. |
| [ECS](engine/ecs.md) | Entities, components, archetypes, queries, scheduling, and serialisation. |
| [Engine](engine/engine.md) | Application startup, layers, frame ordering, configuration, and export. |
| [Modules](engine/modules.md) | Plugin manifests, dependency resolution, loading, and services. |
| [Physics](engine/physics.md) | World simulation, bodies, characters, contacts, and queries. |
| [Platform](engine/platform.md) | Window creation, event delivery, and frame-based input state for native and headless execution. |
| [Renderer](engine/renderer.md) | Render pipelines, materials, meshes, and image assets. |
| [RHI](engine/rhi.md) | Graphics devices, resources, command submission, and backend contracts. |
| [Runtime](engine/runtime.md) | The standalone executable that runs exported games. |
| [Scene](engine/scene.md) | Scene ownership, hierarchy, transforms, prefabs, and snapshots. |
| [Scripting](engine/scripting.md) | Managed hosting, script instances, native APIs, and hot reload. |
