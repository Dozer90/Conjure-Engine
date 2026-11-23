---
kind: struct
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_events.h](../module-events.md)

# module_unloaded_event
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_events.h` |
| Source File Path | N/A |
| API ↗ | [Writing a module](../../../../api/modules/writing-a-module.md) |

## Syntax

```cpp
struct module_unloaded_event
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../module-types/module-id.html">module_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature"><a href="../../core/string-id/string-id.html">core::string_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
