# Conjure Engine - Design Area B: Renderer (`conjure_rhi`, `conjure_renderer`)

Status: FINAL. This document is binding for the implementers of `conjure_rhi` (wave 2) and
`conjure_renderer` (wave 3). Every architectural question in scope is resolved here; implementers
decide only local details (variable names, private helpers, exact constants marked "tunable").

Inputs: `docs/DESIGN_DOC.md` (customer requirements), `docs/design/00_brief.md` (fixed decisions),
`docs/CONVENTIONS.md`, `docs/CODE_STYLE.cpp`. Two panel proposals (performance-first and
robustness-first) were evaluated; the robustness-first proposal is the backbone, with the
performance-first proposal's two-stream vertex layout, per-light shadow filtering, quadtree shadow
atlas, parallel culling/sorting and `render_stats`-driven headless tests grafted on.

---

## 1. Overview and decisions

```
engine (wave 4): EXTRACT systems --write--> renderer::begin_extract() scene ; end_extract() ; renderer::render_frame()
renderer (wave 3): resolve gpu ids -> cull/sort/batch -> build + compile render_graph per view ->
                   execute into one nvrhi command list -> imgui_renderer (editor) -> present, render_stats
rhi (wave 2): device / swapchain / shader_library over nvrhi backends null | d3d12 | vulkan
```

### 1.1 Binding decisions

