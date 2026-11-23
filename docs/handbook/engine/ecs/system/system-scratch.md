---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [system.h](../system.md)

# system_scratch
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/system.h` |
| Source File Path | `engine/src/ecs/system.cpp` |
| API ↗ | [Command buffers and observers](../../../../api/ecs/command-buffers-and-observers.md), [Queries, views and systems](../../../../api/ecs/queries-views-and-systems.md) |

## Syntax

```cpp
struct system_scratch
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">chunks</code></td><td><code class="signature">std::vector&lt;<a href="system-scratch-chunk-ref.html">chunk_ref</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">grainBuffers</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../command-buffer/command-buffer.html">command_<wbr>buffer</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
