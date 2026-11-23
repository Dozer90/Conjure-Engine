---
kind: struct
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_manager.h](../module-manager.md)

# module_manager::pending_operation
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_manager.h` |
| Source File Path | `engine/src/modules/module_manager.cpp` |
| API ↗ | [Loading and managing modules](../../../../api/modules/loading-and-managing.md) |

## Syntax

```cpp
struct pending_operation
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../module-types/module-id.html">module_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">kind</code></td><td><code class="signature"><a href="../module-manager.html#symbol-module-manager-pending-kind">pending_kind</a></code></td><td><code class="signature">pending_<wbr>kind::<wbr>LOAD</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">policy</code></td><td><code class="signature"><a href="../module-types.html#symbol-unload-policy">unload_policy</a></code></td><td><code class="signature">unload_<wbr>policy::<wbr>REFUSE_<wbr>IF_<wbr>IN_<wbr>USE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scope</code></td><td><code class="signature"><a href="../module-types.html#symbol-reload-scope">reload_scope</a></code></td><td><code class="signature">reload_<wbr>scope::<wbr>FULL</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
