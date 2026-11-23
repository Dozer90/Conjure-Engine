---
kind: header
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md)

# editor_layer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/ui/editor_layer.h` |
| API ↗ | [The editor](../../../../api/editor/index.md), [The workspace UI](../../../../api/editor/workspace-ui.md) |

<p>The engine layer that draws the editor: <code>on_update</code> refreshes the panels&#x27; caches, <code>on_imgui</code> draws the workspace inside the frame the engine&#x27;s <code><a href="../../../engine/engine/imgui-layer/imgui-layer.html">imgui_layer</a></code> opened. It is pushed after <code><a href="../../../engine/engine/imgui-layer/imgui-layer.html">imgui_layer</a></code> by the executable through <code><a href="../editor-application/editor-application.html">editor_application</a></code>&#x27;s layer factory.</p>



## Classes

| File | Description |
|---|---|
| [`ui::editor_layer`](editor-layer/ui-editor-layer.md) |  |

## Functions

### Public

#### `ui::create_editor_layer` {#function-ui-create-editor-layer}

<div class="function-declaration" id="function-ui-create-editor-layer-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::unique_ptr&lt;<a href="../../../engine/engine/layer/layer.html">engine::layer</a>&gt; create_editor_layer(<a href="../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<p>The layer factory <code>editor_application::initialise</code> takes.</p>

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
<a id="create_editor_layer"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="editor_layer"></a>
<a id="editor_layereditor_layer"></a>
<a id="editor_layereditor_layer_1"></a>
<a id="editor_layerframes_drawn"></a>
<a id="editor_layermcontext"></a>
<a id="editor_layermframesdrawn"></a>
<a id="editor_layermworkspace"></a>
<a id="editor_layeron_attach"></a>
<a id="editor_layeron_detach"></a>
<a id="editor_layeron_event"></a>
<a id="editor_layeron_imgui"></a>
<a id="editor_layeron_update"></a>
<a id="editor_layerworkspace"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="uicreate_editor_layer"></a>
<a id="uieditor_layer"></a>
<a id="uieditor_layereditor_layer"></a>
<a id="uieditor_layereditor_layer_1"></a>
<a id="uieditor_layerframes_drawn"></a>
<a id="uieditor_layermcontext"></a>
<a id="uieditor_layermframesdrawn"></a>
<a id="uieditor_layermworkspace"></a>
<a id="uieditor_layeron_attach"></a>
<a id="uieditor_layeron_detach"></a>
<a id="uieditor_layeron_event"></a>
<a id="uieditor_layeron_imgui"></a>
<a id="uieditor_layeron_update"></a>
<a id="uieditor_layerworkspace"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
