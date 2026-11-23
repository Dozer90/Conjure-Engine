---
kind: struct
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_resolver.h](../module-resolver.md)

# resolution_result
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_resolver.h` |
| Source File Path | `engine/src/modules/module_resolver.cpp` |
| API ↗ | [Loading and managing modules](../../../../api/modules/loading-and-managing.md) |

## Syntax

```cpp
struct resolution_result
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">failures</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="resolution-failure.html">resolution_<wbr>failure</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">order</code></td><td><code class="signature">std::vector&lt;<a href="../module-types/module-id.html">module_id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
