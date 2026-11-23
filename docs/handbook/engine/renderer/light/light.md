---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [light.h](../light.md)

# light
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/light.h` |
| Source File Path | `engine/src/renderer/light.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct light
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">castsShadows</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">colour</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">1.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">direction</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::<wbr>vec3</a>(<span class="mi">0.0</span>f,<wbr> -<span class="mi">1.0</span>f,<wbr> <span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">entityId</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">filter</code></td><td><code class="signature"><a href="../light.html#symbol-shadow-filter">shadow_filter</a></code></td><td><code class="signature">shadow_<wbr>filter::<wbr>DEFAULT</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">inner<wbr>Cone<wbr>Radians</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.4</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">intensity</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">10.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">outer<wbr>Cone<wbr>Radians</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.6</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">position</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">range</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">10.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">shadow<wbr>Resolution</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u16">u16</a></code></td><td><code class="signature"><span class="mi">512</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">sourceRadius</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.05</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">type</code></td><td><code class="signature"><a href="../light.html#symbol-light-type">light_type</a></code></td><td><code class="signature">light_<wbr>type::<wbr>POINT</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="light.html">light</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
