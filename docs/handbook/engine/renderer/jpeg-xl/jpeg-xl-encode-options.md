---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [jpeg_xl.h](../jpeg-xl.md)

# jpeg_xl_encode_options
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/jpeg_xl.h` |
| Source File Path | N/A |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [JPEG XL images](../../../../api/renderer/jpeg-xl.md) |

## Syntax

```cpp
struct jpeg_xl_encode_options
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">colourSpace</code></td><td><code class="signature"><a href="../texture-asset.html#symbol-texture-colour-space">texture_colour_space</a></code></td><td><code class="signature">texture_<wbr>colour_<wbr>space::<wbr>SRGB</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">distance</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">effort</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">7</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">lossless</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="jpeg-xl-encode-options.html">jpeg_xl_encode_options</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
