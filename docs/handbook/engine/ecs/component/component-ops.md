---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [component.h](../component.md)

# component_ops
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/ecs/component.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
struct component_ops
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">construct<wbr>Default</code></td><td><code class="signature"><span class="k">void</span> (*)(<span class="k">void</span> *destination,<wbr> <a href="../../core/types.html#symbol-u32">u32</a> count)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">copy</code></td><td><code class="signature"><span class="k">void</span> (*)(<span class="k">void</span> *destination,<wbr> <span class="k">const</span> <span class="k">void</span> *source,<wbr> <a href="../../core/types.html#symbol-u32">u32</a> count)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">destruct</code></td><td><code class="signature"><span class="k">void</span> (*)(<span class="k">void</span> *object,<wbr> <a href="../../core/types.html#symbol-u32">u32</a> count)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">relocate</code></td><td><code class="signature"><span class="k">void</span> (*)(<span class="k">void</span> *destination,<wbr> <span class="k">void</span> *source,<wbr> <a href="../../core/types.html#symbol-u32">u32</a> count)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
