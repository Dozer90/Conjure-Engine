---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [view.h](../view.md)

# detail::view_arg_traits
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/view.h` |
| Source File Path | N/A |
| API ↗ | [Queries, views and systems](../../../../api/ecs/queries-views-and-systems.md) |

## Syntax

```cpp
template <typename PackType> struct view_arg_traits
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">IS_CONST</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <span class="k">bool</span></code></td><td><code class="signature">std::<wbr>is_<wbr>const_<wbr>v&lt;<wbr>Pack<wbr>Type&gt;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">IS_OPTIONAL</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
