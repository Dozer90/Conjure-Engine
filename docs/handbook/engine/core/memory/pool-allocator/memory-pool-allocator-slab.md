---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [pool_allocator.h](../pool-allocator.md)

# memory::pool_allocator::slab
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::memory` |
| Include File Path | `engine/include/conjure/core/memory/pool_allocator.h` |
| Source File Path | `engine/src/core/memory/pool_allocator.cpp` |
| API ↗ | [Memory](../../../../../api/core/memory.md) |

## Syntax

```cpp
struct slab
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">blocks</code></td><td><code class="signature"><a href="../../types.html#symbol-byte">byte</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">liveCount</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
