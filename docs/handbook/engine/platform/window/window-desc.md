---
kind: struct
---

[Engine](../../../engine.md) → [Platform](../../platform.md) → [window.h](../window.md)

# window_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::platform` |
| Include File Path | `engine/include/conjure/platform/window.h` |
| Source File Path | N/A |
| API ↗ | [Input](../../../../api/platform/input.md), [Windows and events](../../../../api/platform/windows-and-events.md) |

## Syntax

```cpp
struct window_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">backend</code></td><td><code class="signature"><a href="../backend.html#symbol-backend-kind">backend_kind</a></code></td><td><code class="signature">backend_<wbr>kind::<wbr>GLFW</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">display<wbr>Server</code></td><td><code class="signature"><a href="../window.html#symbol-display-server-hint">display_server_hint</a></code></td><td><code class="signature">display_<wbr>server_<wbr>hint::<wbr>ANY</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../window.html#symbol-window-flags">window_flags</a></code></td><td><code class="signature">window_<wbr>flags::<wbr>DEFAULT</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">fullscreen</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">gamepad<wbr>Mappings</code></td><td><code class="signature">std::string_view</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">height</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">900</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">monitorIndex</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">null<wbr>Content<wbr>Scale</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a>(<span class="mi">1.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">position</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="../../core/math/glm-types.html#symbol-ivec2">math::<wbr>ivec2</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">title</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">"Conjure"</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">width</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1600</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
