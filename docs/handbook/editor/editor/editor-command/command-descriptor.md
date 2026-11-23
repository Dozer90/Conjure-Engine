---
kind: struct
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [editor_command.h](../editor-command.md)

# command_descriptor
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/editor_command.h` |
| Source File Path | `engine/src/editor/core/editor_command.cpp` |
| API ↗ | [Access tokens and the audit log](../../../../api/editor/access-and-audit.md), [The AI control protocol](../../../../api/editor/ai-protocol.md), [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct command_descriptor
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">argsSchema</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">"{}"</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../command-desc.html#symbol-command-flags">command_flags</a></code></td><td><code class="signature">command_<wbr>flags::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">handler</code></td><td><code class="signature"><a href="../editor-command.html#symbol-command-handler">command_handler</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">owner</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">"editor"</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">resultSchema</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">"{}"</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">summary</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
