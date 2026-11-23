---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [render_target.h](../render-target.md)

# render_target_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/render_target.h` |
| Source File Path | `engine/src/renderer/render_target.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct render_target_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">colourFormat</code></td><td><code class="signature"><a href="../../rhi/texture-format.html#symbol-texture-format">rhi::texture_format</a></code></td><td><code class="signature">rhi::<wbr>texture_<wbr>format::<wbr>RGBA8_<wbr>SRGB</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">size</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-uvec2">math::<wbr>uvec2</a>(<span class="mi">0</span>,<wbr> <span class="mi">0</span>)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">withDepth</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="render-target-desc.html">render_target_desc</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
