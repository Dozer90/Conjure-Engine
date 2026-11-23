---
kind: struct
---

[Engine](../../../engine.md) → [Platform](../../platform.md) → [monitor.h](../monitor.md)

# monitor_info
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::platform` |
| Include File Path | `engine/include/conjure/platform/monitor.h` |
| Source File Path | N/A |
| API ↗ | [Windows and events](../../../../api/platform/windows-and-events.md) |

## Syntax

```cpp
struct monitor_info
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">contentScale</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a>(<span class="mi">1.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">currentMode</code></td><td><code class="signature"><a href="video-mode.html">video_mode</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">isPrimary</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">position</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-ivec2">math::ivec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-ivec2">math::ivec2</a>(<span class="mi">0</span>)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">workArea</code></td><td><code class="signature"><a href="../../core/math/rect/urect.html">math::urect</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="monitor-info.html">monitor_info</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
