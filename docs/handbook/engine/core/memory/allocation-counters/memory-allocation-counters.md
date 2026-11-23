---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [allocation_counters.h](../allocation-counters.md)

# memory::allocation_counters
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::memory` |
| Include File Path | `engine/include/conjure/core/memory/allocation_counters.h` |
| Source File Path | `engine/src/core/memory/allocation_counters.cpp` |
| API ↗ | [Memory](../../../../../api/core/memory.md) |

## Syntax

```cpp
struct allocation_counters
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">allocated<wbr>Bytes</code></td><td><code class="signature">std::atomic&lt;<a href="../../types.html#symbol-u64">u64</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">allocation<wbr>Count</code></td><td><code class="signature">std::atomic&lt;<a href="../../types.html#symbol-u64">u64</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">freeCount</code></td><td><code class="signature">std::atomic&lt;<a href="../../types.html#symbol-u64">u64</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
