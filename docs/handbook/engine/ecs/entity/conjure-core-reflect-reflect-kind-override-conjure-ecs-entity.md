---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [entity.h](../entity.md)

# conjure::core::reflect::reflect_kind_override<conjure::ecs::entity>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/entity.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
template <> struct conjure::core::reflect::reflect_kind_override<conjure::ecs::entity>
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">KIND</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/reflect/field-info.html#symbol-reflect-field-kind">conjure::<wbr>core::<wbr>reflect::<wbr>field_<wbr>kind</a></code></td><td><code class="signature">conjure::<wbr>core::<wbr>reflect::<wbr>field_<wbr>kind::<wbr>ENTITY</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">NAME</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code></td><td><code class="signature"><span class="s">"conjure.ecs.entity"</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
