---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [material_asset.h](../material-asset.md)

# material_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/material_asset.h` |
| Source File Path | `engine/src/renderer/assets/material_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
struct CONJURE_API material_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">alphaCutoff</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.5</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">alphaMode</code></td><td><code class="signature"><a href="../material-asset.html#symbol-alpha-mode">alpha_mode</a></code></td><td><code class="signature">alpha_<wbr>mode::<wbr>SOLID</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">base<wbr>Colour<wbr>Factor</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec4">math::vec4</a>(<span class="mi">1.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">doubleSided</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">emissive<wbr>Factor</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">emissive<wbr>Strength</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">metallic<wbr>Factor</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">normalScale</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">occlusion<wbr>Strength</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">roughness<wbr>Factor</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">textures</code></td><td><code class="signature">std::<wbr>array&lt;<wbr><a href="../../assets/asset-ref/asset-ref.html">assets::<wbr>asset_<wbr>ref</a>&lt;<wbr><a href="../texture-asset/texture-asset.html">texture_<wbr>asset</a>&gt;,<wbr> MATERIAL_<wbr>TEXTURE_<wbr>SLOTS&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">texture<wbr>Uv<wbr>Sets</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>u8,<wbr> MATERIAL_<wbr>TEXTURE_<wbr>SLOTS&gt;</code></td><td><code class="signature">{<span class="mi">0</span>, <span class="mi">0</span>, <span class="mi">0</span>, <span class="mi">0</span>, <span class="mi">0</span>}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">unlit</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="material-desc.html">material_desc</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
