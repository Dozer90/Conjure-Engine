---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# debug_overlay_layer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/debug_overlay_layer.h` |
| API ↗ | [The frame, layers and time](../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../api/engine/index.md) |

<p>The frame statistics overlay: compiled in Debug and Development only; the header is empty in Distribution so it still compiles alone. Every frame it formats the latest <code><a href="frame-stats/frame-sample.html">frame_sample</a></code> (frame time, phase times, fixed steps, frame memory, dropped time) into <code>text()</code>; in editor builds with an ImGui frame open it also draws that text in a small window, and it logs the text at <code>INFO</code> every <code>log_interval_frames()</code> frames (0 disables) so a headless run still shows its timing.</p>



## Classes

| File | Description |
|---|---|
| [`debug_overlay_layer`](debug-overlay-layer/debug-overlay-layer.md) |  |

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
<a id="debug_overlay_layer"></a>
<a id="debug_overlay_layerdebug_overlay_layer"></a>
<a id="debug_overlay_layerdebug_overlay_layer_1"></a>
<a id="debug_overlay_layerdefault_log_interval_frames"></a>
<a id="debug_overlay_layerformat_sample"></a>
<a id="debug_overlay_layerframes_seen"></a>
<a id="debug_overlay_layeris_visible"></a>
<a id="debug_overlay_layerlog_interval_frames"></a>
<a id="debug_overlay_layermframesseen"></a>
<a id="debug_overlay_layermlogintervalframes"></a>
<a id="debug_overlay_layermtext"></a>
<a id="debug_overlay_layermvisible"></a>
<a id="debug_overlay_layeron_frame_end"></a>
<a id="debug_overlay_layeron_imgui"></a>
<a id="debug_overlay_layerset_log_interval_frames"></a>
<a id="debug_overlay_layerset_visible"></a>
<a id="debug_overlay_layertext"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
