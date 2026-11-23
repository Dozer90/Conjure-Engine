---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [components.h](../components.md)

# sprite_component
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/components.h` |
| Source File Path | `engine/src/renderer/components.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct sprite_component
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">colour</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a>(<span class="mi">1.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">layer</code></td><td><code class="signature"><a href="../../core/types.html#symbol-i16">i16</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">order</code></td><td><code class="signature"><a href="../../core/types.html#symbol-i16">i16</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">size</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a>(<span class="mi">1.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">space</code></td><td><code class="signature"><a href="../sprite-batch.html#symbol-sprite-space">sprite_space</a></code></td><td><code class="signature">sprite_<wbr>space::<wbr>WORLD</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">texture</code></td><td><code class="signature"><a href="../../assets/asset-ref/asset-ref.html">assets::<wbr>asset_<wbr>ref</a>&lt;<wbr><a href="../texture-asset/texture-asset.html">texture_<wbr>asset</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">uvRect</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec4">math::<wbr>vec4</a>(<span class="mi">0.0</span>f,<wbr> <span class="mi">0.0</span>f,<wbr> <span class="mi">1.0</span>f,<wbr> <span class="mi">1.0</span>f)</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="sprite-component.html">sprite_component</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
