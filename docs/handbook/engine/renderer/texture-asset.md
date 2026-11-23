---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# texture_asset.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/texture_asset.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [JPEG XL images](../../../api/renderer/jpeg-xl.md), [Renderer assets](../../../api/renderer/renderer-assets.md) |

<p>The CPU texture asset. Mips are generated on the CPU at import (box filter, sRGB-aware) and stored in the .ctex blob, so the GPU path is upload-only. Decoding goes through libjxl (JPEG XL) or <code>stb_image</code> (<code>PNG</code>, JPEG, TGA, BMP) in engine/src, chosen by the file&#x27;s signature; the public API only sees bytes.</p>

## Structs

| File | Description |
|---|---|
| [`texture_import_settings`](texture-asset/texture-import-settings.md) | Importer settings, mirrored by the JSON object { &quot;<code>colourSpace</code>&quot;: &quot;srgb&quot; \| &quot;linear&quot;, &quot;filter&quot;: &quot;linear&quot; \| &quot;point&quot;, &quot;wrap&quot;: &quot;repeat&quot; \| &quot;clamp&quot; \| &quot;mirror&quot;, &quot;<code>generateMips</code>&quot;: <code>true</code> }. |
| [`texture_mip`](texture-asset/texture-mip.md) | One mip level, tightly packed rows of <code>bytes_per_pixel(format)</code> pixels. |

## Classes

| File | Description |
|---|---|
| [`texture_asset`](texture-asset/texture-asset.md) |  |
| [`texture_importer`](texture-asset/texture-importer.md) | The texture importer: .png .jpg .jpeg .tga .bmp .jxl -&gt; one .ctex blob (decode + CPU mips). The file&#x27;s signature picks the decoder, so a misnamed file still imports. |
| [`texture_loader`](texture-asset/texture-loader.md) | The <code>conjure.renderer.texture</code> loader; the fallback is a 1x1 white texture. |

## Enumerations

### `sampler_filter` {#symbol-sampler-filter}

```cpp
enum class sampler_filter : u8 {
    LINEAR = 0,
    POINT
};
```

### `texture_colour_space` {#symbol-texture-colour-space}

```cpp
enum class texture_colour_space : u8 {
    SRGB = 0,
    LINEAR
};
```

### `texture_wrap` {#symbol-texture-wrap}

