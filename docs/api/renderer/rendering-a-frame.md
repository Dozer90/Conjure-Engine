# Rendering a frame

📘 **Public API wiki** · 🛠 Handbook: [Renderer module](../../handbook/modules/renderer.md)

How to create the renderer, describe what a frame contains, render into the window or into a target, read
pixels back, draw sprites and debug lines, and tune the look. Headers: `conjure/renderer/renderer.h`,
`conjure/renderer/render_scene.h`, `conjure/renderer/camera.h`, `conjure/renderer/light.h`,
`conjure/renderer/render_settings.h`, `conjure/renderer/render_target.h`, `conjure/renderer/sprite_batch.h`,
`conjure/renderer/debug_draw.h`, `conjure/renderer/gpu_ids.h`, `conjure/renderer/limits.h`,
`conjure/renderer/components.h` and `conjure/renderer/imgui_renderer.h`. Everything is in `namespace
conjure::renderer`.

## At a glance

| You want to... | Use |
|---|---|
| Create the renderer | `renderer::create(renderer_desc)` with a device, a shader library and (optionally) a swapchain and job system |
| Put an asset on the GPU / take it off | `upload_mesh(id, mesh)`, `upload_texture`, `upload_material`, `upload_environment`; `release_*(gpuId)` |
| Find the GPU id of an asset uploaded earlier | `find_mesh(assetId)`, `find_texture`, `find_material`, `find_environment` |
| Render what the engine extracted | `render_scene &scene = begin_extract(); ... end_extract(); render_frame();` |
| Render a scene you built yourself | `render(scene)` |
| Render off-screen | `create_render_target(desc)`, set `render_view::target`, `read_back(target)` for the pixels |
| Add a view of your own to the engine's frame | `submit_view(view)` every frame it should be drawn (an editor viewport, a screenshot) |
| React to window events | `resize_swapchain(size)`, `on_device_lost()` |
| Know what happened | `stats()` (`render_stats`), `frame_count()`, `pending_upload_count()`, `is_lost()` |
| Draw sprites | push `sprite` values into `render_scene::sprites` (world or screen space) |
| Draw debug lines | `render_scene::debugDraw.line(...)`, `box`, `sphere`, `frustum`, `axes`, `grid`; `debug_draw` for timed shapes |
| Project and unproject | `clip_from_view(camera, aspect)`, `frustum_from_clip(clipFromWorld)`, `ray_from_viewport(...)` |
| Register the renderer's components for the ECS | `register_components(typeRegistry)` |

## Quick example

```cpp
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/renderer.h"

using namespace conjure;

void fill_frame(renderer::renderer &frame, renderer::gpu_mesh_id mesh, renderer::gpu_material_id material) {
    renderer::render_scene &scene = frame.begin_extract();
    renderer::render_view view = {};                      // target invalid = the swapchain
    view.cameraData.verticalFovRadians = 1.0472f;
    view.cameraPosition = math::vec3(0.0f, 2.0f, 6.0f);
    view.viewFromWorld = math::look_at(view.cameraPosition, math::vec3(0.0f), math::vec3(0.0f, 1.0f, 0.0f));
    scene.views.push_back(view);

    const std::span<renderer::mesh_instance> instances = scene.allocate_meshes(100);
    for (u32 index = 0; index < instances.size(); ++index) {
        instances[index].mesh = mesh;
        instances[index].materials[0] = material;
        const math::vec3 position(static_cast<f32>(index % 10) - 5.0f, 0.0f, -static_cast<f32>(index / 10));
        instances[index].worldFromLocal = glm::translate(math::mat4(1.0f), position);
        instances[index].worldBounds = math::aabb{position - math::vec3(0.5f), position + math::vec3(0.5f)};
    }
    renderer::light sun = {};
    sun.type = renderer::light_type::DIRECTIONAL;
    sun.direction = glm::normalize(math::vec3(0.3f, -1.0f, 0.2f));
    sun.castsShadows = true;
    scene.lights.push_back(sun);
    frame.end_extract();
}
```

The engine calls `render_frame()` after extraction; the pair of scenes inside the renderer means the frame being
rendered and the frame being extracted never share a buffer.

## Creating the renderer

