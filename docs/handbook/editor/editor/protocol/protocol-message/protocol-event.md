---
kind: struct
---

[Editor](../../../../editor.md) → [Protocol](../../../protocol.md) → [protocol_message.h](../protocol-message.md)

# protocol_event
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/protocol/protocol_message.h` |
| Source File Path | `engine/src/editor/core/protocol/protocol_message.cpp` |
| API ↗ | [The AI control protocol](../../../../../api/editor/ai-protocol.md), [The editor](../../../../../api/editor/index.md) |

## Syntax

```cpp
struct protocol_event
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">data</code></td><td><code class="signature"><a href="../../../../engine/core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">core::<wbr>json::<wbr>document::<wbr>object()</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">frame</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">seq</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