```cpp
enum class texture_wrap : u8 {
    REPEAT = 0,
    CLAMP,
    MIRROR
};
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-TEXTURE_ASSET_TYPE_NAME"></a><code>TEXTURE_ASSET_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.texture"</span></code> |  |
| <a id="variable-TEXTURE_BLOB_VERSION"></a><code>TEXTURE_BLOB_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> |  |
| <a id="variable-TEXTURE_EXTENSIONS"></a><code>TEXTURE_EXTENSIONS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>array&lt;<wbr>std::<wbr>string_<wbr>view,<wbr> <span class="mi">6</span>&gt;</code> | <code class="signature">{ <span class="s">".png"</span>,<wbr> <span class="s">".jpg"</span>,<wbr> <span class="s">".jpeg"</span>,<wbr> <span class="s">".tga"</span>,<wbr> <span class="s">".bmp"</span>,<wbr> <span class="s">".jxl"</span> }</code> |  |
| <a id="variable-TEXTURE_IMPORTER_ID"></a><code>TEXTURE_IMPORTER_ID</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.texture"</span></code> |  |
| <a id="variable-TEXTURE_IMPORTER_VERSION"></a><code>TEXTURE_IMPORTER_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">2</span></code> |  |

## Macros

### `CONJURE_DECLARE_ASSET_TYPE` {#symbol-conjure-declare-asset-type}

```cpp
CONJURE_DECLARE_ASSET_TYPE(conjure::renderer::texture_asset, "conjure.renderer.texture")
```

## Functions

### Public

#### `mip_count_for` {#function-mip-count-for}

<div class="function-declaration" id="function-mip-count-for-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u32">u32</a> mip_count_for(
    <a href="../core/types.html#symbol-u32">u32</a> width,
    <a href="../core/types.html#symbol-u32">u32</a> height) <span class="k">noexcept</span>;</code></pre></div>

<p>Number of mip levels of a full chain down to 1x1.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="texture-asset.html#symbol-texture-colour-space">texture_colour_space</a> value) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="texture-asset.html#symbol-sampler-filter">sampler_filter</a> value) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-3"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="texture-asset.html#symbol-texture-wrap">texture_wrap</a> value) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="conjure_declare_asset_type"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="mip_count_for"></a>
<a id="other-declarations"></a>
<a id="sampler_filter"></a>
<a id="sampler_filterlinear"></a>
<a id="sampler_filterpoint"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="texture_asset"></a>
<a id="texture_asset_type_name"></a>
<a id="texture_assetchecker"></a>
<a id="texture_assetcolour_space"></a>
<a id="texture_assetdecode"></a>
<a id="texture_assetfilter"></a>
<a id="texture_assetformat"></a>
<a id="texture_assetfrom_image"></a>
<a id="texture_assetfrom_rgba8"></a>
<a id="texture_assetgpu_byte_size"></a>
<a id="texture_assetheight"></a>
<a id="texture_assetload_from_file"></a>
<a id="texture_assetmax_dimension"></a>
<a id="texture_assetmcolourspace"></a>
<a id="texture_assetmfilter"></a>
<a id="texture_assetmformat"></a>
<a id="texture_assetmip_count"></a>
<a id="texture_assetmips"></a>
<a id="texture_assetmmips"></a>
<a id="texture_assetmname"></a>
<a id="texture_assetmwrap"></a>
<a id="texture_assetname"></a>
<a id="texture_assetoperator"></a>
<a id="texture_assetoperator_1"></a>
<a id="texture_assetparse_blob"></a>
<a id="texture_assetpixel"></a>
<a id="texture_assetset_filter"></a>
<a id="texture_assetset_name"></a>
<a id="texture_assetset_wrap"></a>
<a id="texture_assetsolid_colour"></a>
<a id="texture_assettexture_asset"></a>
<a id="texture_assettexture_asset_1"></a>
<a id="texture_assetwidth"></a>
<a id="texture_assetwrap"></a>
<a id="texture_assetwrite_blob"></a>
<a id="texture_blob_version"></a>
<a id="texture_colour_space"></a>
<a id="texture_colour_spacelinear"></a>
<a id="texture_colour_spacesrgb"></a>
<a id="texture_extensions"></a>
<a id="texture_import_settings"></a>
<a id="texture_import_settingscolourspace"></a>
<a id="texture_import_settingsfilter"></a>
<a id="texture_import_settingsfrom_json"></a>
<a id="texture_import_settingsgeneratemips"></a>
<a id="texture_import_settingsoperator"></a>
<a id="texture_import_settingsto_document"></a>
<a id="texture_import_settingsto_json"></a>
<a id="texture_import_settingswrap"></a>
<a id="texture_importer"></a>
<a id="texture_importer_id"></a>
<a id="texture_importer_version"></a>
<a id="texture_importerdefault_settings"></a>
<a id="texture_importerextensions"></a>
<a id="texture_importerid"></a>
<a id="texture_importerimport"></a>
<a id="texture_importermjobs"></a>
<a id="texture_importertexture_importer"></a>
<a id="texture_importertexture_importer_1"></a>
<a id="texture_importerversion"></a>
<a id="texture_loader"></a>
<a id="texture_loadercreate_fallback_typed"></a>
<a id="texture_loaderfinalise_typed"></a>
<a id="texture_loaderload_cpu_typed"></a>
<a id="texture_loadertexture_loader"></a>
<a id="texture_loadertexture_loader_1"></a>
<a id="texture_mip"></a>
<a id="texture_mipheight"></a>
<a id="texture_mipoperator"></a>
<a id="texture_mippixels"></a>
<a id="texture_mipwidth"></a>
<a id="texture_wrap"></a>
<a id="texture_wrapclamp"></a>
<a id="texture_wrapmirror"></a>
<a id="texture_wraprepeat"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