`renderer_desc` names the `rhi::device`, the `rhi::shader_library` (the index `conjure_add_shaders` built), an
optional `rhi::swapchain` (null = headless, every view must name a target), an optional `core::jobs::job_system`
(culling and draw-list building run on it), the cascade and atlas resolutions, and `enableImGui`. The atlas
resolution is clamped to [256, 16384] and rounded down to a power of two, with a WARN when that changes it (3000
becomes 2048); `desc()` still reports the value you passed.
`renderer::create` builds every pipeline, binding layout, fallback resource and the BRDF lookup table up front;
it fails with `SHADER_NOT_FOUND` (naming the shader key), `PIPELINE_CREATION_FAILED`, `RESOURCE_CREATION_FAILED`
or `INVALID_ARGUMENT`. Nothing is created mid-frame afterwards.

## Uploads and GPU ids

`upload_mesh(assetId, mesh)` and friends copy the payload bytes immediately, reserve a slot and return a
`gpu_mesh_id` (`gpu_texture_id`, `gpu_material_id`, `gpu_environment_id`) that is valid at once; until the upload
runs at the start of a later frame the id draws the fallback. Uploading under an asset id already known replaces
the GPU mirror and keeps the id. `release_*` makes the id stale now and frees the GPU objects a few frames later.
A stale id draws the fallback and WARNs once. Uploads are budgeted (`UPLOAD_BUDGET_BYTES_PER_FRAME`, 64 MB per
frame); `render_stats::uploadsPending` says how many wait.

> [!TIP]
> Upload textures before the materials that use them: a material resolves its textures by asset id at upload
> time (`find_texture`), and a texture that arrives later is picked up at the next flush.

## Describing a frame

| `render_scene` member | Meaning |
|---|---|
| `views` | up to `MAX_VIEWS` `render_view`s: `cameraData` (`camera`), `viewFromWorld`, `cameraPosition`, `viewport` (empty = whole target), `target` (invalid = swapchain), `flags` (`DRAW_DEBUG`, `NO_POST_PROCESS`, `SKIP_SHADOWS`), `layerMask` |
| `meshes` | `mesh_instance`: `mesh`, `materials[MAX_MATERIAL_SLOTS]`, `worldFromLocal`, `worldBounds` (used for culling: fill it), `entityId`, `layerMask`, `castsShadows`, `receivesShadows`; `allocate_meshes(count)` sizes the array once so jobs can fill ranges |
| `lights` | `light`: `type` (`DIRECTIONAL`, `POINT`, `SPOT`), `position`, `direction`, `colour`, `intensity` (lux or candela), `range`, cone angles, `sourceRadius`, `castsShadows`, `filter`, `shadowResolution` (256, 512 or 1024) |
| `sprites` | `sprite` values in `WORLD` or `SCREEN` space; sorted and batched by the renderer |
| `environment` | `environment_settings`: the environment id, intensity and rotation |
| `postProcess`, `shadows`, `ssao` | `post_process_settings` (`exposure_mode::AUTOMATIC` or `MANUAL`, EV limits, bloom, `tonemap_operator::ACES_FITTED`, `REINHARD_EXTENDED`, `AGX` or `NONE`), `shadow_settings` (cascades, filtering), `ssao_settings` |
| `debugDraw` | a `debug_draw_buffer` of lines for this frame |
| `frameIndex`, `deltaSeconds` | timing for exposure adaptation |

