---
kind: struct
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [game_screen.h](../game-screen.md)

# game_screen
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/game_screen.h` |
| Source File Path | N/A |
| API ↗ | [Engine: the application and the frame](../../../../api/engine/index.md) |

## Syntax

```cpp
struct game_screen
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">contentScale</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec2">math::vec2</a>(<span class="mi">1.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">focused</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">fullscreen</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">minimised</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">pixels</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a>(<span class="mi">0</span>)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">uiScale</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="game-screen.html">game_screen</a> &amp;) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
