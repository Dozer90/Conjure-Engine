---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [native_api.h](../native-api.md)

# conjure_frame_block
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/native_api.h` |
| Source File Path | `engine/src/scripting/native_api.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
struct alignas(64) conjure_frame_block
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">content_<wbr>scale_<wbr>x</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">content_<wbr>scale_<wbr>y</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">current_<wbr>phase</code></td><td><code class="signature">uint8_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">delta_time</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">fixed_<wbr>delta_<wbr>time</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">fixed_<wbr>step_<wbr>index</code></td><td><code class="signature">uint32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">frame_index</code></td><td><code class="signature">uint64_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">in_play_mode</code></td><td><code class="signature">uint8_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">input</code></td><td><code class="signature"><a href="conjure-input-block.html">conjure_input_block</a></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">padding</code></td><td><code class="signature">uint8_t[<span class="na">51</span>]</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">screen_<wbr>height</code></td><td><code class="signature">uint32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">screen_width</code></td><td><code class="signature">uint32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">time_scale</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">time_seconds</code></td><td><code class="signature"><span class="k">double</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">ui_scale</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">unscaled_<wbr>time_<wbr>seconds</code></td><td><code class="signature"><span class="k">double</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">window_state</code></td><td><code class="signature">uint8_t</code></td><td>—</td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
