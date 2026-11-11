# renderer

`conjure::renderer` (target `conjure_renderer`, namespace `conjure::renderer`) turns a `render_scene`
into one command list per frame through a render graph over `conjure::rhi`. It owns the GPU mirrors
of its asset types, the off-screen render targets and, in editor builds, the Dear ImGui backend.
Design: `docs/design/02_renderer.md`; cross-area decisions: `docs/design/09_cross_area_resolutions.md`
(R7-R12, R34, R43); the big picture: `docs/ARCHITECTURE.md` 5 and 9.

## Dependencies

Public: `conjure::core`, `conjure::rhi`, `conjure::assets`. Never `ecs` or `scene` (R7): the
components in `components.h` are plain reflected structs the engine registers.
Private: NVRHI (through `rhi::device::native_device()`), cgltf, stb (image and HDR decoding) and
Dear ImGui (only with `CONJURE_BUILD_EDITOR`, never in Distribution).

## Layout

| Area | Public header (`engine/include/conjure/renderer/`) | Private source (`engine/src/renderer/`) |
| --- | --- | --- |
| Budgets, errors, logging | `limits.h`, `renderer_error.h`, `log_category.h` | `renderer_error.cpp`, `log_category.cpp` |
| Handles | `gpu_ids.h` | `gpu_resource_cache.*` (slot maps, fallbacks, graveyard) |
| Extraction contract | `render_scene.h`, `camera.h`, `light.h`, `render_settings.h` | `render_scene.cpp`, `camera.cpp`, `light.cpp`, `render_settings.cpp` |
| 2D and debug | `sprite_batch.h`, `debug_draw.h` | `sprite_batcher.cpp`, `debug_draw.cpp`, `passes/sprite_pass.cpp`, `passes/debug_draw_pass.cpp` |
| Assets | `mesh_asset.h`, `texture_asset.h`, `material_asset.h`, `model_asset.h`, `environment_asset.h`, `shader_asset.h`, `gltf_importer.h` | `assets/*.cpp`, `import/*` (tangents, mips, image decoding, IBL bake, cgltf) |
| Components | `components.h` | `components.cpp` |
| The frame | `renderer.h`, `render_target.h` | `renderer.cpp`, `renderer_impl.h`, `render_graph.*`, `transient_pool.*`, `framebuffer_cache.*`, `binding_layouts.*`, `pipeline_cache.*`, `frame_resources.*`, `timer_ring.*`, `gpu_types.*`, `culling.*`, `draw_list_builder.*`, `exposure_math.*`, `shadows/*`, `passes/*` |
| ImGui | `imgui_renderer.h` | `imgui/imgui_renderer.cpp` |

Shaders live in `engine/shaders/` and are compiled by `conjure_add_shaders` into
`<build>/shaders/index.json`; `shader_asset::built_in_programs()` lists every permutation the
renderer loads and `pipeline_cache` turns each into a pipeline at `renderer::create`. The structs in
`shared/gpu_types.hlsli` are mirrored in `gpu_types.h` with static_asserted sizes; the tests re-read
the HLSL defines to keep the two in step.

## The frame

`renderer::render(scene)` (render thread) does, in order:

1. `begin_frame`: acquire the back buffer (when a swapchain exists), open the frame command list,
   flush the queued asset uploads under the 64 MB budget, collect the graveyard.
2. `prepare`: build sprites and debug lines once, allocate the local-light shadow atlas scene-wide
   (importance measured from view 0), then per view: cull on jobs, build instanced draw lists (opaque
   / masked sorted by material and mesh, blended far to near), sort the lights (directional first),
   build the cascades of the first shadowing sun, fill the view constants.
3. `render_view_graph`: one render graph per view, passes added only when they have work:
   `shadow_cascade_0..3`, `shadow_atlas`, `depth_prepass`, `light_cull`, `ssao_gtao`, `ssao_blur_h/v`,
   `forward_opaque`, `skybox`, `forward_blend`, `world_sprites`, `debug_draw`, `exposure_histogram`,
   `exposure_adapt`, `bloom_down_0..5`, `bloom_up_4..0`, `tonemap`, `screen_sprites` and `blit` (only
   when the target is not `RGBA8_SRGB`). The graph validates (read before write asserts in Debug,
   logs an ERROR and skips the pass), culls backwards, computes lifetimes and takes its transients
   from the pool (no aliasing, NVRHI automatic barriers).
