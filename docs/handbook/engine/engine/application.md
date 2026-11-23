---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# application.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/application.h` |
| API ↗ | [Engine: the application and the frame](../../../api/engine/index.md), [Running the engine](../../../api/engine/running-the-engine.md) |

<p>The <code><a href="application/application.html">application</a></code>: one object that owns every module object (through <code><a href="application.html">engine_services</a></code>), starts them in the documented order with reverse unwind on failure, drives one frame per <code>step_frame()</code> and exposes the <code><a href="engine-context/engine-context.html">engine_context</a></code> service table. Headless is configuration, not a code path: tests, servers, the editor&#x27;s step button and the runtime all go through <code>step_frame()</code>.</p>



## Classes

| File | Description |
|---|---|
| [`application`](application/application.md) |  |

## Enumerations

### `application_state` {#symbol-application-state}

```cpp
enum class application_state : u8 {
    CREATED,
    STARTING,
    RUNNING,
    STOPPING,
    STOPPED
};
```

### `simulation_mode` {#symbol-simulation-mode}

```cpp
enum class simulation_mode : u8 {
    RUNNING,
    PAUSED,
    STEP_ONCE
};
```

Pause is a simulation mode, not <code>timeScale</code> == 0: <code>PAUSED</code> skips the fixed loop and every <code>UPDATE</code> / <code>LATE_UPDATE</code> system without <code>RUNS_WHILE_PAUSED</code> while <code>TRANSFORM</code>, <code>SYNC</code> and <code>EXTRACT</code> keep running.

## Forward Declarations

### `engine_services` {#symbol-engine-services}

```cpp
struct engine_services;
```

### `frame_loop` {#symbol-frame-loop}

```cpp
class frame_loop;
```

### `frame_pacer` {#symbol-frame-pacer}

```cpp
class frame_pacer;
```

### `main_thread_queue` {#symbol-main-thread-queue}

```cpp
class main_thread_queue;
```

## Macros

### `application::CONJURE_MODULE_SERVICE` {#symbol-application-conjure-module-service}

```cpp
CONJURE_MODULE_SERVICE(conjure::engine::application)
```

## Functions

### Public

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="application.html#symbol-application-state">application_state</a> state) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a state (&quot;<code>RUNNING</code>&quot;).</p>

<div class="function-declaration" id="function-to-string-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="application.html#symbol-simulation-mode">simulation_mode</a> mode) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a mode (&quot;<code>PAUSED</code>&quot;).</p>

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
<a id="application_state"></a>
<a id="application_statecreated"></a>
<a id="application_staterunning"></a>
<a id="application_statestarting"></a>
<a id="application_statestopped"></a>
<a id="application_statestopping"></a>
<a id="applicationadd_subsystem"></a>
<a id="applicationapplication"></a>
<a id="applicationapplication_1"></a>
<a id="applicationcancel_fixed_steps"></a>
<a id="applicationclock"></a>
<a id="applicationconjure_module_service"></a>
<a id="applicationcontext"></a>
<a id="applicationcurrent_exit_code"></a>
<a id="applicationcurrent_simulation_mode"></a>
<a id="applicationdesc"></a>
<a id="applicationengine_directory"></a>
<a id="applicationframe_loop"></a>
<a id="applicationinitialise"></a>
<a id="applicationinject_render_failure"></a>
<a id="applicationis_inside_frame"></a>
<a id="applicationis_quit_requested"></a>
<a id="applicationlayers"></a>
<a id="applicationload_startup_scene"></a>
<a id="applicationmacceptingmodulesubsystems"></a>
<a id="applicationmanaged_directory"></a>
<a id="applicationmclock"></a>
<a id="applicationmcontext"></a>
<a id="applicationmdesc"></a>
<a id="applicationmexitcode"></a>
<a id="applicationmfixedclock"></a>
<a id="applicationmfixedstepspending"></a>
<a id="applicationmframeloop"></a>
<a id="applicationminframe"></a>
<a id="applicationmlayers"></a>
<a id="applicationmmainthreadqueue"></a>
<a id="applicationmownedclock"></a>
<a id="applicationmpacer"></a>
<a id="applicationmpendingfixedsteps"></a>
<a id="applicationmquitrequested"></a>
<a id="applicationmservices"></a>
<a id="applicationmsimulationmode"></a>
<a id="applicationmstartedsubsystemcount"></a>
<a id="applicationmstartupscenefailure"></a>
<a id="applicationmstate"></a>
<a id="applicationmstats"></a>
<a id="applicationmsubsystems"></a>
<a id="applicationmtime"></a>
<a id="applicationmusersubsystembegin"></a>
<a id="applicationmusersubsystemcount"></a>
<a id="applicationmusersubsystems"></a>
<a id="applicationoperator"></a>
<a id="applicationpost_to_main_thread"></a>
<a id="applicationraise_exit_code"></a>
<a id="applicationrequest_quit"></a>
<a id="applicationrun"></a>
<a id="applicationrun_frame"></a>
<a id="applicationset_simulation_mode"></a>
<a id="applicationset_time_scale"></a>
<a id="applicationshutdown"></a>
<a id="applicationstart_core"></a>
<a id="applicationstart_subsystems"></a>
<a id="applicationstartup_scene_failure"></a>
<a id="applicationstate"></a>
<a id="applicationstats"></a>
<a id="applicationstep_fixed"></a>
<a id="applicationstep_frame"></a>
<a id="applicationstop_core"></a>
<a id="applicationstop_subsystems"></a>
<a id="applicationsubsystems"></a>
<a id="applicationtime"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="engine_services"></a>
<a id="extension-points"></a>
<a id="frame_loop"></a>
<a id="frame_pacer"></a>
<a id="internals-and-algorithms"></a>
<a id="main_thread_queue"></a>
<a id="other-declarations"></a>
<a id="simulation_mode"></a>
<a id="simulation_modepaused"></a>
<a id="simulation_moderunning"></a>
<a id="simulation_modestep_once"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
