---
kind: struct
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [environment_asset.h](../environment-asset.md)

# equirect_image
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/environment_asset.h` |
| Source File Path | `engine/src/renderer/assets/environment_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
struct CONJURE_API equirect_image
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">height</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">rgb</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-f32">f32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">width</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `constant` {#function-constant}

<div class="function-declaration" id="function-constant-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="equirect-image.html">equirect_image</a> constant(
    <a href="../../core/types.html#symbol-u32">u32</a> width,
    <a href="../../core/types.html#symbol-u32">u32</a> height,
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;colour);</code></pre></div>

<p>An image of one colour.</p>

#### `decode_hdr` {#function-decode-hdr}

<div class="function-declaration" id="function-decode-hdr-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="equirect-image.html">equirect_image</a>&gt; decode_hdr(
    std::span&lt;<span class="k">const</span> std::byte&gt; encoded,
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *jobs = nullptr);</code></pre></div>

<p>Decodes a Radiance .hdr file (<code>stb_image</code>) or a JPEG XL file (libjxl), recognised by its signature. <code>A</code> JPEG XL file of any sample type, float included, is converted from its colour encoding (Display, an ICC profile, ...) to linear light with sRGB primaries, keeping values above 1, its premultiplied alpha divided out, and its first frame is used; <code>jobs</code> (optional) decodes it on the workers. PQ and HLG files are display light in nits and come out with 1.0 at 203 nits, the reference white of ITU-<code>R</code> BT.2408, where a .hdr or float file keeps its 1.0 (HLG through its system gamma for a display of the file&#x27;s intensity target, 1000 nits unless stated). Every sample is then made one the bake can sum: NaN and negative values become 0 and anything above <code>MAX_ENVIRONMENT_RADIANCE</code>, +inf included, becomes that value; finite HDR radiance below it is kept as decoded, however bright. Fails with <code>renderer_error::DECODE_FAILED</code> when the decoder rejects the data and <code>INVALID_ARGUMENT</code> for empty data, a size above <code>texture_asset::MAX_DIMENSION</code> or a JPEG XL file over one of its decoding limits (<code>texture_asset::decode</code>).</p>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `sample` {#function-sample}

<div class="function-declaration" id="function-sample-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> sample(
    <span class="k">const</span> <a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;direction) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Bilinear lookup in a direction (y up; the image seam is at -<code>Z</code>). Each texel is read as <code>decode_hdr</code> leaves it (NaN and negative values as 0, anything above <code>MAX_ENVIRONMENT_RADIANCE</code> as that value), so an image filled by hand bakes to finite values too.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
