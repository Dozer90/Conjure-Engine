---
kind: header
---

[Editor](../../editor.md) → [Applications](../applications.md)

# editor_context.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/editor_context.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>The editor&#x27;s service table: owns the command registry, dispatcher, undo stack, selection, play controller, events, camera, console and screenshot service, and holds non-owning references into the running <code><a href="../../engine/engine/application/application.html">engine::application</a></code> (taken from <code><a href="../../engine/engine/engine-context/engine-context.html">engine_context</a></code>, rebound when the editor opens another project). Handlers reach everything through it. Main thread only.</p>

## Structs

| File | Description |
|---|---|
| [`editor_context::debug_draw_flags`](editor-context/editor-context-debug-draw-flags.md) |  |

## Classes

| File | Description |
|---|---|
| [`editor_context`](editor-context/editor-context.md) |  |
| [`editor_host`](editor-context/editor-host.md) | What the owning executable does for the context: operations that outlive one <code><a href="../../engine/engine/application/application.html">engine::application</a></code>. |
| [`frame_task`](editor-context/frame-task.md) | Work the editor advances once per frame (<code>editor.wait_frames</code>, <code>editor.exec_file</code>, the prefab commands that wait for their asset): <code>tick()</code> returns <code>true</code> when the task finished and may be destroyed. <code>A</code> task destroyed before it finished (engine restart, shutdown) releases what it registered on the context and fails its reply with <code>CANCELLED</code> when that is still pending. The engine a task works on is shut down before the task is destroyed, so a task that holds something of the engine (an asset handle) lets go of it in <code>release_engine()</code>, which the context calls while the engine&#x27;s asset manager and database still exist. |

## Enumerations

### `editor_context::view_mode` {#symbol-editor-context-view-mode}

```cpp
enum class view_mode : u8 {
    THREE_D = 0,
    TWO_D
};
```

The viewport projection mode <code>view.set_mode</code> switches.

### `editor_display_mode` {#symbol-editor-display-mode}

```cpp
enum class editor_display_mode : u8 {
    WINDOWED,                 // a window with its panels (no --headless)
    HEADLESS,                 // no window and no viewport (--headless)
    HEADLESS_OFFSCREEN        // no window; view commands render off-screen when a device exists (--offscreen)
};
```

How the editor session presents itself, fixed for the context&#x27;s lifetime (<code>is_headless()</code> and <code>offscreen()</code> read it back). One enum instead of two bools, so an off-screen session with a window cannot be expressed.

## Type Aliases

### `editor_context::response_sink` {#symbol-editor-context-response-sink}

```cpp
using response_sink = std::function<void(const protocol_response &response)>;
```

Receives the responses of one in-process client (the headless runner, tests).

## Forward Declarations

### `module_manager` {#symbol-module-manager}

```cpp
class module_manager;
```

### `physics_world` {#symbol-physics-world}

```cpp
class physics_world;
```

### `reflect::type_registry` {#symbol-reflect-type-registry}

```cpp
class type_registry;
```

### `renderer` {#symbol-renderer}

```cpp
class renderer;
```

### `script_runtime` {#symbol-script-runtime}

```cpp
class script_runtime;
```

## Functions

### Public

#### `display_mode_for` {#function-display-mode-for}

<div class="function-declaration" id="function-display-mode-for-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="editor-context.html#symbol-editor-display-mode">editor_display_mode</a> display_mode_for(
    <span class="k">const</span> <a href="editor-options/editor-options.html">editor_options</a> &amp;options) <span class="k">noexcept</span>;</code></pre></div>

