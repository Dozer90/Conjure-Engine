---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# limits.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/limits.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Rendering a frame](../../../api/renderer/rendering-a-frame.md) |

<p>Fixed budgets of the renderer. The same values are #defined in engine/shaders/shared/<code>gpu_types.hlsli</code> (except <code>MAX_PASSES_PER_VIEW</code>, which only the CPU uses); tests/renderer/<code>test_gpu_types.cpp</code> compares both sides.</p>

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-BLOOM_MIP_COUNT"></a><code>BLOOM_MIP_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">6</span></code> | Mip levels of the bloom chain. |
| <a id="variable-CUBEMAP_FACE_COUNT"></a><code>CUBEMAP_FACE_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">6</span></code> | Faces of a cubemap, in the +<code>X</code>, -<code>X</code>, +<code>Y</code>, -<code>Y</code>, +<code>Z</code>, -<code>Z</code> order of D3D and Vulkan. |
| <a id="variable-ENVIRONMENT_MIP_COUNT"></a><code>ENVIRONMENT_MIP_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">6</span></code> | Mip levels of the environment cubemaps (sky and prefiltered specular). |
| <a id="variable-EXPOSURE_HISTOGRAM_BINS"></a><code>EXPOSURE_HISTOGRAM_BINS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">256</span></code> | Bins of the auto-exposure luminance histogram. |
| <a id="variable-INVALID_INDEX"></a><code>INVALID_INDEX</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">0</span>xFFFFFFFFu</code> |  |
| <a id="variable-LIGHT_TILE_SIZE"></a><code>LIGHT_TILE_SIZE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">16</span></code> |  |
| <a id="variable-MATERIAL_TEXTURE_SLOTS"></a><code>MATERIAL_TEXTURE_SLOTS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">5</span></code> | Number of texture slots of a material: base colour, metallic-roughness, normal, occlusion, emissive. |
| <a id="variable-MAX_DEBUG_VERTICES"></a><code>MAX_DEBUG_VERTICES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span>u &lt;&lt; <span class="mi">20</span></code> |  |
| <a id="variable-MAX_LIGHTS_PER_TILE"></a><code>MAX_LIGHTS_PER_TILE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">64</span></code> |  |
| <a id="variable-MAX_LIGHTS_PER_VIEW"></a><code>MAX_LIGHTS_PER_VIEW</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1024</span></code> |  |
| <a id="variable-MAX_MATERIAL_SLOTS"></a><code>MAX_MATERIAL_SLOTS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">8</span></code> |  |
| <a id="variable-MAX_PASSES_PER_VIEW"></a><code>MAX_PASSES_PER_VIEW</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature">MAX_<wbr>SHADOW_<wbr>CASCADES + (<span class="mi">2</span> * BLOOM_<wbr>MIP_<wbr>COUNT - <span class="mi">1</span>) + <span class="mi">16</span></code> | Passes one view&#x27;s render graph executes at most: a pass per shadow cascade, the bloom chain (<code>BLOOM_MIP_COUNT</code> down passes, one up pass fewer) and sixteen single passes (<code>shadow_atlas</code>, <code>depth_prepass</code>, <code>light_cull</code>, <code>ssao_gtao</code>, <code>ssao_blur_h</code>, <code>ssao_blur_v</code>, <code>forward_opaque</code>, skybox, <code>forward_blend</code>, <code>world_sprites</code>, <code><a href="debug-draw/debug-draw.html">debug_draw</a></code>, <code>exposure_histogram</code>, <code>exposure_adapt</code>, tonemap, <code>screen_sprites</code>, blit). <code>render_stats::passNames</code> holds <code>MAX_VIEWS</code> times this many. CPU-only: not mirrored in <code>gpu_types.hlsli</code>. |
| <a id="variable-MAX_SHADOW_CASCADES"></a><code>MAX_SHADOW_CASCADES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">4</span></code> |  |
| <a id="variable-MAX_SPRITES_PER_FRAME"></a><code>MAX_SPRITES_PER_FRAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">65536</span></code> |  |
| <a id="variable-MAX_VIEWS"></a><code>MAX_VIEWS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">8</span></code> |  |
| <a id="variable-TRANSIENT_EVICTION_FRAMES"></a><code>TRANSIENT_EVICTION_FRAMES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">120</span></code> | Frames a transient texture stays pooled without use before it is released. |
| <a id="variable-UPLOAD_BUDGET_BYTES_PER_FRAME"></a><code>UPLOAD_BUDGET_BYTES_PER_FRAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u64">u64</a></code> | <code class="signature"><a href="../core/types.html#symbol-u64">u64</a>{<span class="mi">64</span>} * <span class="mi">1024</span> * <span class="mi">1024</span></code> | Bytes of asset payload uploaded per frame at most; the remainder waits for the next frame. |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="bloom_mip_count"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="cubemap_face_count"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="environment_mip_count"></a>
<a id="exposure_histogram_bins"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="invalid_index"></a>
<a id="light_tile_size"></a>
<a id="material_texture_slots"></a>
<a id="max_debug_vertices"></a>
<a id="max_lights_per_tile"></a>
<a id="max_lights_per_view"></a>
<a id="max_material_slots"></a>
<a id="max_passes_per_view"></a>
<a id="max_shadow_cascades"></a>
<a id="max_sprites_per_frame"></a>
<a id="max_views"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="transient_eviction_frames"></a>
<a id="upload_budget_bytes_per_frame"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
