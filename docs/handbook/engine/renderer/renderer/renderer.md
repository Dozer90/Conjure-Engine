---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [renderer.h](../renderer.md)

# renderer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/renderer.h` |
| Source File Path | `engine/src/renderer/renderer.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md), [Rendering a frame](../../../../api/renderer/rendering-a-frame.md) |

## Syntax

```cpp
class CONJURE_API renderer
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> renderer(
    create_tag);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~renderer();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mImpl</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../renderer.html">impl</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin_extract` {#function-begin-extract}

<div class="function-declaration" id="function-begin-extract-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../render-scene/render-scene.html">render_scene</a> &amp;begin_extract();</code></pre></div>

<p>Clears the write slot of the owned <code><a href="../render-scene/render-scene-pair.html">render_scene_pair</a></code> and returns it for <code>EXTRACT</code>.</p>

#### `create` {#function-create}

<div class="function-declaration" id="function-create-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;std::unique_ptr&lt;<a href="renderer.html">renderer</a>&gt;&gt; create(
    <span class="k">const</span> <a href="renderer-desc.html">renderer_desc</a> &amp;desc);</code></pre></div>

<p>Creates every pipeline, binding layout, fallback resource and the BRDF LUT up front. <code>A</code> missing shader permutation fails creation with <code>renderer_error::SHADER_NOT_FOUND</code> naming the key; a device refusal with <code>PIPELINE_CREATION_FAILED</code> or <code>RESOURCE_CREATION_FAILED</code>; a missing device or shader library with <code>INVALID_ARGUMENT</code>.</p>

#### `create_render_target` {#function-create-render-target}

<div class="function-declaration" id="function-create-render-target-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../gpu-ids.html#symbol-render-target-id">render_target_id</a>&gt; create_render_target(
    <span class="k">const</span> <a href="../render-target/render-target-desc.html">render_target_desc</a> &amp;desc);</code></pre></div>

<p>Creates an off-screen target. Fails with <code>INVALID_ARGUMENT</code> for a zero size or an unsupported colour format and <code>RESOURCE_CREATION_FAILED</code> / <code>PIPELINE_CREATION_FAILED</code> when the device refuses.</p>

#### `desc` {#function-desc}

<div class="function-declaration" id="function-desc-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="renderer-desc.html">renderer_desc</a> &amp;desc() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The description the <code><a href="renderer.html">renderer</a></code> was created from.</p>

#### `destroy_render_target` {#function-destroy-render-target}

<div class="function-declaration" id="function-destroy-render-target-1"><pre><code><span class="k">void</span> destroy_render_target(
    <a href="../gpu-ids.html#symbol-render-target-id">render_target_id</a> id);</code></pre></div>

<p>Destroys a target; unknown ids are ignored.</p>

#### `device` {#function-device}

<div class="function-declaration" id="function-device-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../rhi/device/device.html">rhi::device</a> &amp;device() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The device the <code><a href="renderer.html">renderer</a></code> was created on.</p>

#### `end_extract` {#function-end-extract}

<div class="function-declaration" id="function-end-extract-1"><pre><code><span class="k">void</span> end_extract();</code></pre></div>

<p>Appends the views queued by <code><a href="#function-submit-view-1">submit_view</a>()</code> to the scene being written, then swaps the pair; the extracted scene is immutable from here on.</p>

#### `find_environment` {#function-find-environment}

