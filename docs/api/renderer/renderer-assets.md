# Renderer assets
<!-- doc-template:wiki -->
[API](../index.md) → [Renderer](index.md)

<!-- doc-content:purpose:start -->
The CPU-side asset types the renderer draws from and the importers that produce them: meshes, textures,
materials, models (glTF node trees), environments (HDR skies with baked lighting) and the built-in shader programs.
Headers: `conjure/renderer/mesh_asset.h`, `conjure/renderer/texture_asset.h`, `conjure/renderer/material_asset.h`,
`conjure/renderer/model_asset.h`, `conjure/renderer/environment_asset.h`, `conjure/renderer/shader_asset.h` and
`conjure/renderer/gltf_importer.h`. Everything is in `namespace conjure::renderer`.
<!-- doc-content:purpose:end -->

|---|---|
| Name | Renderer assets |
| Module | Renderer |
| `#include` | `conjure/renderer/environment_asset.h`, `conjure/renderer/gltf_importer.h`, `conjure/renderer/material_asset.h`, `conjure/renderer/mesh_asset.h`, `conjure/renderer/model_asset.h`, `conjure/renderer/shader_asset.h`, `conjure/renderer/texture_asset.h` |
| Engineering Handbook | [Engineering handbook](../../handbook/engine/renderer.md) |

## Syntax

<!-- doc-content:syntax:start -->
<a id="quick-example"></a>

```cpp
#include "conjure/renderer/gltf_importer.h"
#include "conjure/renderer/renderer.h"

using namespace conjure;

core::result<void> import_and_upload(renderer::renderer &frame, const std::filesystem::path &file) {
    const renderer::gltf_importer importer = {};
    CONJURE_TRY_ASSIGN(const renderer::gltf_import_result result, importer.import(file, {}));
    u64 next = 1;
    for (const renderer::texture_asset &texture : result.textures) {
        CONJURE_UNUSED(frame.upload_texture(assets::asset_id(7, next++), texture));
    }
    for (const renderer::material_asset &material : result.materials) {
        CONJURE_UNUSED(frame.upload_material(assets::asset_id(7, next++), material));
    }
    for (const renderer::mesh_asset &mesh : result.meshes) {
        CONJURE_UNUSED(frame.upload_mesh(assets::asset_id(7, next++), mesh));
    }
    // result.model names meshes and materials by index; result.warnings lists skipped features.
    return core::result<void>::success();
}
```

In a game the asset pipeline does this for you: the `.gltf` is imported once into the cache, the model and its
sub-assets (`#mesh/0`, `#material/0`, `#texture/0`) are loaded through `asset_manager`, and the engine uploads
them when they become `LOADED`. The example is what a tool without the pipeline does.
<!-- doc-content:syntax:end -->

## At a glance

<!-- doc-content:at_a_glance:start -->
| You want to... | Use |
|---|---|
| Register the five asset types and four importers | `renderer::register_asset_types(types, importers, jobs)` (the engine does it at start-up) |
| Build a mesh in code | `mesh_asset::unit_cube()`, `uv_sphere(radius, rings, segments)`, `quad(halfSize)`, `mesh_asset::from_geometry(geometry, name, missingTangents)` |
| Build a texture in code | `texture_asset::solid_colour(...)`, `checker(...)`, `from_rgba8(...)`, `from_image(image, ...)`, `decode(bytes, ...)` |
| Read or write JPEG XL | `decode` reads it like any image; `encode_jpeg_xl(image, options)` writes it ([JPEG XL images](jpeg-xl.md)) |
| Describe a material | fill a `material_desc` and construct `material_asset(desc, name)`; `material_asset::fallback()` is the magenta one |
| Import a glTF in code | `gltf_importer().import(path, options)` or `import_bytes(bytes, options, reader)` |
| Read a model's node tree | `model_asset::nodes()`, `meshes()`, `materials()`, `root_nodes()`, `children_of(node)`, `world_transform(node)` |
| Bake lighting from an HDR | `bake_environment(equirect, options, jobs)` or let the `.hdr` importer do it (a JPEG XL sky too) |
| Look at a mesh on the CPU (picking) | `mesh_asset::cpu_geometry()` (Debug and Development builds) |
<!-- doc-content:at_a_glance:end -->

