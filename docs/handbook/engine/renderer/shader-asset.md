---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# shader_asset.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/shader_asset.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Renderer assets](../../../api/renderer/renderer-assets.md) |

<p>Shader programs. In v1 only the built-in programs exist. <code>built_in_programs()</code> is the one list of shaders the renderer loads: <code>renderer::create</code> finds the program of every pipeline (and, in editor builds, of the Dear ImGui backend) with <code>find_built_in()</code> and checks it with <code>validate()</code> before it creates a device shader of it, so a program missing here or a stage missing from the shader library&#x27;s index fails creation with <code>renderer_error::SHADER_NOT_FOUND</code>. The list is not what the build compiles: that is engine/shaders/<code>CMakeLists.txt</code>, which the tests hold against this one in both directions. Custom user materials through <code><a href="shader-asset/shader-asset.html">shader_asset</a></code> are future work.</p>

## Structs

| File | Description |
|---|---|
| [`shader_program_desc`](shader-asset/shader-program-desc.md) |  |

## Classes

| File | Description |
|---|---|
| [`shader_asset`](shader-asset/shader-asset.md) |  |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="shader_asset"></a>
<a id="shader_assetbuilt_in_programs"></a>
<a id="shader_assetdesc"></a>
<a id="shader_assetfind_built_in"></a>
<a id="shader_assetmdesc"></a>
<a id="shader_assetoperator"></a>
<a id="shader_assetoperator_1"></a>
<a id="shader_assetshader_asset"></a>
<a id="shader_assetshader_asset_1"></a>
<a id="shader_assetvalidate"></a>
<a id="shader_program_desc"></a>
<a id="shader_program_desccomputeentry"></a>
<a id="shader_program_descname"></a>
<a id="shader_program_descoperator"></a>
<a id="shader_program_descpixeldefines"></a>
<a id="shader_program_descpixelentry"></a>
<a id="shader_program_descvertexdefines"></a>
<a id="shader_program_descvertexentry"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
