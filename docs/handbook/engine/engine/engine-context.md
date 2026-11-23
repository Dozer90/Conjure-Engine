---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# engine_context.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/engine_context.h` |
| API ↗ | [Engine: the application and the frame](../../../api/engine/index.md), [Running the engine](../../../api/engine/running-the-engine.md) |

<p>The non-owning service table: what layers, subsystems, plugins and bindings reach the engine through. <code>A</code> <code>null</code> pointer means &quot;compiled out or disabled by configuration&quot; and every consumer must early-out on it. Pointers are stable from <code>application::initialise()</code> to <code>shutdown()</code>. Every engine and module type is forward-declared; the members</p>

## Structs

| File | Description |
|---|---|
| [`engine_context`](engine-context/engine-context.md) |  |

## Forward Declarations

### `application` {#symbol-application}

```cpp
class application;
```

### `asset_database` {#symbol-asset-database}

```cpp
class asset_database;
```

### `asset_manager` {#symbol-asset-manager}

```cpp
class asset_manager;
```

### `audio_engine` {#symbol-audio-engine}

```cpp
class audio_engine;
```

### `component_registry` {#symbol-component-registry}

```cpp
class component_registry;
```

### `device` {#symbol-device}

```cpp
class device;
```

### `diagnostics::check_registry` {#symbol-diagnostics-check-registry}

```cpp
class check_registry;
```

### `engine_config` {#symbol-engine-config}

```cpp
struct engine_config;
```

### `engine_time` {#symbol-engine-time}

```cpp
struct engine_time;
```

### `frame_memory` {#symbol-frame-memory}

```cpp
class frame_memory;
```

### `frame_stats` {#symbol-frame-stats}

```cpp
class frame_stats;
```

### `input` {#symbol-input}

```cpp
class input;
```

### `jobs::job_system` {#symbol-jobs-job-system}

```cpp
class job_system;
```

### `module_manager` {#symbol-module-manager}

```cpp
class module_manager;
```

### `physics_world` {#symbol-physics-world}

```cpp
class physics_world;
```

### `project` {#symbol-project}

```cpp
class project;
```

### `renderer` {#symbol-renderer}

```cpp
class renderer;
```

### `scene_manager` {#symbol-scene-manager}

```cpp
class scene_manager;
```

### `scheduler` {#symbol-scheduler}

```cpp
class scheduler;
```

### `script_runtime` {#symbol-script-runtime}

```cpp
class script_runtime;
```

### `vfs::file_system` {#symbol-vfs-file-system}

```cpp
class file_system;
```

### `window` {#symbol-window}

```cpp
class window;
```

### `world` {#symbol-world}

```cpp
class world;
```

## Macros

### `engine_context::CONJURE_MODULE_SERVICE` {#symbol-engine-context-conjure-module-service}

```cpp
CONJURE_MODULE_SERVICE(conjure::engine::engine_context)
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
<a id="application"></a>
<a id="asset_database"></a>
<a id="asset_manager"></a>
<a id="audio_engine"></a>
<a id="check_registry"></a>
<a id="classes"></a>
<a id="component_registry"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="dependencies"></a>
<a id="device"></a>
<a id="diagnosticscheck_registry"></a>
<a id="direct-header-dependencies"></a>
<a id="engine_config"></a>
<a id="engine_context"></a>
<a id="engine_contextapp"></a>
<a id="engine_contextassetdatabase"></a>
<a id="engine_contextassets"></a>
<a id="engine_contextaudio"></a>
<a id="engine_contextchecks"></a>
<a id="engine_contextcomponents"></a>
<a id="engine_contextconfig"></a>
<a id="engine_contextconjure_module_service"></a>
<a id="engine_contextdevice"></a>
<a id="engine_contextfilesystem"></a>
<a id="engine_contextframememory"></a>
<a id="engine_contextinput"></a>
<a id="engine_contextjobs"></a>
<a id="engine_contextmodules"></a>
<a id="engine_contextphysics"></a>
<a id="engine_contextproject"></a>
<a id="engine_contextrenderer"></a>
<a id="engine_contextscenes"></a>
<a id="engine_contextscheduler"></a>
<a id="engine_contextscripting"></a>
<a id="engine_contextstats"></a>
<a id="engine_contexttime"></a>
<a id="engine_contextwindow"></a>
<a id="engine_contextworld"></a>
<a id="engine_time"></a>
<a id="extension-points"></a>
<a id="file_system"></a>
<a id="frame_memory"></a>
<a id="frame_stats"></a>
<a id="functions"></a>
<a id="input"></a>
<a id="internals-and-algorithms"></a>
<a id="job_system"></a>
<a id="jobsjob_system"></a>
<a id="module_manager"></a>
<a id="other-declarations"></a>
<a id="physics_world"></a>
<a id="project"></a>
<a id="renderer"></a>
<a id="scene_manager"></a>
<a id="scheduler"></a>
<a id="script_runtime"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="vfsfile_system"></a>
<a id="window"></a>
<a id="world"></a>
<!-- doc-legacy-anchors:end -->