4. ImGui into the swapchain (editor builds).
5. `end_frame`: close, execute, present, evict idle transients and binding sets, fill `render_stats`.

### Steady-state frames allocate nothing

`08_testing.md` 7.1 gates `renderer.extract_10k` and `renderer.null_frame_10k` at zero allocations after
warm-up, and `tests/renderer/test_renderer_null_frame.cpp` asserts the same for frames 3..10 of a 10k
scene (sun, 63 local lights with four shadow casters, environment, every post-process pass) both with
and without a job system, and for extraction plus culling alone. Everything a frame touches is
therefore owned by the renderer and reset in place:

- `renderer_state::views` is a pool of `frame_view_state` objects (`next_view_state()` resets one,
  `prepared_views()` is this frame's prefix); the draw regions of the main view, the cascades and the
  atlas tile lists (`push_atlas_tile()` / `atlas_tiles()`) keep their capacity.
- `cull_instances` takes a `cull_scratch` (one per renderer, `renderer_state::cullScratch`): the
  job-parallel path writes each chunk's indices into its own slice of a buffer sized once for the
  instance count, no per-call chunk lists. The overload without a scratch still exists for callers off
  the hot path; it decides the path first and builds its scratch only when it runs on jobs, so a serial
  cull through it touches nothing but the output (an empty `std::vector` is a heap block under the MSVC
  Debug runtime).
- `render_graph::reset()` keeps the pass entries (their read / write / target vectors keep capacity)
  and `compile` works in member scratch; `pass_shared::binding_desc()` hands every pass the one
  `nvrhi::BindingSetDesc` (NVRHI's holds a `std::vector`) to fill before it looks the set up in the cache.
- `shadow_atlas_allocator` keeps its all-or-nothing node backup and the settled flags as members.
- `timer_ring` creates a query for all three slots at once the first time a frame needs one more than
  any frame before it (never per slot on its first use, which would allocate in frames 1..3), so a
  steady pass count creates nothing after its first frame and an idle ring holds no query: NVRHI's
  default timer-query budget is 256 per device and every renderer on the device shares it (at most
  `MAX_QUERIES_PER_FRAME` x 3 = 144 per ring). One WARN names the query the device refused; the passes
  beyond it go untimed. `query_count()` exposes the total for tests.
- Lights and shadow casters are ordered with `std::sort` and the scene index as the tie-break, which
  is the stable order without the temporary buffer `std::stable_sort` allocates (libstdc++ always
  allocates one). `light_importance` maps a NaN result (NaN intensity, range or position in user data)
  to 0 so the comparators keep their strict weak ordering.

The null device the gates measure through is allocation-free in steady state as well (`rhi` README,
"Null device"); the renderer gate therefore counts device-side allocations and does not exclude them.

Conventions: reverse-Z everywhere (near = 1, far = 0, `GreaterOrEqual`), classic NVRHI binding sets
with `registerSpaceIsDescriptorSet`, a volatile constant buffer at `b0` and 16 bytes of push constants
at `b1` in space 0, the material set in space 1, matrices uploaded verbatim from glm and consumed with
`mul(vector, matrix)` under `-Zpr`.

## Threading

`create`, `render`, `render_frame`, `begin_extract` / `end_extract`, `find_*`, render targets and
`read_back` belong to the render thread (the main thread in v1). `upload_*` and `release_*` may be
called from any thread: the payload is copied at enqueue time, the slot is reserved under a mutex and
the id resolves to the fallback until the upload flushes at the start of a later frame. Culling and
draw list building run on `job_system::parallel_for` when a job system is given; the render thread
helps with queued jobs while it waits, so a job may call `upload_*` / `release_*` in the middle of
`render()`. `gpu_resource_cache` therefore never hands its mutex to the frame: `resolve_*` lock per
call, the mirrors live in heap entries whose addresses survive table growth, and an entry released
during a frame is retired (its GPU handles get an extra graveyard reference) and freed at the next
`collect()`, so every reference the frame resolved stays valid until the frame is submitted. No
NVRHI object is created outside `flush_uploads`: releasing a texture only marks the materials that
used it, and the next flush rebinds them on the render thread.

## Deviations from the design

- The wave 3 integration recorded these in `docs/design/09_cross_area_resolutions.md` section 3 and
  `02_renderer.md` was corrected where only a name differed (`alpha_mode::SOLID`, the `ray_from_viewport`
  aspect ratio, the draw key layout); the rest stays here as the behavioural record.
- `draw_list_builder` packs the opaque key as `[variant:8][material:22][mesh:22][submesh:4][depth:8]`
  (the design's 24-bit material and mesh fields leave no room for the submesh); `add()` asserts the
  ranges (`MAX_DRAW_KEY_INDEX`, `MAX_DRAW_KEY_SUBMESH`) so an overflow can never merge different
  meshes into one batch.
- PCSS estimates its penumbra in world units: the cascades use the sun's angular radius times the
  blocker distance inside the orthographic light volume; the atlas tiles linearise the reverse-Z
  depths from `LOCAL_SHADOW_NEAR_PLANE` and the light's range and apply the design's
  `(dReceiver - dBlocker) * sourceRadius / dBlocker`. Search and filter radii are clamped to 8 texels.
- The shadow raster bias is `-2` / `-2.0` (the design's magnitudes, negated for reverse-Z: a positive
  bias would move stored occluders toward the light).
- `light::entityId` (not in the design's struct, never reflected) gives a local light a stable shadow
  atlas tile across frames; without it the tile identity is a hash of the light's attributes.
- The AO upsample is a 4-tap bilateral filter over the half-resolution AO and a half-resolution view
  depth the GTAO pass writes (`ao_depth_half`, bound at `t9` of the mesh view set).
- The exposure histogram meters the view's viewport rectangle only; its origin and size travel in
  the draw push constants.

## Failure model

`renderer::create` fails at start-up with `renderer_error::SHADER_NOT_FOUND` (naming the shader
key), `PIPELINE_CREATION_FAILED` or `RESOURCE_CREATION_FAILED`; nothing is created mid-frame except
a blit pipeline the first time a new target format appears (outside frames). `render` fails only with
`rhi::rhi_error::DEVICE_LOST`; every other problem of a scene is clamped and logged (views beyond
`MAX_VIEWS`, lights beyond `MAX_LIGHTS_PER_VIEW`, sprite and debug caps, atlas overflow). A stale
gpu id draws the fallback (unit cube, white texture, magenta unlit material, grey environment) and
WARNs once.

## Tests

`tests/renderer/` holds one file per public header plus the topic files of the design (`culling`,
`draw_list_builder`, `cascade_builder`, `shadow_atlas_allocator`, `render_graph`, `transient_pool`,
`gpu_types`, `tonemap_reference`, `exposure_math`, `pipeline_cache`, `gpu_resource_cache`,
`renderer_null_frame`, `renderer_failures`, `renderer_gpu`). Everything runs on the null device; the
`[gpu]` cases use WARP and skip without `precondition::SOFTWARE_GPU_ADAPTER`. Test glTF, PNG and
HDR inputs are generated in the tests (no binary assets). The allocation gates
(`CONJURE_CHECK_NO_ALLOCATIONS`) live next to the code they pin: the whole frame and extraction plus
culling in `test_renderer_null_frame.cpp`, the warm `cull_scratch` in `test_culling.cpp`, the rebuilt
graph in `test_render_graph.cpp` and the repeated atlas allocation in `test_shadow_atlas_allocator.cpp`.
Their benchmark counterparts are `renderer.extract_10k` and `renderer.null_frame_10k`
(`tests/benchmarks/bench_render_null_frame.cpp`, thresholds `allocs: 0`).

```
python tools/build.py --target conjure_renderer_tests --test -R renderer
```
