---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# environment_asset.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/environment_asset.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Renderer assets](../../../api/renderer/renderer-assets.md) |

<p>Image-based lighting environments. The bake runs on the CPU inside the importer: equirect .hdr -&gt; sky cubemap with a box-filtered mip chain -&gt; SH9 irradiance -&gt; GGX prefiltered specular cubemap. It is deterministic (fixed Hammersley sequence, fixed evaluation order, bit-identical whether faces are baked inline or on jobs) because the importer output is a cache key. The cooked .cenv holds both cubemaps as RGBA16F plus the SH9.</p>

## Structs

| File | Description |
|---|---|
| [`cubemap_data`](environment-asset/cubemap-data.md) | <code>A</code> cubemap with a mip chain, RGBA16F texels stored as packed halves. Faces follow the +<code>X</code>, -<code>X</code>, +<code>Y</code>, -<code>Y</code>, +<code>Z</code>, -<code>Z</code> convention; texels are stored mip-major then face-major, rows top to bottom. |
| [`environment_bake_options`](environment-asset/environment-bake-options.md) | Bake sizes and their defaults. Tests use small sizes. |
| [`equirect_image`](environment-asset/equirect-image.md) | <code>A</code> linear RGB equirectangular image (the decoded .hdr or JPEG XL). |

## Classes

| File | Description |
|---|---|
| [`environment_asset`](environment-asset/environment-asset.md) |  |
| [`environment_importer`](environment-asset/environment-importer.md) | The .hdr importer: decodes the equirect image and bakes it (settings: <code><a href="environment-asset/environment-bake-options.html">environment_bake_options</a></code>). <code>A</code> JPEG XL equirect (float, PQ or HLG; <code>equirect_image::decode_hdr</code>) bakes too when its .meta names this importer; the .jxl extension itself belongs to the texture importer. |
| [`environment_loader`](environment-asset/environment-loader.md) | The <code>conjure.renderer.environment</code> loader; the fallback is the uniform grey environment of <code>FALLBACK_ENVIRONMENT_RADIANCE</code>, the same grey the renderer lights a scene without an environment by. |

## Type Aliases

### `sh9` {#symbol-sh9}

```cpp
using sh9 = std::array<math::vec3, 9>;
```

Nine spherical harmonics coefficients (bands 0..2) of RGB irradiance.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-ENVIRONMENT_ASSET_TYPE_NAME"></a><code>ENVIRONMENT_ASSET_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.environment"</span></code> |  |
| <a id="variable-ENVIRONMENT_BLOB_VERSION"></a><code>ENVIRONMENT_BLOB_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> |  |
| <a id="variable-ENVIRONMENT_EXTENSIONS"></a><code>ENVIRONMENT_EXTENSIONS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>array&lt;<wbr>std::<wbr>string_<wbr>view,<wbr> <span class="mi">1</span>&gt;</code> | <code class="signature">{<span class="s">".hdr"</span>}</code> |  |
| <a id="variable-ENVIRONMENT_IMPORTER_ID"></a><code>ENVIRONMENT_IMPORTER_ID</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.environment"</span></code> |  |
| <a id="variable-ENVIRONMENT_IMPORTER_VERSION"></a><code>ENVIRONMENT_IMPORTER_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">3</span></code> |  |
| <a id="variable-FALLBACK_ENVIRONMENT_RADIANCE"></a><code>FALLBACK_ENVIRONMENT_RADIANCE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">0.25</span>f</code> | The grey radiance of both fallback environments: the loader&#x27;s and the renderer&#x27;s. |
| <a id="variable-MAX_ENVIRONMENT_RADIANCE"></a><code>MAX_ENVIRONMENT_RADIANCE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature">std::<wbr>numeric_<wbr>limits&lt;<wbr><a href="../core/types.html#symbol-f32">f32</a>&gt;::<wbr>max() / <span class="mi">65536.0</span>f</code> | The brightest radiance the bake integrates (about 5.2e33): <code>equirect_image::decode_hdr</code> and <code>equirect_image::sample</code> read +inf and anything brighter as this value. It is the largest float divided by 2^16, so the bake&#x27;s largest sum, the GGX prefilter&#x27;s over at most 4096 samples, stays 16 times below the float limit; the 4-texel mip averages and the SH9 coefficients (at most 4 pi x 0.63 times this) stay further below. Real radiance is many orders of magnitude darker (the sun&#x27;s disc is about 1.6e9 cd/m^2), so a bright sun reaches the SH9 irradiance and the prefiltered mips unclipped; only the stored cubemap texels are clamped, to the half range (<code>bake_environment</code>). |

## Macros

### `CONJURE_DECLARE_ASSET_TYPE` {#symbol-conjure-declare-asset-type}

```cpp
CONJURE_DECLARE_ASSET_TYPE(conjure::renderer::environment_asset, "conjure.renderer.environment")
```

## Functions

### Public

#### `bake_environment` {#function-bake-environment}

<div class="function-declaration" id="function-bake-environment-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="environment-asset/environment-asset.html">environment_asset</a>&gt; bake_environment(
    <span class="k">const</span> <a href="environment-asset/equirect-image.html">equirect_image</a> &amp;source,
    <span class="k">const</span> <a href="environment-asset/environment-bake-options.html">environment_bake_options</a> &amp;options,
    <a href="../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *jobs);</code></pre></div>

