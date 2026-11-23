---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# engine_subsystem.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/engine_subsystem.h` |
| API ↗ | [The frame, layers and time](../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../api/engine/index.md) |

<p>Subsystem adapters: the one interface the engine uses to start, register, step and stop every module object, so the startup unwind is uniform. The built-in adapters (platform, rhi, assets, ecs, scene, audio, physics, renderer, scripting, modules) live in engine/src/engine/subsystems; executables and plugins add their own through <code>application::add_subsystem</code> before <code>initialise()</code> (the bindings subsystem is the first user).</p>



## Classes

| File | Description |
|---|---|
| [`engine_subsystem`](engine-subsystem/engine-subsystem.md) |  |

## Forward Declarations

### `scheduler` {#symbol-scheduler}

```cpp
class scheduler;
```

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="engine_subsystem"></a>
<a id="engine_subsystembegin_frame"></a>
<a id="engine_subsystemend_frame"></a>
<a id="engine_subsystemengine_subsystem"></a>
<a id="engine_subsystemengine_subsystem_1"></a>
<a id="engine_subsystemname"></a>
<a id="engine_subsystemoperator"></a>
<a id="engine_subsystempost_startup"></a>
<a id="engine_subsystemregister_systems"></a>
<a id="engine_subsystemshutdown"></a>
<a id="engine_subsystemstartup"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="scheduler"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
