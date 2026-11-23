---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [field_info.h](../field-info.md)

# reflect::detail::std_array_traits<std::array<ElementType, N>>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::reflect` |
| Include File Path | `engine/include/conjure/core/reflect/field_info.h` |
| Source File Path | `engine/src/core/reflect/field_info.cpp` |
| API ↗ | [Reflection](../../../../../api/core/reflection.md) |

## Syntax

```cpp
template <typename ElementType, usize N> struct std_array_traits<std::array<ElementType, N>>
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">COUNT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature">N</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">IS_ARRAY</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
