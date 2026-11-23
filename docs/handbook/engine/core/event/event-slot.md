---
kind: struct
---

[Engine](../../../engine.md) → [Core](../../core.md) → [event.h](../event.md)

# event::slot
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/event.h` |
| Source File Path | `engine/src/core/time/event.cpp` |
| API ↗ | [Time, events and random](../../../../api/core/time-and-events.md) |

## Syntax

```cpp
struct slot
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">alive</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">handler</code></td><td><code class="signature"><a href="../event.html#symbol-event-handler-type">handler_type</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
