---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [native_api.h](../native-api.md)

# conjure_spawn_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/native_api.h` |
| Source File Path | `engine/src/scripting/native_api.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
struct conjure_spawn_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">component_<wbr>count</code></td><td><code class="signature">int32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">components</code></td><td><code class="signature"><span class="k">const</span> <a href="conjure-spawn-component.html">conjure_<wbr>spawn_<wbr>component</a> *</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">count</code></td><td><code class="signature">int32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature">uint32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">pad</code></td><td><code class="signature">uint32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">parent</code></td><td><code class="signature">uint64_t</code></td><td>—</td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
