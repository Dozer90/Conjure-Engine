# Shaders

📘 **Public API wiki** · 🛠 Handbook: [RHI module](../../handbook/modules/rhi.md)

Compile HLSL at build time and load the compiled blobs at run time. The CMake function `conjure_add_shaders()`
turns a list of `source;entry;stage;defines` entries into DXIL and SPIR-V blobs plus an `index.json`;
`shader_library` in `conjure/rhi/shader_library.h` opens that index for a device and hands out the blob for a
`(name, entry, defines)` key. Everything is in `namespace conjure::rhi`.

## At a glance

| You want to... | Use |
|---|---|
| Compile your module's HLSL as part of the build | `conjure_add_shaders(TARGET ... OUTPUT_DIR ... SHADERS ...)` in your `CMakeLists.txt` |
| Open the compiled shaders for a device | `shader_library::open(device, shaderDirectory)` |
| Get a blob to create an NVRHI shader from | `library.get({"name", "entry", "DEFINE=1"})` |
| Check a permutation exists without touching disk | `library.contains(key)` |
| Pick up recompiled shaders while running | `library.reload()` |
| Name a stage in text | `to_string(shader_stage)`, `parse_shader_stage("pixel")` |

## Quick example

```cpp
#include "conjure/rhi/device.h"
#include "conjure/rhi/shader_library.h"

#include <filesystem>
#include <memory>
#include <string_view>

using namespace conjure;
using namespace conjure::rhi;

class pass_shaders {
    private:
        std::unique_ptr<shader_library> mLibrary = nullptr;

    public:
        [[nodiscard]] core::status open(device &gpu, const std::filesystem::path &shaderDirectory) {
            CONJURE_TRY_ASSIGN(mLibrary, shader_library::open(gpu, shaderDirectory));
            return core::status::success();
        }

        // The returned bytes stay valid until the next reload(); hand them to
        // gpu.native_device()->createShader(...) right away rather than keeping the span.
        [[nodiscard]] core::result<shader_blob> vertex(std::string_view name, std::string_view defines = "") {
            return mLibrary->get({name, "vs_main", defines});
        }

        [[nodiscard]] core::result<shader_blob> pixel(std::string_view name, std::string_view defines = "") {
            return mLibrary->get({name, "ps_main", defines});
        }
};
```

With the engine's own shaders built, `pixel("fullscreen_triangle")` returns the pixel shader of
`engine/shaders/fullscreen_triangle.hlsl` as a `shader_blob { bytes, stage }` whose `stage` is
`shader_stage::PIXEL`.

## Building shaders with CMake

`conjure_add_shaders()` lives in `cmake/ConjureShaders.cmake` and is already included when the renderer is
built. One call compiles every permutation of every listed source into `OUTPUT_DIR` and writes
`OUTPUT_DIR/index.json`. For a module with its own HLSL sources:

```cmake
# engine/src/particles/shaders/CMakeLists.txt
include(ConjureShaders)

conjure_add_shaders(
    TARGET       conjure_particles_shaders
    OUTPUT_DIR   "${PROJECT_BINARY_DIR}/shaders/particles"
    INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/engine/shaders/shared"
    DEPENDENCIES "${PROJECT_SOURCE_DIR}/engine/shaders/shared/common.hlsli"
    SHADERS
        "particles.hlsl;vs_main;vertex;"
        "particles.hlsl;ps_main;pixel;SOFT_PARTICLES=0,1"
        "particles_sim.hlsl;cs_main;compute;"
)
add_dependencies(conjure_particles conjure_particles_shaders)
```

| Argument | Meaning |
|---|---|
| `TARGET` | the custom target that builds the blobs and the index; make your module or its tests depend on it |
| `OUTPUT_DIR` | where `<name>.<entry>[.<defines>].dxil` / `.spv` and `index.json` land; must be inside the build directory |
| `SOURCE_DIR` | where the sources live; default is the calling directory |
| `INCLUDE_DIRS` | `-I` directories; the engine's shared headers are under `engine/shaders/shared` |
| `DEPENDENCIES` | headers your shaders include: rebuild triggers for Visual Studio builds and part of each entry's `sourceHash` |
| `SHADERS` | one `source;entry;stage;defines` entry per shader; `stage` is `vertex`, `pixel` or `compute` |

The `defines` field is a `|`-separated list of `NAME=v1,v2` axes whose cartesian product is enumerated:
`"SOFT_PARTICLES=0,1"` makes two pixel shaders and `"ALPHA_MASK=0,1|FOO=2"` makes two more. The example
above therefore compiles four permutations per enabled backend.

> [!IMPORTANT]
> The shader key you ask the library for must match the index exactly: `name` is the source file's stem
> (`"particles"`), `entry` the entry point (`"ps_main"`) and `defines` the axis string for one permutation in
> the order the `SHADERS` entry declared it (`"SOFT_PARTICLES=1"`, `"ALPHA_MASK=1|FOO=2"`, `""` for none).
> `"FOO=2|ALPHA_MASK=1"` is a different key and is `NOT_FOUND`.

> [!TIP]
> Keep permutation axes few and uniform-driven switches many. Every axis value multiplies the blobs you build
> and the pipelines you create; a branch on a constant-buffer value is free.

## Writing HLSL that compiles on every backend

The same HLSL source compiles to DXIL with `dxc` and to SPIR-V with `glslang`, so sources stay in the HLSL
2018 subset both accept: no templates, no wave intrinsics, no 16-bit types, no `enum class`. Include
`common.hlsli` from the shared directory and follow its conventions:

