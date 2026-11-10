# Renderer module

🛠 **Engineering handbook** · 📘 API: [Rendering](../../api/renderer/index.md), [Rendering a frame](../../api/renderer/rendering-a-frame.md), [Renderer assets](../../api/renderer/renderer-assets.md)

`conjure::renderer` (target `conjure_renderer`, alias `conjure::renderer`) is the wave-3 module that turns a
`render_scene` into one command list per frame through a render graph over `conjure::rhi`. It owns the GPU
mirrors of its asset types, the off-screen render targets and, in editor builds, the Dear ImGui backend. This
chapter is for people changing the module or wiring it into the engine: the frame, the graph, the resource cache,
threading, the deviations from the design and the tests. The binding design is
[`02_renderer.md`](../../design/02_renderer.md); the cross-area decisions are R7-R12, R34 and R43 of
[`09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md); `engine/src/renderer/README.md`
carries the finer notes.

## Contents

- [1. Shape of the module](#1-shape-of-the-module)
- [2. The frame](#2-the-frame)
- [3. Resource cache and uploads](#3-resource-cache-and-uploads)
- [4. Assets and importers](#4-assets-and-importers)
- [5. Threading model](#5-threading-model)
- [6. Deviations from the design](#6-deviations-from-the-design)
- [7. Failure model](#7-failure-model)
- [8. Extension points and engine wiring](#8-extension-points-and-engine-wiring)
- [9. Tests](#9-tests)
- [10. Open items](#10-open-items)
- [Related](#related)

## 1. Shape of the module

`engine/src/renderer/CMakeLists.txt`: `conjure_add_module(renderer ... PUBLIC_DEPS conjure::core conjure::rhi
conjure::assets PRIVATE_DEPS nvrhi cgltf stb [imgui])`, guarded by `CONJURE_WITH_RENDERER` and the presence of
`conjure::rhi` and `conjure::assets`. Never `ecs` or `scene` (R7): the components in `components.h` are plain
reflected structs the engine's `renderer_subsystem` registers and extracts from. `imgui/imgui_renderer.cpp` is
compiled only with `CONJURE_BUILD_EDITOR` (never in Distribution). `conjure_renderer` depends on `conjure_shaders`
so `index.json` exists before the renderer is used.

| Area | Public headers | Private sources |
|---|---|---|
| Budgets, errors, logging | `limits.h`, `renderer_error.h`, `log_category.h` | `renderer_error.cpp`, `log_category.cpp` |
| Handles | `gpu_ids.h` | `gpu_resource_cache.*` (slot maps, fallbacks, graveyard) |
| Extraction contract | `render_scene.h`, `camera.h`, `light.h`, `render_settings.h` | one `.cpp` each |
| 2D and debug | `sprite_batch.h`, `debug_draw.h` | `sprite_batcher.cpp`, `debug_draw.cpp`, `passes/sprite_pass.cpp`, `passes/debug_draw_pass.cpp` |
| Assets | `mesh_asset.h`, `texture_asset.h`, `material_asset.h`, `model_asset.h`, `environment_asset.h`, `shader_asset.h`, `gltf_importer.h` | `assets/*.cpp`, `import/*` (tangents, mips, image decoding, IBL bake, cgltf) |
| Components | `components.h` | `components.cpp` |
| The frame | `renderer.h`, `render_target.h` | `renderer.cpp`, `renderer_impl.h`, `render_graph.*`, `transient_pool.*`, `framebuffer_cache.*`, `binding_layouts.*`, `pipeline_cache.*`, `frame_resources.*`, `timer_ring.*`, `gpu_types.*`, `culling.*`, `draw_list_builder.*`, `exposure_math.*`, `shadows/*`, `passes/*` |
| ImGui | `imgui_renderer.h` | `imgui/imgui_renderer.cpp` |

Shaders live in `engine/shaders/` and are compiled by `conjure_add_shaders` (the permutation list in
`engine/shaders/CMakeLists.txt`) into `<build>/shaders/index.json`; `shader_asset::built_in_programs()` lists the
same permutations and `pipeline_cache` turns each into a pipeline at `renderer::create`. The structs in
`shared/gpu_types.hlsli` are mirrored in `gpu_types.h` with static-asserted sizes; the tests re-read the HLSL
defines to keep the two in step.

## 2. The frame

`renderer::render(scene)` does, in order:

1. `begin_frame`: acquire the back buffer (when a swapchain exists), open the frame command list, flush the queued
   asset uploads under the 64 MB budget, collect the graveyard.
2. `prepare`: build sprites and debug lines once, allocate the local-light shadow atlas scene-wide (importance
   measured from view 0), then per view: cull on jobs, build instanced draw lists (opaque / masked sorted by
   material and mesh, blended far to near), sort the lights (directional first), build the cascades of the first
   shadowing sun, fill the view constants.
3. `render_view_graph`: one render graph per view, passes added only when they have work: `shadow_cascade_0..3`,
   `shadow_atlas`, `depth_prepass`, `light_cull`, `ssao_gtao`, `ssao_blur_h/v`, `forward_opaque`, `skybox`,
   `forward_blend`, `world_sprites`, `debug_draw`, `exposure_histogram`, `exposure_adapt`, `bloom_down_0..5`,
   `bloom_up_4..0`, `tonemap`, `screen_sprites` and `blit` (only when the target is not `RGBA8_SRGB`). The graph
   validates (a read before write asserts in Debug, logs an ERROR and skips the pass), culls backwards, computes
   lifetimes and takes its transients from the pool (no aliasing, NVRHI automatic barriers).
4. ImGui into the swapchain (editor builds).
5. `end_frame`: close, execute, present, evict idle transients and binding sets, fill `render_stats`.

Conventions: reverse-Z everywhere (near = 1, far = 0, `GreaterOrEqual`), classic NVRHI binding sets with
`registerSpaceIsDescriptorSet`, a volatile constant buffer at `b0` and 16 bytes of push constants at `b1` in
space 0, the material set in space 1, matrices uploaded verbatim from glm and consumed with `mul(vector, matrix)`
under `-Zpr`.

## 3. Resource cache and uploads

`gpu_resource_cache` keeps one slot map per mirror type plus the fallbacks (unit cube, white texture, magenta
unlit material, grey environment) and a graveyard. `upload_*` copies the payload at enqueue time (R10), reserves
the slot under a mutex and returns the id; the GPU objects are created in `flush_uploads` at the start of the
next frame, never elsewhere. Releasing a texture only marks the materials that used it; the next flush rebinds
them. An entry released during a frame is retired (its GPU handles get an extra graveyard reference) and freed at
the next `collect()`, so every reference the frame resolved stays valid until the frame is submitted; the mirrors
live in heap entries whose addresses survive table growth, and `resolve_*` lock per call.

## 4. Assets and importers

The five asset types (`mesh`, `texture`, `material`, `model`, `environment`) have CPU-only loaders (R10: no device
in `finalise`) and four importers (glTF with cgltf, textures with stb, `.cmat` JSON, `.hdr` with the CPU IBL
bake of R34). `register_asset_types(types, importers, jobs)` registers them; the engine's `renderer_subsystem`
subscribes to `asset_manager` events and calls `upload_*` on LOADED / RELOADED and `release_*` on UNLOADED.
The glTF adapter makes the model the primary asset with meshes, materials and textures as sub-assets with stable
uuids and declared dependencies, so a blocking load of the model brings everything
(`tests/integration/test_assets_renderer_headless.cpp` proves the whole path on the null device).

## 5. Threading model

`create`, `render`, `render_frame`, `begin_extract` / `end_extract`, `find_*`, render targets and `read_back`
belong to the render thread (the main thread in v1). `upload_*` and `release_*` may be called from any thread.
Culling and draw-list building run on `job_system::parallel_for` when a job system is given; the render thread
helps with queued jobs while it waits, so a job may call `upload_*` / `release_*` in the middle of `render()`,
which is why the cache never hands its mutex to the frame. No NVRHI object is created outside `flush_uploads`.

## 6. Deviations from the design

- `draw_list_builder` packs the opaque key as `[variant:8][material:22][mesh:22][submesh:4][depth:8]` (the
  design's 24-bit material and mesh fields leave no room for the submesh) and asserts the ranges; the blended key
  carries a 24-bit depth.
- PCSS estimates its penumbra in world units; search and filter radii are clamped to 8 texels. The shadow raster
  bias is negated for reverse-Z. Shadow comparison samplers use `Less` (the design's `GreaterOrEqual` inverted by
  the reverse-Z linearisation).
- `light::entityId` (not in the design's struct, never reflected) keeps a local light's atlas tile stable.
- The AO upsample is a 4-tap bilateral filter over the half-resolution AO and a half-resolution view depth
  (`ao_depth_half`, `t9` of the mesh view set).
- The exposure histogram meters the view's viewport rectangle only.
- `camera::ray_from_viewport` takes the aspect ratio the caller measured; `alpha_mode::SOLID` names glTF's
  opaque mode (`OPAQUE` is a Windows macro).

## 7. Failure model

`renderer::create` fails at start-up with `SHADER_NOT_FOUND` (naming the shader key), `PIPELINE_CREATION_FAILED`
or `RESOURCE_CREATION_FAILED`; nothing is created mid-frame except a blit pipeline the first time a new target
format appears (outside frames). `render` fails only with `rhi_error::DEVICE_LOST`; everything else about a scene
is clamped and logged (views beyond `MAX_VIEWS`, lights beyond `MAX_LIGHTS_PER_VIEW`, sprite and debug caps,
atlas overflow). A stale gpu id draws the fallback and WARNs once.

## 8. Extension points and engine wiring

The wave-4 `renderer_subsystem` has everything it needs: `renderer::register_asset_types(types, importers,
jobs)` for the asset manager, `components.h` `register_components(type_registry)` for the ECS registration (the
`CONJURE_COMPONENT` specialisations live in the subsystem), `begin_extract` / `end_extract` / `render_frame`
around the EXTRACT phase, `set_imgui_draw_data` / `imgui()` for `imgui_layer`, and `on_swapchain_resized` /
`on_device_lost` for the window and device events. `model_asset -> prefab` is the engine's job over
`scene::prefab_builder` (`add_node`, `add_component_json` by registered name).

## 9. Tests

`tests/renderer/` holds one file per public header plus the topic files of the design (`culling`,
`draw_list_builder`, `cascade_builder`, `shadow_atlas_allocator`, `render_graph`, `transient_pool`,
`gpu_types`, `tonemap_reference`, `exposure_math`, `pipeline_cache`, `gpu_resource_cache`,
`renderer_null_frame`, `renderer_failures`, `renderer_gpu`). Everything runs on the null device; the `[gpu]`
cases use WARP and skip without `precondition::SOFTWARE_GPU_ADAPTER`. Test glTF, PNG and HDR inputs are
generated in the tests (no binary assets). `tests/integration/test_assets_renderer_headless.cpp` covers the
asset pipeline into the renderer.

```
python tools/build.py --build-dir build/mod_renderer --target conjure_renderer_tests --test -R renderer
```

## 10. Open items

- Dear ImGui is pinned at 1.91.9b-docking; the `ImTextureData` texture protocol path of `imgui_renderer`
  (`IMGUI_HAS_TEXTURES`) is written but only compiles against >= 1.92, which `ARCHITECTURE.md` 9 names as the
  target version.
- `tests/benchmarks/bench_render_null_frame.cpp` (< 3 ms for the 10k-instance frame) waits for the benchmarks
  directory.
- The real-device image of the `feature_test` early scene is a local check until the golden pipeline (wave 5)
  exists.

## Related

- design record: [`02_renderer.md`](../../design/02_renderer.md), [`09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md)
- 📘 API: [Rendering](../../api/renderer/index.md)
- 🛠 Handbook: [RHI module](rhi.md), [Assets module](assets.md), [Build pipeline](../build-pipeline.md)
