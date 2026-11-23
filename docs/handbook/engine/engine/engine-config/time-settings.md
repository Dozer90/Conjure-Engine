---
kind: struct
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [engine_config.h](../engine-config.md)

# time_settings
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/engine_config.h` |
| Source File Path | `engine/src/engine/engine_config.cpp` |
| API ↗ | [Engine: the application and the frame](../../../../api/engine/index.md), [Running the engine](../../../../api/engine/running-the-engine.md) |

## Syntax

```cpp
struct time_settings
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">fixed<wbr>Delta<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f / <span class="mi">60.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">forced<wbr>Frame<wbr>Delta<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">interpolate<wbr>Physics</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">max<wbr>Fixed<wbr>Steps<wbr>Per<wbr>Frame</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">8</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">max<wbr>Frame<wbr>Delta<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.1</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">timeScale</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="time-settings.html">time_settings</a> &amp;) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
