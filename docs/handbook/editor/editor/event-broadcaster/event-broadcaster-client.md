---
kind: struct
---

[Editor](../../../editor.md) → [Protocol](../../protocol.md) → [event_broadcaster.h](../event-broadcaster.md)

# event_broadcaster::client
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/event_broadcaster.h` |
| Source File Path | `engine/src/editor/core/event_broadcaster.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct client
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">minLogLevel</code></td><td><code class="signature"><a href="../../../engine/core/log.html#symbol-log-level">core::log_level</a></code></td><td><code class="signature">core::<wbr>log_<wbr>level::<wbr>INFO</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">sink</code></td><td><code class="signature"><a href="../event-broadcaster.html#symbol-event-sink">event_sink</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">subscriptions</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature">default_<wbr>subscriptions()</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
