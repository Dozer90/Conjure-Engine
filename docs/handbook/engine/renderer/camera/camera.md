---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [camera.h](../camera.md)

# camera
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/camera.h` |
| Source File Path | `engine/src/renderer/camera.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct camera
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">farPlane</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1000.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">nearPlane</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.1</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">orthographic<wbr>Height</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">10.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">projection</code></td><td><code class="signature"><a href="../camera.html#symbol-projection-type">projection_type</a></code></td><td><code class="signature">projection_<wbr>type::<wbr>PERSPECTIVE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">vertical<wbr>Fov<wbr>Radians</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0472</span>f</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="camera.html">camera</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
