---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [mesh_asset.h](../mesh-asset.md)

# packed_vertex_attributes
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/mesh_asset.h` |
| Source File Path | `engine/src/renderer/assets/mesh_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
struct packed_vertex_attributes
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">colour</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span>xFFFFFFFFu</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">normal</code></td><td><code class="signature">std::array&lt;i16, <span class="mi">4</span>&gt;</code></td><td><code class="signature">{<span class="mi">0</span>, <span class="mi">0</span>, <span class="mi">0</span>, <span class="mi">0</span>}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">tangent</code></td><td><code class="signature">std::array&lt;i16, <span class="mi">4</span>&gt;</code></td><td><code class="signature">{<span class="mi">0</span>, <span class="mi">0</span>, <span class="mi">0</span>, <span class="mi">0</span>}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">uv0</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">uv1</code></td><td><code class="signature">std::array&lt;u16, <span class="mi">2</span>&gt;</code></td><td><code class="signature">{<span class="mi">0</span>, <span class="mi">0</span>}</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="packed-vertex-attributes.html">packed_vertex_attributes</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
