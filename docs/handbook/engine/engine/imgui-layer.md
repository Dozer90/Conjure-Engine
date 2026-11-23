---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# imgui_layer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/imgui_layer.h` |
| API ↗ | [The frame, layers and time](../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../api/engine/index.md) |

<p>The Dear ImGui context owner: compiled only in editor builds (<code>CONJURE_BUILD_EDITOR</code>, never in Distribution); the header is empty otherwise so it still compiles alone. The layer creates the single ImGui context, feeds it the platform events, opens the frame in <code>on_frame_begin</code> (<code>ImGui::NewFrame</code>) and ends it in <code>on_imgui_end</code> (<code>ImGui::Render</code> + <code>renderer::set_imgui_draw_data</code>). <code><a href="../renderer/imgui-renderer/imgui-renderer.html">renderer::imgui_renderer</a></code> only draws; no module calls SetCurrentContext.</p>

## Structs

| File | Description |
|---|---|
| [`imgui_layer_stats`](imgui-layer/imgui-layer-stats.md) | Counters of what the layer forwarded; read by the editor&#x27;s diagnostics and the tests. |

## Classes

| File | Description |
|---|---|
| [`imgui_layer`](imgui-layer/imgui-layer.md) |  |

## Forward Declarations

### `ImGuiContext` {#symbol-imguicontext}

```cpp
struct ImGuiContext;
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
<a id="extension-points"></a>
<a id="functions"></a>
<a id="imgui_layer"></a>
<a id="imgui_layer_stats"></a>
<a id="imgui_layer_statseventsconsumed"></a>
<a id="imgui_layer_statseventsforwarded"></a>
<a id="imgui_layer_statsframesbegun"></a>
<a id="imgui_layer_statsframesrendered"></a>
<a id="imgui_layer_statsoperator"></a>
<a id="imgui_layerclipboard_text"></a>
<a id="imgui_layercontext"></a>
<a id="imgui_layerimgui_layer"></a>
<a id="imgui_layerimgui_layer_1"></a>
<a id="imgui_layeris_frame_open"></a>
<a id="imgui_layermclipboard"></a>
<a id="imgui_layermcontext"></a>
<a id="imgui_layermframeopen"></a>
<a id="imgui_layermstats"></a>
<a id="imgui_layermwindow"></a>
<a id="imgui_layeron_attach"></a>
<a id="imgui_layeron_detach"></a>
<a id="imgui_layeron_event"></a>
<a id="imgui_layeron_frame_begin"></a>
<a id="imgui_layeron_imgui_end"></a>
<a id="imgui_layerset_clipboard_text"></a>
<a id="imgui_layerstats"></a>
<a id="imguicontext"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
