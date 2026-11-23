---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [component.h](../component.md)

# component_serializer
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::json` |
| Include File Path | `engine/include/conjure/ecs/component.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
struct component_serializer
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">readJson</code></td><td><code class="signature"><a href="../../core/result/result.html">core::<wbr>result</a>&lt;<wbr><span class="k">void</span>&gt; (*)(<span class="k">void</span> *component,<wbr> <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::<wbr>json::<wbr>value</a> &amp;in)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">remap<wbr>Entities</code></td><td><code class="signature"><span class="k">void</span> (*)(<span class="k">void</span> *component,<wbr> <span class="k">const</span> <a href="../entity/entity-remap.html">entity_<wbr>remap</a> &amp;map)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">writeJson</code></td><td><code class="signature"><a href="../../core/result/result.html">core::<wbr>result</a>&lt;<wbr><span class="k">void</span>&gt; (*)(<span class="k">const</span> <span class="k">void</span> *component,<wbr> <a href="../../core/json/writer/json-writer.html">core::<wbr>json::<wbr>writer</a> &amp;out)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