```hlsl
#include "common.hlsli"

CONJURE_PUSH_CONSTANTS cbuffer draw_constants : register(b1, space0) {
    float4 gColour;
};

float4 ps_main(float4 position : SV_Position) : SV_Target0 {
    return gColour;
}
```

| Convention | Why |
|---|---|
| `CONJURE_PUSH_CONSTANTS cbuffer ... : register(b1, space0)` | expands to `[[vk::push_constant]]` under `CONJURE_VULKAN` and to nothing under `CONJURE_D3D12`, so one declaration is root constants on D3D12 and a push-constant block on Vulkan |
| `register(tN, spaceS)`, `register(bN, spaceS)` | register space `S` is descriptor set `S` on Vulkan; the SPIR-V build shifts bindings so NVRHI finds them without per-shader tables |
| `CONJURE_D3D12` / `CONJURE_VULKAN` | defined as 1 by the matching build; branch on them only for API differences |
| No Y flip | NVRHI's Vulkan backend flips Y through the viewport; clip-space code is shared |

## The index and the blob columns

`index.json` has a `version` (1) and a `shaders` array with one entry per permutation:

```json
{
  "version": 1,
  "shaders": [
    { "name": "fullscreen_triangle", "entry": "vs_main", "stage": "vertex", "defines": "",
      "dxil": "fullscreen_triangle.vs_main.dxil", "spirv": "", "sourceHash": "0a4f...c2" },
    { "name": "fullscreen_triangle", "entry": "ps_main", "stage": "pixel", "defines": "",
      "dxil": "fullscreen_triangle.ps_main.dxil", "spirv": "", "sourceHash": "7b19...e4" }
  ]
}
```

`dxil` and `spirv` are paths relative to the index, empty for a backend that was not compiled. When
`shader_library::open()` runs it selects one column for the device's backend:

| Device backend | Column used | Blob format |
|---|---|---|
| D3D12 | `dxil` | DXIL container (starts with `DXBC`), built by `dxc` with `-T vs_6_5` / `ps_6_5` / `cs_6_5` |
| Vulkan | `spirv` | SPIR-V for `vulkan1.2`, built by `glslang` (or `dxc -spirv` with `CONJURE_SPIRV_COMPILER=dxc`) |
| Null | `dxil`, else `spirv` | whichever exists; the null device checks that a blob is non-empty and never parses it |

`entry_count()` is the number of entries in the index including those without a blob for this backend;
`contains(key)` is true only when a blob exists for it. `backend()` and `directory()` report what was
selected and where from.

## Loading and errors

`get(key)` reads the blob file the first time and caches it; `loaded_blob_count()` tells you how many are in
memory. The returned `shader_blob` is never empty.

| Call | Fails with | When |
|---|---|---|
| `open` | `core::error_code::NOT_FOUND` | no `index.json` in the directory |
| `open` | `core::error_code::IO_FAILURE` | the index cannot be read |
| `open` | `core::error_code::PARSE_FAILURE` | not JSON, not an object, wrong `version`, `shaders` not an array, an entry without `name`, `entry` or `stage`, an unknown stage or a duplicate `name/entry/defines`; the message names the entry |
| `get` | `core::error_code::NOT_FOUND` | no entry matches the key, or the entry has no blob for this backend |
| `get` | `core::error_code::IO_FAILURE` | the blob file is missing or empty |

These are generic codes, so test them with `error.is(core::error_code::NOT_FOUND)`
([Errors and results](../core/errors-and-results.md)).

> [!WARNING]
> A `shader_blob::bytes` span points into the library's cache and is valid only until the next `reload()` or
> the library's destruction. Create the NVRHI shader from it immediately; do not store the span.

## Hot reload

`reload()` re-reads `index.json` and drops every cached blob, so the next `get()` reads the recompiled file.
When the new index does not parse, the previous index stays in effect and the error is returned, which makes
a half-written index during a rebuild harmless. Pipelines created from the old blobs are untouched; recreate
them after a successful reload.

> [!NOTE]
> Every `shader_library` call belongs to the render thread, like the device it was opened on.

## API summary

| Signature (short) | Purpose |
|---|---|
| `enum class shader_stage : u8 { VERTEX, PIXEL, COMPUTE }` | the stages the build supports |
| `std::string_view to_string(shader_stage)` / `result<shader_stage> parse_shader_stage(std::string_view)` | `"vertex"`, `"pixel"`, `"compute"`; the parser also takes `"vs"`, `"ps"`, `"cs"` |
| `struct shader_key { std::string_view name, entry, defines; }` | one permutation |
| `struct shader_blob { std::span<const std::byte> bytes; shader_stage stage; }` | a compiled shader |
| `static result<std::unique_ptr<shader_library>> shader_library::open(device &, const std::filesystem::path &)` | open `index.json` |
| `result<shader_blob> get(const shader_key &)` | load (once) and return a blob |
| `bool contains(const shader_key &)` | entry with a blob for this backend exists |
| `status reload()` | re-read the index, drop the cache |
| `usize entry_count()` / `usize loaded_blob_count()` | sizes |
| `graphics_backend backend()` / `const std::filesystem::path &directory()` | what was opened |
| `conjure_add_shaders(TARGET OUTPUT_DIR [SOURCE_DIR] [INCLUDE_DIRS] [DEPENDENCIES] SHADERS ...)` | CMake: compile and index |

## See also

- 📘 API: [Devices and swapchains](devices-and-swapchains.md) for the device a library is opened on
- 📘 API: [RHI overview](index.md)
- 📘 API: [Files and the virtual file system](../core/files.md) for `std::filesystem::path` conventions
- 📘 API: [Glossary](../glossary.md)
- 🛠 Handbook: [RHI module](../../handbook/modules/rhi.md) for the compiler flags, depfiles and `sourceHash`
