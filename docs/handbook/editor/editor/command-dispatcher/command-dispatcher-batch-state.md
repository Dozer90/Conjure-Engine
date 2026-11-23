---
kind: struct
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [command_dispatcher.h](../command-dispatcher.md)

# command_dispatcher::batch_state
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/command_dispatcher.h` |
| Source File Path | `engine/src/editor/core/command_dispatcher.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct batch_state
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">depth</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">entry</code></td><td><code class="signature"><a href="../undo-stack/undo-entry.html">undo_entry</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">fileCommands</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
