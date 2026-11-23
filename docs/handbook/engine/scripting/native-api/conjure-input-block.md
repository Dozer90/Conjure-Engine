---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [native_api.h](../native-api.md)

# conjure_input_block
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/native_api.h` |
| Source File Path | `engine/src/scripting/native_api.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
struct conjure_input_block
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">gamepads</code></td><td><code class="signature"><a href="conjure-gamepad-state.html">conjure_gamepad_state</a>[<span class="na">4</span>]</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">keys_down</code></td><td><code class="signature">uint64_t[<span class="na">8</span>]</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">keys_pressed</code></td><td><code class="signature">uint64_t[<span class="na">8</span>]</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">keys_<wbr>released</code></td><td><code class="signature">uint64_t[<span class="na">8</span>]</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">mouse_<wbr>buttons_<wbr>down</code></td><td><code class="signature">uint32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">mouse_<wbr>buttons_<wbr>pressed</code></td><td><code class="signature">uint32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">mouse_<wbr>buttons_<wbr>released</code></td><td><code class="signature">uint32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">mouse_dx</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">mouse_dy</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">mouse_x</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">mouse_y</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">scroll_x</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">scroll_y</code></td><td><code class="signature"><span class="k">float</span></code></td><td>—</td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
