---
kind: header
---

[Engine](../../engine.md) → [RHI](../rhi.md)

# texture_format.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/rhi/texture_format.h` |
| API ↗ | [Devices and swapchains](../../../api/rhi/devices-and-swapchains.md) |

<p>Texture formats the engine uses. The list is deliberately short: every renderer target, vertex stream and asset payload is one of these, and engine/src/rhi/<code>texture_format_mapping.h</code> maps each one to its NVRHI format (verified at compile time). The block-compressed formats are reserved for a later cook step and never produced yet.</p>

## Enumerations

### `texture_format` {#symbol-texture-format}

```cpp
enum class texture_format : u8 {
    UNKNOWN = 0,
    R8_UNORM,
    RG8_UNORM,
    RGBA8_UNORM,
    RGBA8_SRGB,
    BGRA8_SRGB,
    RG16_SNORM,
    RGBA16_SNORM,
    R16_FLOAT,
    RG16_FLOAT,
    RGBA16_FLOAT,
    R32_FLOAT,
    RG32_FLOAT,
    RGB32_FLOAT,
    RGBA32_FLOAT,
    R32_UINT,
    R11G11B10_FLOAT,
    D32_FLOAT,
    BC1_SRGB,
    BC5_UNORM,
    BC7_SRGB,
    COUNT
};
```

## Functions

### Public

#### `block_size` {#function-block-size}

<div class="function-declaration" id="function-block-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a> block_size(
    <a href="texture-format.html#symbol-texture-format">texture_format</a> format) <span class="k">noexcept</span>;</code></pre></div>

<p>Width and height in pixels of one storage block: 4 for the compressed formats, 1 otherwise (0 for <code>UNKNOWN</code>).</p>

#### `bytes_per_block` {#function-bytes-per-block}

<div class="function-declaration" id="function-bytes-per-block-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a> bytes_per_block(
    <a href="texture-format.html#symbol-texture-format">texture_format</a> format) <span class="k">noexcept</span>;</code></pre></div>

<p>Bytes of one storage block: <code><a href="#function-bytes-per-pixel-1">bytes_per_pixel</a>()</code> for uncompressed formats, 8 for BC1 and 16 for BC5 and BC7.</p>

#### `bytes_per_pixel` {#function-bytes-per-pixel}

<div class="function-declaration" id="function-bytes-per-pixel-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a> bytes_per_pixel(
    <a href="texture-format.html#symbol-texture-format">texture_format</a> format) <span class="k">noexcept</span>;</code></pre></div>

<p>Bytes of one pixel of an uncompressed format. 0 for <code>UNKNOWN</code> and for block-compressed formats, whose storage is described by <code><a href="#function-bytes-per-block-1">bytes_per_block</a>()</code> and <code><a href="#function-block-size-1">block_size</a>()</code>.</p>

#### `is_compressed` {#function-is-compressed}

<div class="function-declaration" id="function-is-compressed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_compressed(
    <a href="texture-format.html#symbol-texture-format">texture_format</a> format) <span class="k">noexcept</span>;</code></pre></div>

<p>True for the block-compressed formats (BC1, BC5, BC7).</p>

#### `is_depth` {#function-is-depth}

<div class="function-declaration" id="function-is-depth-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_depth(
    <a href="texture-format.html#symbol-texture-format">texture_format</a> format) <span class="k">noexcept</span>;</code></pre></div>

<p>True for depth formats (<code>D32_FLOAT</code>).</p>

#### `is_srgb` {#function-is-srgb}

<div class="function-declaration" id="function-is-srgb-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_srgb(
    <a href="texture-format.html#symbol-texture-format">texture_format</a> format) <span class="k">noexcept</span>;</code></pre></div>

<p>True for formats whose values are sRGB encoded and decoded by the sampler.</p>

#### `parse_texture_format` {#function-parse-texture-format}

<div class="function-declaration" id="function-parse-texture-format-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="texture-format.html#symbol-texture-format">texture_format</a>&gt; parse_texture_format(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Parses an enumerator name case-insensitively. Fails with <code>core::error_code::INVALID_ARGUMENT</code> for anything that is not a format name; &quot;<code>UNKNOWN</code>&quot; parses to <code>UNKNOWN</code>.</p>

#### `row_pitch` {#function-row-pitch}

<div class="function-declaration" id="function-row-pitch-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../core/types.html#symbol-u64">u64</a> row_pitch(
    <a href="texture-format.html#symbol-texture-format">texture_format</a> format,
    <a href="../core/types.html#symbol-u32">u32</a> width) <span class="k">noexcept</span>;</code></pre></div>

<p>Bytes of one tightly packed row of <code>width</code> pixels, rounded up to whole blocks for the compressed formats. 0 for <code>UNKNOWN</code>.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="texture-format.html#symbol-texture-format">texture_format</a> format) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a format (&quot;<code>RGBA8_SRGB</code>&quot;); &quot;<code>UNKNOWN</code>&quot; for <code>UNKNOWN</code> and out-of-range values.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="block_size"></a>
<a id="bytes_per_block"></a>
<a id="bytes_per_pixel"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="is_compressed"></a>
<a id="is_depth"></a>
<a id="is_srgb"></a>
<a id="other-declarations"></a>
<a id="parse_texture_format"></a>
<a id="row_pitch"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="texture_format"></a>
<a id="texture_formatbc1_srgb"></a>
<a id="texture_formatbc5_unorm"></a>
<a id="texture_formatbc7_srgb"></a>
<a id="texture_formatbgra8_srgb"></a>
<a id="texture_formatcount"></a>
<a id="texture_formatd32_float"></a>
<a id="texture_formatr11g11b10_float"></a>
<a id="texture_formatr16_float"></a>
<a id="texture_formatr32_float"></a>
<a id="texture_formatr32_uint"></a>
<a id="texture_formatr8_unorm"></a>
<a id="texture_formatrg16_float"></a>
<a id="texture_formatrg16_snorm"></a>
<a id="texture_formatrg32_float"></a>
<a id="texture_formatrg8_unorm"></a>
<a id="texture_formatrgb32_float"></a>
<a id="texture_formatrgba16_float"></a>
<a id="texture_formatrgba16_snorm"></a>
<a id="texture_formatrgba32_float"></a>
<a id="texture_formatrgba8_srgb"></a>
<a id="texture_formatrgba8_unorm"></a>
<a id="texture_formatunknown"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
