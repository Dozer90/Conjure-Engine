# renderer

`conjure::renderer` (target `conjure_renderer`, namespace `conjure::renderer`) turns a `render_scene`
into one command list per frame through a render graph over `conjure::rhi`. It owns the GPU mirrors
of its asset types, the off-screen render targets and, in editor builds, the Dear ImGui backend.
Design: `docs/design/02_renderer.md`; cross-area decisions: `docs/design/09_cross_area_resolutions.md`
(R7-R12, R34, R43); the big picture: `docs/ARCHITECTURE.md` 5 and 9.

## Dependencies

Public: `conjure::core`, `conjure::rhi`, `conjure::assets`. Never `ecs` or `scene` (R7): the
components in `components.h` are plain reflected structs the engine registers.
Private: NVRHI (through `rhi::device::native_device()`), cgltf, stb (image and HDR decoding), libjxl
(`conjure::thirdparty::jxl`: JPEG XL decoding and encoding, every configuration) and Dear ImGui (only with
`CONJURE_BUILD_EDITOR`, never in Distribution).

## Layout

| Area | Public header (`engine/include/conjure/renderer/`) | Private source (`engine/src/renderer/`) |
| --- | --- | --- |
| Budgets, errors, logging | `limits.h`, `renderer_error.h`, `log_category.h` | `renderer_error.cpp`, `log_category.cpp` |
| Handles | `gpu_ids.h` | `gpu_resource_cache.*` (slot maps, fallbacks, graveyard) |
| Extraction contract | `render_scene.h`, `camera.h`, `light.h`, `render_settings.h` | `render_scene.cpp`, `camera.cpp`, `light.cpp`, `render_settings.cpp` |
| 2D and debug | `sprite_batch.h`, `debug_draw.h` | `sprite_batcher.cpp`, `debug_draw.cpp`, `passes/sprite_pass.cpp`, `passes/debug_draw_pass.cpp` |
| Assets | `mesh_asset.h`, `texture_asset.h`, `material_asset.h`, `model_asset.h`, `environment_asset.h`, `shader_asset.h`, `gltf_importer.h`, `jpeg_xl.h` | `assets/*.cpp`, `import/*` (tangents, mips, image decoding, JPEG XL codec, IBL bake, cgltf) |
| Components | `components.h` | `components.cpp` |
| The frame | `renderer.h`, `render_target.h` | `renderer.cpp`, `renderer_impl.h`, `render_graph.*`, `transient_pool.*`, `framebuffer_cache.*`, `binding_layouts.*`, `pipeline_cache.*`, `shader_program_loader.*`, `frame_resources.*`, `timer_ring.*`, `warn_latch.h`, `gpu_types.*`, `culling.*`, `draw_list_builder.*`, `exposure_math.*`, `shadows/*`, `passes/*` |
| ImGui | `imgui_renderer.h` | `imgui/imgui_renderer.cpp` |

Shaders live in `engine/shaders/` and are compiled by `conjure_add_shaders` into
`<build>/bin/shaders/index.json`. The structs in `shared/gpu_types.hlsli` are mirrored in `gpu_types.h`
with static_asserted sizes; the tests re-read the HLSL defines to keep the two in step.

### Which shaders are loaded

There are two lists and no third (design R18, code sweep F395):

- `engine/shaders/CMakeLists.txt` says what the build compiles into the index.
- `shader_asset::built_in_programs()` (the table in `assets/shader_asset.cpp`) says what the renderer
  loads: one `shader_program_desc` per program permutation (name, the entry of each stage it has, the
  pixel and vertex define strings).

No other source names a shader blob. `pipeline_cache.cpp` maps every `graphics_variant` and
`compute_variant`, and the blit, to the identity of a table entry (`key_of`, exposed to the tests as
`pipeline_cache::program_of` / `blit_program`); `imgui/imgui_renderer.cpp` asks for the `imgui` program.
Both go through `shader_program_loader.h`: `resolve_built_in_program` is `shader_asset::find_built_in`
plus `validate` against the shader library, so a program is refused whole, before any of its device
shaders exists, when the table lacks it or the index lacks one of its stages; `create_program_shader`
then loads one stage's blob and creates the device shader. The input layouts take the vertex shader of a
variant that draws with them (`create_vertex_shader`), not a name of their own.

