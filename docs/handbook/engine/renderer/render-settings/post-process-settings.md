---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [render_settings.h](../render-settings.md)

# post_process_settings
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/render_settings.h` |
| Source File Path | `engine/src/renderer/render_settings.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct post_process_settings
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">adaptation<wbr>Speed<wbr>Down</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">adaptation<wbr>Speed<wbr>Up</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">3.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">agxLook</code></td><td><code class="signature"><a href="../render-settings.html#symbol-agx-look">agx_look</a></code></td><td><code class="signature">agx_<wbr>look::<wbr>DEFAULT</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">bloomEnabled</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">bloom<wbr>Intensity</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.04</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">exposure</code></td><td><code class="signature"><a href="../render-settings.html#symbol-exposure-mode">exposure_mode</a></code></td><td><code class="signature">exposure_<wbr>mode::<wbr>AUTOMATIC</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">exposure<wbr>Compensation</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">manualEv100</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">12.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">maxEv100</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">16.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">minEv100</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature">-<span class="mi">10.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">reinhard<wbr>White<wbr>Point</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">4.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">tonemap</code></td><td><code class="signature"><a href="../render-settings.html#symbol-tonemap-operator">tonemap_operator</a></code></td><td><code class="signature">tonemap_<wbr>operator::<wbr>ACES_<wbr>FITTED</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="post-process-settings.html">post_process_settings</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
