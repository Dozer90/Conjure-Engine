---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [native_api.h](../native-api.md)

# conjure_managed_memory
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/native_api.h` |
| Source File Path | `engine/src/scripting/native_api.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
struct conjure_managed_memory
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">gen0_<wbr>collections</code></td><td><code class="signature">uint32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">gen2_<wbr>collections</code></td><td><code class="signature">uint32_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">heap_bytes</code></td><td><code class="signature">uint64_t</code></td><td>—</td><td></td></tr>
<tr><td>public</td><td><code class="signature">thread_<wbr>allocated_<wbr>bytes</code></td><td><code class="signature">uint64_t</code></td><td>—</td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
