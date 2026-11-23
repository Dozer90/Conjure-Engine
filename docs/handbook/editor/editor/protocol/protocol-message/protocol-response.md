---
kind: struct
---

[Editor](../../../../editor.md) → [Protocol](../../../protocol.md) → [protocol_message.h](../protocol-message.md)

# protocol_response
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/protocol/protocol_message.h` |
| Source File Path | `engine/src/editor/core/protocol/protocol_message.cpp` |
| API ↗ | [The AI control protocol](../../../../../api/editor/ai-protocol.md), [The editor](../../../../../api/editor/index.md) |

## Syntax

```cpp
struct protocol_response
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">clientId</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">errorCode</code></td><td><code class="signature"><a href="../../editor-error.html#symbol-protocol-error-code">protocol_error_code</a></code></td><td><code class="signature">protocol_<wbr>error_<wbr>code::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">errorDetails</code></td><td><code class="signature"><a href="../../../../engine/core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">core::<wbr>json::<wbr>document::<wbr>object()</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">errorMessage</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../../../../engine/core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">ok</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">result</code></td><td><code class="signature"><a href="../../../../engine/core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">core::<wbr>json::<wbr>document::<wbr>object()</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