## Variables and Properties

<!-- doc-content:variables:start -->
Types, fields, and state are described in the reference sections below.
<!-- doc-content:variables:end -->

<!-- doc-content:body:start -->
## The asset types

| Type name | C++ type | Source | Importer | Notes |
|---|---|---|---|---|
| `conjure.renderer.mesh` | `mesh_asset` | sub-asset of a glTF | glTF | packed vertex streams, submeshes, bounds; positions and indices stay resident for CPU picking |
| `conjure.renderer.texture` | `texture_asset` | `.png .jpg .jpeg .tga .bmp .jxl` | `texture_importer` (`texture_import_settings`: colour space, mips, filter, wrap) | RGBA8, sRGB or linear, full mip chain |
| `conjure.renderer.material` | `material_asset` | `.cmat` (JSON) or a glTF sub-asset | `material_importer` | `material_desc` below |
| `conjure.renderer.model` | `model_asset` | `.gltf .glb` (the primary asset) | `gltf_asset_importer` | the node tree; the scene module turns it into a prefab |
| `conjure.renderer.environment` | `environment_asset` | `.hdr` (a JPEG XL through "import as") | `environment_importer` (`environment_bake_options`) | sky cubemap, prefiltered specular mips, SH9 irradiance, baked on the CPU at import |

Every type has a loader (`mesh_loader`, `texture_loader`,...) that parses the cooked blob on a job and a fallback
the renderer draws for a missing asset; none of them touches the GPU (that is `renderer::upload_*`).

### Meshes

`mesh_geometry` is the unpacked input (`positions`, `normals`, `tangents`, `uv0`, `uv1`, `colours`, `indices`,
`submeshIndexCounts`, `submeshMaterialSlots`); `mesh_asset::from_geometry` packs it into the GPU layout (`mesh_streams`: 12-byte positions plus packed
attributes, 16- or 32-bit indices), computes bounds and validates (`INVALID_ARGUMENT` for a non-triangle index
count, an index out of range, inconsistent submeshes or a non-finite (NaN or infinite) position). Normals a
geometry lacks are computed flat per face. Tangents it lacks are built as the third argument says:
`tangent_generation::FROM_UVS` (the default) takes them from the `uv0` derivatives, which is the frame a normal
map needs; `tangent_generation::FROM_NORMAL` builds a unit vector perpendicular to the normal, handedness +1,
and reads no UV. A geometry without `uv0` gets the `FROM_NORMAL` tangent under either value, and tangents the
geometry carries are kept whatever the argument says. `submeshes()`
lists `{firstIndex, indexCount, materialSlot, bounds}`; `bounds()`, `vertex_count()`, `index_count()`,
`triangle_count()`, `gpu_byte_size()` and `name()` describe it. `parse_blob` / `write_blob` are the cooked form
(`MESH_BLOB_VERSION`); `parse_blob` also rejects non-finite positions and non-finite or inverted (min above max)
submesh and mesh bounds with `INVALID_ASSET`.

### Textures

`texture_asset` holds RGBA8 mips (`mips()`, `width()`, `height()`, `mip_count()`), a colour space
(`texture_colour_space::SRGB` or `LINEAR`), a `sampler_filter` and a `texture_wrap`; `pixel(x, y)` reads the top
mip. `decode(bytes, colourSpace, generateMips, jobs)` runs the image decoders, chosen by the file's signature and
not its name (JPEG XL, PNG, JPEG, TGA, BMP), `from_image` wraps a `core::image`, `load_from_file` reads a file (any
path, Unicode included; `name()` is its UTF-8 filename), `mip_count_for(width, height)` says how many mips a size
gets. `MAX_DIMENSION` is 16384: `decode` reads the image header first and refuses a larger image with
`INVALID_ARGUMENT` before decoding any of it. Only JPEG XL is colour-managed (converted to sRGB for an sRGB
texture, kept as stored for a linear one); [JPEG XL images](jpeg-xl.md) lists what its decoder does. `parse_blob` accepts only a mip chain the base size allows and an
RGBA8 format that matches the colour space (`RGBA8_SRGB` for sRGB, `RGBA8_UNORM` for linear).

### Materials