<p>Bakes an environment on the CPU: cubemap, mips, SH9, GGX prefilter. Faces are baked through <code>jobs-&gt;parallel_for</code> when a job system is given and inline otherwise; the output is bit-identical either way. The mip chain, the SH9 projection and the prefilter run on 32-bit floats of the image&#x27;s radiance as <code>equirect_image::sample</code> reads it, so a sun far brighter than a half holds still lights the irradiance and the rough mips fully; only the stored texels of both cubemaps are clamped to the finite half range [0, 65504] (NaN reads as 0), so a bright sun never packs to infinity. Fails with <code>INVALID_ARGUMENT</code> for an invalid image or options (sizes not powers of two or below 8, zero samples).</p>

#### `cubemap_texel_direction` {#function-cubemap-texel-direction}

<div class="function-declaration" id="function-cubemap-texel-direction-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/math/glm-types.html#symbol-vec3">math::vec3</a> cubemap_texel_direction(
    <a href="../core/types.html#symbol-u32">u32</a> face,
    <a href="../core/types.html#symbol-u32">u32</a> x,
    <a href="../core/types.html#symbol-u32">u32</a> y,
    <a href="../core/types.html#symbol-u32">u32</a> size) <span class="k">noexcept</span>;</code></pre></div>

<p>The direction through the centre of a cubemap texel, for a face index in the +<code>X</code>..-<code>Z</code> order and texel coordinates in [0, size).</p>

#### `evaluate_sh9_irradiance` {#function-evaluate-sh9-irradiance}

<div class="function-declaration" id="function-evaluate-sh9-irradiance-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/math/glm-types.html#symbol-vec3">math::vec3</a> evaluate_sh9_irradiance(
    <span class="k">const</span> <a href="environment-asset.html#symbol-sh9">sh9</a> &amp;coefficients,
    <span class="k">const</span> <a href="../core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;normal) <span class="k">noexcept</span>;</code></pre></div>

<p>Evaluates SH9 irradiance (already convolved with the cosine lobe) in a direction.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="bake_environment"></a>
<a id="conjure_declare_asset_type"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="cubemap_data"></a>
<a id="cubemap_dataface_length"></a>
<a id="cubemap_dataface_offset"></a>
<a id="cubemap_datais_consistent"></a>
<a id="cubemap_datamip_size"></a>
<a id="cubemap_datamipcount"></a>
<a id="cubemap_dataoperator"></a>
<a id="cubemap_datasize"></a>
<a id="cubemap_datatexel"></a>
<a id="cubemap_datatexels"></a>
<a id="cubemap_texel_direction"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="environment_asset"></a>
<a id="environment_asset_type_name"></a>
<a id="environment_assetenvironment_asset"></a>
<a id="environment_assetenvironment_asset_1"></a>
<a id="environment_assetgpu_byte_size"></a>
<a id="environment_assetirradiance"></a>
<a id="environment_assetmirradiance"></a>
<a id="environment_assetmname"></a>
<a id="environment_assetmprefiltered"></a>
<a id="environment_assetmsky"></a>
<a id="environment_assetname"></a>
<a id="environment_assetoperator"></a>
<a id="environment_assetoperator_1"></a>
<a id="environment_assetparse_blob"></a>
<a id="environment_assetprefiltered"></a>
<a id="environment_assetset_name"></a>
<a id="environment_assetsky"></a>
<a id="environment_assetuniform"></a>
<a id="environment_assetwrite_blob"></a>
<a id="environment_bake_options"></a>
<a id="environment_bake_optionsfrom_json"></a>
<a id="environment_bake_optionsoperator"></a>
<a id="environment_bake_optionsprefilteredsize"></a>
<a id="environment_bake_optionssamplecount"></a>
<a id="environment_bake_optionsskysize"></a>
<a id="environment_bake_optionsto_document"></a>
<a id="environment_bake_optionsto_json"></a>
<a id="environment_blob_version"></a>
<a id="environment_extensions"></a>
<a id="environment_importer"></a>
<a id="environment_importer_id"></a>
<a id="environment_importer_version"></a>
<a id="environment_importerdefault_settings"></a>
<a id="environment_importerenvironment_importer"></a>
<a id="environment_importerenvironment_importer_1"></a>
<a id="environment_importerextensions"></a>
<a id="environment_importerid"></a>
<a id="environment_importerimport"></a>
<a id="environment_importermjobs"></a>
<a id="environment_importerversion"></a>
<a id="environment_loader"></a>
<a id="environment_loadercreate_fallback_typed"></a>
<a id="environment_loaderenvironment_loader"></a>
<a id="environment_loaderenvironment_loader_1"></a>
<a id="environment_loaderfinalise_typed"></a>
<a id="environment_loaderload_cpu_typed"></a>
<a id="equirect_image"></a>
<a id="equirect_imageconstant"></a>
<a id="equirect_imagedecode_hdr"></a>
<a id="equirect_imageheight"></a>
<a id="equirect_imageis_valid"></a>
<a id="equirect_imagergb"></a>
<a id="equirect_imagesample"></a>
<a id="equirect_imagewidth"></a>
<a id="evaluate_sh9_irradiance"></a>
<a id="extension-points"></a>
<a id="fallback_environment_radiance"></a>
<a id="internals-and-algorithms"></a>
<a id="max_environment_radiance"></a>
<a id="other-declarations"></a>
<a id="sh9"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
