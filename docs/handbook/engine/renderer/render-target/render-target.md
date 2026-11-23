---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [render_target.h](../render-target.md)

# render_target
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/render_target.h` |
| Source File Path | `engine/src/renderer/render_target.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
class CONJURE_API render_target
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">render_target();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~render_target();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mImpl</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../render-target.html">impl</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `colour_texture` {#function-colour-texture}

<div class="function-declaration" id="function-colour-texture-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../gpu-ids.html#symbol-gpu-texture-id">gpu_texture_id</a> colour_texture() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The colour texture registered with the renderer&#x27;s texture table, so the editor can bind it through <code>imgui_renderer::register_texture</code>. The id stays the same across <code>renderer::resize_render_target</code> and then resolves to the new texture, so a registered ImGui handle or a sprite holding it keeps showing the target.</p>

#### `desc` {#function-desc}

<div class="function-declaration" id="function-desc-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="render-target-desc.html">render_target_desc</a> &amp;desc() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The description as created or last resized.</p>

#### `frames_rendered` {#function-frames-rendered}

<div class="function-declaration" id="function-frames-rendered-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> frames_rendered() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of frames rendered into this target since creation or the last resize.</p>

#### `has_depth` {#function-has-depth}

<div class="function-declaration" id="function-has-depth-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_depth() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Current size in pixels.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
