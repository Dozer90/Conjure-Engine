---
kind: struct
---

[Editor](../../../../editor.md) → [Protocol](../../../protocol.md) → [protocol_server.h](../protocol-server.md)

# client_state
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/protocol/protocol_server.h` |
| Source File Path | `engine/src/editor/core/protocol/protocol_server.cpp` |
| API ↗ | [Access tokens and the audit log](../../../../../api/editor/access-and-audit.md), [The AI control protocol](../../../../../api/editor/ai-protocol.md) |

## Syntax

```cpp
struct client_state
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">access</code></td><td><code class="signature"><a href="../access-token.html#symbol-access-level">access_level</a></code></td><td><code class="signature">access_<wbr>level::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">closing</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">connected</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">dropped<wbr>Log<wbr>Events</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">kind</code></td><td><code class="signature"><a href="../protocol-transport.html#symbol-transport-kind">transport_kind</a></code></td><td><code class="signature">transport_<wbr>kind::<wbr>LOCAL</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">spoke</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">transport<wbr>Index</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `authenticated` {#function-authenticated}

<div class="function-declaration" id="function-authenticated-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> authenticated() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True once the client may send commands and receives events: at once on a transport that requires no token, after a <code>session.hello</code> with one of the two tokens otherwise.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