To add a shader permutation: list it in `engine/shaders/CMakeLists.txt`, add or extend its row in the
table, and map a variant to it in `key_of`. `test_shader_asset` fails when the two lists disagree in
either direction (the index may hold nothing but the table's stages and the rhi module's
fullscreen-triangle pair), `test_pipeline_cache` when a variant does not resolve or a table row is used
by nothing. `validate` reads the index only: a blob the index lists but the directory lacks is reported
by `create_program_shader`, also as `SHADER_NOT_FOUND`.

### Where a mesh's tangents come from

Stream 1 always holds a tangent (design R8), so "no tangents" is not a state a packed mesh can be in.
`mesh_asset::from_geometry(geometry, name, missingTangents)` packs the tangents the geometry carries;
when it carries none it builds them, as `tangent_generation` says (design R20, code sweep F391):

- `FROM_UVS` (the default): `detail::generate_tangents`, the uv0 derivatives of the triangles around a
  vertex, Gram-Schmidt against the normal, handedness from the UV winding. A geometry without uv0, and a
  vertex whose UVs give no direction, falls back to the next one.
- `FROM_NORMAL`: `detail::normal_tangents`, `any_perpendicular(normal)` with handedness +1. No UV is read.
  Lighting works, a normal map does not: the tangent does not follow the texture.

Both live in `import/tangent_generator.*`, behind `detail::missing_tangents(..., tangent_generation)`. The
glTF importer picks by `gltf_import_options::generateTangents` (`import_mesh`). A mesh whose primitives all
have a usable `TANGENT` (one per vertex) keeps them; when no primitive has one, `from_geometry` builds them
all. A mixed mesh, with `TANGENT` on some primitives only, is decided per primitive with the option off:
`import_mesh` calls `missing_tangents` itself and fills the primitives without, so the file's tangents of
the others survive. With the option on, the default, the mixed mesh keeps the rule from before F391 (which
decided the off case only): the tangent array is cleared and `from_geometry` generates all of it from the
UVs. One mixed mesh keeps none under either value: one with a primitive that lacks `NORMAL`. The normals
are then generated inside `from_geometry` (one vertex per triangle corner), the importer has no normal to
build the missing tangents from, and the file's tangents would sit on normals that are not used, so the
tangent array is cleared and `from_geometry` builds all of it. No vertex flag, material flag or shader
branch marks a normal-only tangent; with the option off the importer warns instead, once per mesh, naming
the lit materials with a normal texture that a primitive with normal-only tangents draws with
(`normal_tangent_warning`; a primitive past `MAX_MATERIAL_SLOTS` draws with the material of the primitive
that owns the last slot). The warning follows the option, so a primitive without `TEXCOORD_0` imported with
the option on gets the same tangents unreported. An output change of the importer bumps
`GLTF_IMPORTER_VERSION` (3 since this one).

## The frame

`renderer::render(scene)` (render thread) does, in order:

1. `begin_frame`: acquire the back buffer (when a swapchain exists), open the frame command list,
   flush the queued asset uploads under the 64 MB budget, collect the graveyard.
