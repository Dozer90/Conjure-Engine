---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [command_buffer.h](../command-buffer.md)

# command_buffer::instantiate_entry
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/command_buffer.h` |
| Source File Path | `engine/src/ecs/command_buffer.cpp` |
| API ↗ | [Command buffers and observers](../../../../api/ecs/command-buffers-and-observers.md) |

## Syntax

```cpp
struct instantiate_entry
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">archive</code></td><td><code class="signature">std::<wbr>shared_<wbr>ptr&lt;<wbr><span class="k">const</span> <a href="../entity-archive/entity-archive.html">entity_<wbr>archive</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">copies</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">ids</code></td><td><code class="signature">std::vector&lt;<a href="../entity/entity.html">entity</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
