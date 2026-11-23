---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# gltf_importer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/gltf_importer.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Renderer assets](../../../api/renderer/renderer-assets.md) |

<p>glTF 2.0 import: .gltf / .glb -&gt; CPU meshes, materials, textures and a model node tree. cgltf does the parsing in engine/src; this header carries only engine types. Unsupported features (skins, animations, <code>KHR_texture_transform</code>, unknown extensions) are warnings, and so is a normal-mapped mesh imported with <code>generateTangents</code> off; parse errors, missing buffers, non-triangle topology and unsupported accessor layouts are failures.</p>

## Structs

| File | Description |
|---|---|
| [`gltf_import_options`](gltf-importer/gltf-import-options.md) | <code>generateTangents</code> decides how a primitive gets its tangents when the file does not give them: <code>true</code> builds them from the <code>TEXCOORD_0</code> derivatives (<code>tangent_generation::FROM_UVS</code>), <code>false</code> from the normal alone (<code>FROM_NORMAL</code>), which is only right where no normal map is drawn. <code>A</code> mesh whose primitives all have a TANGENT (an element per vertex) keeps them under either value. In a mesh where only some primitives have one, <code>false</code> keeps those and builds the others from the normal, while <code>true</code> generates every tangent of the mesh from the UVs. One exception: in a mesh where some primitive lacks <code>NORMAL</code> the normals of all primitives are generated, and if only some of its primitives have a TANGENT, none of those is kept under either value. With <code>false</code>, a mesh gets one import warning when a lit material with a normal texture is drawn by a primitive whose tangents were built from the normal: the materials the file gives the mesh are looked at (a primitive after the first <code>MAX_MATERIAL_SLOTS</code> draws with the material of the last slot), a material assigned to the mesh after the import is not. With <code>true</code> nothing is reported, also not for a mesh without <code>TEXCOORD_0</code>, whose generated tangents come from the normal alone too. <code>scale</code> multiplies the translation and the scale of the root nodes. <code>importCameras</code> and <code>importLights</code> put glTF cameras and <code>KHR_lights_punctual</code> lights on the model&#x27;s nodes. |
| [`gltf_import_result`](gltf-importer/gltf-import-result.md) |  |

## Classes

| File | Description |
|---|---|
| [`gltf_asset_importer`](gltf-importer/gltf-asset-importer.md) | The assets-module adapter: the primary asset is the model (<code>conjure.renderer.model</code>); meshes, materials and textures are sub-assets (&quot;mesh/&lt;index&gt;&quot;, &quot;material/&lt;index&gt;&quot;, &quot;texture/&lt;index&gt;&quot;) with stable uuids. Settings mirror <code><a href="gltf-importer/gltf-import-options.html">gltf_import_options</a></code> as { &quot;<code>generateTangents</code>&quot;: <code>true</code>, &quot;scale&quot;: 1.0, &quot;<code>importCameras</code>&quot;: <code>false</code>, &quot;<code>importLights</code>&quot;: <code>true</code> }. The importer&#x27;s warnings become the import&#x27;s warnings. |
| [`gltf_importer`](gltf-importer/gltf-importer.md) |  |

## Type Aliases

### `gltf_file_reader` {#symbol-gltf-file-reader}

```cpp
using gltf_file_reader = std::function<core::result<std::vector<std::byte>>(std::string_view relativePath)>;
```

Reads a file next to the glTF (external buffers and images) by its relative URI.

### `material_texture_indices` {#symbol-material-texture-indices}

```cpp
using material_texture_indices = std::array<u32, MATERIAL_TEXTURE_SLOTS>;
```

