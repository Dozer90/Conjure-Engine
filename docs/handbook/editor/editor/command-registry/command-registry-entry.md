---
kind: struct
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [command_registry.h](../command-registry.md)

# command_registry::entry
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/command_registry.h` |
| Source File Path | `engine/src/editor/core/command_registry.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct entry
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">argsSchema</code></td><td><code class="signature"><a href="../schema-validator/compiled-schema.html">compiled_schema</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">descriptor</code></td><td><code class="signature"><a href="../editor-command/command-descriptor.html">command_descriptor</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">resultSchema</code></td><td><code class="signature"><a href="../schema-validator/compiled-schema.html">compiled_schema</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
