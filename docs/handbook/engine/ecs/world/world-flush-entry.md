---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [world.h](../world.md)

# world::flush_entry
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/world.h` |
| Source File Path | `engine/src/ecs/world.cpp` |
| API ↗ | [Archives and serialisation](../../../../api/ecs/archives-and-serialization.md), [Command buffers and observers](../../../../api/ecs/command-buffers-and-observers.md), [Entities and components](../../../../api/ecs/entities-and-components.md), [Entities, components and systems](../../../../api/ecs/index.md), [Queries, views and systems](../../../../api/ecs/queries-views-and-systems.md) |

## Syntax

```cpp
struct flush_entry
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">addMask</code></td><td><code class="signature"><a href="../component/component-mask.html">component_mask</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">alive</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">commands</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">destroy</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../entity/entity.html">entity</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">payloads</code></td><td><code class="signature"><a href="../../core/containers/small-vector/small-vector.html">core::<wbr>small_<wbr>vector</a>&lt;<wbr><a href="../command-buffer/command-buffer-record.html">command_<wbr>buffer::<wbr>record</a> *,<wbr> FLUSH_<wbr>INLINE_<wbr>PAYLOADS&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">removeMask</code></td><td><code class="signature"><a href="../component/component-mask.html">component_mask</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