<div class="function-declaration" id="function-find-environment-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../gpu-ids.html#symbol-gpu-environment-id">gpu_environment_id</a> find_environment(
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `find_material` {#function-find-material}

<div class="function-declaration" id="function-find-material-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../gpu-ids.html#symbol-gpu-material-id">gpu_material_id</a> find_material(
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `find_mesh` {#function-find-mesh}

<div class="function-declaration" id="function-find-mesh-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../gpu-ids.html#symbol-gpu-mesh-id">gpu_mesh_id</a> find_mesh(
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Returns the gpu id previously uploaded for the asset, or an invalid id. Render thread.</p>

#### `find_texture` {#function-find-texture}

<div class="function-declaration" id="function-find-texture-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../gpu-ids.html#symbol-gpu-texture-id">gpu_texture_id</a> find_texture(
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `frame_count` {#function-frame-count}

<div class="function-declaration" id="function-frame-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> frame_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of frames rendered.</p>

#### `get_render_target` {#function-get-render-target}

<div class="function-declaration" id="function-get-render-target-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../render-target/render-target.html">render_target</a> *get_render_target(
    <a href="../gpu-ids.html#symbol-render-target-id">render_target_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<p>The target, or <code>nullptr</code> for an unknown id. The pointer is valid until <code>destroy_render_target</code>.</p>

#### `imgui` {#function-imgui}

<div class="function-declaration" id="function-imgui-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../imgui-renderer/imgui-renderer.html">imgui_renderer</a> *imgui() <span class="k">noexcept</span>;</code></pre></div>

<p>The ImGui backend; <code>null</code> in Distribution builds, without a swapchain or when disabled.</p>

#### `is_lost` {#function-is-lost}

<div class="function-declaration" id="function-is-lost-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_lost() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True after <code><a href="#function-on-device-lost-1">on_device_lost</a>()</code> or after the device reported loss during a frame.</p>

#### `on_device_lost` {#function-on-device-lost}

<div class="function-declaration" id="function-on-device-lost-1"><pre><code><span class="k">void</span> on_device_lost();</code></pre></div>

<p>Device loss: stops recording and drops the GPU mirrors; every later <code><a href="#function-render-1">render</a>()</code> fails with <code>DEVICE_LOST</code> and the engine quits.</p>

#### `pending_upload_count` {#function-pending-upload-count}

<div class="function-declaration" id="function-pending-upload-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> pending_upload_count() <span class="k">const</span>;</code></pre></div>

<p>Uploads queued but not yet flushed by a <code><a href="#function-render-1">render</a>()</code>.</p>

#### `read_back` {#function-read-back}

<div class="function-declaration" id="function-read-back-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../core/image/image.html">core::image</a>&gt; read_back(
    <a href="../gpu-ids.html#symbol-render-target-id">render_target_id</a> target);</code></pre></div>

<p>Synchronous readback of a render target&#x27;s colour texture as RGBA8 (waits for the GPU). On the <code>null</code> device the image is zero-filled and the right size. Fails with <code>NOT_FOUND</code> for an unknown target, <code>rhi_error::DEVICE_LOST</code> on a lost device and <code>renderer_error::RESOURCE_CREATION_FAILED</code> when the staging texture cannot be created or mapped.</p>

#### `read_back_depth` {#function-read-back-depth}

<div class="function-declaration" id="function-read-back-depth-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../render-target/depth-read-back.html">depth_read_back</a>&gt; read_back_depth(
    <a href="../gpu-ids.html#symbol-render-target-id">render_target_id</a> target);</code></pre></div>

<p>Synchronous readback of a render target&#x27;s depth (waits for the GPU): the scene depth the first view of the last frame with a view into the target drew (<code><a href="../render-target/render-target.html">render_target</a>.h</code>), reverse-<code>Z</code>. <code>A</code> target no view has rendered into since its creation or last resize reads as all zeros (the far plane) without a GPU copy, and on the <code>null</code> device every value is zero. Fails with <code>NOT_FOUND</code> for an unknown target, <code>INVALID_ARGUMENT</code> for a target created without depth, <code>rhi_error::DEVICE_LOST</code> on a lost device and <code>renderer_error::RESOURCE_CREATION_FAILED</code> when the staging texture cannot be created or mapped. Render thread.</p>

#### `register_asset_types` {#function-register-asset-types}

<div class="function-declaration" id="function-register-asset-types-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_asset_types(
    <a href="../../assets/asset-type-registry/asset-type-registry.html">assets::asset_type_registry</a> &amp;types,
    <a href="../../assets/asset-importer/importer-registry.html">assets::importer_registry</a> &amp;importers,
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *jobs = nullptr);</code></pre></div>

<p>Registers this module&#x27;s asset types (mesh, texture, material, model, environment) and importers (glTF, texture, .cmat, .hdr); called by the engine&#x27;s <code>renderer_subsystem</code>. The optional job system lets the environment importer bake faces in parallel. Errors are those of the two registries.</p>

#### `release_environment` {#function-release-environment}

<div class="function-declaration" id="function-release-environment-1"><pre><code><span class="k">void</span> release_environment(
    <a href="../gpu-ids.html#symbol-gpu-environment-id">gpu_environment_id</a> id);</code></pre></div>

#### `release_material` {#function-release-material}

<div class="function-declaration" id="function-release-material-1"><pre><code><span class="k">void</span> release_material(
    <a href="../gpu-ids.html#symbol-gpu-material-id">gpu_material_id</a> id);</code></pre></div>

#### `release_mesh` {#function-release-mesh}

<div class="function-declaration" id="function-release-mesh-1"><pre><code><span class="k">void</span> release_mesh(
    <a href="../gpu-ids.html#symbol-gpu-mesh-id">gpu_mesh_id</a> id);</code></pre></div>

<p>Releases a GPU mirror; the id becomes stale at once, the GPU objects die bufferCount + 1 frames later. Safe from any thread. Unknown or stale ids are ignored.</p>

#### `release_texture` {#function-release-texture}

<div class="function-declaration" id="function-release-texture-1"><pre><code><span class="k">void</span> release_texture(
    <a href="../gpu-ids.html#symbol-gpu-texture-id">gpu_texture_id</a> id);</code></pre></div>

#### `render` {#function-render}

<div class="function-declaration" id="function-render-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; render(
    <span class="k">const</span> <a href="../render-scene/render-scene.html">render_scene</a> &amp;scene);</code></pre></div>

<p>Renders one scene. Render thread only. Fails only with <code>rhi_error::DEVICE_LOST</code>; everything else is clamped and logged so a bad scene never aborts a frame. <code>A</code> problem that lasts is logged (<code>WARN</code>) by the frame in which it starts and not again until a frame has seen it end, whatever its size does in between: more views than <code>MAX_VIEWS</code>, more lights than <code>MAX_LIGHTS_PER_VIEW</code>, more sprites than <code>MAX_SPRITES_PER_FRAME</code>; per settings group, a scene group (<code>render_scene::postProcess</code>, shadows, ssao) out of its range; per view index and group, a camera that had to be clamped, an override group out of its range (<code>render_view::settingsOverride</code>) or a render target the <code><a href="renderer.html">renderer</a></code> does not know; a swapchain that refuses the acquire (the frame is skipped) or the present. Out-of-range settings are clamped (sanitise): the scene&#x27;s groups once per frame, a view&#x27;s override groups once per view. Only a frame that looks at a problem can see it end: a frame skipped by a refused acquire looks at nothing else, the per-view problems are looked at only while the scene has a view at that index (the camera and the override groups only when that view is drawn), and the light budget only when a view is drawn. <code>A</code> view with debugMode <code>AMBIENT_OCCLUSION</code> renders the <code>SSAO</code> term instead of its lit output; a view with any debugMode but <code>NONE</code> draws no shadows (as <code>SKIP_SHADOWS</code>). <code>A</code> view targeting the swapchain of a <code><a href="renderer.html">renderer</a></code> without one is logged once per <code><a href="renderer.html">renderer</a></code>. <code><a href="#function-stats-1">stats</a>()</code> counts what every frame clamped.</p>

#### `render_frame` {#function-render-frame}

<div class="function-declaration" id="function-render-frame-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; render_frame();</code></pre></div>

<p>Renders the last extracted scene: <code><a href="#function-render-1">render</a>(pair.read())</code>. Render thread only.</p>

#### `resize_render_target` {#function-resize-render-target}

<div class="function-declaration" id="function-resize-render-target-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; resize_render_target(
    <a href="../gpu-ids.html#symbol-render-target-id">render_target_id</a> id,
    <a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a> size);</code></pre></div>

<p>Recreates the target&#x27;s textures at a new size (waiting for the GPU to go idle). The target id and its <code>colour_texture()</code> id stay the same; the id resolves to the new texture from here on. Fails with <code>NOT_FOUND</code> for an unknown id and <code>INVALID_ARGUMENT</code> for a zero size.</p>

#### `resize_swapchain` {#function-resize-swapchain}

<div class="function-declaration" id="function-resize-swapchain-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; resize_swapchain(
    <a href="../../core/math/glm-types.html#symbol-uvec2">math::uvec2</a> size);</code></pre></div>

<p>Resizes the swapchain the <code><a href="renderer.html">renderer</a></code> presents to; the engine calls it for a framebuffer resize instead of <code>swapchain::resize</code>. The <code><a href="renderer.html">renderer</a></code> holds framebuffers over the back buffers (the view targets and the ImGui pass), and a swapchain cannot recreate buffers that are still referenced, so this waits for the GPU, drops those framebuffers and the binding sets and only then resizes. Fails with <code>INVALID_ARGUMENT</code> without a swapchain and otherwise with what <code>swapchain::resize</code> reports; after a failure the caches are empty and refill on the next frame.</p>

#### `set_imgui_draw_data` {#function-set-imgui-draw-data}

<div class="function-declaration" id="function-set-imgui-draw-data-1"><pre><code><span class="k">void</span> set_imgui_draw_data(
    <a href="../renderer.html">ImDrawData</a> *drawData) <span class="k">noexcept</span>;</code></pre></div>

<p>Editor builds: draw data produced by <code><a href="../../engine/imgui-layer/imgui-layer.html">engine::imgui_layer</a></code>, drawn after the views of the next <code><a href="#function-render-1">render</a>()</code> into the swapchain. Ignored without a swapchain or outside editor builds.</p>

#### `stats` {#function-stats}

<div class="function-declaration" id="function-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="render-stats.html">render_stats</a> &amp;stats() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Statistics of the last <code><a href="#function-render-1">render</a>()</code>.</p>

#### `submit_view` {#function-submit-view}

<div class="function-declaration" id="function-submit-view-1"><pre><code><span class="k">void</span> submit_view(
    <span class="k">const</span> <a href="../render-scene/render-view.html">render_view</a> &amp;view);</code></pre></div>

<p>Queues a view that no scene camera stands for: the editor&#x27;s viewport camera, a screenshot through a chosen camera. The next <code><a href="#function-end-extract-1">end_extract</a>()</code> appends the queued views to the extracted scene, after the scene&#x27;s own, and forgets them, so each call renders the view for one frame; call it again every frame the view should be drawn, at any point before that frame&#x27;s <code><a href="#function-end-extract-1">end_extract</a>()</code>. The views render in submission order and before the ImGui pass, so a panel may show a render target in the frame it was drawn into. <code>A</code> view normally names a render target (an invalid one means the swapchain); one whose target was destroyed is skipped and counted in <code>render_stats::viewsSkipped</code> every frame, with a <code>WARN</code> when that starts (render). Render thread.</p>

#### `submitted_view_count` {#function-submitted-view-count}

<div class="function-declaration" id="function-submitted-view-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> submitted_view_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Views queued by <code><a href="#function-submit-view-1">submit_view</a>()</code> that no <code><a href="#function-end-extract-1">end_extract</a>()</code> has taken yet.</p>

#### `upload_environment` {#function-upload-environment}

<div class="function-declaration" id="function-upload-environment-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../gpu-ids.html#symbol-gpu-environment-id">gpu_environment_id</a> upload_environment(
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id,
    <span class="k">const</span> <a href="../environment-asset/environment-asset.html">environment_asset</a> &amp;env);</code></pre></div>

#### `upload_material` {#function-upload-material}

<div class="function-declaration" id="function-upload-material-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../gpu-ids.html#symbol-gpu-material-id">gpu_material_id</a> upload_material(
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id,
    <span class="k">const</span> <a href="../material-asset/material-asset.html">material_asset</a> &amp;material);</code></pre></div>

#### `upload_mesh` {#function-upload-mesh}

<div class="function-declaration" id="function-upload-mesh-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../gpu-ids.html#symbol-gpu-mesh-id">gpu_mesh_id</a> upload_mesh(
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id,
    <span class="k">const</span> <a href="../mesh-asset/mesh-asset.html">mesh_asset</a> &amp;mesh);</code></pre></div>

<p>Queues an upload; safe from any thread. The payload bytes are copied into the upload record at enqueue time. The returned id resolves to the fallback until the upload has run at the start of a later <code><a href="#function-render-1">render</a>()</code>. Uploading again under an asset id already known replaces the GPU mirror and keeps the id.</p>

#### `upload_texture` {#function-upload-texture}

<div class="function-declaration" id="function-upload-texture-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../gpu-ids.html#symbol-gpu-texture-id">gpu_texture_id</a> upload_texture(
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id,
    <span class="k">const</span> <a href="../texture-asset/texture-asset.html">texture_asset</a> &amp;texture);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