<p>The display mode of a session started with <code>options</code>: <code>HEADLESS_OFFSCREEN</code> for headless with offscreen, <code>HEADLESS</code> for headless alone, <code>WINDOWED</code> otherwise. Offscreen without headless is <code>WINDOWED</code>: <code>parse_editor_options</code> refuses --offscreen without --headless, and options built in code that carry it get a window, as <code>editor_application::make_engine_desc</code> and the editor&#x27;s layer setup treat them. <code><a href="editor-application/editor-application.html">editor_application</a></code> builds its context with this mode.</p>

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
<a id="editor_context"></a>
<a id="editor_contextadd_frame_task"></a>
<a id="editor_contextasset_database"></a>
<a id="editor_contextassets"></a>
<a id="editor_contextbind_engine"></a>
<a id="editor_contextbump_revision"></a>
<a id="editor_contextcamera"></a>
<a id="editor_contextclose_trust_prompt_for"></a>
<a id="editor_contextcomponents"></a>
<a id="editor_contextconsole"></a>
<a id="editor_contextcurrent_view_mode"></a>
<a id="editor_contextdebug_draw"></a>
<a id="editor_contextdebug_draw_flags"></a>
<a id="editor_contextdebug_draw_flagsbounds"></a>
<a id="editor_contextdebug_draw_flagsgrid"></a>
<a id="editor_contextdebug_draw_flagsphysics"></a>
<a id="editor_contextdefault_viewport_aspect"></a>
<a id="editor_contextdeliver_response"></a>
<a id="editor_contextdescribe_state"></a>
<a id="editor_contextdismiss_trust_prompt"></a>
<a id="editor_contextdispatcher"></a>
<a id="editor_contextdotnet_executable"></a>
<a id="editor_contexteditor_context"></a>
<a id="editor_contexteditor_context_1"></a>
<a id="editor_contextemit_selection_changed"></a>
<a id="editor_contextengine"></a>
<a id="editor_contextengine_context"></a>
<a id="editor_contextevents"></a>
<a id="editor_contextfilesystem"></a>
<a id="editor_contextforward_log_events"></a>
<a id="editor_contextframe_index"></a>
<a id="editor_contextframe_task_count"></a>
<a id="editor_contexthas_project"></a>
<a id="editor_contexthas_scene"></a>
<a id="editor_contextheadless"></a>
<a id="editor_contexthost"></a>
<a id="editor_contextinput"></a>
<a id="editor_contextinstall_module_consumer"></a>
<a id="editor_contextis_open_scene"></a>
<a id="editor_contextlast_script_configuration"></a>
<a id="editor_contextmcamera"></a>
<a id="editor_contextmconsole"></a>
<a id="editor_contextmconsumerinstalled"></a>
<a id="editor_contextmdebugdraw"></a>
<a id="editor_contextmdispatcher"></a>
<a id="editor_contextmdotnetexecutable"></a>
<a id="editor_contextmengine"></a>
<a id="editor_contextmevents"></a>
<a id="editor_contextmframetasks"></a>
<a id="editor_contextmheadless"></a>
<a id="editor_contextmhost"></a>
<a id="editor_contextminbound"></a>
<a id="editor_contextmlastlogseq"></a>
<a id="editor_contextmlastscriptconfiguration"></a>
<a id="editor_contextmmoduleconsumer"></a>
<a id="editor_contextmodule_consumer"></a>
<a id="editor_contextmodules"></a>
<a id="editor_contextmoffscreen"></a>
<a id="editor_contextmplay"></a>
<a id="editor_contextmproject"></a>
<a id="editor_contextmregistry"></a>
<a id="editor_contextmrequestsperframe"></a>
<a id="editor_contextmresponses"></a>
<a id="editor_contextmresponsesinks"></a>
<a id="editor_contextmrevision"></a>
<a id="editor_contextmscenedirty"></a>
<a id="editor_contextmsceneopen"></a>
<a id="editor_contextmscenepath"></a>
<a id="editor_contextmscreenshots"></a>
<a id="editor_contextmscriptcompiletoken"></a>
<a id="editor_contextmselection"></a>
<a id="editor_contextmselectionsubscription"></a>
<a id="editor_contextmserver"></a>
<a id="editor_contextmtrust"></a>
<a id="editor_contextmtrustprompt"></a>
<a id="editor_contextmtrustpromptenabled"></a>
<a id="editor_contextmundo"></a>
<a id="editor_contextmviewmode"></a>
<a id="editor_contextmviewportaspect"></a>
<a id="editor_contextnames_asset"></a>
<a id="editor_contextnormalise_scene_path"></a>
<a id="editor_contextoffscreen"></a>
<a id="editor_contextoperator"></a>
<a id="editor_contextphysics"></a>
<a id="editor_contextplay"></a>
<a id="editor_contextproject"></a>
<a id="editor_contextraise_trust_prompt"></a>
<a id="editor_contextrebind"></a>
<a id="editor_contextregister_response_sink"></a>
<a id="editor_contextregistry"></a>
<a id="editor_contextrenderer"></a>
<a id="editor_contextresponse_sink"></a>
<a id="editor_contextrevision"></a>
<a id="editor_contextscene"></a>
<a id="editor_contextscene_dirty"></a>
<a id="editor_contextscene_manager"></a>
<a id="editor_contextscene_path"></a>
<a id="editor_contextscreenshots"></a>
<a id="editor_contextscript_compile_pending"></a>
<a id="editor_contextscripting"></a>
<a id="editor_contextselection"></a>
<a id="editor_contextselection_payload"></a>
<a id="editor_contextserver"></a>
<a id="editor_contextset_dotnet_executable"></a>
<a id="editor_contextset_last_script_configuration"></a>
<a id="editor_contextset_requests_per_frame"></a>
<a id="editor_contextset_scene_dirty"></a>
<a id="editor_contextset_scene_state"></a>
<a id="editor_contextset_script_compile_token"></a>
<a id="editor_contextset_server"></a>
<a id="editor_contextset_trust"></a>
<a id="editor_contextset_trust_prompt_enabled"></a>
<a id="editor_contextset_view_mode"></a>
<a id="editor_contextset_viewport_aspect"></a>
<a id="editor_contexttick"></a>
<a id="editor_contexttrust"></a>
<a id="editor_contexttrust_prompt"></a>
<a id="editor_contexttrust_prompt_enabled"></a>
<a id="editor_contextundo"></a>
<a id="editor_contextunregister_response_sink"></a>
<a id="editor_contextview_mode"></a>
<a id="editor_contextview_modethree_d"></a>
<a id="editor_contextview_modetwo_d"></a>
<a id="editor_contextviewport_aspect"></a>
<a id="editor_contextworld"></a>
<a id="editor_host"></a>
<a id="editor_hosteditor_host"></a>
<a id="editor_hosteditor_host_1"></a>
<a id="editor_hosthas_pending_project_open"></a>
<a id="editor_hostoperator"></a>
<a id="editor_hostrequest_open_project"></a>
<a id="editor_hostrequest_quit"></a>
<a id="extension-points"></a>
<a id="frame_task"></a>
<a id="frame_taskframe_task"></a>
<a id="frame_taskframe_task_1"></a>
<a id="frame_taskoperator"></a>
<a id="frame_tasktick"></a>
<a id="internals-and-algorithms"></a>
<a id="module_manager"></a>
<a id="other-declarations"></a>
<a id="physics_world"></a>
<a id="reflecttype_registry"></a>
<a id="renderer"></a>
<a id="script_runtime"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="type_registry"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
