---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# jpeg_xl.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/jpeg_xl.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [JPEG XL images](../../../api/renderer/jpeg-xl.md) |

<p>JPEG XL (ISO/IEC 18181) in the renderer: the signature check the image decoder routes by and an RGBA8 encoder for screenshots and tools. Decoding needs no call of its own: <code>texture_asset::decode</code>, the texture importer (.jxl), glTF images and <code>equirect_image::decode_hdr</code> recognise a JPEG XL file by its signature. libjxl does the work in engine/src; this header carries only engine types.</p>

## Structs

| File | Description |
|---|---|
| [`jpeg_xl_encode_options`](jpeg-xl/jpeg-xl-encode-options.md) | How <code>encode_jpeg_xl</code> compresses an image. <code>A</code> lossy encode targets a Butteraugli distance in (0, 25], where 1 is visually lossless. The colour space is what the file is tagged as: sRGB, or linear sRGB for data. |

## Functions

### Public

#### `encode_jpeg_xl` {#function-encode-jpeg-xl}

<div class="function-declaration" id="function-encode-jpeg-xl-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;std::vector&lt;std::byte&gt;&gt; encode_jpeg_xl(
    <span class="k">const</span> <a href="../core/image/image.html">core::image</a> &amp;image,
    <span class="k">const</span> <a href="jpeg-xl/jpeg-xl-encode-options.html">jpeg_xl_encode_options</a> &amp;options = {},
    <a href="../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *jobs = nullptr);</code></pre></div>

<p>Encodes an RGBA8 image as a JPEG XL file tagged sRGB or linear sRGB (<code>options.colourSpace</code>), on <code>jobs</code> when it has workers (the bytes are the same either way). <code>A</code> file of an opaque image still stores its alpha channel, so it decodes back to the same RGBA. Fails with <code>INVALID_ARGUMENT</code> for an empty image, an effort outside 1..10 or a lossy distance outside (0, 25], and with <code>BACKEND_FAILURE</code> when libjxl fails.</p>

#### `is_jpeg_xl` {#function-is-jpeg-xl}

<div class="function-declaration" id="function-is-jpeg-xl-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_jpeg_xl(
    std::span&lt;<span class="k">const</span> std::byte&gt; bytes) <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>bytes</code> start with a JPEG XL signature: a bare codestream (FF 0A) or the container&#x27;s signature box. Two bytes decide a codestream and twelve a container; fewer read as <code>false</code>.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="encode_jpeg_xl"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="is_jpeg_xl"></a>
<a id="jpeg_xl_encode_options"></a>
<a id="jpeg_xl_encode_optionscolourspace"></a>
<a id="jpeg_xl_encode_optionsdistance"></a>
<a id="jpeg_xl_encode_optionseffort"></a>
<a id="jpeg_xl_encode_optionslossless"></a>
<a id="jpeg_xl_encode_optionsoperator"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
