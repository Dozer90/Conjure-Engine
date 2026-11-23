---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [render_scene.h](../render-scene.md)

# render_scene
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/render_scene.h` |
| Source File Path | `engine/src/renderer/render_scene.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
struct CONJURE_API render_scene
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">debugDraw</code></td><td><code class="signature"><a href="../debug-draw/debug-draw-buffer.html">debug_draw_buffer</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">deltaSeconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">environment</code></td><td><code class="signature"><a href="../render-settings/environment-settings.html">environment_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">frameIndex</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">lights</code></td><td><code class="signature">std::vector&lt;<a href="../light/light.html">light</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">meshes</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="mesh-instance.html">mesh_<wbr>instance</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">postProcess</code></td><td><code class="signature"><a href="../render-settings/post-process-settings.html">post_process_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">shadows</code></td><td><code class="signature"><a href="../render-settings/shadow-settings.html">shadow_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">sprites</code></td><td><code class="signature">std::vector&lt;<a href="../sprite-batch/sprite.html">sprite</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">ssao</code></td><td><code class="signature"><a href="../render-settings/ssao-settings.html">ssao_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">views</code></td><td><code class="signature">std::vector&lt;<a href="render-view.html">render_view</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `allocate_meshes` {#function-allocate-meshes}

<div class="function-declaration" id="function-allocate-meshes-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<a href="mesh-instance.html">mesh_instance</a>&gt; allocate_meshes(
    <a href="../../core/types.html#symbol-u32">u32</a> count);</code></pre></div>

<p>Resizes the mesh array once and returns the whole span so jobs can fill disjoint ranges. Every element is default-initialised.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Clears every array but keeps capacity so steady-state frames do not allocate. Settings return to their defaults.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
