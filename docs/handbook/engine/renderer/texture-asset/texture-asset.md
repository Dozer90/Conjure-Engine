---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [texture_asset.h](../texture-asset.md)

# texture_asset
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/texture_asset.h` |
| Source File Path | `engine/src/renderer/assets/texture_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [JPEG XL images](../../../../api/renderer/jpeg-xl.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
class CONJURE_API texture_asset
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">texture_asset();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">texture_asset(
    <span class="k">const</span> <a href="texture-asset.html">texture_asset</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">texture_asset(
    <a href="texture-asset.html">texture_asset</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~texture_asset();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">MAX_<wbr>DIMENSION</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">16384</span></code></td><td>Largest width or height accepted (the <code>D3D12</code> / Vulkan guaranteed limit).</td></tr>
<tr><td>private</td><td><code class="signature">mColourSpace</code></td><td><code class="signature"><a href="../texture-asset.html#symbol-texture-colour-space">texture_colour_space</a></code></td><td><code class="signature">texture_<wbr>colour_<wbr>space::<wbr>SRGB</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFilter</code></td><td><code class="signature"><a href="../texture-asset.html#symbol-sampler-filter">sampler_filter</a></code></td><td><code class="signature">sampler_<wbr>filter::<wbr>LINEAR</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFormat</code></td><td><code class="signature"><a href="../../rhi/texture-format.html#symbol-texture-format">rhi::texture_format</a></code></td><td><code class="signature">rhi::<wbr>texture_<wbr>format::<wbr>RGBA8_<wbr>SRGB</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMips</code></td><td><code class="signature">std::vector&lt;<a href="texture-mip.html">texture_mip</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWrap</code></td><td><code class="signature"><a href="../texture-asset.html#symbol-texture-wrap">texture_wrap</a></code></td><td><code class="signature">texture_<wbr>wrap::<wbr>REPEAT</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `checker` {#function-checker}

<div class="function-declaration" id="function-checker-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="texture-asset.html">texture_asset</a> checker(
    <a href="../../core/image/rgba8.html">core::rgba8</a> first,
    <a href="../../core/image/rgba8.html">core::rgba8</a> second,
    <a href="../../core/types.html#symbol-u32">u32</a> size = <span class="mi">16</span>,
    <a href="../../core/types.html#symbol-u32">u32</a> cellSize = <span class="mi">4</span>,
    <a href="../texture-asset.html#symbol-texture-colour-space">texture_colour_space</a> colourSpace = texture_colour_space::SRGB);</code></pre></div>

<p><code>A</code> checkerboard of two colours with <code>cellSize</code> pixel cells.</p>

#### `colour_space` {#function-colour-space}

<div class="function-declaration" id="function-colour-space-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../texture-asset.html#symbol-texture-colour-space">texture_colour_space</a> colour_space() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `decode` {#function-decode}

<div class="function-declaration" id="function-decode-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="texture-asset.html">texture_asset</a>&gt; decode(
    std::span&lt;<span class="k">const</span> std::byte&gt; encoded,
    <a href="../texture-asset.html#symbol-texture-colour-space">texture_colour_space</a> colourSpace,
    <span class="k">bool</span> generateMips = true,
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *jobs = nullptr);</code></pre></div>

<p>Decodes an encoded image file (JPEG XL, <code>PNG</code>, JPEG, TGA, BMP), recognised by its signature. <code>A</code> JPEG XL file is colour-managed: <code>SRGB</code> converts its colour profile to sRGB, <code>LINEAR</code> keeps its samples as stored (data such as normals must not change). For <code>SRGB</code> a PQ or HLG file is taken to linear light with 1.0 at 203 nits (BT.2408&#x27;s reference white), clipped at 1 and sRGB-encoded, so reference white is white and brighter highlights clip. Its orientation is applied, premultiplied alpha is divided out in the encoding the file stores, before any conversion, an animation keeps its first frame (with a warning), and colour and alpha deeper than 8 bits keep their high <code><a href="../../core/types.html#symbol-byte">byte</a></code>, as a 16-bit <code>PNG</code> does (8-bit colour beside a deeper alpha is rounded). The other formats ignore colour profiles. <code>jobs</code> (optional) decodes a JPEG XL file on its workers; the result is the same bytes either way, and so is the memory limit below. Fails with <code>renderer_error::DECODE_FAILED</code> for anything the decoder rejects, naming a truncated file apart from a corrupt one, and with <code>INVALID_ARGUMENT</code> for empty data, an image larger than <code>MAX_DIMENSION</code>, or a JPEG XL file whose header asks for more than its limits allow: more than 16 extra channels or a layer larger than <code>MAX_DIMENSION</code> (both refused before any frame is decoded), or more decoding memory than the image&#x27;s size allows.</p>

#### `filter` {#function-filter}

<div class="function-declaration" id="function-filter-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../texture-asset.html#symbol-sampler-filter">sampler_filter</a> filter() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `format` {#function-format}

<div class="function-declaration" id="function-format-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../rhi/texture-format.html#symbol-texture-format">rhi::texture_format</a> format() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `from_image` {#function-from-image}

<div class="function-declaration" id="function-from-image-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="texture-asset.html">texture_asset</a>&gt; from_image(
    <span class="k">const</span> <a href="../../core/image/image.html">core::image</a> &amp;source,
    <a href="../texture-asset.html#symbol-texture-colour-space">texture_colour_space</a> colourSpace,
    <span class="k">bool</span> generateMips = true);</code></pre></div>

<p><code>from_rgba8</code> over a <code><a href="../../core/image/image.html">core::image</a></code>.</p>

#### `from_rgba8` {#function-from-rgba8}

<div class="function-declaration" id="function-from-rgba8-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="texture-asset.html">texture_asset</a>&gt; from_rgba8(
    <a href="../../core/types.html#symbol-u32">u32</a> width,
    <a href="../../core/types.html#symbol-u32">u32</a> height,
    std::span&lt;<span class="k">const</span> std::byte&gt; pixels,
    <a href="../texture-asset.html#symbol-texture-colour-space">texture_colour_space</a> colourSpace,
    <span class="k">bool</span> generateMips = true);</code></pre></div>

<p>Builds a texture from RGBA8 pixels (rows top to bottom, no padding). <code>SRGB</code> selects the <code>RGBA8_SRGB</code> format, <code>LINEAR</code> <code>RGBA8_UNORM</code>; with <code>generateMips</code> the full chain is built with a box filter in linear space. Fails with <code>INVALID_ARGUMENT</code> for a zero size, a size above <code>MAX_DIMENSION</code> or a pixel array of the wrong length.</p>

#### `gpu_byte_size` {#function-gpu-byte-size}

<div class="function-declaration" id="function-gpu-byte-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> gpu_byte_size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Bytes of every mip together.</p>

#### `height` {#function-height}

<div class="function-declaration" id="function-height-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> height() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `load_from_file` {#function-load-from-file}

<div class="function-declaration" id="function-load-from-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="texture-asset.html">texture_asset</a>&gt; load_from_file(
    <span class="k">const</span> std::filesystem::path &amp;path,
    <a href="../texture-asset.html#symbol-texture-colour-space">texture_colour_space</a> colourSpace,
    <span class="k">bool</span> generateMips = true,
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *jobs = nullptr);</code></pre></div>

<p>Reads and decodes a file. Fails with <code>NOT_FOUND</code> when the file does not exist, <code>IO_FAILURE</code> when it cannot be read, then as <code><a href="#function-decode-1">decode</a>()</code>.</p>

#### `mip_count` {#function-mip-count}

<div class="function-declaration" id="function-mip-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> mip_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `mips` {#function-mips}

<div class="function-declaration" id="function-mips-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="texture-mip.html">texture_mip</a>&gt; mips() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view name() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `parse_blob` {#function-parse-blob}

<div class="function-declaration" id="function-parse-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="texture-asset.html">texture_asset</a>&gt; parse_blob(
    std::span&lt;<span class="k">const</span> std::byte&gt; bytes);</code></pre></div>

<p>Parses a .ctex blob. Fails with <code>renderer_error::INVALID_ASSET</code> naming the problem.</p>

#### `pixel` {#function-pixel}

<div class="function-declaration" id="function-pixel-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/image/rgba8.html">core::rgba8</a> pixel(
    <a href="../../core/types.html#symbol-u32">u32</a> x,
    <a href="../../core/types.html#symbol-u32">u32</a> y) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The pixel of mip 0 at (x, y) as RGBA8 (the 8-bit formats only; other formats read opaque black). Out of range asserts and reads black.</p>

#### `set_filter` {#function-set-filter}

<div class="function-declaration" id="function-set-filter-1"><pre><code><span class="k">void</span> set_filter(
    <a href="../texture-asset.html#symbol-sampler-filter">sampler_filter</a> filter) <span class="k">noexcept</span>;</code></pre></div>

#### `set_name` {#function-set-name}

<div class="function-declaration" id="function-set-name-1"><pre><code><span class="k">void</span> set_name(
    std::string name);</code></pre></div>

#### `set_wrap` {#function-set-wrap}

<div class="function-declaration" id="function-set-wrap-1"><pre><code><span class="k">void</span> set_wrap(
    <a href="../texture-asset.html#symbol-texture-wrap">texture_wrap</a> wrap) <span class="k">noexcept</span>;</code></pre></div>

#### `solid_colour` {#function-solid-colour}

<div class="function-declaration" id="function-solid-colour-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="texture-asset.html">texture_asset</a> solid_colour(
    <a href="../../core/image/rgba8.html">core::rgba8</a> colour,
    <a href="../texture-asset.html#symbol-texture-colour-space">texture_colour_space</a> colourSpace,
    <a href="../../core/types.html#symbol-u32">u32</a> width = <span class="mi">1</span>,
    <a href="../../core/types.html#symbol-u32">u32</a> height = <span class="mi">1</span>);</code></pre></div>

<p><code>A</code> 1x1 (or width x height) texture of one colour.</p>

#### `width` {#function-width}

<div class="function-declaration" id="function-width-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> width() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `wrap` {#function-wrap}

<div class="function-declaration" id="function-wrap-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../texture-asset.html#symbol-texture-wrap">texture_wrap</a> wrap() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `write_blob` {#function-write-blob}

<div class="function-declaration" id="function-write-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;std::byte&gt; write_blob() <span class="k">const</span>;</code></pre></div>

<p>Serialises the texture as a .ctex blob (little-endian, deterministic).</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="texture-asset.html">texture_asset</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="texture-asset.html">texture_asset</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="texture-asset.html">texture_asset</a> &amp;<span class="k">operator</span>=(
    <a href="texture-asset.html">texture_asset</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="texture-asset.html">texture_asset</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
