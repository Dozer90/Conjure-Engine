---
kind: struct
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md) → [console_buffer.h](../console-buffer.md)

# console_buffer::entry
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/console_buffer.h` |
| Source File Path | `engine/src/editor/core/console_buffer.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct entry
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">category<wbr>Length</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">frame</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">level</code></td><td><code class="signature"><a href="../../../engine/core/log.html#symbol-log-level">core::log_level</a></code></td><td><code class="signature">core::<wbr>log_<wbr>level::<wbr>INFO</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">seq</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">textLength</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">textOffset</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
