---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [render_settings.h](../render-settings.md)

# shadow_settings
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/render_settings.h` |
| Source File Path | `engine/src/renderer/render_settings.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct shadow_settings
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">cascadeCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature">MAX_<wbr>SHADOW_<wbr>CASCADES</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">caster<wbr>Padding</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">50.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">default<wbr>Filter</code></td><td><code class="signature"><a href="../light.html#symbol-shadow-filter">shadow_filter</a></code></td><td><code class="signature">shadow_<wbr>filter::<wbr>PCF</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">distance</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">100.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">enabled</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">normal<wbr>Offset<wbr>Texels</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.5</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">pcf<wbr>Radius<wbr>Texels</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.5</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">splitLambda</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.7</span>f</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="shadow-settings.html">shadow_settings</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