2. `prepare`: build sprites and debug lines once (past `MAX_SPRITES_PER_FRAME` the batcher keeps the
   `SCREEN` sprites and drops the `WORLD` sprites last in sort order), allocate the local-light shadow
   atlas scene-wide (importance measured from view 0; casters culled with `atlasLayerMask`, the union
   of the layer masks of the views without `SKIP_SHADOWS`), then per view: cull on jobs, build
   instanced draw lists (opaque / masked sorted by material and mesh, blended far to near), sort the
   lights (directional first), build the cascades of the first shadowing sun (the view's own mask),
   fill the view constants.
3. `render_view_graph`: one render graph per view, passes added only when they have work:
   `shadow_cascade_0..3`, `shadow_atlas`, `depth_prepass`, `light_cull`, `ssao_gtao`, `ssao_blur_h/v`,
   `forward_opaque`, `skybox`, `forward_blend`, `world_sprites`, `debug_draw`, `exposure_histogram`,
   `exposure_adapt`, `bloom_down_0..5`, `bloom_up_4..0`, `tonemap`, `screen_sprites` and `blit` (only
   when the target is not `RGBA8_SRGB`). The graph validates (read before write asserts in Debug,
   logs an ERROR and skips the pass), culls backwards, computes lifetimes and takes its transients
   from the pool (no aliasing, NVRHI automatic barriers). `world_sprites` and `debug_draw` write `hdr`
   before exposure, bloom and the tonemapper; 2D views that want authored colours set
   `NO_POST_PROCESS` (the camera's `postProcess = false`).
4. ImGui into the swapchain (editor builds).
5. `end_frame`: close, execute, present, evict idle transients and binding sets, fill `render_stats`.

A window resize goes through `renderer::resize_swapchain(size)`, never through `swapchain::resize` alone: the
framebuffer cache holds a framebuffer over every back buffer a view or the ImGui pass has drawn into, and a
swapchain cannot recreate buffers that are still referenced (D3D12 refuses, and so does the null backend, so the
order is tested headless). The call waits for the GPU, clears the framebuffer and binding-set caches, collects
what the last command list held and only then resizes (code sweep F044).

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
used it, and the next flush rebinds them on the render thread. `resize_render_target` (render thread,
after `wait_idle`) keeps the target's colour id: `replace_external_texture` points the id's entry at
the new texture and the replacement target's own id is unregistered, so an ImGui handle or a sprite
holding the id follows the resize.

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

## Image decoding and JPEG XL

`import/image_loader.cpp` holds the only two source-image decoders (`detail::decode_rgba8`, `detail::decode_hdr_rgb`);
`texture_asset::decode`, the texture importer, glTF images and `equirect_image::decode_hdr` share them. They route by
signature (`is_jpeg_xl`): JPEG XL to `import/jpeg_xl_decoder.cpp`, the rest to stb_image. Only the three
`import/jpeg_xl_*.cpp` files include libjxl:

| File | Holds |
| --- | --- |
| `jpeg_xl_codec.h` | the private API: `decode_jpeg_xl_rgba8`, `decode_jpeg_xl_rgb_float`, `jpeg_xl_encode_desc` and `encode_jpeg_xl` (every layout, for tools and the tests' fixtures) |
| `jpeg_xl_decoder.cpp` | the decoder: the limits read from the headers (size, extra channels, every layer in a header-only pass), the memory budget, the colour plan, the 16-bit reduction callback, frame skipping, the engine's float steps (CMS conversions, unpremultiplying, the PQ and HLG scale), `is_jpeg_xl` |
| `jpeg_xl_encoder.cpp` | the encoder and the public RGBA8 `encode_jpeg_xl` |
| `jpeg_xl_hooks.*` | the `JxlMemoryManager` over `core::memory` with its per-decode `jpeg_xl_memory_budget`, and the `JxlParallelRunner` over `core::jobs` |

Rules the decoder keeps (`docs/design/02_renderer.md` 3.13): an `SRGB` texture is converted to sRGB through
skcms, a `LINEAR` one keeps the file's own encoding; PQ and HLG come out with 1.0 at 203 nits (BT.2408's reference
white), an `SRGB` texture of one clipped at 1; colour and alpha deeper than 8 bits keep their high byte (stb's
reduction of a 16-bit PNG), 8-bit colour beside them is rounded; orientation applied, premultiplied alpha divided
out in the stored encoding before any conversion (by libjxl, into integers, when no conversion follows), the first
animation frame kept with a WARN; the image size, the extra channels (at most 16) and every layer's size checked
from the headers before any frame is decoded, and libjxl's memory held to a budget derived from the image size and
16 threads whatever the worker count, on at most 16 threads (`docs/handbook/libjxl.md` 9.3). libjxl cannot render a lossy (XYB) image into an ICC profile (a
debug build stops on `stage_from_linear.cc` "Invalid target encoding"), so for a `LINEAR` texture such a file is
decoded to linear sRGB floats and converted to its ICC profile through `JxlCmsInterface` row by row.
`equirect_image::decode_hdr` and `sample` read NaN and negative samples as 0 and anything above
`MAX_ENVIRONMENT_RADIANCE` (the largest float / 2^16) as that bound, so the bake's float sums stay finite while a sun
brighter than a half holds is integrated unclipped; only `pack_cubemap` clamps to 65504.
The texture and environment importers pass the job system `register_asset_types` received; glTF images decode
on the calling thread. `TEXTURE_IMPORTER_VERSION` is 2 since JPEG XL; `ENVIRONMENT_IMPORTER_VERSION` is 3 since the
bake reads samples that way.

## Failure model

`renderer::create` fails at start-up with `renderer_error::SHADER_NOT_FOUND` (naming the shader
key; a stage missing from the index is found before any shader of its program is created, a blob file
missing from the directory when that stage is loaded, see "Which shaders are loaded"),
`PIPELINE_CREATION_FAILED` or `RESOURCE_CREATION_FAILED`; nothing is created mid-frame except
a blit pipeline the first time a new target format appears (outside frames). `render` fails only with
`rhi::rhi_error::DEVICE_LOST`; every other problem of a scene is clamped and counted (views beyond
`MAX_VIEWS`, lights beyond `MAX_LIGHTS_PER_VIEW`, sprite and debug caps, atlas overflow). The frame
path logs a WARN when a problem starts and not again while it lasts, whatever its size does; a frame
that sees the problem gone arms the WARN again (`warn_latch.h`, design R19, code sweep F612). That
covers the view, light and sprite budgets, per view index a clamped camera and an unknown render
target, and a swapchain refusing the acquire or the present without device loss. `render_stats`
(`viewsSkipped`, `lightsDropped`, `spritesDropped`) has every frame's count; the handbook page
(`docs/handbook/modules/renderer.md` section 7) lists what each latch looks at and the warnings
that are logged once and never armed again.

A stale gpu id draws the fallback (unit cube, white texture, magenta unlit material, grey
environment) and WARNs once per id (for the first 1024 such ids). An upload whose GPU objects the
device refuses logs an ERROR naming the kind, name and asset, counts in `upload_report::failed` and
leaves its id on the fallback (a failed replacement drops the previous mirror too); the materials
using a refused texture rebind that slot to its fallback (white, flat normal or black).

## Tests

`tests/renderer/` holds one file per public header plus the topic files of the design (`culling`,
`draw_list_builder`, `cascade_builder`, `shadow_atlas_allocator`, `render_graph`, `transient_pool`,
`gpu_types`, `tonemap_reference`, `exposure_math`, `pipeline_cache`, `gpu_resource_cache`,
`warn_latch`, `renderer_null_frame`, `renderer_failures`, `renderer_gpu`). Everything runs on the null device; the
`[gpu]` cases use WARP and skip without `precondition::SOFTWARE_GPU_ADAPTER`. Test glTF, PNG, HDR,
JPEG XL and ICC inputs are generated in the tests (no binary assets): `test_jpeg_xl.cpp` writes every JPEG XL
layout through the private encoder, a 16-bit PNG and ICC v2 profiles by hand. The allocation gates
(`CONJURE_CHECK_NO_ALLOCATIONS`) live next to the code they pin: the whole frame and extraction plus
culling in `test_renderer_null_frame.cpp`, the warm `cull_scratch` in `test_culling.cpp`, the rebuilt
graph in `test_render_graph.cpp` and the repeated atlas allocation in `test_shadow_atlas_allocator.cpp`.
Their benchmark counterparts are `renderer.extract_10k` and `renderer.null_frame_10k`
(`tests/benchmarks/bench_render_null_frame.cpp`, thresholds `allocs: 0`). Every other case gets its
null device from `renderer_test_support` with NVRHI's validation layer on (`enableDebugLayer`), so
command-list misuse logs an ERROR and fails any case holding a `log_capture`; the gates use the raw
device (`make_raw_device`, `make_raw_headless_renderer`) to measure the renderer alone.

```
python tools/build.py --target conjure_renderer_tests --test -R renderer
```
