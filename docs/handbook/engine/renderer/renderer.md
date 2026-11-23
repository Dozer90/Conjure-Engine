---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# renderer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/renderer.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Renderer assets](../../../api/renderer/renderer-assets.md), [Rendering a frame](../../../api/renderer/rendering-a-frame.md) |

<p>The <code><a href="renderer/renderer.html">renderer</a></code>: a render graph over the rhi that turns a <code><a href="render-scene/render-scene.html">render_scene</a></code> into one command list per frame. Owns the <code><a href="render-scene/render-scene-pair.html">render_scene_pair</a></code>, every GPU mirror of the <code><a href="renderer/renderer.html">renderer</a></code> asset types, the render targets and the ImGui backend.</p>

## Structs

| File | Description |
|---|---|
| [`pass_timing`](renderer/pass-timing.md) | GPU time of one executed pass (Development builds, devices with timer queries). |
| [`render_stats`](renderer/render-stats.md) | What the last <code>render()</code> did. Fully populated on the <code>null</code> device too, which is how the tests observe the <code><a href="renderer/renderer.html">renderer</a></code>. The counters of what a frame clamped (<code>viewsSkipped</code>, <code>lightsDropped</code>, <code>spritesDropped</code>, <code>shadowTilesDropped</code>) are filled every frame. The log says less: the <code>WARN</code> about views, lights or sprites beyond their budget or about an unknown render target is logged when the problem starts and not again while it lasts (<code>renderer::render</code>), and dropped shadow tiles are not logged at all. Read the counters to follow a problem that lasts. <code>A</code> frame whose back buffer could not be acquired counts every view of the scene as skipped and fills nothing else. |
| [`renderer::create_tag`](renderer/renderer-create-tag.md) |  |
| [`renderer_desc`](renderer/renderer-desc.md) |  |

## Classes

| File | Description |
|---|---|
| [`renderer`](renderer/renderer.md) |  |

## Forward Declarations

### `ImDrawData` {#symbol-imdrawdata}

```cpp
struct ImDrawData;
```

### `imgui_renderer` {#symbol-imgui-renderer}

```cpp
class imgui_renderer;
```

### `renderer::impl` {#symbol-renderer-impl}

```cpp
struct impl;
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
<a id="imdrawdata"></a>
<a id="imgui_renderer"></a>
<a id="internals-and-algorithms"></a>
<a id="pass_timing"></a>
<a id="pass_timinggpumilliseconds"></a>
<a id="pass_timingname"></a>
<a id="render_stats"></a>
<a id="render_statscount"></a>
<a id="render_statsdebugverticesdrawn"></a>
<a id="render_statsdispatches"></a>
<a id="render_statsdrawcalls"></a>
<a id="render_statsexecuted"></a>
<a id="render_statsfallbacksused"></a>
<a id="render_statsinstancesculled"></a>
<a id="render_statsinstancesdrawn"></a>
<a id="render_statslightsdropped"></a>
<a id="render_statslightsrendered"></a>
<a id="render_statspassesculled"></a>
<a id="render_statspassesexecuted"></a>
<a id="render_statspassnames"></a>
<a id="render_statspasstimings"></a>
<a id="render_statsreset"></a>
<a id="render_statsshadowcascadesrendered"></a>
<a id="render_statsshadowtilesallocated"></a>
<a id="render_statsshadowtilesdropped"></a>
<a id="render_statsshadowtilesreused"></a>
<a id="render_statsspritesdrawn"></a>
<a id="render_statsspritesdropped"></a>
<a id="render_statstransienttexturescreated"></a>
<a id="render_statstransienttextureslive"></a>
<a id="render_statstrianglesdrawn"></a>
<a id="render_statsuploadbytes"></a>
<a id="render_statsuploadscompleted"></a>
<a id="render_statsuploadspending"></a>
<a id="render_statsviewsrendered"></a>
<a id="render_statsviewsskipped"></a>
<a id="renderer"></a>
<a id="renderer_desc"></a>
<a id="renderer_descatlasresolution"></a>
<a id="renderer_desccascaderesolution"></a>
<a id="renderer_descdevice"></a>
<a id="renderer_descenableimgui"></a>
<a id="renderer_descjobs"></a>
<a id="renderer_descshaders"></a>
<a id="renderer_descswapchain"></a>
<a id="rendererbegin_extract"></a>
<a id="renderercreate"></a>
<a id="renderercreate_render_target"></a>
<a id="renderercreate_tag"></a>
<a id="rendererdesc"></a>
<a id="rendererdestroy_render_target"></a>
<a id="rendererdevice"></a>
<a id="rendererend_extract"></a>
<a id="rendererfind_environment"></a>
<a id="rendererfind_material"></a>
<a id="rendererfind_mesh"></a>
<a id="rendererfind_texture"></a>
<a id="rendererframe_count"></a>
<a id="rendererget_render_target"></a>
<a id="rendererimgui"></a>
<a id="rendererimpl"></a>
<a id="rendereris_lost"></a>
<a id="renderermimpl"></a>
<a id="rendereron_device_lost"></a>
<a id="rendereroperator"></a>
<a id="rendererpending_upload_count"></a>
<a id="rendererread_back"></a>
<a id="rendererregister_asset_types"></a>
<a id="rendererrelease_environment"></a>
<a id="rendererrelease_material"></a>
<a id="rendererrelease_mesh"></a>
<a id="rendererrelease_texture"></a>
<a id="rendererrender"></a>
<a id="rendererrender_frame"></a>
<a id="rendererrenderer"></a>
<a id="rendererrenderer_1"></a>
<a id="rendererresize_render_target"></a>
<a id="rendererresize_swapchain"></a>
<a id="rendererset_imgui_draw_data"></a>
<a id="rendererstats"></a>
<a id="renderersubmit_view"></a>
<a id="renderersubmitted_view_count"></a>
<a id="rendererupload_environment"></a>
<a id="rendererupload_material"></a>
<a id="rendererupload_mesh"></a>
<a id="rendererupload_texture"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
