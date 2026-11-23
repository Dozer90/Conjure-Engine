---
kind: class
---

[Editor](../../../editor.md) → [Applications](../../applications.md) → [editor_context.h](../editor-context.md)

# editor_context
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::reflect` |
| Include File Path | `engine/include/conjure/editor/editor_context.h` |
| Source File Path | `engine/src/editor/core/editor_context.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API editor_context
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">editor_context(
    <a href="../../../engine/engine/application/application.html">engine::application</a> &amp;engine,
    <a href="editor-host.html">editor_host</a> *host,
    <a href="../editor-context.html#symbol-editor-display-mode">editor_display_mode</a> displayMode);</code></td><td>Binds to an initialised application. The built-in commands are registered, the console sink installed and the engine put into the <code>EDITING</code> mode (<code>PAUSED</code>). <code>host</code> may be <code>null</code> in tests (<code>editor.quit</code> and <code>project.open</code> then answer <code>NOT_AVAILABLE</code>). <code>displayMode</code> is kept for the context&#x27;s lifetime, across rebind.</td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~editor_context();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">DEFAULT_<wbr>VIEWPORT_<wbr>ASPECT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../engine/core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">16.0</span>f / <span class="mi">9.0</span>f</code></td><td>The width / height a view command assumes until a viewport panel reports its own (headless sessions keep it).</td></tr>
<tr><td>private</td><td><code class="signature">mCamera</code></td><td><code class="signature"><a href="../editor-camera/editor-camera.html">editor_camera</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mConsole</code></td><td><code class="signature"><a href="../console-buffer/console-buffer.html">console_buffer</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Consumer<wbr>Installed</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDebugDraw</code></td><td><code class="signature"><a href="editor-context-debug-draw-flags.html">debug_draw_flags</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDispatcher</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../command-dispatcher/command-dispatcher.html">command_<wbr>dispatcher</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDisplayMode</code></td><td><code class="signature"><a href="../editor-context.html#symbol-editor-display-mode">editor_display_mode</a></code></td><td><code class="signature">editor_<wbr>display_<wbr>mode::<wbr>HEADLESS</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Dotnet<wbr>Executable</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">"dotnet"</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEngine</code></td><td><code class="signature"><a href="../../../engine/engine/application/application.html">engine::application</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEvents</code></td><td><code class="signature"><a href="../event-broadcaster/event-broadcaster.html">event_broadcaster</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFrameTasks</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="frame-task.html">frame_<wbr>task</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mHost</code></td><td><code class="signature"><a href="editor-host.html">editor_host</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mInbound</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../protocol/protocol-message/protocol-request.html">protocol_<wbr>request</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLastLogSeq</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Last<wbr>Script<wbr>Configuration</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">"Debug"</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Module<wbr>Consumer</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../editor-module-consumer/editor-module-consumer.html">editor_<wbr>module_<wbr>consumer</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPlay</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../play-controller/play-controller.html">play_<wbr>controller</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mProject</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="../../../engine/engine/project/project.html">engine::<wbr>project</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRegistry</code></td><td><code class="signature"><a href="../command-registry/command-registry.html">command_registry</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Requests<wbr>Per<wbr>Frame</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature">protocol_<wbr>server::<wbr>DEFAULT_<wbr>REQUESTS_<wbr>PER_<wbr>FRAME</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mResponses</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../protocol/protocol-message/protocol-response.html">protocol_<wbr>response</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Response<wbr>Sinks</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>pair&lt;<wbr>u32,<wbr> <a href="../editor-context.html#symbol-editor-context-response-sink">response_<wbr>sink</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRevision</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSceneDirty</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSceneOpen</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mScenePath</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mScreenshots</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../screenshot-service/screenshot-service.html">screenshot_<wbr>service</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Script<wbr>Compile<wbr>Token</code></td><td><code class="signature">std::weak_ptr&lt;<span class="k">void</span>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSelection</code></td><td><code class="signature"><a href="../selection-set/selection-set.html">selection_set</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Selection<wbr>Subscription</code></td><td><code class="signature"><a href="../../../engine/core/event/event-subscription.html">core::event_subscription</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mServer</code></td><td><code class="signature"><a href="../protocol/protocol-server/protocol-server.html">protocol_server</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTrust</code></td><td><code class="signature"><a href="../project-trust/project-trust.html">project_trust</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTrustPrompt</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr>std::<wbr>filesystem::<wbr>path&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Trust<wbr>Prompt<wbr>Enabled</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mUndo</code></td><td><code class="signature"><a href="../undo-stack/undo-stack.html">undo_stack</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mViewMode</code></td><td><code class="signature"><a href="../editor-context.html#symbol-editor-context-view-mode">view_mode</a></code></td><td><code class="signature">view_<wbr>mode::<wbr>THREE_<wbr>D</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Viewport<wbr>Aspect</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-f32">f32</a></code></td><td><code class="signature">DEFAULT_<wbr>VIEWPORT_<wbr>ASPECT</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add_frame_task` {#function-add-frame-task}

