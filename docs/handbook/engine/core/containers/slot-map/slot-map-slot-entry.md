---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [slot_map.h](../slot-map.md)

# slot_map::slot_entry
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/containers/slot_map.h` |
| Source File Path | N/A |
| API ↗ | [Containers](../../../../../api/core/containers.md) |

## Syntax

```cpp
struct slot_entry
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">denseIndex</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature">INVALID_INDEX</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">generation</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">nextFree</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature">INVALID_INDEX</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
