---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [imgui_renderer.h](../imgui-renderer.md)

# imgui_renderer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/imgui_renderer.h` |
| Source File Path | `engine/src/renderer/imgui/imgui_renderer.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
class CONJURE_API imgui_renderer
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> imgui_renderer(
    create_tag);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~imgui_renderer();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mImpl</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../imgui-renderer.html">impl</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `create` {#function-create}

<div class="function-declaration" id="function-create-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;std::unique_ptr&lt;<a href="imgui-renderer.html">imgui_renderer</a>&gt;&gt; create(
    <a href="../../rhi/device/device.html">rhi::device</a> &amp;device,
    <a href="../../rhi/shader-library/shader-library.html">rhi::shader_library</a> &amp;shaders,
    <a href="../../rhi/texture-format.html#symbol-texture-format">rhi::texture_format</a> targetFormat,
    <a href="../imgui-renderer.html#symbol-imgui-texture-resolver">imgui_texture_resolver</a> resolver);</code></pre></div>

<p>Builds the pipeline (<code>imgui.hlsl</code>: premultiplied alpha, scissor, no depth) for the given framebuffer colour format from the &quot;imgui&quot; program of <code><a href="../shader-asset/shader-asset.html">shader_asset</a></code>&#x27;s built-in table. Fails with <code>renderer_error::SHADER_NOT_FOUND</code> naming the shader key when the library&#x27;s index lacks a stage of that program (both stages are checked before any device object is created) or a listed blob file cannot be read (found when that stage is loaded), with <code>PIPELINE_CREATION_FAILED</code> when the device refuses a shader, the layouts or the pipeline, and with <code>RESOURCE_CREATION_FAILED</code> when it refuses the sampler or the buffers. <code>A</code> <code>null</code> resolver makes <code>register_texture</code> always fail.</p>

#### `invalidate_font_atlas` {#function-invalidate-font-atlas}

<div class="function-declaration" id="function-invalidate-font-atlas-1"><pre><code><span class="k">void</span> invalidate_font_atlas() <span class="k">noexcept</span>;</code></pre></div>

<p>Marks the font atlas as changed so the next <code><a href="#function-render-1">render</a>()</code> re-uploads it (ImGui 1.91: call it after <code>io.Fonts-&gt;Build()</code>, or reset <code>io.Fonts-&gt;TexID</code> to 0, which has the same effect). <code>A</code> no-op under the 1.92 texture protocol, where ImGui reports the change itself.</p>

#### `register_texture` {#function-register-texture}

<div class="function-declaration" id="function-register-texture-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> register_texture(
    <a href="../gpu-ids.html#symbol-gpu-texture-id">gpu_texture_id</a> texture);</code></pre></div>

<p>Makes a renderer texture usable as an ImTextureID (returned packed as a <code><a href="../../core/types.html#symbol-u64">u64</a></code>). The id stays valid until <code>unregister_texture</code> or destruction; a stale texture draws the font atlas white texel instead.</p>

#### `render` {#function-render}

<div class="function-declaration" id="function-render-1"><pre><code><span class="k">void</span> render(
    <a href="../imgui-renderer.html">ImDrawData</a> *drawData,
    nvrhi::IFramebuffer *target,
    nvrhi::ICommandList &amp;commandList);</code></pre></div>

<p>Draws one frame of ImGui output into <code>target</code> (whose colour format matches the one given at creation). Handles the texture requests carried by the draw data first (ImGui &gt;= 1.92) or the font atlas (1.91). <code>A</code> <code>null</code> <code>drawData</code> or an empty frame draws nothing.</p>

#### `stats` {#function-stats}

<div class="function-declaration" id="function-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="imgui-render-stats.html">imgui_render_stats</a> &amp;stats() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `texture_count` {#function-texture-count}

<div class="function-declaration" id="function-texture-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> texture_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of textures the backend currently knows (font atlas and registered ones).</p>

#### `unregister_texture` {#function-unregister-texture}

<div class="function-declaration" id="function-unregister-texture-1"><pre><code><span class="k">void</span> unregister_texture(
    <a href="../../core/types.html#symbol-u64">u64</a> textureId);</code></pre></div>

<p>Forgets a registered texture; unknown handles are ignored.</p>

#### `uses_texture_protocol` {#function-uses-texture-protocol}

<div class="function-declaration" id="function-uses-texture-protocol-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">bool</span> uses_texture_protocol() <span class="k">noexcept</span>;</code></pre></div>

<p>True when the running ImGui implements the 1.92 texture protocol.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
