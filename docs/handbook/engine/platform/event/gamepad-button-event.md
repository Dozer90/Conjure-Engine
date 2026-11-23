---
kind: struct
---

[Engine](../../../engine.md) → [Platform](../../platform.md) → [event.h](../event.md)

# gamepad_button_event
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::platform` |
| Include File Path | `engine/include/conjure/platform/event.h` |
| Source File Path | `engine/src/platform/event.cpp` |
| API ↗ | [Windows and events](../../../../api/platform/windows-and-events.md) |

## Syntax

```cpp
struct gamepad_button_event
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">action</code></td><td><code class="signature"><a href="../button-action.html#symbol-button-action">button_action</a></code></td><td><code class="signature">button_<wbr>action::<wbr>RELEASE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">button</code></td><td><code class="signature"><a href="../gamepad.html#symbol-gamepad-button">gamepad_button</a></code></td><td><code class="signature">gamepad_<wbr>button::<wbr>A</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">gamepadIndex</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u8">u8</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
