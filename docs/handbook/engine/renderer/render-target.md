---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# render_target.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/render_target.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Rendering a frame](../../../api/renderer/rendering-a-frame.md) |

<p>Off-screen targets: the editor viewport and the AI agent&#x27;s screenshots render into one and read it back synchronously through <code>renderer::read_back</code>. Created, resized and destroyed through the renderer; this class exposes only what the editor needs.</p>

## Structs

| File | Description |
|---|---|
| [`render_target_desc`](render-target/render-target-desc.md) |  |

## Classes

| File | Description |
|---|---|
| [`render_target`](render-target/render-target.md) |  |

## Forward Declarations

### `render_target::impl` {#symbol-render-target-impl}

```cpp
struct impl;
```

The private state (engine/src/renderer/<code>render_target_impl.h</code>), reachable through <code><a href="render-target.html">renderer_access</a></code>.

### `renderer_access` {#symbol-renderer-access}

```cpp
struct renderer_access;
```

## Functions

### Public

#### `is_render_target_colour_format` {#function-is-render-target-colour-format}

<div class="function-declaration" id="function-is-render-target-colour-format-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_render_target_colour_format(
    <a href="../rhi/texture-format.html#symbol-texture-format">rhi::texture_format</a> format) <span class="k">noexcept</span>;</code></pre></div>

<p>True for the colour formats a render target may use: <code>RGBA8_UNORM</code>, <code>RGBA8_SRGB</code>, <code>BGRA8_SRGB</code>, <code>RGBA16_FLOAT</code> and <code>R11G11B10_FLOAT</code>.</p>

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
<a id="internals-and-algorithms"></a>
<a id="is_render_target_colour_format"></a>
<a id="render_target"></a>
<a id="render_target_desc"></a>
<a id="render_target_desccolourformat"></a>
<a id="render_target_descoperator"></a>
<a id="render_target_descsize"></a>
<a id="render_target_descwithdepth"></a>
<a id="render_targetcolour_texture"></a>
<a id="render_targetdesc"></a>
<a id="render_targetframes_rendered"></a>
<a id="render_targethas_depth"></a>
<a id="render_targetimpl"></a>
<a id="render_targetmimpl"></a>
<a id="render_targetoperator"></a>
<a id="render_targetrender_target"></a>
<a id="render_targetrender_target_1"></a>
<a id="render_targetrenderer_access"></a>
<a id="render_targetsize"></a>
<a id="renderer_access"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
