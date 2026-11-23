---
kind: class
---

[Editor](../../../../editor.md) → [User Interface](../../../user-interface.md) → [editor_workspace.h](../editor-workspace.md)

# ui::editor_workspace
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/editor_workspace.h` |
| Source File Path | `engine/src/editor/ui/editor_workspace.cpp` |
| API ↗ | [The workspace UI](../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API editor_workspace
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">editor_workspace();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~editor_workspace();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mDrawers</code></td><td><code class="signature"><a href="../property-drawer/ui-property-drawer-registry.html">property_drawer_registry</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFramesDrawn</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mInstances</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="ui-editor-workspace-panel-instance.html">panel_<wbr>instance</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLayoutFile</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Layout<wbr>Loaded</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPanels</code></td><td><code class="signature"><a href="../editor-panel/ui-panel-registry.html">panel_registry</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSaveAsError</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Save<wbr>As<wbr>Open<wbr>Requested</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSaveAsPath</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Save<wbr>As<wbr>Pending</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mShowDemo</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Trust<wbr>Prompt<wbr>Error</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `answer_trust_prompt` {#function-answer-trust-prompt}

<div class="function-declaration" id="function-answer-trust-prompt-1"><pre><code><span class="k">bool</span> answer_trust_prompt(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <span class="k">bool</span> trust);</code></pre></div>

<p>Answers the question the context&#x27;s <code>trust_prompt()</code> asks; <code>false</code>, and nothing happens, when it asks none. With <code>trust</code> (the &quot;Trust and Open&quot; button) the project is trusted through <code>project.trust</code> and then opened through <code>project.open</code>, the same two requests a protocol client sends: <code>true</code> when both were accepted. <code>A</code> <code>project.trust</code> that fails leaves the question open and its error in <code><a href="#function-trust-prompt-error-1">trust_prompt_error</a>()</code>; a <code>project.open</code> that fails afterwards (playing, say) is logged like any menu command, and the project stays trusted. Without <code>trust</code> (&quot;Don&#x27;t Open&quot;) the question is closed, nothing is recorded and the editor stays as it is: <code>true</code>.</p>

#### `close_panel` {#function-close-panel}

<div class="function-declaration" id="function-close-panel-1"><pre><code><span class="k">bool</span> close_panel(
    std::string_view name);</code></pre></div>

<p>Destroys a panel instance. False when not open.</p>

#### `create_default_panels` {#function-create-default-panels}

<div class="function-declaration" id="function-create-default-panels-1"><pre><code><span class="k">void</span> create_default_panels(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<p>Registers the built-in panels and instantiates the ones open by default.</p>

#### `draw` {#function-draw}

<div class="function-declaration" id="function-draw-1"><pre><code><span class="k">void</span> draw(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<p>Draws the dockspace, the menu bar and every open panel. Call inside an ImGui frame.</p>

#### `drawers` {#function-drawers}

<div class="function-declaration" id="function-drawers-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../property-drawer/ui-property-drawer-registry.html">property_drawer_registry</a> &amp;drawers() <span class="k">noexcept</span>;</code></pre></div>

#### `find_panel` {#function-find-panel}

<div class="function-declaration" id="function-find-panel-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../editor-panel/ui-editor-panel.html">editor_panel</a> *find_panel(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

#### `frames_drawn` {#function-frames-drawn}

<div class="function-declaration" id="function-frames-drawn-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-u32">u32</a> frames_drawn() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `layout_file` {#function-layout-file}

<div class="function-declaration" id="function-layout-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;layout_file() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `menu_requests` {#function-menu-requests}

<div class="function-declaration" id="function-menu-requests-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::vector&lt;<a href="ui-menu-request.html">menu_request</a>&gt; menu_requests();</code></pre></div>

<p>Every request the File, Edit and Play menus fire with fixed arguments, as the menu sends them (Delete Selection builds its entity list at click time and Save Scene As takes the path typed in its dialog, so neither is listed). Tests validate them against the command registry&#x27;s schemas.</p>

#### `open_panel` {#function-open-panel}

<div class="function-declaration" id="function-open-panel-1"><pre><code><span class="k">bool</span> open_panel(
    std::string_view name);</code></pre></div>

<p>Instantiates a registered panel (no-op when already open). False when unknown.</p>

#### `open_panel_count` {#function-open-panel-count}

<div class="function-declaration" id="function-open-panel-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-usize">usize</a> open_panel_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `panels` {#function-panels}

<div class="function-declaration" id="function-panels-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../editor-panel/ui-panel-registry.html">panel_registry</a> &amp;panels() <span class="k">noexcept</span>;</code></pre></div>

#### `request_save_scene` {#function-request-save-scene}

<div class="function-declaration" id="function-request-save-scene-1"><pre><code><span class="k">void</span> request_save_scene(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<p>What File &gt; Save Scene and Ctrl+<code>S</code> do: <code>scene.save</code> when the scene has a path, otherwise the Save Scene As dialog (a scene made with New Scene has no path yet). No-op without a scene and while playing or paused: saving is an editing-state command, the playing scene is not the one to keep.</p>

#### `request_save_scene_as` {#function-request-save-scene-as}

<div class="function-declaration" id="function-request-save-scene-as-1"><pre><code><span class="k">void</span> request_save_scene_as(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<p>Opens the Save Scene As dialog on the next draw, its path field set to the scene&#x27;s path, or to &quot;scenes/&lt;scene name&gt;&quot; (&quot;scenes/untitled&quot; for an unnamed scene) when it has none. No-op without a scene, while playing or paused, or while the dialog is already pending.</p>

#### `save_as_error` {#function-save-as-error}

<div class="function-declaration" id="function-save-as-error-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;save_as_error() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The error of the last failed <code><a href="#function-save-scene-as-1">save_scene_as</a>()</code> (&quot;&quot; after a success or a new request).</p>

#### `save_as_path` {#function-save-as-path}

<div class="function-declaration" id="function-save-as-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;save_as_path() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The path in the Save Scene As dialog&#x27;s field.</p>

#### `save_as_pending` {#function-save-as-pending}

<div class="function-declaration" id="function-save-as-pending-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> save_as_pending() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True from <code><a href="#function-request-save-scene-as-1">request_save_scene_as</a>()</code> until the dialog saves, is cancelled or loses its scene.</p>

#### `save_layout` {#function-save-layout}

<div class="function-declaration" id="function-save-layout-1"><pre><code><span class="k">void</span> save_layout();</code></pre></div>

<p>Writes the ImGui ini settings to the layout file (IO failures are logged).</p>

#### `save_scene_as` {#function-save-scene-as}

<div class="function-declaration" id="function-save-scene-as-1"><pre><code><span class="k">bool</span> save_scene_as(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context,
    std::string_view path);</code></pre></div>

<p>Saves the scene through <code>scene.save_as</code> (the path is relative to assets:// unless it names a scheme; .<code>scene.json</code> is added). True on success, which also closes the dialog; on failure the error text is kept for the dialog (<code><a href="#function-save-as-error-1">save_as_error</a>()</code>) and <code>false</code> is returned.</p>

#### `set_layout_file` {#function-set-layout-file}

<div class="function-declaration" id="function-set-layout-file-1"><pre><code><span class="k">void</span> set_layout_file(
    std::filesystem::path path);</code></pre></div>

<p>The layout file; loaded on the first draw when it exists, saved by <code><a href="#function-save-layout-1">save_layout</a>()</code>.</p>

#### `sync_module_panels` {#function-sync-module-panels}

<div class="function-declaration" id="function-sync-module-panels-1"><pre><code><span class="k">void</span> sync_module_panels(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<p>Adds the panels of loaded modules to the registry (and removes those of unloaded ones).</p>

#### `trust_prompt_error` {#function-trust-prompt-error}

<div class="function-declaration" id="function-trust-prompt-error-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;trust_prompt_error() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The error of the last &quot;Trust and Open&quot; whose <code>project.trust</code> failed (&quot;&quot; otherwise).</p>

#### `update` {#function-update}

<div class="function-declaration" id="function-update-1"><pre><code><span class="k">void</span> update(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<p>Per-frame updates of every panel instance (open or closed).</p>

### Private

#### `draw_menu_bar` {#function-draw-menu-bar}

<div class="function-declaration" id="function-draw-menu-bar-1"><pre><code><span class="k">void</span> draw_menu_bar(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

#### `draw_save_as_modal` {#function-draw-save-as-modal}

<div class="function-declaration" id="function-draw-save-as-modal-1"><pre><code><span class="k">void</span> draw_save_as_modal(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

#### `draw_trust_prompt` {#function-draw-trust-prompt}

<div class="function-declaration" id="function-draw-trust-prompt-1"><pre><code><span class="k">void</span> draw_trust_prompt(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

#### `handle_global_shortcuts` {#function-handle-global-shortcuts}

<div class="function-declaration" id="function-handle-global-shortcuts-1"><pre><code><span class="k">void</span> handle_global_shortcuts(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

#### `load_layout_once` {#function-load-layout-once}

<div class="function-declaration" id="function-load-layout-once-1"><pre><code><span class="k">void</span> load_layout_once();</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
