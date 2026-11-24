---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [render_scene.h](../render-scene.md)

# render_view
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/render_scene.h` |
| Source File Path | `engine/src/renderer/render_scene.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct render_view
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">cameraData</code></td><td><code class="signature"><a href="../camera/camera.html">camera</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">camera<wbr>Position</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">debugMode</code></td><td><code class="signature"><a href="../render-scene.html#symbol-view-debug-mode">view_debug_mode</a></code></td><td><code class="signature">view_<wbr>debug_<wbr>mode::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../render-scene.html#symbol-view-flags">view_flags</a></code></td><td><code class="signature">view_flags::NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">layerMask</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span>xFFFFFFFFu</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">postProcess</code></td><td><code class="signature"><a href="../render-settings/post-process-settings.html">post_process_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">settings<wbr>Override</code></td><td><code class="signature"><a href="../render-settings.html#symbol-view-settings-override-flags">view_<wbr>settings_<wbr>override_<wbr>flags</a></code></td><td><code class="signature">view_<wbr>settings_<wbr>override_<wbr>flags::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">ssao</code></td><td><code class="signature"><a href="../render-settings/ssao-settings.html">ssao_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">target</code></td><td><code class="signature"><a href="../gpu-ids.html#symbol-render-target-id">render_target_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">view<wbr>From<wbr>World</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-mat4">math::mat4</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-mat4">math::mat4</a>(<span class="mi">1.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">viewport</code></td><td><code class="signature"><a href="../../core/math/rect/urect.html">math::urect</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
