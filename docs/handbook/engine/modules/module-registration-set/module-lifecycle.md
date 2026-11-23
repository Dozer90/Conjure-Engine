---
kind: struct
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_registration_set.h](../module-registration-set.md)

# module_lifecycle
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_registration_set.h` |
| Source File Path | N/A |
| API ↗ | [Writing a module](../../../../api/modules/writing-a-module.md) |

## Syntax

```cpp
struct module_lifecycle
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">onActivated</code></td><td><code class="signature"><span class="k">void</span> (*)(<a href="../module-context/module-context.html">module_<wbr>context</a> &amp;context)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">on<wbr>Deactivating</code></td><td><code class="signature"><span class="k">void</span> (*)(<a href="../module-context/module-context.html">module_<wbr>context</a> &amp;context)</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
