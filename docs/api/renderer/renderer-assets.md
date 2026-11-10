# Renderer assets

📘 **Public API wiki** · 🛠 Handbook: [Renderer module](../../handbook/modules/renderer.md)

The CPU-side asset types the renderer draws from and the importers that produce them: meshes, textures,
materials, models (glTF node trees), environments (HDR skies with baked lighting) and the built-in shader programs.
Headers: `conjure/renderer/mesh_asset.h`, `conjure/renderer/texture_asset.h`, `conjure/renderer/material_asset.h`,
`conjure/renderer/model_asset.h`, `conjure/renderer/environment_asset.h`, `conjure/renderer/shader_asset.h` and
`conjure/renderer/gltf_importer.h`. Everything is in `namespace conjure::renderer`.

## At a glance

| You want to... | Use |
|---|---|
| Register the five asset types and four importers | `renderer::register_asset_types(types, importers, jobs)` (the engine does it at start-up) |
| Build a mesh in code | `mesh_asset::unit_cube()`, `uv_sphere(radius, rings, segments)`, `quad(halfSize)`, `mesh_asset::from_geometry(geometry, name)` |
| Build a texture in code | `texture_asset::solid_colour(...)`, `checker(...)`, `from_rgba8(...)`, `from_image(image, ...)`, `decode(bytes, ...)` |
| Describe a material | fill a `material_desc` and construct `material_asset(desc, name)`; `material_asset::fallback()` is the magenta one |
| Import a glTF in code | `gltf_importer().import(path, options)` or `import_bytes(bytes, options, reader)` |
| Read a model's node tree | `model_asset::nodes()`, `meshes()`, `materials()`, `root_nodes()`, `children_of(node)`, `world_transform(node)` |
| Bake lighting from an HDR | `bake_environment(equirect, options, jobs)` or let the `.hdr` importer do it |
| Look at a mesh on the CPU (picking) | `mesh_asset::cpu_geometry()` (Debug and Development builds) |

## Quick example

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

## The asset types

| Type name | C++ type | Source | Importer | Notes |
|---|---|---|---|---|
| `conjure.renderer.mesh` | `mesh_asset` | sub-asset of a glTF | glTF | packed vertex streams, submeshes, bounds; positions and indices stay resident for CPU picking |
| `conjure.renderer.texture` | `texture_asset` | `.png .jpg .jpeg .tga .bmp` | `texture_importer` (`texture_import_settings`: colour space, mips, filter, wrap) | RGBA8, sRGB or linear, full mip chain |
| `conjure.renderer.material` | `material_asset` | `.cmat` (JSON) or a glTF sub-asset | `material_importer` | `material_desc` below |
| `conjure.renderer.model` | `model_asset` | `.gltf .glb` (the primary asset) | `gltf_asset_importer` | the node tree; the scene module turns it into a prefab |
| `conjure.renderer.environment` | `environment_asset` | `.hdr` | `environment_importer` (`environment_bake_options`) | sky cubemap, prefiltered specular mips, SH9 irradiance, baked on the CPU at import |

Every type has a loader (`mesh_loader`, `texture_loader`, ...) that parses the cooked blob on a job and a fallback
the renderer draws for a missing asset; none of them touches the GPU (that is `renderer::upload_*`).

### Meshes

`mesh_geometry` is the unpacked input (`positions`, `normals`, `tangents`, `uv0`, `uv1`, `colours`, `indices`,
`submeshIndexCounts`, `submeshMaterialSlots`); `mesh_asset::from_geometry` packs it into the GPU layout (`mesh_streams`: 12-byte positions plus packed
attributes, 16- or 32-bit indices), computes bounds and validates (`INVALID_ARGUMENT` for a non-triangle index
count, an index out of range or inconsistent submeshes). `submeshes()` lists `{firstIndex, indexCount,
materialSlot, bounds}`; `bounds()`, `vertex_count()`, `index_count()`, `triangle_count()`, `gpu_byte_size()` and
`name()` describe it. `parse_blob` / `write_blob` are the cooked form (`MESH_BLOB_VERSION`).

### Textures

`texture_asset` holds RGBA8 mips (`mips()`, `width()`, `height()`, `mip_count()`), a colour space
(`texture_colour_space::SRGB` or `LINEAR`), a `sampler_filter` and a `texture_wrap`; `pixel(x, y)` reads the top
mip. `decode(bytes, settings)` runs the image decoders, `from_image` wraps a `core::image`, `load_from_file` reads a
file, `mip_count_for(width, height)` says how many mips a size gets. `MAX_DIMENSION` is 16384.

### Materials

