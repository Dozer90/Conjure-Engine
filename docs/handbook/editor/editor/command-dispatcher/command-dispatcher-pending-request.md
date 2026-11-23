---
kind: struct
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [command_dispatcher.h](../command-dispatcher.md)

# command_dispatcher::pending_request
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/command_dispatcher.h` |
| Source File Path | `engine/src/editor/core/command_dispatcher.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct pending_request
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">clientId</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">command</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">deadline<wbr>Seconds</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../../../engine/core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">ticket</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