How a material of the result references the result&#x27;s textures: an index into <code>gltf_import_result::textures</code> per slot, or <code>INVALID_INDEX</code>.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-GLTF_EXTENSIONS"></a><code>GLTF_EXTENSIONS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>array&lt;<wbr>std::<wbr>string_<wbr>view,<wbr> <span class="mi">2</span>&gt;</code> | <code class="signature">{<span class="s">".gltf"</span>, <span class="s">".glb"</span>}</code> |  |
| <a id="variable-GLTF_IMPORTER_ID"></a><code>GLTF_IMPORTER_ID</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.gltf"</span></code> |  |
| <a id="variable-GLTF_IMPORTER_VERSION"></a><code>GLTF_IMPORTER_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">3</span></code> | Bumped whenever the importer&#x27;s output changes for some input: a cached import made by another version is stale. 2: a light without a range gets one from its intensity. 3: <code>generateTangents</code> = <code>false</code> builds tangents from the normal alone, and with it a primitive keeps its TANGENT when other primitives of its mesh have none (sweep-2026-11-12). |
| <a id="variable-GLTF_LIGHT_RANGE_CUTOFF"></a><code>GLTF_LIGHT_RANGE_CUTOFF</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">0.01</span>f</code> | <code>A</code> point or spot light without a glTF range gets the distance at which its inverse-square falloff, intensity / d^2, drops to <code>GLTF_LIGHT_RANGE_CUTOFF</code> lux: <code>sqrt(intensity / GLTF_LIGHT_RANGE_CUTOFF)</code> metres (about 32 m for 10 cd), clamped to [<code>GLTF_MIN_LIGHT_RANGE</code>, <code>GLTF_MAX_LIGHT_RANGE</code>] because the range is also the light&#x27;s shadow far plane and bounds its light-culling tiles. Fixed at import. |
| <a id="variable-GLTF_MAX_LIGHT_RANGE"></a><code>GLTF_MAX_LIGHT_RANGE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">100.0</span>f</code> |  |
| <a id="variable-GLTF_MIN_LIGHT_RANGE"></a><code>GLTF_MIN_LIGHT_RANGE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">0.1</span>f</code> |  |

## Functions

### Public

#### `gltf_import_options_from_json` {#function-gltf-import-options-from-json}

<div class="function-declaration" id="function-gltf-import-options-from-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="gltf-importer/gltf-import-options.html">gltf_import_options</a>&gt; gltf_import_options_from_json(
    <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;source);</code></pre></div>

<p>Reads the options object; missing keys keep their defaults. Fails with <code>INVALID_ARGUMENT</code> for a non-object or a wrong member type.</p>

#### `gltf_import_options_to_document` {#function-gltf-import-options-to-document}

<div class="function-declaration" id="function-gltf-import-options-to-document-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/json/document/json-document.html">core::json::document</a> gltf_import_options_to_document(
    <span class="k">const</span> <a href="gltf-importer/gltf-import-options.html">gltf_import_options</a> &amp;options);</code></pre></div>

<p>Writes the options object with sorted keys.</p>

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
<a id="gltf_asset_importer"></a>
<a id="gltf_asset_importerdefault_settings"></a>
<a id="gltf_asset_importerextensions"></a>
<a id="gltf_asset_importergltf_asset_importer"></a>
<a id="gltf_asset_importergltf_asset_importer_1"></a>
<a id="gltf_asset_importerid"></a>
<a id="gltf_asset_importerimport"></a>
<a id="gltf_asset_importerversion"></a>
<a id="gltf_extensions"></a>
<a id="gltf_file_reader"></a>
<a id="gltf_import_options"></a>
<a id="gltf_import_options_from_json"></a>
<a id="gltf_import_options_to_document"></a>
<a id="gltf_import_optionsgeneratetangents"></a>
<a id="gltf_import_optionsimportcameras"></a>
<a id="gltf_import_optionsimportlights"></a>
<a id="gltf_import_optionsoperator"></a>
<a id="gltf_import_optionsscale"></a>
<a id="gltf_import_result"></a>
<a id="gltf_import_resultmaterials"></a>
<a id="gltf_import_resultmaterialtextures"></a>
<a id="gltf_import_resultmeshes"></a>
<a id="gltf_import_resultmodel"></a>
<a id="gltf_import_resulttextures"></a>
<a id="gltf_import_resultwarnings"></a>
<a id="gltf_importer"></a>
<a id="gltf_importer_id"></a>
<a id="gltf_importer_version"></a>
<a id="gltf_importergltf_importer"></a>
<a id="gltf_importergltf_importer_1"></a>
<a id="gltf_importerimport"></a>
<a id="gltf_importerimport_bytes"></a>
<a id="gltf_importeroperator"></a>
<a id="gltf_light_range_cutoff"></a>
<a id="gltf_max_light_range"></a>
<a id="gltf_min_light_range"></a>
<a id="internals-and-algorithms"></a>
<a id="material_texture_indices"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
