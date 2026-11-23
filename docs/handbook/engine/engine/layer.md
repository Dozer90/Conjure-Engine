---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# layer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/layer.h` |
| API ↗ | [The frame, layers and time](../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../api/engine/index.md) |

<p>Layers: hooks for executables (editor workspace, ImGui bracket, debug overlay, game bootstrap). Game logic lives in ECS systems and scripts, never in layers. Events travel from the top (overlays) downwards and stop at the first <code><a href="layer/layer.html">layer</a></code> returning <code>true</code>; the frame hooks travel bottom-up.</p>



## Classes

| File | Description |
|---|---|
| [`layer`](layer/layer.md) |  |

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
<a id="internals-and-algorithms"></a>
<a id="layer"></a>
<a id="layerlayer"></a>
<a id="layerlayer_1"></a>
<a id="layermname"></a>
<a id="layername"></a>
<a id="layeron_attach"></a>
<a id="layeron_detach"></a>
<a id="layeron_event"></a>
<a id="layeron_frame_begin"></a>
<a id="layeron_frame_end"></a>
<a id="layeron_imgui"></a>
<a id="layeron_imgui_end"></a>
<a id="layeron_update"></a>
<a id="layeroperator"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
