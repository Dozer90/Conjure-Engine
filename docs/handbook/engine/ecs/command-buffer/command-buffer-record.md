---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [command_buffer.h](../command-buffer.md)

# command_buffer::record
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/command_buffer.h` |
| Source File Path | `engine/src/ecs/command_buffer.cpp` |
| API ↗ | [Command buffers and observers](../../../../api/ecs/command-buffers-and-observers.md) |

## Syntax

```cpp
struct record
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../component/component-id.html">component_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">instantiate<wbr>Index</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">next</code></td><td><code class="signature"><a href="command-buffer-record.html">record</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">op</code></td><td><code class="signature"><a href="../command-buffer.html#symbol-command-buffer-opcode">opcode</a></code></td><td><code class="signature">opcode::CREATE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">payload</code></td><td><code class="signature"><span class="k">void</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">payloadLive</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">payloadSize</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">target</code></td><td><code class="signature"><a href="../entity/entity.html">entity</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