<div class="function-declaration" id="function-add-frame-task-1"><pre><code><span class="k">void</span> add_frame_task(
    std::unique_ptr&lt;<a href="frame-task.html">frame_task</a>&gt; task);</code></pre></div>

<p>Adds a task <code><a href="#function-tick-1">tick</a>()</code> advances every frame until it reports completion.</p>

#### `asset_database` {#function-asset-database}

<div class="function-declaration" id="function-asset-database-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/assets/asset-database/asset-database.html">assets::asset_database</a> *asset_database() <span class="k">noexcept</span>;</code></pre></div>

#### `assets` {#function-assets}

<div class="function-declaration" id="function-assets-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/assets/asset-manager/asset-manager.html">assets::asset_manager</a> *assets() <span class="k">noexcept</span>;</code></pre></div>

#### `bump_revision` {#function-bump-revision}

<div class="function-declaration" id="function-bump-revision-1"><pre><code><a href="../../../engine/core/types.html#symbol-u64">u64</a> bump_revision();</code></pre></div>

<p>Increments the revision (every mutating command). Undoable mutations and history commands also mark the scene dirty; <code>NO_UNDO</code> commands (<code>scene.save</code>, <code>scene.new</code>, ...) manage the flag themselves.</p>

#### `camera` {#function-camera}

<div class="function-declaration" id="function-camera-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../editor-camera/editor-camera.html">editor_camera</a> &amp;camera() <span class="k">noexcept</span>;</code></pre></div>

#### `close_trust_prompt_for` {#function-close-trust-prompt-for}

<div class="function-declaration" id="function-close-trust-prompt-for-1"><pre><code><span class="k">void</span> close_trust_prompt_for(
    <span class="k">const</span> std::filesystem::path &amp;projectRoot);</code></pre></div>

<p>Closes the question when it is about <code>projectRoot</code> (the same <code>project_trust::project_key</code>): the project was trusted by somebody, or it is the project that is now open. Another project&#x27;s question stays.</p>

#### `components` {#function-components}

