# Rendering

📘 **Public API wiki** · 🛠 Handbook: [Renderer module](../../handbook/modules/renderer.md)

The renderer draws a frame: physically based 3D with image-based lighting, cascaded and local shadows, ambient
occlusion and HDR post-processing, plus 2D sprites and debug lines, into the window or into off-screen targets. You
feed it a `render_scene` (cameras, mesh instances, lights, sprites, settings) and it produces one command list
per frame on the graphics device. The CPU-side asset types (meshes, textures, materials, models, environments)
and their importers live here too. Everything on these pages is in `namespace conjure::renderer`.

## The mental model in five sentences

1. Assets are CPU data: a `mesh_asset`, `texture_asset`, `material_asset` or `environment_asset` is a plain
   object the asset pipeline cooks and loads with no GPU in sight. `renderer::upload_*` copies one into GPU
   memory and hands back a small id (`gpu_mesh_id`, ...); `release_*` drops it again.
2. A `render_scene` is what one frame looks like: `views` (a camera, a view matrix and a target), `meshes`
   (`mesh_instance`: a mesh id, material ids and a world matrix), `lights`, `sprites`, the environment and the
   post-processing settings. It holds ids and numbers, never pointers into the world.
3. The frame loop asks the renderer for a scene to fill (`begin_extract`), fills it from the ECS, hands it back
   (`end_extract`) and calls `render_frame()`. Tests call `render(scene)` on a scene they built by hand.
4. Views render into the swapchain or into a `render_target` you created; `read_back` fetches a target's pixels
   for screenshots and golden images.
5. Nothing needs a GPU to be tested: the null device records what was drawn, and `render_stats` says which passes
   ran and how many instances, lights and uploads were involved.

The ECS components (`mesh_renderer_component`, `camera_component`, `light_component`, `sprite_component`,
`environment_component`) are plain reflected structs; the engine registers them and fills the `render_scene`
from them every frame. The renderer itself never includes the ECS.

## Where to go next

| Page | Read it when you want to... |
|---|---|
| [Rendering a frame](rendering-a-frame.md) | create the renderer, describe cameras, lights, instances, sprites and debug lines, render into targets, read pixels back, use the null device in tests, tune post-processing, shadows and ambient occlusion |
| [Renderer assets](renderer-assets.md) | build or import meshes, textures, materials, models and environments, understand the glTF importer, the cooked formats and the shader programs the renderer loads |
| [JPEG XL images](jpeg-xl.md) | import `.jxl` textures and skies, know what colour profiles, bit depths, orientation and animation turn into, write JPEG XL files |

## Headers

| Header | What it holds |
|---|---|
| `conjure/renderer/renderer.h` | `renderer`, `renderer_desc`, `render_stats`, `pass_timing` |
| `conjure/renderer/render_scene.h` | `render_scene`, `render_view`, `mesh_instance`, `view_flags`, `render_scene_pair` |
| `conjure/renderer/camera.h` | `camera`, `projection_type`, `clip_from_view()`, `frustum_from_clip()`, `ray_from_viewport()` |
| `conjure/renderer/light.h` | `light`, `light_type`, `shadow_filter`, `clamp_shadow_resolution()` |
| `conjure/renderer/render_settings.h` | `post_process_settings`, `shadow_settings`, `ssao_settings`, `environment_settings`, `tonemap_operator`, `exposure_mode` |
| `conjure/renderer/render_target.h` | `render_target_desc`, `render_target`, `is_render_target_colour_format()` |
| `conjure/renderer/sprite_batch.h` | `sprite`, `sprite_space`, `sprite_batcher` |
| `conjure/renderer/debug_draw.h` | `debug_draw_buffer`, `debug_draw` (timed shapes), `debug_depth` (TESTED / OVERLAY), `debug_vertex`, `pack_debug_colour()` |
| `conjure/renderer/gpu_ids.h` | `gpu_mesh_id`, `gpu_texture_id`, `gpu_material_id`, `gpu_environment_id`, `render_target_id` |
| `conjure/renderer/limits.h` | the budgets: `MAX_VIEWS`, `MAX_LIGHTS_PER_VIEW`, `MAX_SHADOW_CASCADES`, `MAX_SPRITES_PER_FRAME`, `MAX_MATERIAL_SLOTS`, ... |
| `conjure/renderer/components.h` | the five reflected components and `register_components()` |
| `conjure/renderer/imgui_renderer.h` | `imgui_renderer` (editor builds: draws `ImDrawData`) |
| `conjure/renderer/mesh_asset.h` | `mesh_asset`, `mesh_geometry`, `tangent_generation`, `submesh`, the vertex packing helpers, `mesh_loader` |
| `conjure/renderer/texture_asset.h` | `texture_asset`, `texture_import_settings`, `texture_importer`, `texture_loader` |
| `conjure/renderer/jpeg_xl.h` | `encode_jpeg_xl()`, `jpeg_xl_encode_options`, `is_jpeg_xl()` |
| `conjure/renderer/material_asset.h` | `material_asset`, `material_desc`, `alpha_mode`, `material_importer`, `material_loader` |
| `conjure/renderer/model_asset.h` | `model_asset`, `model_node`, `model_loader` |
| `conjure/renderer/environment_asset.h` | `environment_asset`, `cubemap_data`, `equirect_image`, `bake_environment()`, `environment_importer`, `environment_loader` |
| `conjure/renderer/shader_asset.h` | `shader_asset`, `shader_program_desc`, the built-in program list |
| `conjure/renderer/gltf_importer.h` | `gltf_importer`, `gltf_import_options`, `gltf_import_result`, `gltf_asset_importer` |

