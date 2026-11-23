---
kind: struct
---

[Editor](../../../../editor.md) → [Protocol](../../../protocol.md) → [audit_log.h](../audit-log.md)

# audit_entry
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/protocol/audit_log.h` |
| Source File Path | `engine/src/editor/core/protocol/audit_log.cpp` |
| API ↗ | [Access tokens and the audit log](../../../../../api/editor/access-and-audit.md), [The AI control protocol](../../../../../api/editor/ai-protocol.md) |

## Syntax

```cpp
struct audit_entry
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">access</code></td><td><code class="signature"><a href="../access-token.html#symbol-access-level">access_level</a></code></td><td><code class="signature">access_<wbr>level::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">args</code></td><td><code class="signature"><a href="../../../../engine/core/json/value/json-value.html">core::json::value</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">async</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">batchChild</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">clientId</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">command</code></td><td><code class="signature">std::string_view</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">error</code></td><td><code class="signature"><a href="../../editor-error.html#symbol-protocol-error-code">protocol_error_code</a></code></td><td><code class="signature">protocol_<wbr>error_<wbr>code::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">ok</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">transport</code></td><td><code class="signature"><a href="../protocol-transport.html#symbol-transport-kind">transport_kind</a></code></td><td><code class="signature">transport_<wbr>kind::<wbr>LOCAL</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