<div class="function-declaration" id="function-components-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../../engine/ecs/component-registry/component-registry.html">ecs::component_registry</a> &amp;components() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `console` {#function-console}

<div class="function-declaration" id="function-console-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../console-buffer/console-buffer.html">console_buffer</a> &amp;console() <span class="k">noexcept</span>;</code></pre></div>

#### `current_view_mode` {#function-current-view-mode}

<div class="function-declaration" id="function-current-view-mode-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../editor-context.html#symbol-editor-context-view-mode">view_mode</a> current_view_mode() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `debug_draw` {#function-debug-draw}

<div class="function-declaration" id="function-debug-draw-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="editor-context-debug-draw-flags.html">debug_draw_flags</a> &amp;debug_draw() <span class="k">noexcept</span>;</code></pre></div>

#### `deliver_response` {#function-deliver-response}

<div class="function-declaration" id="function-deliver-response-1"><pre><code><span class="k">void</span> deliver_response(
    <span class="k">const</span> <a href="../protocol/protocol-message/protocol-response.html">protocol_response</a> &amp;response);</code></pre></div>

<p>Hands a response to its client&#x27;s sink or to the server.</p>

#### `describe_state` {#function-describe-state}

<div class="function-declaration" id="function-describe-state-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> describe_state() <span class="k">const</span>;</code></pre></div>

<p>Writes {project, scene, playState, revision, selection, headless, offscreen, port, clients, frame, trustPrompt}; trustPrompt is the path of the project the window asks about, or <code>null</code>.</p>

#### `dismiss_trust_prompt` {#function-dismiss-trust-prompt}

<div class="function-declaration" id="function-dismiss-trust-prompt-1"><pre><code><span class="k">void</span> dismiss_trust_prompt() <span class="k">noexcept</span>;</code></pre></div>

<p>Closes the question unanswered (&quot;Don&#x27;t open&quot;): nothing is recorded, the project stays as it was.</p>

#### `dispatcher` {#function-dispatcher}

<div class="function-declaration" id="function-dispatcher-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../command-dispatcher/command-dispatcher.html">command_dispatcher</a> &amp;dispatcher() <span class="k">noexcept</span>;</code></pre></div>

#### `display_mode` {#function-display-mode}

<div class="function-declaration" id="function-display-mode-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../editor-context.html#symbol-editor-display-mode">editor_display_mode</a> display_mode() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The display mode given to the constructor.</p>

#### `dotnet_executable` {#function-dotnet-executable}

<div class="function-declaration" id="function-dotnet-executable-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;dotnet_executable() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The dotnet program <code>script.compile</code> spawns (&quot;dotnet&quot; by default; tests point it elsewhere).</p>

#### `emit_selection_changed` {#function-emit-selection-changed}

<div class="function-declaration" id="function-emit-selection-changed-1"><pre><code><span class="k">void</span> emit_selection_changed();</code></pre></div>

<p>Emits <code>selection.changed</code> with <code><a href="#function-selection-payload-1">selection_payload</a>()</code>. The selection&#x27;s <code>on_changed</code> and <code>play.stop</code> both emit through it, so every <code>selection.changed</code> has the same shape.</p>

#### `engine` {#function-engine}

<div class="function-declaration" id="function-engine-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/engine/application/application.html">engine::application</a> &amp;engine() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-engine-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../../engine/engine/application/application.html">engine::application</a> &amp;engine() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `engine_context` {#function-engine-context}

<div class="function-declaration" id="function-engine-context-1"><pre><code>[[<span class="na">nodiscard</span>]] engine::engine_context &amp;engine_context() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-engine-context-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> engine::engine_context &amp;engine_context() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `events` {#function-events}

<div class="function-declaration" id="function-events-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../event-broadcaster/event-broadcaster.html">event_broadcaster</a> &amp;events() <span class="k">noexcept</span>;</code></pre></div>

#### `filesystem` {#function-filesystem}

<div class="function-declaration" id="function-filesystem-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;filesystem() <span class="k">noexcept</span>;</code></pre></div>

#### `frame_index` {#function-frame-index}

<div class="function-declaration" id="function-frame-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u64">u64</a> frame_index() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `frame_task_count` {#function-frame-task-count}

<div class="function-declaration" id="function-frame-task-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-usize">usize</a> frame_task_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `has_project` {#function-has-project}

<div class="function-declaration" id="function-has-project-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_project() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `has_scene` {#function-has-scene}

<div class="function-declaration" id="function-has-scene-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_scene() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `headless` {#function-headless}

<div class="function-declaration" id="function-headless-1"><pre><code>[[<span class="na">deprecated</span>(<span class="s">"use is_headless()"</span>)]] [[<span class="na">nodiscard</span>]] <span class="k">bool</span> headless() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The former name of <code><a href="#function-is-headless-1">is_headless</a>()</code>, kept for one wave; returns <code><a href="#function-is-headless-1">is_headless</a>()</code>.</p>

#### `host` {#function-host}

<div class="function-declaration" id="function-host-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="editor-host.html">editor_host</a> *host() <span class="k">noexcept</span>;</code></pre></div>

#### `input` {#function-input}

<div class="function-declaration" id="function-input-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/platform/input/input.html">platform::input</a> *input() <span class="k">noexcept</span>;</code></pre></div>

#### `install_module_consumer` {#function-install-module-consumer}

<div class="function-declaration" id="function-install-module-consumer-1"><pre><code><span class="k">void</span> install_module_consumer();</code></pre></div>

<p>Installs the module consumer into the engine&#x27;s module manager (<code>unload_all</code>, <code>add_consumer</code>, <code>load_all</code>, since the engine loads its static modules before the editor exists). Called by the constructor and <code><a href="#function-rebind-1">rebind</a>()</code>; public for tests that construct the context over a bare application.</p>

#### `is_headless` {#function-is-headless}

<div class="function-declaration" id="function-is-headless-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_headless() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for <code>HEADLESS</code> and <code>HEADLESS_OFFSCREEN</code>: the session has no window.</p>

#### `is_open_scene` {#function-is-open-scene}

<div class="function-declaration" id="function-is-open-scene-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_open_scene(
    <span class="k">const</span> <a href="../../../engine/assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id);</code></pre></div>

<p>True when the asset is the file behind the open scene (<code><a href="#function-names-asset-1">names_asset</a>()</code> of <code><a href="#function-scene-path-1">scene_path</a>()</code>): the one test <code>asset.delete</code> applies before it asks for closeOpenScene and the asset browser before it asks the user. False without an open scene and for a scene that has no path yet.</p>

#### `last_script_configuration` {#function-last-script-configuration}

<div class="function-declaration" id="function-last-script-configuration-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;last_script_configuration() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The build configuration of the last <code>script.compile</code> that succeeded (&quot;Debug&quot; until one did, and again after a <code>project.open</code>): <code>script.reload</code> loads scripts/bin/&lt;configuration&gt;/<code>Game.dll</code> by default.</p>

#### `module_consumer` {#function-module-consumer}

<div class="function-declaration" id="function-module-consumer-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../editor-module-consumer/editor-module-consumer.html">editor_module_consumer</a> &amp;module_consumer() <span class="k">noexcept</span>;</code></pre></div>

#### `modules` {#function-modules}

<div class="function-declaration" id="function-modules-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/modules/module-manager/module-manager.html">modules::module_manager</a> *modules() <span class="k">noexcept</span>;</code></pre></div>

#### `names_asset` {#function-names-asset}

<div class="function-declaration" id="function-names-asset-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> names_asset(
    std::string_view assetPath,
    <span class="k">const</span> <a href="../../../engine/assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id);</code></pre></div>

<p>True when <code>assetPath</code> names the source file of the registered asset <code>id</code>. Decided by file, not by spelling: the database looks a path up exactly, while a case-insensitive volume opens &quot;Scenes/Main&quot; as the file registered as scenes/main, so a path the lookup does not resolve to the asset is compared with the asset&#x27;s file on disk (assets:// only, the mount whose directory the project names). False for &quot;&quot;, for an unknown asset and without an asset database.</p>

#### `normalise_scene_path` {#function-normalise-scene-path}

<div class="function-declaration" id="function-normalise-scene-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::string normalise_scene_path(
    std::string_view path);</code></pre></div>

<p>Resolves a scene argument to the canonical asset path the database registers: a &quot;scheme://&quot; path keeps its scheme, anything else is relative to assets://; the relative part gets forward slashes and loses empty, &quot;.&quot; and resolvable &quot;..&quot; segments and any leading separator; &quot;.<code>scene.json</code>&quot; is appended when missing (compared without case; a bare &quot;.json&quot; is replaced). &quot;&quot; for a blank path and for one that leaves its mount (&quot;..&quot;, &quot;../x&quot;, a drive-letter path).</p>

#### `offscreen` {#function-offscreen}

<div class="function-declaration" id="function-offscreen-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> offscreen() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for <code>HEADLESS_OFFSCREEN</code> only: view commands are meant to render off-screen.</p>

#### `physics` {#function-physics}

<div class="function-declaration" id="function-physics-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/physics/physics-world/physics-world.html">physics::physics_world</a> *physics() <span class="k">noexcept</span>;</code></pre></div>

#### `play` {#function-play}

<div class="function-declaration" id="function-play-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../play-controller/play-controller.html">play_controller</a> &amp;play() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-play-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../play-controller/play-controller.html">play_controller</a> &amp;play() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `project` {#function-project}

<div class="function-declaration" id="function-project-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/engine/project/project.html">engine::project</a> *project() <span class="k">noexcept</span>;</code></pre></div>

<p>The editor&#x27;s copy of the project (mutable: <code>project.set_settings</code> edits and saves it).</p>

<div class="function-declaration" id="function-project-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../../engine/engine/project/project.html">engine::project</a> *project() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `raise_trust_prompt` {#function-raise-trust-prompt}

<div class="function-declaration" id="function-raise-trust-prompt-1"><pre><code><span class="k">bool</span> raise_trust_prompt(
    <span class="k">const</span> std::filesystem::path &amp;projectRoot);</code></pre></div>

<p>Makes <code>projectRoot</code> the project the window asks about (&quot;Do you trust the authors of this project?&quot;), replacing an earlier one, and returns <code>true</code>. The question keeps the folder as <code>project_trust::project_key</code> resolves it (absolute, links followed), which is what an answer records and what <code>PROJECT_NOT_TRUSTED</code> reports, so the person is shown the folder that would be trusted whatever spelling asked for it; the path as given only when it has no key. Does nothing and returns <code>false</code> while prompts are not enabled or for an empty path. The prompt is editor state, not a command: it survives an engine restart and is reported by <code>editor.state</code> as trustPrompt.</p>

#### `rebind` {#function-rebind}

<div class="function-declaration" id="function-rebind-1"><pre><code><span class="k">void</span> rebind(
    <a href="../../../engine/engine/application/application.html">engine::application</a> &amp;engine);</code></pre></div>

<p>Points the context at another initialised application (after <code>project.open</code> restarted the engine): scene state, undo stack and selection are cleared, every pending asynchronous request answers <code>CANCELLED</code>, the frame tasks are dropped and the module consumer re-installed.</p>

#### `register_response_sink` {#function-register-response-sink}

<div class="function-declaration" id="function-register-response-sink-1"><pre><code><span class="k">void</span> register_response_sink(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId,
    <a href="../editor-context.html#symbol-editor-context-response-sink">response_sink</a> sink);</code></pre></div>

<p>Routes the responses of a client to an in-process sink instead of the server (the headless runner registers its exec client here). <code>A</code> later registration for the same id replaces it.</p>

#### `registry` {#function-registry}

<div class="function-declaration" id="function-registry-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../command-registry/command-registry.html">command_registry</a> &amp;registry() <span class="k">noexcept</span>;</code></pre></div>

#### `renderer` {#function-renderer}

<div class="function-declaration" id="function-renderer-1"><pre><code>[[<span class="na">nodiscard</span>]] renderer::renderer *renderer() <span class="k">noexcept</span>;</code></pre></div>

#### `revision` {#function-revision}

<div class="function-declaration" id="function-revision-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-u64">u64</a> revision() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `scene` {#function-scene}

<div class="function-declaration" id="function-scene-1"><pre><code>[[<span class="na">nodiscard</span>]] scene::scene &amp;scene() <span class="k">noexcept</span>;</code></pre></div>

<p>The one scene over the world (asserts that the scene manager exists).</p>

<div class="function-declaration" id="function-scene-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> scene::scene &amp;scene() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `scene_dirty` {#function-scene-dirty}

<div class="function-declaration" id="function-scene-dirty-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> scene_dirty() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `scene_manager` {#function-scene-manager}

<div class="function-declaration" id="function-scene-manager-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/scene/scene-manager/scene-manager.html">scene::scene_manager</a> &amp;scene_manager() <span class="k">noexcept</span>;</code></pre></div>

#### `scene_path` {#function-scene-path}

<div class="function-declaration" id="function-scene-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;scene_path() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The asset path of the open scene (&quot;assets://scenes/<code>main.scene.json</code>&quot;), &quot;&quot; for an unsaved one.</p>

#### `screenshots` {#function-screenshots}

<div class="function-declaration" id="function-screenshots-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../screenshot-service/screenshot-service.html">screenshot_service</a> &amp;screenshots() <span class="k">noexcept</span>;</code></pre></div>

#### `script_compile_pending` {#function-script-compile-pending}

<div class="function-declaration" id="function-script-compile-pending-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> script_compile_pending() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while a <code>script.compile</code> runs: its task holds the token registered with <code>set_script_compile_token</code>, and the token expires when the task finishes or is dropped.</p>

#### `scripting` {#function-scripting}

<div class="function-declaration" id="function-scripting-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/scripting/script-runtime/script-runtime.html">scripting::script_runtime</a> *scripting() <span class="k">noexcept</span>;</code></pre></div>

#### `selection` {#function-selection}

<div class="function-declaration" id="function-selection-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../selection-set/selection-set.html">selection_set</a> &amp;selection() <span class="k">noexcept</span>;</code></pre></div>

#### `selection_payload` {#function-selection-payload}

<div class="function-declaration" id="function-selection-payload-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> selection_payload() <span class="k">const</span>;</code></pre></div>

<p>The current selection as {guids, primary}: the sorted guids and the primary guid, <code>null</code> when nothing is selected. The one builder of that shape: <code>selection.changed</code> carries exactly this, and the selection.* answers add &quot;entities&quot; and &quot;count&quot; to it.</p>

#### `server` {#function-server}

<div class="function-declaration" id="function-server-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../protocol/protocol-server/protocol-server.html">protocol_server</a> *server() <span class="k">noexcept</span>;</code></pre></div>

<p>The protocol server the application runs, or <code>nullptr</code> (tests without one).</p>

#### `set_dotnet_executable` {#function-set-dotnet-executable}

<div class="function-declaration" id="function-set-dotnet-executable-1"><pre><code><span class="k">void</span> set_dotnet_executable(
    std::string executable);</code></pre></div>

#### `set_last_script_configuration` {#function-set-last-script-configuration}

<div class="function-declaration" id="function-set-last-script-configuration-1"><pre><code><span class="k">void</span> set_last_script_configuration(
    std::string configuration);</code></pre></div>

<p>Records the configuration of a <code>script.compile</code> that succeeded (<code>compile_task</code> calls it); <code><a href="#function-rebind-1">rebind</a>()</code> resets it to &quot;Debug&quot;.</p>

#### `set_requests_per_frame` {#function-set-requests-per-frame}

<div class="function-declaration" id="function-set-requests-per-frame-1"><pre><code><span class="k">void</span> set_requests_per_frame(
    <a href="../../../engine/core/types.html#symbol-usize">usize</a> count) <span class="k">noexcept</span>;</code></pre></div>

#### `set_scene_dirty` {#function-set-scene-dirty}

<div class="function-declaration" id="function-set-scene-dirty-1"><pre><code><span class="k">void</span> set_scene_dirty(
    <span class="k">bool</span> dirty) <span class="k">noexcept</span>;</code></pre></div>

#### `set_scene_state` {#function-set-scene-state}

<div class="function-declaration" id="function-set-scene-state-1"><pre><code><span class="k">void</span> set_scene_state(
    <span class="k">bool</span> open,
    std::string path);</code></pre></div>

#### `set_script_compile_token` {#function-set-script-compile-token}

<div class="function-declaration" id="function-set-script-compile-token-1"><pre><code><span class="k">void</span> set_script_compile_token(
    <span class="k">const</span> std::shared_ptr&lt;<span class="k">void</span>&gt; &amp;token) <span class="k">noexcept</span>;</code></pre></div>

<p>Registers the token the running <code>script.compile</code> task owns (see <code>script_compile_pending</code>).</p>

#### `set_server` {#function-set-server}

<div class="function-declaration" id="function-set-server-1"><pre><code><span class="k">void</span> set_server(
    <a href="../protocol/protocol-server/protocol-server.html">protocol_server</a> *server) <span class="k">noexcept</span>;</code></pre></div>

#### `set_trust` {#function-set-trust}

<div class="function-declaration" id="function-set-trust-1"><pre><code><span class="k">void</span> set_trust(
    <a href="../project-trust/project-trust.html">project_trust</a> *answers) <span class="k">noexcept</span>;</code></pre></div>

#### `set_trust_prompt_enabled` {#function-set-trust-prompt-enabled}

<div class="function-declaration" id="function-set-trust-prompt-enabled-1"><pre><code><span class="k">void</span> set_trust_prompt_enabled(
    <span class="k">bool</span> enabled) <span class="k">noexcept</span>;</code></pre></div>

<p>Whether a refused project is asked about in the editor window. The application turns it on for a windowed session that runs neither --exec nor --export; everywhere else nobody could answer.</p>

#### `set_view_mode` {#function-set-view-mode}

<div class="function-declaration" id="function-set-view-mode-1"><pre><code><span class="k">void</span> set_view_mode(
    <a href="../editor-context.html#symbol-editor-context-view-mode">view_mode</a> mode) <span class="k">noexcept</span>;</code></pre></div>

#### `set_viewport_aspect` {#function-set-viewport-aspect}

<div class="function-declaration" id="function-set-viewport-aspect-1"><pre><code><span class="k">void</span> set_viewport_aspect(
    <a href="../../../engine/core/types.html#symbol-f32">f32</a> aspect) <span class="k">noexcept</span>;</code></pre></div>

<p>Records the viewport&#x27;s width / height; a value that is not finite or not positive is ignored.</p>

#### `tick` {#function-tick}

<div class="function-declaration" id="function-tick-1"><pre><code><span class="k">void</span> tick();</code></pre></div>

<p>The per-frame body (docs 3.2): server poll, completed async replies, inbound requests (capped), play tick, screenshot tick, log events, server flush. Called by the editor layer&#x27;s <code>on_update</code> or by <code>editor_application::headless_tick</code>.</p>

#### `trust` {#function-trust}

<div class="function-declaration" id="function-trust-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../project-trust/project-trust.html">project_trust</a> *trust() <span class="k">noexcept</span>;</code></pre></div>

<p>The answers about which projects may be opened, or <code>nullptr</code> (a context built over a bare application in tests: <code>project.trust</code>, <code>project.revoke_trust</code> and <code>project.<a href="../project-trust.html#symbol-trust-state">trust_state</a></code> then answer <code>NOT_AVAILABLE</code>). The <code><a href="../editor-application/editor-application.html">editor_application</a></code> owns it and applies it where a project is opened.</p>

#### `trust_prompt` {#function-trust-prompt}

<div class="function-declaration" id="function-trust-prompt-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::optional&lt;std::filesystem::path&gt; &amp;trust_prompt() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The project the window asks about, as <code>raise_trust_prompt</code> kept it; nullopt when it asks about none.</p>

#### `trust_prompt_enabled` {#function-trust-prompt-enabled}

<div class="function-declaration" id="function-trust-prompt-enabled-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> trust_prompt_enabled() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `undo` {#function-undo}

<div class="function-declaration" id="function-undo-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../undo-stack/undo-stack.html">undo_stack</a> &amp;undo() <span class="k">noexcept</span>;</code></pre></div>

#### `unregister_response_sink` {#function-unregister-response-sink}

<div class="function-declaration" id="function-unregister-response-sink-1"><pre><code><span class="k">void</span> unregister_response_sink(
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> clientId);</code></pre></div>

#### `viewport_aspect` {#function-viewport-aspect}

<div class="function-declaration" id="function-viewport-aspect-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-f32">f32</a> viewport_aspect() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The width / height of the viewport the UI draws (<code>DEFAULT_VIEWPORT_ASPECT</code> until a viewport panel reports one): <code>view.pick</code> builds its ray with it when the request names no aspect.</p>

#### `world` {#function-world}

<div class="function-declaration" id="function-world-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/ecs/world/world.html">ecs::world</a> &amp;world() <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `bind_engine` {#function-bind-engine}

<div class="function-declaration" id="function-bind-engine-1"><pre><code><span class="k">void</span> bind_engine(
    <a href="../../../engine/engine/application/application.html">engine::application</a> &amp;engine);</code></pre></div>

#### `forward_log_events` {#function-forward-log-events}

<div class="function-declaration" id="function-forward-log-events-1"><pre><code><span class="k">void</span> forward_log_events();</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