`material_desc` is the glTF metallic-roughness model: `baseColourFactor`, `emissiveFactor` and `emissiveStrength`,
`metallicFactor`, `roughnessFactor`, `normalScale`, `occlusionStrength`, `alphaCutoff`, `alphaMode`
(`alpha_mode::SOLID`, `MASK`, `BLEND`; `SOLID` is glTF's opaque mode under a name that is not a Windows macro),
`doubleSided`, `unlit`, and `textures[MATERIAL_TEXTURE_SLOTS]` as `asset_ref<texture_asset>` (base colour,
metallic-roughness, normal, occlusion, emissive; `material_texture` names the slots) with per-slot UV set
indices. `parse_json` / `to_json` are the `.cmat` text form, `parse_blob` / `write_blob` the cooked one.

### Models

`model_asset` is a flat, parent-first list of `model_node {name, parent, localTransform, mesh, materials[],
hasLight, lightData, hasCamera, cameraData}` plus the mesh and material asset ids the nodes index. `add_node`,
`add_mesh`, `add_material` build one; `root_nodes()`, `children_of(node)` and `world_transform(node)` walk it. The
JSON form (`parse_json`, `to_json`) is what the importer cooks.

### Environments

An `environment_asset` carries the sky `cubemap_data` (six faces, mip chain), the `prefiltered()` specular
cubemap (`ENVIRONMENT_MIP_COUNT` roughness levels) and the `irradiance()` SH9 coefficients.
`bake_environment(equirect_image, environment_bake_options {skySize, prefilteredSize, sampleCount}, jobs)`
produces one from a decoded `.hdr` (`equirect_image::decode_hdr(bytes)`); `environment_asset::uniform(radiance,
size)` is a flat sky for tests;
`evaluate_sh9_irradiance` and `cubemap_texel_direction` are the helpers the shaders mirror.

> [!NOTE]
> Baking is CPU work at import time (seconds per HDRI), cached like every other import; it is job-parallel per
> face when `register_asset_types` is given a job system.

## The glTF importer

`gltf_importer::import(path, options)` reads a `.gltf` or `.glb` with its external buffers and images;
`import_bytes(bytes, options, reader)` reads from memory and asks `reader(relativePath)` for the rest. The result
holds `meshes`, `materials` (plus `materialTextures`, the texture indices per material slot), `textures`, the
`model` and `warnings`. `gltf_import_options` has `generateTangents`, `scale`, `importCameras` and
`importLights` (KHR_lights_punctual). Skins, animations, texture transforms and unknown extensions are warnings;
malformed files are `PARSE_FAILURE`, missing buffers `NOT_FOUND`, points and lines `UNSUPPORTED`.

`gltf_asset_importer` is the pipeline adapter: the model is the primary asset, meshes, materials and textures are
sub-assets with stable uuids (`mesh/<i>`, `material/<i>`, `texture/<i>`), and the model depends on them so a
load brings everything. Its settings JSON mirrors the options (`gltf_import_options_from_json`,
`gltf_import_options_to_document`).

## Shader programs

`shader_asset::built_in_programs()` lists every `shader_program_desc` (shader key, stage, defines) the renderer
loads at creation; `find_built_in(name)` finds one and `validate(library)` checks that a `rhi::shader_library`
holds its blob. This is what `renderer::create` fails with `SHADER_NOT_FOUND` on; game code does not need it.

## API summary

| Signature (short) | Purpose |
|---|---|
| `static result<mesh_asset> mesh_asset::from_geometry(const mesh_geometry &, std::string name = {})` | pack and validate |
| `static mesh_asset mesh_asset::unit_cube()`, `uv_sphere(f32 radius, u32 rings = 16, u32 segments = 32)`, `quad(f32 halfSize = 0.5f)` | primitives |
| `static result<mesh_asset> mesh_asset::parse_blob(span<const byte>)`, `vector<byte> write_blob() const` | cooked form |
| `static texture_asset texture_asset::solid_colour(...)`, `checker(...)`, `static result<texture_asset> from_rgba8(...)`, `from_image(...)`, `decode(...)`, `load_from_file(...)` | construction |
| `material_asset(material_desc, std::string name = {})`, `static material_asset fallback()`, `static result<material_asset> parse_json(string_view)`, `std::string to_json() const` | materials |
| `u32 model_asset::add_node(model_node)`, `add_mesh(const asset_id &)`, `add_material(const asset_id &)`; `vector<u32> root_nodes() const`, `children_of(u32) const`; `transform world_transform(u32) const` | models |
| `result<environment_asset> bake_environment(const equirect_image &, const environment_bake_options &, job_system *)`, `static environment_asset environment_asset::uniform(const vec3 &, u32 size = 8)` | environments |
| `result<gltf_import_result> gltf_importer::import(const path &, const gltf_import_options &) const`, `import_bytes(span<const byte>, const gltf_import_options &, const gltf_file_reader &) const` | glTF |
| `static span<const shader_program_desc> shader_asset::built_in_programs()` | the program list |
| `static result<void> renderer::register_asset_types(asset_type_registry &, importer_registry &, job_system * = nullptr)` | registration |

## See also

- 📘 API: [Rendering a frame](rendering-a-frame.md), [Database, importers and hot reload](../assets/database-and-import.md), [Processes, libraries and images](../core/system.md)
- 🛠 Handbook: [Renderer module](../../handbook/modules/renderer.md)