| # | Decision | Rationale |
|---|---|---|
| R1 | **Forward renderer with a depth + normal prepass and tiled (16x16) compute light culling.** No G-buffer. Reverse-Z `D32_FLOAT`, finite far plane. Single `RGBA16_FLOAT` HDR colour target. | One shading path for opaque, alpha-masked, blended and world-space sprites; 2D and 3D share a pipeline. The prepass feeds SSAO (depth + normals) and the light culler, and removes forward overdraw. Clustered culling was rejected for v1: tiled is half the code and the pass is isolated so clustered can replace it later. |
| R2 | **Classic NVRHI binding sets, no bindless in v1.** Set 0 (`space0`) = per-pass/per-view resources + push constants, set 1 (`space1`) = per-material binding set cached on the GPU material. One 16-byte push constant block per draw. `registerSpaceIsDescriptorSet = true` on every layout. | Identical behaviour on D3D12, Vulkan and MoltenVK. NVRHI's programming guide states descriptor tables keep no strong references and have no barrier tracking; that is exactly the class of lifetime bug a team of parallel implementers must not debug. Draws are sorted by (pipeline, material, mesh) and instanced, so a binding-set switch happens once per material run, not per instance; a few hundred `setGraphicsState` calls per frame is far inside budget for the 10,000-entity target. Bindless remains a v2 optimisation behind `material_binding` (see 11). |
| R3 | **Render graph rebuilt every frame**: declaration order is execution order, dead passes are culled, transient textures are pooled by descriptor hash, **no memory aliasing**, **NVRHI automatic barriers stay enabled**. Static assets use permanent resource states. | Fewer than 40 passes per view; compile cost is microseconds. Aliasing and manual barriers are the two leading sources of GPU hazards; neither is needed at 1080p for this scope. Manual barriers are listed as a measured, future optimisation. |
| R4 | **Double-buffered `render_scene` (AoS vectors) is the only ECS -> renderer bridge.** The renderer never touches the ECS. The engine module writes it; disjoint-range writes from jobs are supported through `allocate_meshes()`. | Clear ownership, trivially testable (tests build a `render_scene` by hand), and a render thread (area F) becomes a flip, not a redesign. |
| R5 | **Render-thread ownership**: all NVRHI object creation, command recording and `render()` run on one thread (main thread in v1). Asset uploads are queued from any thread and flushed at the start of `render()`. Culling, sorting and instance-buffer filling are job-parallel over disjoint ranges. One command list per frame. | NVRHI permits concurrent recording of different command lists, but a single list in v1 removes ordering bugs; recording a few hundred draws is < 1 ms. Parallel recording is a v2 item. |
| R6 | **All per-frame GPU data goes through `ICommandList::writeBuffer`** into persistent buffers (instances, lights, tile lists, sprites, debug lines) and volatile constant buffers for view/pass constants. No application-level ring buffers. | NVRHI's command-list upload manager recycles staging memory on fence completion and the copy executes in queue order before the frame's draws, so frame N's write cannot race frame N-1's reads. Removes an entire category of frames-in-flight bugs. |
| R7 | **One PBR material model (glTF metallic-roughness).** Texture presence, double-sided, unlit, UV transform are **uniform-driven**. The only static shader permutation axis is `ALPHA_MASK` (forward PS, prepass PS, shadow PS). Blend mode is pipeline state, not a shader permutation. Every permutation is enumerated in CMake and created at `renderer::create` ("fail at init, never mid-frame"). | Branches on constant-buffer values are coherent and free; permutation explosion is the classic source of untested shader paths. |
| R8 | **Vertex layout: two streams.** Stream 0 = position `RGB32_FLOAT` (12 B). Stream 1 = normal `RGBA16_SNORM` (w = tangent sign), tangent `RGBA16_SNORM`, uv0 `RG32_FLOAT`, uv1 `RG16_FLOAT`, colour `RGBA8_UNORM` (32 B). Indices `R32_UINT`. | Shadow and depth-only passes bind stream 0 only (one third of the bandwidth of an interleaved layout) while keeping full-precision positions and UV0. Meshes without colours get white, without uv1 get zero, so no permutations. |
| R9 | **Shadows**: directional = 4 stabilised cascades in a `D32_FLOAT` `Texture2DArray` (2048^2 per slice, tunable), practical split lambda 0.7, texel snapping; spot/point = one 4096^2 `D32_FLOAT` atlas with a quadtree tile allocator (1024 / 512 / 256 tiles), point lights take 6 tiles. Filter is **per light**: `HARD`, `PCF` (16-tap rotated Poisson), `PCSS`; `DEFAULT` resolves to `shadow_settings::defaultFilter` so the editor can flip all lights at once. | Two shadow SRVs for the whole scene; fixed budgets mean no in-frame allocation; per-light filter lets the sun use PCSS while cheap point lights stay hard. |
| R10 | **SSAO = GTAO-style horizon search at half resolution** (2 slices x 4 steps), 4x4 interleaved rotation pattern, separable depth-aware 4-tap blur, bilateral upsample in the forward shader. Output `R8_UNORM`. No temporal filter in v1. | Matches the brief; temporal filtering needs motion vectors and history, deferred to the TAA work item. |
| R11 | **HDR chain**: 256-bin log-luminance histogram auto-exposure (50th-90th percentile) with up/down adaptation speeds, or manual EV100; 6-mip bloom (13-tap downsample with Karis average on mip 0, 3x3 tent upsample); tonemap `ACES_FITTED`, `REINHARD_EXTENDED`, `AGX`, `NONE`, chosen by uniform. Tonemap curves live in `tonemap.hlsli`, written in a C++-compilable subset and unit-tested natively. | All three customer operators plus a pass-through for pixel-exact 2D games. CPU-testable curves catch regressions without a GPU. |
| R12 | **2D**: `WORLD` sprites draw into HDR after blended meshes (depth-tested, premultiplied alpha); `SCREEN` sprites draw after tonemap into the LDR target in pixel space. Orthographic cameras are `projection_type::ORTHOGRAPHIC`; every 3D pass is projection-agnostic. A view with `view_flags::NO_POST_PROCESS` skips exposure, bloom and the operator (exposure 1, sRGB encode only). | No second renderer. A Pac-Man-class game is a 3D frame whose 3D passes are culled by the graph because they have no inputs, with colours untouched. |
| R13 | **IBL baked on the CPU inside the environment importer** (equirect -> 512^2 cubemap -> SH9 irradiance -> 128^2 x 6-mip GGX prefiltered specular; job-parallel per face; deterministic) and stored in the cooked `.cenv`; the 256^2 `RG16_FLOAT` split-sum BRDF LUT is baked once on the GPU at renderer creation. (Cross-area resolution R34: importers run on worker threads without a device and must be deterministic, so a GPU bake at import is impossible under the asset pipeline's rules.) | Load is upload-only; cooked projects never re-bake; the bake is unit-testable headless and byte-identical across machines, which the content-hash cache requires. |
| R14 | **Shader build**: `conjure_add_shaders()` enumerates `source;entry;stage;defines` entries, runs `dxc` (DXIL, `-T xx_6_5`, `-HV 2018`) and `glslang` (SPIR-V, `vulkan1.2`) per permutation with NVRHI's default Vulkan binding offsets, writes `shaders/index.json`, and supports `CONJURE_SPIRV_COMPILER=glslang|dxc`. `shader_library` resolves `(name, entry, defines)` -> blob. | Honours the fixed decision. glslang's README (checked 2026-11-06) marks its HLSL front-end deprecated since April 2026 with removal at the next major version, so `Dependencies.cmake` must pin a pre-removal tag and the compiler switch keeps the long-term path open without touching shader sources. |
| R15 | **NVRHI stays in `src/`.** Public `rhi` headers forward-declare `nvrhi::IDevice` / `nvrhi::ITexture` only (no includes) and expose non-owning `native_*()` accessors used by `conjure_renderer`. | Satisfies "no third-party headers in public headers" literally without inventing a parallel type system over NVRHI. |
| R16 | **`imgui_renderer` is part of `conjure_renderer`**, compiled out under `CONJURE_DISTRIBUTION`, implements `ImGuiBackendFlags_RendererHasTextures` (ImGui >= 1.92 texture protocol). | One NVRHI consumer; the texture protocol is required by upstream for all backends. |
| R17 | **Null backend** = full `nvrhi::IDevice` implementation (`engine/src/rhi/backends/null/`) that stores descriptors and returns valid handles; command lists record only counters and debug-marker names into `null_device_stats`. The whole renderer (graph, culling, sorting, stats, readback) runs on it in CTest. Command-list misuse checks are NVRHI's validation layer's (`enableDebugLayer`), not the null list's: the renderer tests run the null device under it, except the allocation gates (10; sweep-2026-11-12 F171). | Requirement 4 of the brief (everything testable headless). Re-implementing NVRHI's validation in the null list would duplicate it for about 40 recording methods. |

### 1.2 Explicit non-goals for v1
Skinning, GPU-driven culling/indirect draws, TAA, order-independent transparency, block-compressed
textures (enumerators reserved), memory aliasing, manual barriers, parallel command-list recording,
custom user materials, `KHR_texture_transform` (warned, ignored), text rendering (ImGui covers it).

## 2. Data model

### 2.1 Limits (`conjure/renderer/limits.h`)

`constexpr std::uint32_t` in `namespace conjure::renderer`: `MAX_VIEWS = 8`, `MAX_LIGHTS_PER_VIEW = 1024`,
`MAX_LIGHTS_PER_TILE = 64`, `LIGHT_TILE_SIZE = 16`, `MAX_SHADOW_CASCADES = 4`,
`MAX_SPRITES_PER_FRAME = 65536`, `MAX_DEBUG_VERTICES = 1 << 20`, `MAX_MATERIAL_SLOTS = 8`,
`INVALID_INDEX = 0xFFFFFFFFu`, `MAX_PASSES_PER_VIEW = MAX_SHADOW_CASCADES + (2 * BLOOM_MIP_COUNT - 1) + 16`
(CPU-only, not mirrored in `gpu_types.hlsli`; it sizes `render_stats::passNames`). The other values are
`#define`d in `gpu_types.hlsli` and compared by `test_gpu_types`.

### 2.2 GPU ids (`gpu_ids.h`)
`gpu_mesh_id`, `gpu_texture_id`, `gpu_material_id`, `gpu_environment_id`, `render_target_id`: distinct
`core::slot_map` key structs with `is_valid()`. The default `core::slot_key` is `u32 index` + `u32 generation`
(`10_core.md` 2.3); an id that wants the compact 32-bit index + 16-bit generation layout is a custom key type
satisfying the `slot_key_type` concept (`slot_map<T, my_id>`), in which case the slot retires after 65535 reuses.
A stale id resolves to the fallback object (unit cube, white texture, magenta material, grey environment) and
logs `WARN` once.

### 2.3 Render scene (the extraction contract, `render_scene.h`)

```cpp
struct mesh_instance {
    gpu_mesh_id mesh = {};
    std::array<gpu_material_id, MAX_MATERIAL_SLOTS> materials = {};
    math::mat4 worldFromLocal = math::mat4(1.0f);
    math::aabb worldBounds = {};                    // computed by extraction from mesh bounds
    std::uint32_t entityId = 0;                     // debug visualisation only; picking is CPU-side in the editor (R11)
    std::uint32_t layerMask = 1;
    bool castsShadows = true;
    bool receivesShadows = true;
};

struct sprite {
    gpu_texture_id texture = {};
    math::mat4 worldFromLocal = math::mat4(1.0f);  // SCREEN: pixel-space transform, y down
    math::vec4 uvRect = math::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    math::vec4 colour = math::vec4(1.0f);           // linear, premultiplied on the GPU
    math::vec2 size = math::vec2(1.0f);             // world units or pixels
    sprite_space space = sprite_space::WORLD;
    std::int16_t layer = 0;
    std::int16_t order = 0;
};

struct render_view {
    camera cameraData = {};
    math::mat4 viewFromWorld = math::mat4(1.0f);
    math::vec3 cameraPosition = math::vec3(0.0f);
    math::urect viewport = {};                      // pixels inside the target
    render_target_id target = {};                   // invalid => swapchain
    view_flags flags = view_flags::NONE;            // DRAW_DEBUG, NO_POST_PROCESS, SKIP_SHADOWS (enum class, R31)
    std::uint32_t layerMask = 0xFFFFFFFFu;
};

struct render_scene {
    std::uint64_t frameIndex = 0;
    float deltaSeconds = 0.0f;
    std::vector<render_view> views;
    std::vector<mesh_instance> meshes;
    std::vector<light> lights;
    std::vector<sprite> sprites;
    environment_settings environment = {};          // gpu_environment_id, intensity, rotation
    post_process_settings postProcess = {};
    shadow_settings shadows = {};
    debug_draw_buffer debugDraw = {};

    /// <summary>Resizes the mesh array once and returns the whole span so jobs can fill disjoint ranges.</summary>
    [[nodiscard]] std::span<mesh_instance> allocate_meshes(std::uint32_t count);
    /// <summary>Clears every array but keeps capacity so steady-state frames do not allocate.</summary>
    void clear() noexcept;
};
```

`render_scene_pair` holds two scenes and is **owned by the renderer** (R8): `renderer::begin_extract()` clears
the write slot and returns it as `render_scene &`, the engine's EXTRACT systems fill it, `end_extract()` swaps,
and `render_frame()` renders `read()`. `render(const render_scene &)` stays public so tests build a scene by
hand. The pair is not internally synchronised; the engine loop guarantees extraction of frame N and
rendering of frame N-1 never touch the same slot (debug-only owner-thread assert).

### 2.4 Light (`light.h`)

```cpp
enum class light_type : std::uint8_t { DIRECTIONAL, POINT, SPOT };
enum class shadow_filter : std::uint8_t { DEFAULT, HARD, PCF, PCSS };

struct light {
    light_type type = light_type::POINT;
    math::vec3 position = math::vec3(0.0f);
    math::vec3 direction = math::vec3(0.0f, -1.0f, 0.0f);
    math::vec3 colour = math::vec3(1.0f);           // linear
    float intensity = 10.0f;                        // lux (directional) or candela (point/spot), glTF units
    float range = 10.0f;
    float innerConeRadians = 0.4f;
    float outerConeRadians = 0.6f;
    float sourceRadius = 0.05f;                     // PCSS penumbra; angular radius for directional
    bool castsShadows = false;
    shadow_filter filter = shadow_filter::DEFAULT;
    std::uint16_t shadowResolution = 512;           // atlas tile request, clamped to {256, 512, 1024}
};
```

### 2.5 GPU-side structures (`engine/shaders/shared/gpu_types.hlsli` mirrored by `src/renderer/gpu_types.h`, `static_assert` on `sizeof`)

| Struct | Size | Binding | Contents |
|---|---|---|---|
| `gpu_view_constants` | 1024 B | volatile CB `b0, space0` (`maxVersions = MAX_VIEWS * 3`) | viewFromWorld, clipFromView, clipFromWorld, worldFromClip, cameraPosition, viewportSize, invViewportSize, nearFar, exposure, frameIndex, tileCountX/Y, lightCount, ssao params, iblIntensity, skyRotation, directional light, 4 cascade clipFromWorld + split depths, postProcess flags |
| `gpu_instance` | 96 B | structured `t0, space0` | worldFromLocal (3x4), normalFromLocal (3x3 padded), entityId, flags (receivesShadows) |
| `gpu_light` | 64 B | structured `t1, space0` | position, range, direction, spotCosOuter, colour, intensity, spotCosInner, type, shadowTileIndex or cascade marker, filter, atlas uv rect, lightFromWorld index |
| `gpu_shadow_matrix` | 64 B | structured `t2, space0` | one `clipFromWorld` per atlas tile (6 per point light) |
| `gpu_material_constants` | 64 B | static CB `b0, space1` | baseColourFactor, emissiveFactor + strength, metallic, roughness, normalScale, occlusionStrength, alphaCutoff, textureFlags, flags (DOUBLE_SIDED, UNLIT), pad |
| `gpu_draw_constants` | 16 B | push constants `b1, space0` | firstInstance, materialSlot, viewIndex, pad |
| `gpu_sprite_vertex` | 32 B | vertex stream | position (xyz), uv, colour (RGBA8), pad |
| `gpu_debug_vertex` | 16 B | vertex stream | position, colour (RGBA8) |

`SV_InstanceID` is zero-based on D3D12 regardless of `startInstanceLocation` but includes the base
on Vulkan, so every draw uses `startInstanceLocation = 0` and reads
`instances[firstInstance + SV_InstanceID]`.

### 2.6 GPU asset mirrors (private, owned by `gpu_resource_cache`)

| Public CPU asset | Private GPU mirror | Contents |
|---|---|---|
| `mesh_asset` | `gpu_mesh` | stream 0 buffer, stream 1 buffer, index buffer (all `setPermanentBufferState`), `std::vector<submesh>`, bounds |
| `texture_asset` | `gpu_texture` | `nvrhi::TextureHandle` (permanent `ShaderResource`), format, mip count, sampler choice |
| `material_asset` | `gpu_material` | `gpu_material_constants` CB (static, 64 B), `nvrhi::BindingSetHandle` (set 1: CB + 5 textures + 1 sampler), `alpha_mode`, double-sided flag, generation |
| `environment_asset` | `gpu_environment` | sky cubemap 512^2 RGBA16F 6 mips, prefiltered cubemap 128^2 RGBA16F 6 mips, SH9 (27 floats, CB 128 B) |

Texture mips are generated on the CPU at import (box filter, sRGB-aware) and stored in `.ctex`, so
the GPU path is upload-only. Fallback textures: slot 0 white, 1 flat normal (0.5, 0.5, 1), 2 black,
3 magenta checker. A material whose texture id is invalid binds the matching fallback and clears its
`textureFlags` bit.

### 2.7 Render graph (private, `src/renderer/render_graph.h`)

```cpp
using graph_resource = std::uint16_t;
constexpr graph_resource INVALID_GRAPH_RESOURCE = 0xFFFFu;

// graph_texture_desc: width, height, arraySize, mipLevels (all std::uint32_t, default 1),
// rhi::texture_format format, bool isRenderTarget, isUnorderedAccess, isCubemap, const char *debugName.

class render_pass {
    public:
        virtual ~render_pass();
    public:
        [[nodiscard]] virtual const char *name() const noexcept = 0;
        virtual void setup(pass_builder &builder) = 0;
        virtual void execute(pass_context &context) = 0;
};
```

`pass_builder` offers `create_texture(desc)`, `create_buffer(desc)`, `read(resource)`,
`write(resource)`, `colour_target(resource, slot)`, `depth_target(resource)`, `side_effect()`.
`pass_context` offers `command_list()`, `texture(resource)`, `buffer(resource)`, `framebuffer()`
(from `framebuffer_cache`, keyed by attachment texture handles), `view_constants()`.
Transient textures are created with `keepInitialState = true` and `initialState = ShaderResource`
(render targets / depth) or `UnorderedAccess` (compute outputs) so NVRHI's tracking starts from a
known state without `beginTrackingTextureState` calls.

`transient_pool`: `std::unordered_map<std::uint64_t, std::vector<pooled_texture>>` keyed by
`hash(graph_texture_desc)` without the debug name; a texture is never handed out twice in one frame;
textures unused for 120 frames are released in `end_frame()`.

### 2.8 Draw lists (private, `draw_list_builder`)
`draw_item { std::uint64_t sortKey; std::uint32_t instanceIndex; }`. Opaque/masked key =
`[pipelineVariant:8][materialIndex:22][meshIndex:22][submesh:4][depthBucket:8]` (as implemented: the submesh
needs its own field so an overflow can never merge different meshes into one batch), ascending; runs with equal
(pipeline, material, mesh, submesh) collapse into one `drawIndexed` with `instanceCount > 1` and
`firstInstance` in push constants. Blended key = `[layer:8][inverseDepth:24][material:32]` (24-bit depth), no
instancing across different depths. Shadow lists reuse the builder with a `castsShadows` filter and
the cascade or tile frustum. Sorting uses `core::radix_sort_u64`.

---

## 3. Algorithms and flow

### 3.1 Frame (`renderer::render(const render_scene &scene)`)

1. `begin_frame`: `swapchain::acquire()` (if present), `device.run_garbage_collection()`,
   `transient_pool.end_frame()`, open the frame command list, `gpu_uploader.flush()` (texture/mesh/
   material/environment uploads and IBL bakes queued since the previous frame, bounded to 64 MB of
   payload per frame; the remainder waits), `timer_queries.resolve_previous()`.
2. `prepare`: resolve ids through `gpu_resource_cache`, write `gpu_instance` array (job-parallel
   fill of a CPU vector, one `writeBuffer`), build `gpu_light` array per view (clamp to
   `MAX_LIGHTS_PER_VIEW`, importance = `intensity * range^2 / distance^2`), allocate shadow atlas
   tiles (3.4), compute cascades per view containing a directional shadow caster.
3. For each view (max `MAX_VIEWS`): frustum-cull (3.2), build draw lists, build the graph (3.3),
   `compile()`, `execute()` into the frame command list.
4. Editor builds: `imgui_renderer::render(drawData, swapchainFramebuffer)` with the `ImDrawData *` handed over
   through `renderer::set_imgui_draw_data()` by `engine::imgui_layer`, which owns the single ImGui context (R9).
5. `end_frame`: close and `executeCommandLists`, `swapchain::present()`, publish `render_stats`.

### 3.2 Visibility
Frustum planes from `clipFromWorld`; plane-vs-AABB with centre/extent. `parallel_for` in chunks of
1024 instances when `meshes.size() > 2048`, otherwise serial; each job appends to its own
`small_vector`, concatenated afterwards (no atomics). Cascade culling uses 5 planes (no far plane)
so casters behind the camera still cast. Layer masks are tested first.

### 3.3 Pass list per view (declaration order = execution order; a pass whose outputs are unread and that writes no imported resource is culled)

| Pass | Type | Reads | Writes | Notes |
|---|---|---|---|---|
| `shadow_cascades` | GFX x4 | instances | `shadow_cascades` D32 array | position stream only; `ALPHA_MASK=1` draws bind stream 1 too for UV |
| `shadow_atlas` | GFX xN | instances | `shadow_atlas` D32 4096^2 | one viewport/scissor per tile |
| `depth_prepass` | GFX | instances | `depth` D32, `view_normals` RG16_SNORM (octahedral) | reverse-Z, `GreaterOrEqual` |
| `light_cull` | COMP | `depth`, lights | `tile_lists` (2 x 64 uint per tile), `tile_counts` | min/max depth per tile; list A depth-bounded (opaque), list B unbounded (blend + sprites) |
| `ssao_gtao` | COMP | `depth`, `view_normals` | `ao_half` R8 | half res |
| `ssao_blur` | COMP x2 | `ao_half`, `depth` | `ao_blurred` R8 | separable, depth-aware |
| `forward_opaque` | GFX | shadows, lists, AO, IBL, materials | `hdr` RGBA16F (depth loaded, `Equal` test) | opaque then masked |
| `skybox` | GFX | env cubemap | `hdr` | depth `GreaterOrEqual` against far; culled without environment |
| `forward_blend` | GFX | as opaque, list B | `hdr` | back-to-front, depth test on, write off |
| `world_sprites` | GFX | sprite vertices | `hdr` | premultiplied alpha, depth test on |
| `debug_draw` | GFX | debug vertices | `hdr` | depth-tested lines then overlay lines |
| `exposure_histogram` | COMP | `hdr` | `histogram` 256 uint | culled when manual exposure or `NO_POST_PROCESS` |
| `exposure_adapt` | COMP | `histogram`, `exposure_state` (imported, persistent) | `exposure_state` | one frame of latency, accepted |
| `bloom_down[0..5]` | COMP | `hdr` / previous mip | `bloom_mip[N]` | 13-tap, Karis on mip 0, starts at half res |
| `bloom_up[4..0]` | COMP | mip N+1, mip N | mip N | 3x3 tent, additive |
| `tonemap` | GFX | `hdr`, `bloom_mip[0]`, `exposure_state` | `ldr` RGBA8_SRGB | operator by uniform; `NONE` = clamp |
| `screen_sprites` | GFX | sprite vertices | `ldr` | pixel-space ortho, y down |
| `blit` | GFX | `ldr` | swapchain or `render_target` colour | culled when `ldr` is the target texture itself |

A pure-2D scene (no meshes, no lights, no environment) allocates only `hdr`, `ldr` and the sprite
buffer; everything else is culled. With `NO_POST_PROCESS`, `hdr` is still used (sprites may exceed
1.0) but exposure is forced to 1 and the operator to `NONE`. Post-processing stays on by default for world
sprites and debug lines, which draw into `hdr` before exposure, bloom and the tonemapper; screen sprites draw after
it. A 2D project opts out per camera with `camera_component::postProcess = false`, which sets `NO_POST_PROCESS` on
its view; the Grid Sweeper build script does so (sweep F403).

### 3.4 Shadows
Cascades (`cascade_builder`): splits `lerp(uniform, logarithmic, 0.7)` over
`[near, shadows.distance]` (default 100 m). Each slice: 8 frustum corners -> bounding sphere with
radius rounded up to 1 cm; orthographic light projection sized to the sphere; translation snapped to
`2r / resolution`; depth range padded by `shadows.casterPadding` (default 50 m) toward the light so
off-screen casters stay inside. Selection in the shader by view depth with a 10% blend band. Bias =
raster `depthBias = 2`, `slopeScaledDepthBias = 2.0f` plus normal offset `1.5 texels` in the shader.
Atlas (`shadow_atlas_allocator`): quadtree over 4096^2 with leaf sizes 1024, 512, 256. Each frame,
shadow-casting local lights are sorted by importance; the requested `shadowResolution` is granted
while space remains, otherwise halved down to 256, otherwise the light renders unshadowed
(`render_stats::shadowTilesDropped`). A light that keeps its size keeps its tile across frames
(stable map coordinates). The atlas is scene-wide and rendered once, by the first view that draws with
shadows; its casters are culled with the union of the layer masks of every view without `SKIP_SHADOWS`,
computed before the per-view loop, so the result does not depend on the view order: every view sees at least
its own casters' local-light shadows and may also see those of casters on layers only another view renders.
Cascades use each view's own mask (sweep-2026-11-12 F377). Planned: a per-camera opt-in to isolated atlas
culling for split-screen scenes whose halves are different worlds; the union stays the default.
Filtering (`shadows.hlsli`): `HARD` 1 comparison tap; `PCF` 16-tap
Poisson disk rotated by a per-pixel interleaved-gradient-noise angle, radius `shadows.pcfRadiusTexels`
(default 1.5); `PCSS` 16-tap blocker search -> penumbra `(dReceiver - dBlocker) * sourceRadius /
dBlocker` -> 16-tap PCF with that radius, clamped to 8 texels. All taps use `SampleCmpLevelZero`.

### 3.5 BRDF (`brdf.hlsli`)
Cook-Torrance: GGX NDF, height-correlated Smith visibility, Schlick Fresnel with
`F0 = lerp(0.04, baseColour, metallic)`; Lambert diffuse scaled by `(1 - F) * (1 - metallic)`;
roughness clamped to `[0.045, 1]`. Punctual lights: `radiance = intensity / d^2` windowed by
`saturate(1 - (d / range)^4)^2`; spot cone smoothstep between cos(outer) and cos(inner); directional
`radiance = intensity` (lux). Ambient: `irradianceSH9(n) * diffuseColour` and
`prefiltered.SampleLevel(r, roughness * 5) * (F0 * lut.x + lut.y)` with multiple-scattering
compensation. Material occlusion multiplies ambient only; SSAO multiplies ambient and specular
ambient. Emissive adds `emissiveFactor * emissiveStrength * emissiveTexture`. All inputs linear;
sRGB decode happens through `_SRGB` texture formats only.

### 3.6 IBL bake (`import/ibl_baker`, CPU, runs inside `environment_importer::import` on a worker; R34)
1. Equirect `.hdr` (stb_image float) -> 512^2 RGBA16F cubemap (bilinear equirect lookup per face texel), then a
   6-level box-filtered mip chain. Faces are independent, so the baker uses `job_system::parallel_for` over
   faces when a job system is available and loops inline otherwise; the output is bit-identical either way.
2. SH9 irradiance: integrate mip 4 (32^2 x 6 texels) with solid-angle weights into 27 floats.
3. GGX prefilter: 128^2 x 6 mips, importance sampling with a fixed Hammersley sequence (128 samples) and
   PDF-based source-mip selection; roughness = mip / 5. Deterministic: no clocks, no hash-map order, `float`
   math in a fixed evaluation order (the importer is a cache key, see `05_assets.md` D8).
4. `ibl_brdf_lut.hlsl`: 256^2 RG16F split-sum LUT baked once on the GPU at `renderer::create`, 1024 samples,
   device lifetime (recomputed on the null device as a no-op draw; the LUT texture still exists).
The importer writes cubemaps + SH9 into the cooked `.cenv`; loading is upload-only on every device, and the
null device produces identical draw lists and stats because nothing depends on GPU results.

### 3.7 GTAO
Half resolution. Per pixel: view position from reverse-Z depth, normal from `view_normals`, 2 slice
directions rotated by the 4x4 interleaved pattern (16-frame cycle), 4 horizon steps per side with
world radius `ssao.radius` (default 1.0 m) projected and clamped to 128 px, thickness heuristic 0.5,
cosine-weighted visibility integral, `pow(ao, ssao.intensity)`. Blur: 4-tap depth-aware per axis.
Forward shader upsamples with 4 bilinear taps weighted by depth proximity.

### 3.8 Exposure, bloom, tonemap
Histogram over log2 luminance in `[-10, +16]` EV, 16x16 groups with groupshared atomics.
`exposure_adapt`: mean of the 50th-90th percentile -> `EV100 = log2(L * 100 / 12.5)` +
`exposureCompensation`; adaptation `lerp(prev, target, 1 - exp(-dt * speed))` with separate
`adaptationSpeedUp` (3/s) and `adaptationSpeedDown` (1/s); `exposure = 1 / (1.2 * 2^EV100)`. Manual
mode writes `exposure` on the CPU and culls both passes. Bloom is threshold-free (Karis average kills
fireflies), `bloomIntensity` default 0.04. `tonemap.hlsl`: `colour = hdr * exposure + bloom *
intensity`, then `tonemap_aces` (Hill fit), `tonemap_reinhard_extended(whitePoint)`,
`tonemap_agx(look)` or `NONE`, then sRGB OETF unless the target format is `_SRGB`. The three curves
are pure functions in `tonemap.hlsli` restricted to `float`, `float3`, `float3x3`, `saturate`,
`pow`, `exp2`, `log2`, `max`, `min`, `dot`, `mul`, `lerp`; `tests/renderer/hlsl_shim.h` maps those to
glm so the same file compiles natively.

### 3.9 Sprites (`sprite_batcher`, pure CPU, public for tests)
Sorts by `(space, layer, order, texture)`, emits 4 vertices + 6 indices per sprite into a CPU array
uploaded with one `writeBuffer`, and yields `sprite_run { space, texture, firstIndex, indexCount }`;
one `drawIndexed` per run. `WORLD` vertices are world space (quad of `size` centred on the
transform); `SCREEN` vertices are pixel space, origin top-left, y down, projected with an
orthographic matrix from the viewport. Texture filter is the texture's `sampler_filter`
(`LINEAR` or `POINT` for pixel art). Capped at `MAX_SPRITES_PER_FRAME` with a once-per-change WARN that names
the dropped count per space. `SCREEN` sprites (the HUD) take their slots first, `min(screen count, cap)`, and the
remaining slots go to `WORLD` sprites in sort order, so the overflow drops the front-most world sprites and UI
never vanishes under a burst of world sprites (sweep-2026-11-12 F386).

### 3.10 Debug draw
`debug_draw_buffer` (in the write scene) collects `line/box/sphere/frustum/axes/grid` as
`gpu_debug_vertex` pairs into two lists (depth-tested, overlay). Timed entries (`durationSeconds >
0`) live in `debug_draw::timed_entries` owned by the engine's `debug_draw` facade, which calls
`tick(deltaSeconds)` and `append_to(render_scene&)` each frame; the facade guards its lists with a
mutex because physics and scripting may call it from jobs. The renderer uploads one vertex array
per frame and issues two `draw` calls.

### 3.11 ImGui backend
`imgui_renderer::create(rhi::device&, rhi::shader_library&)` builds the pipeline (`imgui.hlsl`,
premultiplied alpha, scissor, no depth), sets `ImGuiBackendFlags_RendererHasTextures`, and keeps a
`slot_map<imgui_texture>`; `ImTextureID` is the slot key. `render(ImDrawData *drawData,
nvrhi::IFramebuffer *target, nvrhi::ICommandList &commandList)`: for every `ImTextureData` in
`drawData->Textures` whose status is not `ImTextureStatus_OK`, handle `WantCreate` (create RGBA8
texture, upload, `SetTexID`, `SetStatus(OK)`), `WantUpdates` (upload `UpdateRect`),
`WantDestroy` (release once `UnusedFrames > 0`, `SetTexID(ImTextureID_Invalid)`,
`SetStatus(Destroyed)`); then write vertices/indices with `writeBuffer`, set projection via push
constants, and per `ImDrawCmd` set scissor + binding set and `drawIndexed`. `register_texture(
gpu_texture_id) -> ImTextureID` exposes render-target colour textures to the editor viewport. A target's
colour id is stable across `resize_render_target`: the resize waits for the GPU, points the id's cache entry at
the new texture (`gpu_resource_cache::replace_external_texture`) and retires the replacement's id, so the handle
the viewport registered once follows every panel resize (sweep-2026-11-12 F385).
Shutdown destroys everything in `ImGui::GetPlatformIO().Textures`. ImGuizmo needs nothing extra.

### 3.12 Shader build (`cmake/ConjureShaders.cmake`)

```cmake
conjure_add_shaders(
  TARGET conjure_shaders
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders
  INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/engine/shaders/shared
  SHADERS
    "forward_pbr.hlsl;vs_main;vertex;"
    "forward_pbr.hlsl;ps_main;pixel;ALPHA_MASK=0,1"
    "depth_prepass.hlsl;vs_main;vertex;"
    "depth_prepass.hlsl;ps_main;pixel;ALPHA_MASK=0,1"
    "shadow_depth.hlsl;vs_main;vertex;"
    "shadow_depth.hlsl;ps_main;pixel;ALPHA_MASK=1"
    "light_cull.hlsl;cs_main;compute;"
    ...
)
```

Each entry is `source;entry;stage;defines`, `defines` being a `|`-separated list of `NAME=v1,v2`
axes whose cartesian product is enumerated. Per permutation two custom commands run:

- DXIL (Windows, `CONJURE_RHI_D3D12`): `dxc -T <vs|ps|cs>_6_5 -E <entry> -HV 2018 -Zpr -WX
  -D CONJURE_D3D12=1 -D<defines> -I <include dirs> -Fo <out>.dxil` plus `-Zi -Qembed_debug` in
  Debug/Development and `-O3` otherwise, followed by a second invocation with `-MF <out>.d` for the
  depfile: dxc 1.8 treats `-MF` as a dependency-only mode and compiles nothing in that run (wave 2
  integration, `cmake/ConjureShaders.cmake`).
- SPIR-V (`CONJURE_RHI_VULKAN`), glslang default: `glslang -D -S <vert|frag|comp> -e <entry> -V
  --target-env vulkan1.2 --hlsl-iomap --auto-map-locations --shift-texture-binding 0
  --shift-sampler-binding 128 --shift-ubo-binding 256 --shift-uav-binding 384
  --shift-ssbo-binding 0 -DCONJURE_VULKAN=1 -D<defines> -I<include dirs> --depfile <out>.d
  -o <out>.spv` (`-g` in Debug, `-Os` otherwise). With `CONJURE_SPIRV_COMPILER=dxc` the equivalent
  is `dxc -spirv -fspv-target-env=vulkan1.2 -fvk-t-shift 0 all -fvk-s-shift 128 all -fvk-b-shift 256
  all -fvk-u-shift 384 all`.

The shifts equal NVRHI's default `VulkanBindingOffsets` (t=0, s=128, b=256, u=384); read-only
`StructuredBuffer` (SSBO class in glslang) shares the texture offset 0, which is where NVRHI binds
`StructuredBuffer_SRV`. Register `spaceN` is descriptor set N (`registerSpaceIsDescriptorSet =
true` everywhere). Push constants are declared as
`CONJURE_PUSH_CONSTANTS cbuffer draw_constants : register(b1, space0)` where the macro expands to
`[[vk::push_constant]]` under `CONJURE_VULKAN` and nothing under `CONJURE_D3D12`. NVRHI's Vulkan
backend flips Y via a negative viewport height, so shaders never invert Y. Shaders are written to
the HLSL 2018 subset both compilers accept: no templates, no wave intrinsics, no 16-bit types, no
`enum class`. The function writes `index.json` entries
`{ name, entry, stage, defines, dxil, spirv, sourceHash }` and makes `conjure_shaders` a dependency of
`conjure_renderer` and its tests. Missing compilers: `dxc` from the Windows SDK or a fetched DXC
release; `glslang` via FetchContent pinned to the last tag that ships the HLSL front-end.

---

## 4. Public API

All headers: `#pragma once`, Conjure + standard includes only, `/// <summary>` on public methods.

### 4.1 `engine/include/conjure/rhi/`

| Header | Contents |
|---|---|
| `log_category.h` | `CONJURE_LOG_CATEGORY_RHI` |
| `backend.h` | `enum class graphics_backend : std::uint8_t { NULL_DEVICE, D3D12, VULKAN }`, `struct device_capabilities` |
| `device.h` | `struct device_desc`, `class device` |
| `swapchain.h` | `struct swapchain_desc`, `class swapchain` |
| `texture_format.h` | `enum class texture_format : std::uint8_t` (UNKNOWN, R8_UNORM, RG8_UNORM, RGBA8_UNORM, RGBA8_SRGB, BGRA8_SRGB, RG16_SNORM, RGBA16_SNORM, R16_FLOAT, RG16_FLOAT, RGBA16_FLOAT, R32_FLOAT, RG32_FLOAT, RGB32_FLOAT, RGBA32_FLOAT, R32_UINT, R11G11B10_FLOAT, D32_FLOAT, BC1_SRGB, BC5_UNORM, BC7_SRGB), `bytes_per_pixel()`, `is_depth()`, `is_srgb()`, `is_compressed()` |
| `shader_library.h` | `struct shader_key`, `struct shader_blob`, `enum class shader_stage : std::uint8_t { VERTEX, PIXEL, COMPUTE }`, `class shader_library` |

```cpp
namespace nvrhi {
    class IDevice;
    class ITexture;
}

namespace conjure::rhi {

    struct device_desc {
        graphics_backend backend = graphics_backend::NULL_DEVICE;
        bool enableDebugLayer = false;             // D3D12 debug layer / Vulkan validation layers
        bool enableGpuValidation = false;
        std::uint32_t adapterIndex = 0;
        bool preferSoftwareAdapter = false;        // WARP (D3D12) / VK_DRIVER_FILES ICD (Vulkan); golden tests, area H
        native_window window = {};                 // platform::window::native_handle() copied field for field
                                                   // (native_window.h; rhi never includes platform); NONE = headless
    };

    struct device_capabilities {
        graphics_backend backend = graphics_backend::NULL_DEVICE;
        std::string adapterName = "";
        std::uint32_t maxTextureDimension2d = 16384;
        std::uint64_t dedicatedVideoMemoryBytes = 0;
        bool computeQueue = false;
        bool timerQueries = false;
    };

    class device {
        private:
            struct impl;
            std::unique_ptr<impl> mImpl = nullptr;

        public:
            device();
            ~device();

        public:
            /// <summary>
            /// Creates the NVRHI device for the requested backend. Fails with error::UNSUPPORTED when the
            /// backend is not compiled in and error::DEVICE_CREATION_FAILED when the API refuses.
            /// </summary>
            [[nodiscard]] static core::result<std::unique_ptr<device>> create(const device_desc &desc);

            [[nodiscard]] graphics_backend backend() const noexcept;
            [[nodiscard]] const device_capabilities &capabilities() const noexcept;

            /// <summary>True once the API reported device removal; the owner must recreate everything.</summary>
            [[nodiscard]] bool is_lost() const noexcept;

            /// <summary>Non-owning NVRHI device for in-engine modules. Never exposed to game code.</summary>
            [[nodiscard]] nvrhi::IDevice *native_device() const noexcept;

            void run_garbage_collection();
            void wait_idle();
    };

    struct swapchain_desc {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t bufferCount = 3;
        texture_format format = texture_format::RGBA8_SRGB;
        bool vsync = true;
    };

    class swapchain {
        private:
            struct impl;
            std::unique_ptr<impl> mImpl = nullptr;

        public:
            swapchain();
            ~swapchain();

        public:
            [[nodiscard]] static core::result<std::unique_ptr<swapchain>> create(
              device &device,
              const native_window &window,        // the engine copies window.native_handle() into it (wave 4)
              const swapchain_desc &desc
            );

            /// <summary>Acquires the next back buffer. Fails with error::DEVICE_LOST or error::OUT_OF_DATE.</summary>
            [[nodiscard]] core::result<std::uint32_t> acquire();
            [[nodiscard]] core::result<void> present();
            [[nodiscard]] core::result<void> resize(std::uint32_t width, std::uint32_t height);
            [[nodiscard]] nvrhi::ITexture *back_buffer(std::uint32_t index) const noexcept;
            [[nodiscard]] std::uint32_t buffer_count() const noexcept;
            [[nodiscard]] std::uint32_t current_index() const noexcept;
    };

    struct shader_key {
        std::string_view name = "";        // "forward_pbr"
        std::string_view entry = "";       // "ps_main"
        std::string_view defines = "";     // "ALPHA_MASK=1" (axis order as declared in CMake, '|' separated)
    };

    class shader_library {
        private:
            struct impl;
            std::unique_ptr<impl> mImpl = nullptr;

        public:
            shader_library();
            ~shader_library();

        public:
            /// <summary>Opens shaders/index.json and selects the blob set for the device's backend.</summary>
            [[nodiscard]] static core::result<std::unique_ptr<shader_library>> open(
              device &device,
              const std::filesystem::path &shaderDirectory
            );

            /// <summary>Returns the compiled blob, loading it lazily. Fails with error::NOT_FOUND; never null.</summary>
            [[nodiscard]] core::result<shader_blob> get(const shader_key &key);

            /// <summary>Re-reads index.json and drops cached blobs (development hot reload).</summary>
            [[nodiscard]] core::result<void> reload();
    };
}
```

`shader_blob { std::span<const std::byte> bytes; shader_stage stage; }` is valid until `reload()`.
Backend selection: `D3D12` on Windows by default, `VULKAN` elsewhere; the engine falls back to the
next compiled backend and finally `NULL_DEVICE` in headless mode.

### 4.2 `engine/include/conjure/renderer/`

| Header | Contents |
|---|---|
| `log_category.h` | `CONJURE_LOG_CATEGORY_RENDERER` |
| `limits.h` | constants from 2.1 |
| `gpu_ids.h` | `gpu_mesh_id`, `gpu_texture_id`, `gpu_material_id`, `gpu_environment_id`, `render_target_id` |
| `camera.h` | `enum class projection_type : std::uint8_t { PERSPECTIVE, ORTHOGRAPHIC }`, `struct camera`, projection helpers |
| `light.h` | `light_type`, `shadow_filter`, `struct light` |
| `render_settings.h` | `enum class tonemap_operator : std::uint8_t { ACES_FITTED, REINHARD_EXTENDED, AGX, NONE }`, `enum class exposure_mode : std::uint8_t { AUTOMATIC, MANUAL }`, `struct post_process_settings`, `struct shadow_settings`, `struct ssao_settings`, `struct environment_settings` |
| `render_scene.h` | `mesh_instance`, `sprite`, `sprite_space`, `view_flags`, `render_view`, `render_scene`, `class render_scene_pair` |
| `render_target.h` | `struct render_target_desc { math::uvec2 size; rhi::texture_format colourFormat = RGBA8_SRGB; bool withDepth = true; }`, `class render_target` |
| `renderer.h` | `struct renderer_desc`, `struct render_stats`, `class renderer` |
| `mesh_asset.h` | `struct mesh_streams`, `struct submesh`, `class mesh_asset` (`cpu_geometry()` keeps positions + indices in Debug/Development for the editor's CPU picking, R11) |
| `texture_asset.h` | `enum class texture_colour_space : std::uint8_t { SRGB, LINEAR }`, `enum class sampler_filter : std::uint8_t { LINEAR, POINT }`, `struct texture_mip`, `class texture_asset` |
| `material_asset.h` | `enum class alpha_mode : std::uint8_t { SOLID, MASK, BLEND }` (`SOLID` is glTF's `OPAQUE`, a Windows macro name, CONVENTIONS 3), `struct material_desc`, `class material_asset` |
| `environment_asset.h` | `using sh9 = std::array<math::vec3, 9>;`, `class environment_asset` |
| `shader_asset.h` | `struct shader_program_desc { std::string name; std::string vertexEntry; std::string pixelEntry; }`, `class shader_asset` (v1: built-in programs only, `validate(rhi::shader_library&)`) |
| `model_asset.h` | `struct model_node`, `class model_asset` (glTF node tree that `scene` turns into a prefab) |
| `gltf_importer.h` | `struct gltf_import_options`, `struct gltf_import_result`, `class gltf_importer` |
| `components.h` | `mesh_renderer_component`, `sprite_component`, `camera_component`, `light_component`, `environment_component`, `register_components()` |
| `debug_draw.h` | `struct debug_vertex`, `class debug_draw_buffer`, `class debug_draw` (facade with timed entries) |
| `sprite_batch.h` | `struct sprite_vertex`, `struct sprite_run`, `class sprite_batcher` |
| `imgui_renderer.h` | `class imgui_renderer` (`#if !CONJURE_DISTRIBUTION`; forward-declares `ImDrawData`) |

```cpp
namespace conjure::renderer {

    struct camera {
        projection_type projection = projection_type::PERSPECTIVE;
        float verticalFovRadians = 1.0472f;       // 60 degrees
        float orthographicHeight = 10.0f;         // world units visible vertically
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
    };

    /// <summary>Reverse-Z projection: near maps to depth 1, far to depth 0, for both projection types.</summary>
    [[nodiscard]] math::mat4 clip_from_view(const camera &camera, float aspectRatio) noexcept;
    [[nodiscard]] math::frustum frustum_from_clip(const math::mat4 &clipFromWorld) noexcept;
    [[nodiscard]] math::ray ray_from_viewport(
      const camera &camera,
      const math::mat4 &worldFromView,
      math::vec2 viewportUv,
      f32 aspectRatio                      // viewport width over height (a non-square viewport cannot be unprojected without it)
    ) noexcept;

    struct renderer_desc {
        rhi::device *device = nullptr;            // non-owning, must outlive the renderer
        rhi::swapchain *swapchain = nullptr;      // null = headless (views must target render targets)
        rhi::shader_library *shaders = nullptr;   // non-owning
        core::jobs::job_system *jobs = nullptr;   // null = serial culling/sorting (R30)
        std::uint32_t cascadeResolution = 2048;
        std::uint32_t atlasResolution = 4096;
        bool enableImGui = true;                  // ignored in Distribution
    };

    // render_stats: std::uint32_t viewsRendered, passesExecuted, passesCulled, drawCalls, instancesDrawn,
    // instancesCulled, trianglesDrawn, lightsRendered, shadowTilesAllocated, shadowTilesDropped,
    // spritesDrawn, uploadsCompleted, fallbacksUsed, transientTexturesLive, transientTexturesCreated;
    // std::uint64_t uploadBytes;
    // core::small_vector<const char *, MAX_VIEWS * MAX_PASSES_PER_VIEW> passNames (executed, in order);
    // core::small_vector<pass_timing, 48> passTimings (name + GPU milliseconds, Development builds).

    class renderer {
        private:
            struct impl;
            std::unique_ptr<impl> mImpl = nullptr;

        public:
            renderer();
            ~renderer();

        public:
            /// <summary>
            /// Creates every pipeline, binding layout, fallback resource and the BRDF LUT up front. A missing
            /// shader permutation fails creation with error::SHADER_NOT_FOUND naming the key.
            /// </summary>
            [[nodiscard]] static core::result<std::unique_ptr<renderer>> create(const renderer_desc &desc);

            /// <summary>
            /// Queues an upload; safe from any thread. The payload bytes are copied into the upload record at
            /// enqueue time, so the caller has no lifetime obligation (R10). Called by the engine's
            /// renderer_subsystem from asset_manager LOADED / RELOADED events; release_* from UNLOADED.
            /// </summary>
            [[nodiscard]] gpu_mesh_id upload_mesh(assets::asset_id id, const mesh_asset &mesh);
            [[nodiscard]] gpu_texture_id upload_texture(assets::asset_id id, const texture_asset &texture);
            [[nodiscard]] gpu_material_id upload_material(assets::asset_id id, const material_asset &material);
            [[nodiscard]] gpu_environment_id upload_environment(assets::asset_id id, const environment_asset &env);
            void release_mesh(gpu_mesh_id id);
            void release_texture(gpu_texture_id id);
            void release_material(gpu_material_id id);
            void release_environment(gpu_environment_id id);

            /// <summary>Returns the gpu id previously uploaded for the asset, or an invalid id.</summary>
            [[nodiscard]] gpu_mesh_id find_mesh(assets::asset_id id) const noexcept;
            [[nodiscard]] gpu_texture_id find_texture(assets::asset_id id) const noexcept;
            [[nodiscard]] gpu_material_id find_material(assets::asset_id id) const noexcept;
            [[nodiscard]] gpu_environment_id find_environment(assets::asset_id id) const noexcept;

            [[nodiscard]] core::result<render_target_id> create_render_target(const render_target_desc &desc);
            [[nodiscard]] core::result<void> resize_render_target(render_target_id id, math::uvec2 size);
            void destroy_render_target(render_target_id id);
            [[nodiscard]] render_target *get_render_target(render_target_id id) noexcept;

            /// <summary>Clears the write slot of the owned render_scene_pair and returns it for EXTRACT (R8).</summary>
            [[nodiscard]] render_scene &begin_extract();

            /// <summary>Swaps the pair; the extracted scene is immutable from here on.</summary>
            void end_extract();

            /// <summary>Renders the last extracted scene: render(pair.read()). Main/render thread only.</summary>
            [[nodiscard]] core::result<void> render_frame();

            /// <summary>Renders one scene. Render thread only. Fails only with rhi_error::DEVICE_LOST.</summary>
            [[nodiscard]] core::result<void> render(const render_scene &scene);

            /// <summary>Resizes the swapchain: waits for the GPU, drops the framebuffers over the back buffers,
            /// then swapchain::resize. The engine calls it for a framebuffer resize (code sweep F044: a
            /// swapchain cannot recreate buffers that are still referenced).</summary>
            [[nodiscard]] core::result<void> resize_swapchain(math::uvec2 size);

            /// <summary>Device loss: stops recording and drops GPU mirrors; the engine then quits (R8).</summary>
            void on_device_lost();

            /// <summary>Editor builds: draw data produced by engine::imgui_layer, consumed in step 4 of 3.1 (R9).</summary>
            void set_imgui_draw_data(ImDrawData *drawData) noexcept;

            /// <summary>Registers this module's asset types and importers; called by the engine's renderer_subsystem.</summary>
            static void register_asset_types(assets::asset_type_registry &types, assets::importer_registry &importers);

            /// <summary>Synchronous readback (waits for idle). Used for screenshots and the AI agent.</summary>
            [[nodiscard]] core::result<core::image> read_back(render_target_id target);

            [[nodiscard]] const render_stats &stats() const noexcept;
            [[nodiscard]] imgui_renderer *imgui() noexcept;     // null in Distribution or when disabled
    };

    class gltf_importer {
        public:
            gltf_importer();
            ~gltf_importer();

        public:
            /// <summary>
            /// Imports .gltf/.glb into CPU assets. Unsupported extensions are warned and skipped. Fails only on
            /// parse errors, missing buffers, non-triangle topology or unsupported accessor layouts.
            /// </summary>
            [[nodiscard]] core::result<gltf_import_result> import(
              const std::filesystem::path &sourcePath,
              const gltf_import_options &options
            ) const;
    };
}
```

`material_desc` carries the glTF factors with glTF defaults (`baseColourFactor` 1, `emissiveFactor` 0,
`emissiveStrength` 1, `metallicFactor` 1, `roughnessFactor` 1, `normalScale` 1, `occlusionStrength` 1,
`alphaCutoff` 0.5, `alphaMode` OPAQUE, `doubleSided` false, `unlit` false) and five
`assets::asset_ref<texture_asset>` slots (base colour, metallic-roughness, normal, occlusion,
emissive). `gltf_import_options { bool generateTangents = true; float scale = 1.0f; bool importCameras
= false; }`. `gltf_import_result` holds `std::vector` of `mesh_asset`, `material_asset`,
`texture_asset`, a `model_asset` (node tree referencing them by index) and `warnings`.

glTF mapping: node -> `model_node` (TRS or matrix decomposed); primitive -> one `submesh`
(triangles, strips and fans triangulated, points/lines rejected with `error::UNSUPPORTED`);
attributes POSITION, NORMAL, TANGENT, TEXCOORD_0, TEXCOORD_1, COLOR_0 (missing normals computed
flat per face, missing tangents via UV derivatives + Gram-Schmidt, missing colours white); indices
widened to `R32_UINT`; `pbrMetallicRoughness` -> `material_desc` 1:1 (metallic in B, roughness in G
as glTF defines, no swizzle, occlusion in R); `normalTexture.scale`, `occlusionTexture.strength`,
`emissiveFactor`, `KHR_materials_emissive_strength`, `alphaMode/alphaCutoff/doubleSided`,
`KHR_materials_unlit` -> `unlit`, `KHR_lights_punctual` -> `model_node::light` (a point or spot light without
`range`, infinite in glTF, gets `sqrt(intensity / GLTF_LIGHT_RANGE_CUTOFF)`, where `intensity / d^2` falls to
0.01 lux, about 32 m for 10 cd, clamped to `[GLTF_MIN_LIGHT_RANGE, GLTF_MAX_LIGHT_RANGE]` = [0.1, 100] m because
the range is also the shadow far plane, the importance weight and the light-culling extent; a warning names it;
fixed at import, so a later intensity change does not move it; directional lights ignore range; sweep-2026-11-12
F392); base colour and emissive textures import as `SRGB`, all others `LINEAR`; samplers map to
`LINEAR`/`POINT` + wrap mode on the texture asset (`wrapS`; a sampler whose `wrapT` differs warns, since
`.ctex` holds one wrap mode); skins, animations, `KHR_texture_transform` warn and skip. Any change to the
importer's output bumps `GLTF_IMPORTER_VERSION` (2 since the derived range).

`components.h` holds plain structs registered with `core::reflect` through
`register_components()`; the renderer does not depend on `ecs`. The engine's `renderer_subsystem` registers them
as ECS components (the `CONJURE_COMPONENT` name specialisations live in `renderer_subsystem.cpp`) and owns the
`engine.render_sync` (SYNC) and `engine.render_extract` (EXTRACT) systems (R7). `mesh_renderer_component`
(`asset_ref<mesh_asset> mesh`, `std::array<asset_ref<material_asset>, MAX_MATERIAL_SLOTS>
materials`, `layerMask = 1`, `castsShadows = true`, `receivesShadows = true`); `sprite_component`
(`asset_ref<texture_asset> texture`, `uvRect`, `colour`, `size`, `space`, `layer`, `order`, same
defaults as `sprite`); `camera_component` (`camera cameraData`, `layerMask = 0xFFFFFFFFu`,
`priority = 0`, `isMain = true`, `postProcess = true`); `light_component` (`light lightData`);
`environment_component` (`asset_ref<environment_asset> environment`, `intensity = 1.0f`,
`rotationRadians = 0.0f`). The engine's extraction system maps these 1:1 onto the `render_scene`
structs (2.3), resolving asset refs to gpu ids through `renderer::find_*`.

### 4.3 Files

```
engine/include/conjure/rhi/{log_category,backend,device,swapchain,texture_format,shader_library}.h
engine/src/rhi/{device,swapchain,shader_library,message_callback,texture_format}.cpp  README.md
engine/src/rhi/backends/{d3d12,vulkan}/{device,swapchain}_<backend>.{h,cpp}
engine/src/rhi/backends/null/{null_device,null_resources,null_command_list,null_swapchain}.{h,cpp}
engine/include/conjure/renderer/<4.2 table>
engine/src/renderer/{renderer,render_graph,pass_builder,transient_pool,framebuffer_cache,pipeline_cache,
  binding_layouts,gpu_types,gpu_uploader,gpu_resource_cache,draw_list_builder,culling,render_target,
  timer_ring,render_scene,camera,debug_draw,sprite_batcher}.{h,cpp}  README.md
engine/src/renderer/passes/<one .h/.cpp per pass row in 3.3>   shadows/{cascade_builder,shadow_atlas_allocator}
engine/src/renderer/{imgui/imgui_renderer,import/{gltf_importer,tangent_generator,image_loader,mip_generator,
  ibl_baker},assets/<one .cpp per asset header>}
engine/shaders/shared/{common,gpu_types,brdf,shadows,tonemap,colour,noise,octahedral}.hlsli
engine/shaders/<one .hlsl per pass in 3.3 + imgui + ibl_brdf_lut>
engine/shaders/CMakeLists.txt   cmake/ConjureShaders.cmake
tests/rhi/*.cpp  tests/renderer/*.cpp  tests/renderer/hlsl_shim.h
tests/renderer/fixtures/{quad.gltf,box_textured.glb,alpha_mask.gltf,strips.gltf,small.hdr,checker.png}
```

CMake: `conjure_add_module(rhi SOURCES ... PUBLIC_DEPS conjure_core conjure_platform PRIVATE_DEPS
conjure::thirdparty::nvrhi [nvrhi_d3d12] [nvrhi_vk])`; `conjure_add_module(renderer SOURCES ...
PUBLIC_DEPS conjure_core conjure_rhi conjure_assets PRIVATE_DEPS conjure::thirdparty::nvrhi
conjure::thirdparty::cgltf conjure::thirdparty::stb conjure::thirdparty::imgui)`;
`add_dependencies(conjure_renderer conjure_shaders)`.

---

## 5. Threading and ownership

- **Render thread** (main thread in v1; area F may move it): `rhi::device` creation, `swapchain`,
  `renderer::create/render/destroy`, `imgui_renderer`, every NVRHI object creation, all command
  recording. `render()` asserts the owner thread in Debug.
- **Any thread**: `renderer::upload_*` / `release_*` push `{assetId, owned payload bytes, generation}` onto
  a mutex-guarded queue and return a pre-allocated id immediately (the slot is created under the
  same mutex). Payload bytes are copied at enqueue time, so no caller lifetime rule exists (R10);
  `render_stats::uploadsCompleted` is informational. `debug_draw` facade: mutex-guarded. `find_*`: lock-free read of the id map
  is not guaranteed; they are documented render-thread-only (extraction runs on the main thread or
  on jobs synchronised by the engine before `render()`).
- **Jobs inside `render()`**: culling, sort-key generation, `gpu_instance` fill, sprite batching
  write into pre-sized arrays over disjoint ranges via `core::jobs::parallel_for`; the render thread
  joins before recording. Nothing touches NVRHI from a job.
- **Ownership**: the engine `application` owns `rhi::device`, `rhi::swapchain`, `rhi::shader_library` and
  the `renderer` (which owns its `render_scene_pair`, R8), destroyed in reverse order (the renderer asserts its device is
  still alive in its destructor and calls `wait_idle()` first). `renderer::impl` owns every
  `nvrhi::*Handle` (intrusive `RefCountPtr`), the pools and caches. CPU assets are owned by the
  assets module; GPU mirrors by `gpu_resource_cache`; releases are deferred `bufferCount + 1` frames.
  Users receive `asset_ref<T>` and gpu ids, never raw GPU handles.

---

## 6. Failure handling

| Situation | Behaviour |
|---|---|
| Backend not compiled in / API refuses | `device::create` -> `error::UNSUPPORTED` / `error::DEVICE_CREATION_FAILED` with the backend message; engine tries the next backend, then `NULL_DEVICE` in headless mode. |
| Device removed (`DXGI_ERROR_DEVICE_REMOVED`, `VK_ERROR_DEVICE_LOST`, NVRHI `Fatal` message) | `device::is_lost()` becomes true; `swapchain::present` / `renderer::render` return `rhi_error::DEVICE_LOST`; the engine logs, calls `renderer::on_device_lost()` (recording stops, GPU mirrors dropped) and quits with `exit_code::DEVICE_LOST`. Device recreation and re-upload is a v2 item (R8, `06_engine_loop.md` 6). |
| Missing shader / permutation / pipeline creation failure | `renderer::create` fails with `error::SHADER_NOT_FOUND` or `error::PIPELINE_CREATION_FAILED` naming the key. Nothing is lazily created at frame time. |
| glTF parse error, missing buffer, bad accessor, non-triangle topology | `gltf_importer::import` -> `error_code::PARSE_FAILURE` / `error_code::NOT_FOUND` / `error_code::UNSUPPORTED`; unknown extensions are `warnings`, never failures. |
| Texture decode failure, zero-sized image, > `maxTextureDimension2d` | `texture_asset::load_from_file` -> `error::DECODE_FAILED` / `error::INVALID_ARGUMENT`. |
| Stale or invalid gpu id in `render_scene` | Fallback object + `WARN` once per id; `render_stats::fallbacksUsed` increments. |
| More than `MAX_LIGHTS_PER_VIEW`, `MAX_VIEWS`, `MAX_SPRITES_PER_FRAME`, `MAX_DEBUG_VERTICES` | Clamped by importance / order with a `WARN` once per change of count; `CONJURE_ASSERT` in Debug. |
| Shadow atlas exhausted | Least important lights render unshadowed; `shadowTilesDropped` counts them. |
| Upload budget exceeded in a frame | Remaining uploads wait for the next frame; the asset keeps its fallback until then. |
| Graph validation (read before write, write to an unknown resource, missing depth target for a depth-tested pass) | `CONJURE_ASSERT` in Debug; in Development/Distribution the pass is skipped and `ERROR` logged once. |
| Readback on the null device | Zero-filled `core::image` of the right size; never an error. |
| NVRHI validation / runtime messages | `message_callback` routes `Info/Warning/Error` to `CONJURE_LOG_*(RHI, ...)`; `Fatal` also sets `is_lost()`. |

No frame-time path returns an error other than `render()` on device loss; everything else is
clamped and logged, so a bad scene never aborts a frame.

---

## 7. Headless and null behaviour

- `renderer_desc::swapchain == nullptr`: no swapchain; every `render_view` must target a
  `render_target` (views targeting the swapchain are skipped with a `WARN`). With a real backend this
  is the editor's `--headless --exec` screenshot path (`read_back`).
- `graphics_backend::NULL_DEVICE`: `null_device` implements every `nvrhi::IDevice` method, storing
  descriptors and returning valid handles; `queryFeatureSupport` returns true for `ComputeQueue`,
  `CopyQueue`, `ConstantBufferRanges`, `DeferredCommandLists`, `VirtualResources` and
  `WaveLaneCountMinMax` (`Feature::TimerQueries` does not exist in the pinned NVRHI; timer queries
  are created and read as objects, `getTimerQueryTime` returns 0); `executeCommandLists` returns
  incrementing fence values; `mapStagingTexture` returns zero-filled memory. `null_swapchain` has `bufferCount`
  stub textures and `acquire` cycles indices. `null_device::stats()` exposes `rhi::null_device_stats`
  (resource creations by type, command-list submissions, draws, dispatches, debug-marker pass names) for
  area H's recorder-style assertions.
- On the null device the entire CPU path (extraction contract, uploads, id resolution, culling,
  cascades, atlas allocation, sorting, batching, graph compile/cull, pipeline creation from real
  compiled blobs, exposure math, glTF import, CPU IBL bake) runs identically and `render_stats` is
  fully populated; tests assert on it.
- `conjure_renderer_tests` depends on `conjure_shaders` so `index.json` and the blobs exist; the
  null device validates blob presence (non-empty) but does not parse DXIL/SPIR-V.

---

## 8. Integration points

**Needs** (cross-area; see assumptions list at the end)

- `core`: `result<T>`/`error` (every `error::X` in this document denotes `rhi_error::X` or `renderer_error::X`,
  each module's own `enum class` behind its `make_error`, R30), `log`, `CONJURE_ASSERT/VERIFY`,
  `math` (`vec2/3/4`, `uvec2`, `mat3/4`, `transform`, `aabb`, `frustum`, `ray`, `urect`),
  `slot_map`, `small_vector`, `radix_sort_u64`, `jobs::job_system::parallel_for`, `memory::frame_allocator`,
  `json`, `vfs`, `image` (RGBA8 CPU image used by `read_back`), `reflect` for component registration.
- `platform`: `window::native_handle()` (opaque per-OS struct: HWND, X11/Wayland, NSWindow),
  `window::required_vulkan_instance_extensions()` and `window::create_vulkan_surface(void *instance,
  void **outSurface)` wrapping `glfwGetRequiredInstanceExtensions` / `glfwCreateWindowSurface` with
  opaque pointers (no Vulkan headers in `platform`), `framebuffer_resized` event.
- `assets`: `asset_id` (uuid), `asset_ref<T>`, `asset_type_registry`, `importer_registry`
  (`renderer::register_asset_types` registers `gltf_importer` for `.gltf/.glb`, `texture_importer` for
  `.png/.jpg/.tga`, `material_importer` for `.cmat`, `environment_importer` for `.hdr` -> `.cenv`; the engine's
  `renderer_subsystem` calls it), cooked pack reader/writer (renderer defines payloads `.cmesh`, `.ctex`,
  `.cmat` json, `.cenv`, `.cmodel`), `asset_manager` events: the renderer's loaders are CPU-only (`load_cpu`
  decodes, `finalise` is trivial) and the engine's `renderer_subsystem` calls `upload_*` on LOADED/RELOADED
  (same `asset_id`; the renderer swaps the GPU mirror and bumps the generation) and `release_*` on UNLOADED (R10).
- `scene` (wave 3): `render_world_transform` per entity for extraction (`06_engine_loop.md` 3.2); prefab
  instantiation from `model_asset`.
- `engine` (wave 4): owns device/swapchain/shader_library/renderer; registers the renderer component types into
  the ECS (R7); owns `engine.render_sync` (SYNC) and `engine.render_extract` (EXTRACT: queries
  `(render_world_transform, mesh_renderer_component)` etc., `allocate_meshes`, job-parallel fill,
  `camera_component` -> `render_view` with aspect from the target, `light_component` -> `light`,
  `environment_component` -> `environment_settings`, debug draw `append_to`); runs
  `begin_extract -> EXTRACT -> end_extract -> render_frame -> stats`, resize, and the device-loss shutdown (R8).
- `editor`: viewport `render_target` + synchronous `read_back` for `view.screenshot` (R12),
  `imgui_renderer::register_texture` for the viewport image, `mesh_asset::cpu_geometry()` for CPU picking (R11),
  gizmo rays via `ray_from_viewport`, profiler panel reading `render_stats`. The ImGui context is owned by
  `engine::imgui_layer`, not by this module (R9).

**Offers**: `render_scene` contract; `renderer::upload_*/find_*` for every GPU asset type;
`render_target` + `read_back` for the editor viewport and the AI agent's screenshots;
`imgui_renderer`; `debug_draw` for physics/navigation/editor; `sprite_batcher` and components for
`bindings` (C#); `render_stats` for the profiler; importers for area E; `conjure_add_shaders()`
reusable by game modules for their own compute shaders.

---

## 9. Implementation plan (ordered; each step lands with its tests and compiles warning-free)

1. **rhi/texture_format + backend + log_category**: enums, `bytes_per_pixel`, NVRHI format mapping table in `src/rhi/texture_format.cpp`.
2. **rhi/null backend**: `null_device`, `null_resources`, `null_command_list`, `null_swapchain`; pin the NVRHI commit in `Dependencies.cmake`.
3. **rhi/device + message_callback + swapchain** on the null backend; then `d3d12` and `vulkan` backends (device creation, DXGI/VkSwapchainKHR, back-buffer wrapping via `createHandleForNativeTexture`, resize, device-lost detection).
4. **cmake/ConjureShaders.cmake** + `engine/shaders/CMakeLists.txt` with one trivial shader; `rhi/shader_library` reading `index.json`.
5. **renderer public data types**: `limits`, `gpu_ids`, `camera`, `light`, `render_settings`, `render_scene` + `render_scene_pair`, `debug_draw`, `sprite_batcher` (all CPU, fully unit-tested before any GPU code).
6. **Assets (CPU side)**: `mesh_asset` (streams, packing, bounds, `.cmesh`), `texture_asset` (stb decode, CPU mips, `.ctex`), `material_asset` (json), `environment_asset` (`.cenv`), `model_asset`, `shader_asset`; `gltf_importer` + `tangent_generator`.
7. **Renderer core**: `gpu_types` (static_asserts against `gpu_types.hlsli`), `binding_layouts`, `pipeline_cache`, `framebuffer_cache`, `transient_pool`, `render_graph` + `pass_builder`, `gpu_uploader`, `gpu_resource_cache` with fallbacks, `renderer::create` with no passes; `test_renderer_null_frame` renders an empty scene.
8. **Geometry path**: `culling`, `draw_list_builder`, `depth_prepass`, `forward_pass` with constant ambient, `skybox_pass`, `blit_pass`, `render_target`, `tonemap_pass` with `NONE`; first image on a real device (`samples/feature_test` early).
9. **Lighting**: `light_cull_pass`, punctual lights in `forward_pbr.hlsl`, `brdf.hlsli`.
10. **Shadows**: `cascade_builder`, `shadow_atlas_allocator`, `shadow_passes`, `shadows.hlsli` (HARD/PCF/PCSS).
11. **IBL**: CPU `ibl_baker` inside the environment importer (cubemap, SH9, GGX prefilter on jobs; R34), GPU BRDF LUT at init, ambient terms in the forward shader.
12. **SSAO**: `ssao_pass` (GTAO + blur) and bilateral upsample in the forward shader.
13. **HDR chain**: `exposure_pass`, `bloom_pass`, full `tonemap_pass` + `tonemap.hlsli` CPU reference test.
14. **2D + debug**: `sprite_pass` (world/screen), `debug_draw_pass`, `NO_POST_PROCESS` path.
15. **Editor support**: `imgui_renderer` with the 1.92 texture protocol and `set_imgui_draw_data`, `mesh_asset::cpu_geometry()`, timer queries into `render_stats`.
16. **Hardening**: device-loss recovery test on the null device (simulated `Fatal` message), upload budget, benchmarks, `README.md` for both modules.

---

## 10. Tests (doctest, all on `NULL_DEVICE`; `tests/rhi/`, `tests/renderer/`)

rhi: `test_texture_format` (bytes per pixel and `is_depth/is_srgb` for every enumerator, NVRHI mapping round trip),
`test_null_device` (create/destroy, capabilities, 100 execute/garbage-collect cycles, every resource type creatable, `is_lost` false),
`test_device_lost` (injected `Fatal` message sets `is_lost`), `test_null_swapchain` (acquire cycles indices, present ok, resize to 0 rejected, back buffers non-null),
`test_shader_library` (index parse, hit, unknown name/entry/defines -> `NOT_FOUND`, corrupt index -> `PARSE_FAILURE`, reload drops cache).

renderer: `test_camera` (reverse-Z near -> 1 and far -> 0 for perspective and ortho, aspect handling, frustum planes contain known points, viewport ray hits the expected world point),
`test_render_scene_pair` (swap semantics, `allocate_meshes` from 8 jobs covers every slot exactly once, `clear` keeps capacity),
`test_culling` (inside/outside/straddling boxes, 5-plane cascade mode, empty input, 10,000 random boxes equal a scalar reference, layer mask),
`test_draw_list_builder` (sort key ordering, instancing merge count, blended back-to-front, mixed opaque/masked/blend partition, 10k items batch count bound),
`test_cascade_builder` (splits monotonic and cover `distance`, lambda 0/1 equal uniform/log, snapping: half-texel camera translation gives identical light-space origin, all slice corners inside each cascade),
`test_shadow_atlas_allocator` (sizes honoured, point light takes 6 tiles, exhaustion halves then drops, stability across frames, empty input),
`test_render_graph` (declaration order kept, dead pass culled, imported-target pass never culled, read-before-write asserts, lifetimes `[first,last]` correct, framebuffer cache hits, 2D-only scene allocates only `hdr`/`ldr`),
`test_transient_pool` (same desc reused across frames, different debug names share, no reuse within a frame, eviction after 120 frames, `transientTexturesCreated` zero on frame 2),
`test_gpu_types` (`sizeof`/offsets equal the HLSL constants),
`test_mesh_asset` (bounds, normal/tangent packing round trip within 1e-3, tangent generation on a quad yields +X, `.cmesh` round trip, out-of-range index rejected),
`test_texture_asset` (mip count, sRGB vs linear format, 1x1 edge, box filter of a checker is mid-grey in linear space, decode failure -> error, `.ctex` round trip),
`test_material_asset` (defaults equal glTF defaults, json round trip, texture flags from missing refs),
`test_environment_asset` (CPU bake of a constant environment returns the radiance in SH band 0 and a constant prefiltered chain, two bakes are byte-identical, inline and 8-worker bakes are byte-identical, `.cenv` round trip),
`test_gltf_importer` (fixtures: node tree, material factors, missing normals/tangents generated, strips triangulated, unsupported extension warns, missing buffer -> error, points -> `UNSUPPORTED`, 16-bit indices widened),
`test_sprite_batcher` (sort by space/layer/order/texture, run count, pixel-space transform, cap at `MAX_SPRITES_PER_FRAME` dropping world sprites before screen sprites),
`test_debug_draw` (box emits 24 vertices, sphere/frustum counts, timed entries expire, concurrent append from 8 jobs),
`test_tonemap_reference` (ACES/Reinhard/AgX monotonic, 0 -> 0, large input <= 1, Reinhard reaches 1 at the white point, `NONE` is identity below 1),
`test_exposure_math` (EV100 <-> luminance, `exposure` formula, adaptation converges with up/down speeds),
`test_pipeline_cache` (variant count equals the CMake permutation list, key equality),
`test_gpu_resource_cache` (upload from a worker thread resolves after one frame, stale generation -> fallback and one WARN, deferred release after `bufferCount + 1` frames),
`test_renderer_null_frame` (10,000 instances, 64 lights, 4 shadowed: expected `passNames` in order, draw/instance counts, zero views is a no-op, `NO_POST_PROCESS` culls exposure/bloom, headless view targeting the swapchain is skipped),
`test_renderer_failures` (missing shader -> `SHADER_NOT_FOUND` naming the key, `MAX_LIGHTS_PER_VIEW` clamp, device lost -> `DEVICE_LOST` and no further recording),
`test_render_target` (create/resize/destroy, the colour id kept across a resize, `read_back` size and zero fill),
`test_imgui_renderer` (two command lists -> two draws, `WantCreate/WantUpdates/WantDestroy` handled, scissor clamp, texture registration).

The renderer tests' null device runs under NVRHI's validation layer (`renderer_test_support`: `make_device`,
`make_headless_renderer` set `enableDebugLayer`), so command-list misuse logs an ERROR and fails every case
holding a `log_capture`. The allocation gates (`null_frame` frames 3 to 10 and extraction, the rebuilt render
graph, the timer ring) use the raw null device (`make_raw_device`, `make_raw_headless_renderer`) so they measure
the renderer, not the validation layer's own bookkeeping (sweep-2026-11-12 F171).

benchmarks (`tests/benchmarks`): `bench_culling_10k`, `bench_draw_list_10k` (< 2 ms cull + sort + batch),
`bench_render_graph_compile`, `bench_render_null_frame` (full `render()` CPU time for the 10k scene, target < 3 ms).

---

## 11. Out of scope / future

- Bindless material textures behind `material_binding` (removes per-material binding-set switches) once MoltenVK descriptor indexing is verified in CI.
- Clustered light culling replacing the tiled pass; manual barriers derived from graph declarations with NVRHI tracking as a validation mode; parallel command-list recording per pass group; transient memory aliasing.
- TAA with motion vectors (`previousWorldFromLocal` slot is reserved in `gpu_instance` padding), temporal GTAO/PCSS filtering.
- Skinning, GPU-driven culling, indirect draws, BC texture compression at cook time, `KHR_texture_transform`, custom user materials/shaders via `shader_asset`, text rendering, order-independent transparency, infinite far plane.
- GPU id-buffer picking (`id_pass`, `request_pick`): the editor picks on the CPU in v1 (`04_editor.md` D10, R11); the `entityId` slot in `gpu_instance` is kept so the pass can return for skinned/instanced meshes.
- Device-loss recovery (recreate device, swapchain, renderer and re-upload resident assets); v1 quits with `exit_code::DEVICE_LOST` (R8).
- GPU IBL bake (compute equirect/irradiance/prefilter shaders) as an optional fast path for the editor; the CPU importer bake is the cached source of truth (R34).
- Replacing glslang with dxc for SPIR-V (`CONJURE_SPIRV_COMPILER=dxc`) when the pinned glslang tag becomes unmaintainable.
