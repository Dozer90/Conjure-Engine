---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [world.h](../world.md)

# world_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/world.h` |
| Source File Path | `engine/src/ecs/world.cpp` |
| API ↗ | [Archives and serialisation](../../../../api/ecs/archives-and-serialization.md), [Command buffers and observers](../../../../api/ecs/command-buffers-and-observers.md), [Entities and components](../../../../api/ecs/entities-and-components.md), [Entities, components and systems](../../../../api/ecs/index.md), [Queries, views and systems](../../../../api/ecs/queries-views-and-systems.md) |

## Syntax

```cpp
struct world_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">chunks<wbr>Per<wbr>Slab</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">64</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">initial<wbr>Entity<wbr>Capacity</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1024</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">max<wbr>Chunk<wbr>Slabs</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">"world"</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
