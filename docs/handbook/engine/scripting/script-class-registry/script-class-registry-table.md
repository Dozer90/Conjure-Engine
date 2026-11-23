---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_class_registry.h](../script-class-registry.md)

# script_class_registry::table
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_class_registry.h` |
| Source File Path | `engine/src/scripting/script_class_registry.cpp` |
| API ↗ | [Scripting](../../../../api/scripting/index.md), [Scripts, components and the runtime](../../../../api/scripting/scripts-and-components.md) |

## Syntax

```cpp
struct table
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">classes</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="script-class-info.html">script_<wbr>class_<wbr>info</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">indexByHash</code></td><td><code class="signature">std::<wbr>unordered_<wbr>map&lt;<wbr>u64,<wbr> <a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
