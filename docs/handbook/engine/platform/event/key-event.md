---
kind: struct
---

[Engine](../../../engine.md) → [Platform](../../platform.md) → [event.h](../event.md)

# key_event
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::platform` |
| Include File Path | `engine/include/conjure/platform/event.h` |
| Source File Path | `engine/src/platform/event.cpp` |
| API ↗ | [Windows and events](../../../../api/platform/windows-and-events.md) |

## Syntax

```cpp
struct key_event
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">action</code></td><td><code class="signature"><a href="../button-action.html#symbol-button-action">button_action</a></code></td><td><code class="signature">button_<wbr>action::<wbr>RELEASE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">keyCode</code></td><td><code class="signature"><a href="../key.html#symbol-key">key</a></code></td><td><code class="signature">key::UNKNOWN</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">modifiers</code></td><td><code class="signature"><a href="../key.html#symbol-key-modifier-flags">key_modifier_flags</a></code></td><td><code class="signature">key_<wbr>modifier_<wbr>flags::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scancode</code></td><td><code class="signature"><a href="../../core/types.html#symbol-i32">i32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
