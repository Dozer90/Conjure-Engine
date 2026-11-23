---
kind: struct
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [command_desc.h](../command-desc.md)

# command_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/command_desc.h` |
| Source File Path | N/A |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct command_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">argsSchema</code></td><td><code class="signature"><span class="k">const</span> <span class="k">char</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">EXTENSION_<wbr>POINT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../../engine/core/string-id/string-id.html">core::<wbr>string_<wbr>id</a></code></td><td><code class="signature">{<span class="s">"editor.command"</span>}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../command-desc.html#symbol-command-flags">command_flags</a></code></td><td><code class="signature">command_<wbr>flags::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">handler</code></td><td><code class="signature"><a href="../command-desc.html#symbol-command-handler-fn">command_handler_fn</a></code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature"><span class="k">const</span> <span class="k">char</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">resultSchema</code></td><td><code class="signature"><span class="k">const</span> <span class="k">char</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">summary</code></td><td><code class="signature"><span class="k">const</span> <span class="k">char</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">userData</code></td><td><code class="signature"><span class="k">void</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
