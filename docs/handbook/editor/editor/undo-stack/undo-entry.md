---
kind: struct
---

[Editor](../../../editor.md) → [Commands and Undo](../../commands-and-undo.md) → [undo_stack.h](../undo-stack.md)

# undo_entry
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/undo_stack.h` |
| Source File Path | `engine/src/editor/core/undo_stack.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct CONJURE_API undo_entry
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">byteSize</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">clientId</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">coalesceKey</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">label</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">redo</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../editor-command/command-request.html">command_<wbr>request</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">undo</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../editor-command/command-inverse.html">command_<wbr>inverse</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `single_command_name` {#function-single-command-name}

<div class="function-declaration" id="function-single-command-name-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view single_command_name() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The name of the only redo command, or &quot;&quot; when the entry holds several (coalescing needs one).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