`material_desc` is the glTF metallic-roughness model: `baseColourFactor`, `emissiveFactor` and `emissiveStrength`,
`metallicFactor`, `roughnessFactor`, `normalScale`, `occlusionStrength`, `alphaCutoff`, `alphaMode`
(`alpha_mode::SOLID`, `MASK`, `BLEND`; `SOLID` is glTF's opaque mode under a name that is not a Windows macro),
`doubleSided`, `unlit`, and `textures[MATERIAL_TEXTURE_SLOTS]` as `asset_ref<texture_asset>` (base colour,
metallic-roughness, normal, occlusion, emissive; `material_texture` names the slots) with per-slot UV set
indices. `parse_json` / `to_json` are the `.cmat` text form, `parse_blob` / `write_blob` the cooked one. The
`.cmat` keys are the British spellings `to_json` writes; unknown keys import with a warning, which names the
`.cmat` key for a glTF spelling when that key exists at the same level (a top-level `baseColorFactor` is reported
as "did you mean 'baseColourFactor'?", a `baseColor` inside `textures` as "did you mean 'baseColour'?").

### Models

`model_asset` is a flat, parent-first list of `model_node {name, parent, localTransform, mesh, materials[],
hasLight, lightData, hasCamera, cameraData}` plus the mesh and material asset ids the nodes index. `add_node`,
`add_mesh`, `add_material` build one; `root_nodes()`, `children_of(node)` and `world_transform(node)` walk it. The
JSON form (`parse_json`, `to_json`) is what the importer cooks; `-1` is the written form of "no index", and an
index of 4294967295 or more fails with `INVALID_ARGUMENT` instead of wrapping.

### Environments

An `environment_asset` carries the sky `cubemap_data` (six faces, mip chain), the `prefiltered()` specular
cubemap (`ENVIRONMENT_MIP_COUNT` roughness levels) and the `irradiance()` SH9 coefficients.
`bake_environment(equirect_image, environment_bake_options {skySize, prefilteredSize, sampleCount}, jobs)`
produces one from a decoded `.hdr` or JPEG XL (`equirect_image::decode_hdr(bytes, jobs)`). The bake reads every texel
as finite, non-negative radiance: NaN and negative values count as 0 and anything above `MAX_ENVIRONMENT_RADIANCE`
(about 5.2e33, +inf included) as that bound, so an image filled by hand cannot turn the irradiance into NaN and no sum
of the bake overflows. Finite radiance below it is integrated as it is, so a sun of 1e5 lights the SH9 irradiance and
the rough prefiltered mips fully; only the stored cubemap texels are clamped to the finite half range (65504), so the
sun's own texels bake as 65504 rather than infinity. `decode_hdr` returns its samples already made finite, and a PQ
or HLG JPEG XL with 1.0 at 203 nits (📘 API: [JPEG XL images](jpeg-xl.md#hdr-skies)). `ENVIRONMENT_IMPORTER_VERSION`
is 3 since the bake reads samples that way, so a cached environment whose source holds NaN, negative or out-of-range
radiance bakes again once.
`environment_asset::uniform(radiance, size)` is a flat sky for tests; the loader's fallback and the renderer's
ambient for a scene without an environment are both `uniform(FALLBACK_ENVIRONMENT_RADIANCE)` (0.25).
`evaluate_sh9_irradiance` and `cubemap_texel_direction` are the helpers the shaders mirror.

> [!NOTE]
> Baking is CPU work at import time (seconds per HDRI), cached like every other import; it is job-parallel per
> face when `register_asset_types` is given a job system.

## The glTF importer

`gltf_importer::import(path, options)` reads a `.gltf` or `.glb` with its external buffers and images; those URIs
must stay inside the model's directory: absolute paths and `..` segments that leave it fail with `NOT_FOUND`.
`import_bytes(bytes, options, reader)` reads from memory and asks `reader(relativePath)` for the rest. The result
holds `meshes`, `materials` (plus `materialTextures`, the texture indices per material slot), `textures`, the
`model` and `warnings`. `gltf_import_options` has `generateTangents` (below), `scale`, `importCameras` and
`importLights` (KHR_lights_punctual). Skins, animations, texture transforms and unknown extensions are warnings;
malformed files are `PARSE_FAILURE` (the document is validated first: an accessor or buffer view that reaches past
its buffer, a `byteStride` above 252, a node cycle or a `COLOR_0` that is not `VEC3` or `VEC4` is malformed),
missing buffers `NOT_FOUND`, points and lines `UNSUPPORTED`. Node trees of any depth import.

A glTF point or spot light without a `range` (infinite in glTF) gets the distance at which its light,
`intensity / d^2`, falls to `GLTF_LIGHT_RANGE_CUTOFF` (0.01 lux): `sqrt(intensity / GLTF_LIGHT_RANGE_CUTOFF)`,
about 32 m for 10 candela, clamped to `GLTF_MIN_LIGHT_RANGE` and `GLTF_MAX_LIGHT_RANGE` (0.1 m and 100 m), with a
warning. The range is also the light's shadow far plane and decides how much of the screen its light culling
touches. A directional light keeps the default range, which it ignores. A texture uses its sampler's `wrapS` on
both axes, because a `texture_asset` holds one wrap mode; a sampler whose `wrapT` differs is a warning.

> [!WARNING]
> The derived range is fixed at import: raising the light's intensity later, in the editor or a script, does not
> lengthen it. Set `range` yourself when you change the intensity a lot.

### Tangents and `generateTangents`

Every imported vertex has a tangent. Where it comes from is decided for each primitive of a glTF mesh:

| The primitive has | `generateTangents = true` (default) | `generateTangents = false` |
|---|---|---|
| `TANGENT`, one per vertex, as every primitive of the mesh | the file's tangents | the file's tangents |
| `TANGENT`, one per vertex, while another primitive has none | generated from the UVs, like the rest of the mesh | the file's tangents |
| no `TANGENT` | generated from the `TEXCOORD_0` derivatives (`tangent_generation::FROM_UVS`) | built from the normal alone (`FROM_NORMAL`) |

A tangent built from the normal alone is enough for lighting, but it does not follow the texture, so a normal
map drawn there is applied in the wrong direction. Turn the option off only for models without normal maps.
When it is off, a mesh adds one entry to `warnings` if a primitive of the last row draws with a lit,
normal-mapped material, naming the mesh and each such material (one line, wrapped here):

```
mesh 'wall': generateTangents is false and it has no TANGENT, so its tangents are built from the normal alone;
the normal texture of material 'bricks' will not shade correctly
```

A mesh with `TANGENT` on only some primitives is told so instead of "it has no TANGENT", and the materials named
are those of the primitives without it; the others keep the file's tangents and shade as authored.

> [!WARNING]
> Under either value, one mesh keeps none of the file's tangents: a mesh that has `TANGENT` on only some
> primitives and lacks `NORMAL` on at least one. Its normals are generated per face for every primitive, and all
> its tangents are then built as the option says. With the option off the warning says that the file's tangents
> are not used.

> [!NOTE]
> The check reads the materials the file gives the mesh. A primitive after the first `MAX_MATERIAL_SLOTS` draws
> with the material of the last slot, so that is the one checked for it. An unlit material is skipped, since it
> never samples its normal texture, and a material you assign to the mesh after the import is not checked. The
> warning belongs to the option: with `generateTangents = true`, a primitive without `TEXCOORD_0` also ends up
> with tangents from the normal alone and is not reported.

Changing the setting in a model's `.meta` makes the model stale, like any change to an importer's settings (see
[Database, importers and hot reload](../assets/database-and-import.md)). `GLTF_IMPORTER_VERSION` is 3 since the
option took effect, so models cached by an older build are imported again.

`gltf_asset_importer` is the pipeline adapter: the model is the primary asset, meshes, materials and textures are
sub-assets with stable uuids (`mesh/<i>`, `material/<i>`, `texture/<i>`), and the model depends on them so a
load brings everything. Its settings JSON mirrors the options (`gltf_import_options_from_json`,
`gltf_import_options_to_document`).

## Shader programs

`shader_asset::built_in_programs()` is the list of shaders the renderer loads: one `shader_program_desc` per
program permutation, holding the shader's name, the entry point of each stage it has (`vertexEntry`, `pixelEntry`,
`computeEntry`; `""` for a stage it lacks) and the define strings of its pixel and vertex stages. Every pipeline
the renderer builds, and the Dear ImGui backend in editor builds, takes its shaders from this list and
nowhere else. `find_built_in(name, pixelDefines, vertexDefines)` returns one program or `NOT_FOUND`; the define
strings must match exactly (`""` means "no defines", it is not a wildcard, so `find_built_in("forward_pbr")`
fails while `find_built_in("forward_pbr", "ALPHA_MASK=0")` succeeds). `validate(library)` checks that a
`rhi::shader_library` lists a blob for its backend for every stage of the program and fails with
`SHADER_NOT_FOUND` naming the first missing shader key.

`renderer::create` runs that check for each program before it creates any of the program's shaders, so its
`SHADER_NOT_FOUND` names the same key. `validate` reads the library's index, not the blob files: a blob the
index lists but the shader directory lacks passes `validate` and fails `renderer::create` when it is loaded, also
with `SHADER_NOT_FOUND`. The ImGui program is in the list in every configuration but is loaded only by an editor
build's renderer created with `renderer_desc::enableImGui` and a swapchain. Game code does not need any of this;
it is useful for checking a shader directory before starting the renderer (an export, a tool) and it is where
custom shader programs will enter later.
<!-- doc-content:body:end -->

## Functions

<!-- doc-content:functions:start -->
<a id="api-summary"></a>

| Signature (short) | Purpose |
|---|---|
| `static result<mesh_asset> mesh_asset::from_geometry(const mesh_geometry &, std::string name = {}, tangent_generation missingTangents = tangent_generation::FROM_UVS)` | pack and validate; how missing tangents are built |
| `static mesh_asset mesh_asset::unit_cube()`, `uv_sphere(f32 radius, u32 rings = 16, u32 segments = 32)`, `quad(f32 halfSize = 0.5f)` | primitives |
| `static result<mesh_asset> mesh_asset::parse_blob(span<const byte>)`, `vector<byte> write_blob() const` | cooked form |
| `static texture_asset texture_asset::solid_colour(...)`, `checker(...)`, `static result<texture_asset> from_rgba8(...)`, `from_image(...)`, `decode(...)`, `load_from_file(...)` | construction |
| `material_asset(material_desc, std::string name = {})`, `static material_asset fallback()`, `static result<material_asset> parse_json(string_view)`, `std::string to_json() const` | materials |
| `u32 model_asset::add_node(model_node)`, `add_mesh(const asset_id &)`, `add_material(const asset_id &)`; `vector<u32> root_nodes() const`, `children_of(u32) const`; `transform world_transform(u32) const` | models |
| `result<environment_asset> bake_environment(const equirect_image &, const environment_bake_options &, job_system *)`, `static environment_asset environment_asset::uniform(const vec3 &, u32 size = 8)` | environments |
| `MAX_ENVIRONMENT_RADIANCE`, `ENVIRONMENT_IMPORTER_VERSION` (3), `FALLBACK_ENVIRONMENT_RADIANCE` | the brightest radiance the bake integrates, the importer's cache version, the fallback grey |
| `result<gltf_import_result> gltf_importer::import(const path &, const gltf_import_options &) const`, `import_bytes(span<const byte>, const gltf_import_options &, const gltf_file_reader &) const` | glTF |
| `static span<const shader_program_desc> shader_asset::built_in_programs()`, `static result<shader_asset> find_built_in(string_view name, string_view pixelDefines = {}, string_view vertexDefines = {})`, `result<void> validate(rhi::shader_library &) const` | the list of shaders the renderer loads, lookup, check against a library |
| `static result<void> renderer::register_asset_types(asset_type_registry &, importer_registry &, job_system * = nullptr)` | registration |
<!-- doc-content:functions:end -->

------------------------------------------

**See also:**

<!-- doc-content:related:start -->
- 📘 API: [Rendering a frame](rendering-a-frame.md), [JPEG XL images](jpeg-xl.md), [Database, importers and hot reload](../assets/database-and-import.md), [Processes, libraries and images](../core/system.md)
- 🛠 Handbook: [Renderer module](../../handbook/engine/renderer.md)
<!-- doc-content:related:end -->

<!-- doc-legacy-anchors:start -->
<a id="see-also"></a>
<!-- doc-legacy-anchors:end -->
