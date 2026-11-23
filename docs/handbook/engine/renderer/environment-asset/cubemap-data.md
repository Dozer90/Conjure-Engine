---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [environment_asset.h](../environment-asset.md)

# cubemap_data
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/environment_asset.h` |
| Source File Path | `engine/src/renderer/assets/environment_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
struct CONJURE_API cubemap_data
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">mipCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">size</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">texels</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u16">u16</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `face_length` {#function-face-length}

<div class="function-declaration" id="function-face-length-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> face_length(
    <a href="../../core/types.html#symbol-u32">u32</a> mip) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of halves one face of a mip occupies.</p>

#### `face_offset` {#function-face-offset}

<div class="function-declaration" id="function-face-offset-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> face_offset(
    <a href="../../core/types.html#symbol-u32">u32</a> mip,
    <a href="../../core/types.html#symbol-u32">u32</a> face) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Index of the first half of a face of a mip inside <code>texels</code>.</p>

#### `is_consistent` {#function-is-consistent}

<div class="function-declaration" id="function-is-consistent-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_consistent() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>texels</code> has exactly the length the size and mip count imply.</p>

#### `mip_size` {#function-mip-size}

<div class="function-declaration" id="function-mip-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> mip_size(
    <a href="../../core/types.html#symbol-u32">u32</a> mip) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Edge of a mip (at least 1).</p>

#### `texel` {#function-texel}

<div class="function-declaration" id="function-texel-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> texel(
    <a href="../../core/types.html#symbol-u32">u32</a> mip,
    <a href="../../core/types.html#symbol-u32">u32</a> face,
    <a href="../../core/types.html#symbol-u32">u32</a> x,
    <a href="../../core/types.html#symbol-u32">u32</a> y) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The RGB radiance of a texel.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="cubemap-data.html">cubemap_data</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
