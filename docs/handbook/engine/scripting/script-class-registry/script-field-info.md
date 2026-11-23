---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_class_registry.h](../script-class-registry.md)

# script_field_info
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_class_registry.h` |
| Source File Path | `engine/src/scripting/script_class_registry.cpp` |
| API ↗ | [Scripting](../../../../api/scripting/index.md), [Scripts, components and the runtime](../../../../api/scripting/scripts-and-components.md) |

## Syntax

```cpp
struct script_field_info
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">defaultValue</code></td><td><code class="signature"><a href="../../core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">elementKind</code></td><td><code class="signature"><a href="../native-api.html#symbol-script-field-kind">script_field_kind</a></code></td><td><code class="signature">script_<wbr>field_<wbr>kind::<wbr>INT32</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">hash</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">kind</code></td><td><code class="signature"><a href="../native-api.html#symbol-script-field-kind">script_field_kind</a></code></td><td><code class="signature">script_<wbr>field_<wbr>kind::<wbr>INT32</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">meta</code></td><td><code class="signature"><a href="../../core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
