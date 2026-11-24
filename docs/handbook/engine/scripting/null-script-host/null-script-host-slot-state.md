---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [null_script_host.h](../null-script-host.md)

# null_script_host::slot_state
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/null_script_host.h` |
| Source File Path | `engine/src/scripting/null_script_host.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
struct slot_state
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">active</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">classHash</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">collect<wbr>Override</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr>std::<wbr>vector&lt;<wbr><a href="../../core/types.html#symbol-u8">u8</a>&gt;&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">entity</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">fields</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u8">u8</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">live</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">phaseCounts</code></td><td><code class="signature">std::array&lt;u32, <span class="mi">3</span>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">started</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
