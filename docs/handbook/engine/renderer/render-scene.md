---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# render_scene.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/render_scene.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Rendering a frame](../../../api/renderer/rendering-a-frame.md) |

<p>The extraction contract: the engine&#x27;s <code>EXTRACT</code> systems fill a <code><a href="render-scene/render-scene.html">render_scene</a></code>, the renderer renders it. The renderer owns a <code><a href="render-scene/render-scene-pair.html">render_scene_pair</a></code>; <code>begin_extract</code> clears and returns the write slot, <code>end_extract</code> swaps, <code>render_frame</code> renders the read slot. The pair is not internally synchronised: the engine loop guarantees extraction of frame <code>N</code> and rendering of frame <code>N</code>-1 never touch the same slot (asserted by owner thread in Debug).</p>

## Structs

| File | Description |
|---|---|
| [`mesh_instance`](render-scene/mesh-instance.md) |  |
| [`render_scene`](render-scene/render-scene.md) |  |
| [`render_view`](render-scene/render-view.md) |  |

## Classes

| File | Description |
|---|---|
| [`render_scene_pair`](render-scene/render-scene-pair.md) | Two scenes: one being written by extraction, one being read by the renderer. |

## Enumerations

### `view_flags` {#symbol-view-flags}

```cpp
enum class view_flags : u8 {
    NONE = 0,
    DRAW_DEBUG = 1 << 0,
    NO_POST_PROCESS = 1 << 1,
    SKIP_SHADOWS = 1 << 2
};
```

## Macros

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(view_flags)
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
<a id="conjure_enum_flags"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="mesh_instance"></a>
<a id="mesh_instancecastsshadows"></a>
<a id="mesh_instanceentityid"></a>
<a id="mesh_instancelayermask"></a>
<a id="mesh_instancematerials"></a>
<a id="mesh_instancemesh"></a>
<a id="mesh_instancereceivesshadows"></a>
<a id="mesh_instanceworldbounds"></a>
<a id="mesh_instanceworldfromlocal"></a>
<a id="other-declarations"></a>
<a id="render_scene"></a>
<a id="render_scene_pair"></a>
<a id="render_scene_pairbegin_extract"></a>
<a id="render_scene_pairend_extract"></a>
<a id="render_scene_pairis_extracting"></a>
<a id="render_scene_pairmextracting"></a>
<a id="render_scene_pairmowner"></a>
<a id="render_scene_pairmscenes"></a>
<a id="render_scene_pairmwriteindex"></a>
<a id="render_scene_pairoperator"></a>
<a id="render_scene_pairread"></a>
<a id="render_scene_pairrender_scene_pair"></a>
<a id="render_scene_pairrender_scene_pair_1"></a>
<a id="render_scene_pairwrite"></a>
<a id="render_scene_pairwrite_index"></a>
<a id="render_sceneallocate_meshes"></a>
<a id="render_sceneclear"></a>
<a id="render_scenedebugdraw"></a>
<a id="render_scenedeltaseconds"></a>
<a id="render_sceneenvironment"></a>
<a id="render_sceneframeindex"></a>
<a id="render_scenelights"></a>
<a id="render_scenemeshes"></a>
<a id="render_scenepostprocess"></a>
<a id="render_sceneshadows"></a>
<a id="render_scenesprites"></a>
<a id="render_scenessao"></a>
<a id="render_sceneviews"></a>
<a id="render_view"></a>
<a id="render_viewcameradata"></a>
<a id="render_viewcameraposition"></a>
<a id="render_viewflags"></a>
<a id="render_viewlayermask"></a>
<a id="render_viewtarget"></a>
<a id="render_viewviewfromworld"></a>
<a id="render_viewviewport"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="view_flags"></a>
<a id="view_flagsdraw_debug"></a>
<a id="view_flagsno_post_process"></a>
<a id="view_flagsnone"></a>
<a id="view_flagsskip_shadows"></a>
<!-- doc-legacy-anchors:end -->
