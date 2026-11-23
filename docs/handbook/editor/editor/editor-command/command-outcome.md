---
kind: struct
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [editor_command.h](../editor-command.md)

# command_outcome
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/editor_command.h` |
| Source File Path | `engine/src/editor/core/editor_command.cpp` |
| API ↗ | [Access tokens and the audit log](../../../../api/editor/access-and-audit.md), [The AI control protocol](../../../../api/editor/ai-protocol.md), [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct command_outcome
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">inverse</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="command-inverse.html">command_<wbr>inverse</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">label</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">replay</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="command-request.html">command_<wbr>request</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">result</code></td><td><code class="signature"><a href="../../../engine/core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">core::<wbr>json::<wbr>document::<wbr>object()</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">skip<wbr>Recording</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
