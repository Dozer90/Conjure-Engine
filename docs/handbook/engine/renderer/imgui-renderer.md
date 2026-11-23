---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# imgui_renderer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/imgui_renderer.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Rendering a frame](../../../api/renderer/rendering-a-frame.md) |

<p>The Dear ImGui render backend. Compiled only in editor builds (<code>CONJURE_BUILD_EDITOR</code>, never in Distribution); the header is empty otherwise so it still compiles alone. The ImGui context belongs to <code><a href="../engine/imgui-layer/imgui-layer.html">engine::imgui_layer</a></code>: this class only draws. With ImGui &gt;= 1.92 it implements the texture protocol (<code>ImGuiBackendFlags_RendererHasTextures</code>); with the pinned 1.91 it uploads the font atlas once and again when the atlas changed: a new pixel pointer, a new size, a TexID that is not the backend&#x27;s (<code>io.Fonts-&gt;SetTexID(0)</code> after a rebuild), or an explicit <code>invalidate_font_atlas()</code>. The owner of the context must do one of the last two after <code>io.Fonts-&gt;Build()</code>: a rebuilt atlas of the same size can land at the same address.</p>

## Structs

| File | Description |
|---|---|
| [`imgui_render_stats`](imgui-renderer/imgui-render-stats.md) | Statistics of the last <code>render()</code>. |
| [`imgui_renderer::create_tag`](imgui-renderer/imgui-renderer-create-tag.md) |  |

## Classes

| File | Description |
|---|---|
| [`imgui_renderer`](imgui-renderer/imgui-renderer.md) |  |

## Type Aliases

### `imgui_texture_resolver` {#symbol-imgui-texture-resolver}

```cpp
using imgui_texture_resolver = std::function<nvrhi::ITexture *(gpu_texture_id)>;
```

Resolves a renderer texture id to its NVRHI texture (<code>nullptr</code> when stale); supplied by the renderer.

## Forward Declarations

### `ICommandList` {#symbol-icommandlist}

```cpp
class ICommandList;
```

### `IFramebuffer` {#symbol-iframebuffer}

```cpp
class IFramebuffer;
```

### `ImDrawData` {#symbol-imdrawdata}

```cpp
struct ImDrawData;
```

### `imgui_renderer::impl` {#symbol-imgui-renderer-impl}

```cpp
struct impl;
```

### `ITexture` {#symbol-itexture}

```cpp
class ITexture;
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
<a id="icommandlist"></a>
<a id="iframebuffer"></a>
<a id="imdrawdata"></a>
<a id="imgui_render_stats"></a>
<a id="imgui_render_statscommandsclipped"></a>
<a id="imgui_render_statsdrawcalls"></a>
<a id="imgui_render_statsindices"></a>
<a id="imgui_render_statstexturescreated"></a>
<a id="imgui_render_statstexturesdestroyed"></a>
<a id="imgui_render_statstexturesupdated"></a>
<a id="imgui_render_statsvertices"></a>
<a id="imgui_renderer"></a>
<a id="imgui_renderercreate"></a>
<a id="imgui_renderercreate_tag"></a>
<a id="imgui_rendererimgui_renderer"></a>
<a id="imgui_rendererimgui_renderer_1"></a>
<a id="imgui_rendererimpl"></a>
<a id="imgui_rendererinvalidate_font_atlas"></a>
<a id="imgui_renderermimpl"></a>
<a id="imgui_rendereroperator"></a>
<a id="imgui_rendererregister_texture"></a>
<a id="imgui_rendererrender"></a>
<a id="imgui_rendererstats"></a>
<a id="imgui_renderertexture_count"></a>
<a id="imgui_rendererunregister_texture"></a>
<a id="imgui_rendereruses_texture_protocol"></a>
<a id="imgui_texture_resolver"></a>
<a id="internals-and-algorithms"></a>
<a id="itexture"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