`camera` is projection only (`PERSPECTIVE` with `verticalFovRadians` or `ORTHOGRAPHIC` with
`orthographicHeight`, near and far planes); the view's `viewFromWorld` matrix places it. `clip_from_view(camera,
aspect)` builds the reverse-Z projection, `frustum_from_clip` the culling planes and `ray_from_viewport` a picking
ray from a pixel.

Everything beyond a budget is clamped and logged, never fatal: views past `MAX_VIEWS` are skipped
(`render_stats::viewsSkipped`), lights past `MAX_LIGHTS_PER_VIEW` dropped (`lightsDropped`), sprites past
`MAX_SPRITES_PER_FRAME` and debug vertices past `MAX_DEBUG_VERTICES` counted as dropped. A view whose `camera`
`clip_from_view` would reject (near not above zero, far not beyond near, a field of view outside (0, pi), an
orthographic height not above zero, or a non-finite value) renders with those fields clamped or reset to the
defaults, with one WARN per view until its camera is valid again.

A view draws an instance when the two layer masks share a bit (`instance.layerMask & view.layerMask`). The sun's
shadow cascades are culled with each view's own mask. The shadows of spot and point lights live in one atlas
shared by every view of the frame, so its casters are culled with the union of the masks of every view that
draws with shadows (every view without `SKIP_SHADOWS`).

> [!NOTE]
> Every view sees the local-light shadows of its own casters, whatever the view order. A view may also see the
> shadows of casters on layers that only another view renders.

## Targets and read-back

`create_render_target(render_target_desc {size, colourFormat, withDepth})` returns a `render_target_id`;
`get_render_target(id)` gives `size()`, `colour_texture()` (a `gpu_texture_id` you may draw as a sprite or in
ImGui) and `frames_rendered()`; `resize_render_target(id, size)` recreates its textures and keeps both the target
id and the `colour_texture()` id, so a sprite or an ImGui handle that shows the target keeps working after the
resize; `destroy_render_target(id)` frees it. `read_back(id)` waits for the GPU and returns a `core::image`
(RGBA8); on the null device the image is zero-filled and the right size. It fails with `NOT_FOUND` for an unknown
target, `rhi_error::DEVICE_LOST` on a lost device and `RESOURCE_CREATION_FAILED` when the staging texture cannot be
created or mapped, never with a blank image. `is_render_target_colour_format(format)` tells you which
`rhi::texture_format`s a target accepts.

## Sprites and debug drawing

A `sprite` has a texture id, a `worldFromLocal` matrix (pixel space, y down, for `SCREEN` sprites), a UV
rectangle, a colour, a `size` (world units or pixels), a `sprite_space` and a layer / order pair; the renderer
sorts world-space sprites with the 3D scene and screen-space ones on top.
`sprite_batcher` is the CPU batching the renderer uses, exposed for tools that want the vertex runs. Past
`MAX_SPRITES_PER_FRAME`, screen-space sprites (the HUD) keep their place: the world-space sprites last in sort
order, the front-most ones, are dropped first, with one WARN naming the count per space.

`debug_draw_buffer` collects lines (`line`, `box`, `sphere`, `frustum`, `axes`, `grid`); each shape takes a
`debug_depth` after its colour (after `length` for `axes`; `sphere` takes its segment count after it):
`debug_depth::TESTED` (the default, hidden behind geometry) or `debug_depth::OVERLAY` (drawn on top).
`render_scene::debugDraw` is one. `debug_draw` keeps shapes alive for a duration: `line(...,
durationSeconds, depth)`, `tick(deltaSeconds)` and `append_to(buffer)` each frame. `debug_depth` is a typed
argument, so it can never be passed where the facade expects the duration. Vertices dropped at the
`MAX_DEBUG_VERTICES` cap are counted by `dropped_count()` and carried along by `append` and `append_to`.

```cpp
scene.debugDraw.box(bounds, math::vec4(1.0f, 0.0f, 0.0f, 1.0f), renderer::debug_depth::OVERLAY);
debugDraw.line(from, to, math::vec4(0.0f, 1.0f, 0.0f, 1.0f), 2.0f);   // depth-tested, kept for two seconds
```

World-space sprites and debug lines draw into the HDR target before exposure, bloom and the tonemapper, so they
are post-processed like the 3D scene: a debug colour scales with the exposure, and a bright sprite can bloom.
Screen-space sprites draw after the tonemapper and keep their authored colours.

> [!TIP]
> A 2D game whose world sprites should keep their authored colours sets `camera_component::postProcess` to
> `false` (a hand-built view sets `view_flags::NO_POST_PROCESS`). The view then skips exposure and bloom and
> tonemaps with `NONE`.

## The null device and statistics

`render_stats` is filled on every device: `viewsRendered`, `passesExecuted` and `passNames` (in order),
`drawCalls`, `instancesDrawn`, `instancesCulled`, `trianglesDrawn`, `lightsRendered`, `shadowCascadesRendered`,
`shadowTilesAllocated`, `spritesDrawn`, `uploadsCompleted`, `fallbacksUsed`, `transientTexturesLive` and more;
`executed("tonemap")` and `count(name)` query the pass list, which holds every executed pass of the largest frame
(`MAX_VIEWS` views of `MAX_PASSES_PER_VIEW` passes each). On the null device the `rhi::device::null_stats()`
counters say what was recorded (draws, dispatches, buffers and textures created, command lists submitted), which
is how every renderer test asserts without a GPU.

> [!NOTE]
> `render()` fails only with `rhi_error::DEVICE_LOST`; after `on_device_lost()` every later frame fails the same
> way and the engine quits. Everything else about a bad scene is clamped and logged.

## Components

`components.h` declares the reflected structs the engine registers as ECS components and fills the scene from:
`mesh_renderer_component {asset_ref<mesh_asset> mesh; asset_ref<material_asset> materials[MAX_MATERIAL_SLOTS];
layerMask; castsShadows; receivesShadows}`, `sprite_component`, `camera_component {camera cameraData; layerMask;
priority; isMain; postProcess}`, `light_component {light lightData}` and `environment_component`.
`register_components(core::reflect::type_registry &)` registers their descriptions.

The engine copies `mesh_renderer_component::layerMask` into each `mesh_instance` and `camera_component::layerMask`
into each view, so those two masks alone decide which camera draws which mesh. The scene's `layer` component is a
grouping label for `find_by_layer`, scripts and the editor and plays no part (its 32 layers use the same bit
numbering, so a game can keep them in step). Sprites carry no mask: every view draws every sprite, and
`sprite_component::layer` is a sort key, not a visibility layer.

## ImGui (editor builds)

`imgui_renderer` draws `ImDrawData` into a framebuffer: the engine's `imgui_layer` owns the ImGui context and
hands the draw data to `renderer::set_imgui_draw_data(drawData)`, which draws it after the views of the next
frame into the swapchain. `renderer::imgui()` returns the backend (null in Distribution, without a swapchain or
when disabled); `register_texture(gpuTextureId)` returns the `ImTextureID` to show an engine texture in a panel.

## API summary

| Signature (short) | Purpose |
|---|---|
| `static result<unique_ptr<renderer>> renderer::create(const renderer_desc &)` | construction |
| `gpu_mesh_id upload_mesh(const asset_id &, const mesh_asset &)` (and texture, material, environment) | GPU mirrors; any thread |
| `void release_mesh(gpu_mesh_id)` (and texture, material, environment) | drop a mirror; any thread |
| `gpu_mesh_id find_mesh(const asset_id &) const` (and texture, material, environment) | id lookup |
| `result<render_target_id> create_render_target(const render_target_desc &)`, `result<void> resize_render_target(id, uvec2)`, `void destroy_render_target(id)`, `render_target *get_render_target(id)` | targets |
| `render_scene &begin_extract()`, `void end_extract()`, `result<void> render_frame()` | the engine's frame |
| `void submit_view(const render_view &)`, `u32 submitted_view_count() const` | queue a view no scene camera stands for: the next `end_extract()` appends it to the extracted scene, for one frame per call; it renders after the scene's own views and before the ImGui pass, so a panel can show its target in the same frame |
| `result<void> render(const render_scene &)` | render one scene now |
| `result<void> resize_swapchain(math::uvec2 size)` | resize the swapchain the renderer presents to: it drops its framebuffers over the back buffers first, which `swapchain::resize` alone cannot do |
| `void on_device_lost()`, `bool is_lost() const` | device events |
| `void set_imgui_draw_data(ImDrawData *)`, `imgui_renderer *imgui()` | editor UI |
| `result<core::image> read_back(render_target_id)` | pixels of a target |
| `const render_stats &stats() const`, `u64 frame_count() const`, `u32 pending_upload_count() const` | statistics |
| `static result<void> register_asset_types(asset_type_registry &, importer_registry &, job_system * = nullptr)` | the renderer's asset types (see [Renderer assets](renderer-assets.md)) |
| `mat4 clip_from_view(const camera &, f32 aspectRatio)`, `frustum frustum_from_clip(const mat4 &)`, `ray ray_from_viewport(...)` | `camera.h` |
| `u16 clamp_shadow_resolution(u16)` | `light.h` |
| `result<void> register_components(core::reflect::type_registry &)` | `components.h` |
| `render_view make_camera_view(const camera_component &, const mat4 &worldFromCamera, render_target_id = {})` | `components.h`: the view a camera component at a world matrix renders (rigid inverse of the matrix, the camera's data, layer mask and `NO_POST_PROCESS` unless `postProcess`); what the engine's extraction and the editor's screenshots both use |

## See also

- 📘 API: [Renderer assets](renderer-assets.md), [Devices and swapchains](../rhi/devices-and-swapchains.md), [Shaders](../rhi/shaders.md)
- 🛠 Handbook: [Renderer module](../../handbook/modules/renderer.md)