## Quick tour

```cpp
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/renderer.h"

using namespace conjure;

core::result<void> draw_one_cube(rhi::device &device, rhi::shader_library &shaders) {
    renderer::renderer_desc desc = {};
    desc.device = &device;
    desc.shaders = &shaders;                      // swapchain = nullptr: headless, views need targets
    CONJURE_TRY_ASSIGN(std::unique_ptr<renderer::renderer> frame, renderer::renderer::create(desc));

    const renderer::mesh_asset cube = renderer::mesh_asset::unit_cube();
    const renderer::gpu_mesh_id mesh = frame->upload_mesh(assets::asset_id(1, 1), cube);
    renderer::render_target_desc targetDesc = {};
    targetDesc.size = math::uvec2(320, 180);
    CONJURE_TRY_ASSIGN(const renderer::render_target_id target, frame->create_render_target(targetDesc));

    renderer::render_scene scene = {};
    renderer::render_view view = {};
    view.cameraPosition = math::vec3(0.0f, 1.0f, 4.0f);
    view.viewFromWorld = math::look_at(view.cameraPosition, math::vec3(0.0f), math::vec3(0.0f, 1.0f, 0.0f));
    view.target = target;
    scene.views.push_back(view);
    renderer::mesh_instance instance = {};
    instance.mesh = mesh;                         // no material: the magenta fallback draws it
    instance.worldBounds = cube.bounds();
    scene.meshes.push_back(instance);
    CONJURE_TRY(frame->render(scene));
    return core::result<void>::success();         // frame->stats().drawCalls > 0
}
```

> [!WARNING]
> The renderer, its targets and `render()` belong to the main thread (there is no render thread in v1). Only
> `upload_*` and `release_*` may be called from any thread; they copy the payload at the call and the GPU work
> happens at the start of the next frame.

## Local terms

| Term | Meaning |
|---|---|
| View | one camera rendering into one target (or the swapchain), with its own layer mask and flags |
| Instance | one drawn mesh: `mesh_instance` with a mesh id, up to `MAX_MATERIAL_SLOTS` material ids and a world matrix |
| Fallback | what a stale or missing id draws: a unit cube, a white texture, a magenta unlit material, a grey environment |
| Pass | one step of the frame's render graph (`depth_prepass`, `forward_opaque`, `tonemap`, ...); `render_stats::passNames` lists the ones that ran |
| Reverse-Z | depth 1 at the near plane and 0 at the far plane, for precision; `camera.h` builds such projections |

## See also

- 📘 API: [Graphics device layer (RHI)](../rhi/index.md), [Referencing and loading assets](../assets/referencing-and-loading.md), [Math](../core/math.md)
- 🛠 Handbook: [Renderer module](../../handbook/modules/renderer.md)
