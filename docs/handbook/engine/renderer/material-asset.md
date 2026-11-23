---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# material_asset.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/material_asset.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Renderer assets](../../../api/renderer/renderer-assets.md) |

<p>The one PBR material model: glTF metallic-roughness with glTF defaults. Texture presence, double-sidedness, unlit and the UV set are uniform-driven; the alpha mode is pipeline state. The cooked form (.cmat) is JSON with texture uuids.</p>

## Structs

| File | Description |
|---|---|
| [`material_desc`](material-asset/material-desc.md) |  |

## Classes

| File | Description |
|---|---|
| [`material_asset`](material-asset/material-asset.md) |  |
| [`material_importer`](material-asset/material-importer.md) | The .cmat importer: validates the JSON and cooks it canonically; texture uuids become dependencies. |
| [`material_loader`](material-asset/material-loader.md) | The <code>conjure.renderer.material</code> loader; the fallback is the magenta material. |

## Enumerations

### `alpha_mode` {#symbol-alpha-mode}

```cpp
enum class alpha_mode : u8 {
    SOLID = 0,
    MASK,
    BLEND
};
```

glTF alpha modes. <code>SOLID</code> is glTF&#x27;s OPAQUE (a Windows macro name).

### `material_texture` {#symbol-material-texture}

```cpp
enum class material_texture : u8 {
    BASE_COLOUR = 0,
    METALLIC_ROUGHNESS,
    NORMAL,
    OCCLUSION,
    EMISSIVE
};
```

Texture slots of a material, in binding order (t0..t4 of register space 1).

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MATERIAL_ASSET_TYPE_NAME"></a><code>MATERIAL_ASSET_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.material"</span></code> |  |
| <a id="variable-MATERIAL_EXTENSIONS"></a><code>MATERIAL_EXTENSIONS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>array&lt;<wbr>std::<wbr>string_<wbr>view,<wbr> <span class="mi">1</span>&gt;</code> | <code class="signature">{<span class="s">".cmat"</span>}</code> |  |
| <a id="variable-MATERIAL_IMPORTER_ID"></a><code>MATERIAL_IMPORTER_ID</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.material"</span></code> |  |
| <a id="variable-MATERIAL_IMPORTER_VERSION"></a><code>MATERIAL_IMPORTER_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> |  |

## Macros

### `CONJURE_DECLARE_ASSET_TYPE` {#symbol-conjure-declare-asset-type}

```cpp
CONJURE_DECLARE_ASSET_TYPE(conjure::renderer::material_asset, "conjure.renderer.material")
```

## Functions

### Public

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="material-asset.html#symbol-alpha-mode">alpha_mode</a> value) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="material-asset.html#symbol-material-texture">material_texture</a> slot) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="alpha_mode"></a>
<a id="alpha_modeblend"></a>
<a id="alpha_modemask"></a>
<a id="alpha_modesolid"></a>
<a id="conjure_declare_asset_type"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="material_asset"></a>
<a id="material_asset_type_name"></a>
<a id="material_assetdesc"></a>
<a id="material_assetfallback"></a>
<a id="material_assetmaterial_asset"></a>
<a id="material_assetmaterial_asset_1"></a>
<a id="material_assetmdesc"></a>
<a id="material_assetmname"></a>
<a id="material_assetname"></a>
<a id="material_assetoperator"></a>
<a id="material_assetoperator_1"></a>
<a id="material_assetparse_blob"></a>
<a id="material_assetparse_json"></a>
<a id="material_assetset_name"></a>
<a id="material_assettexture_flags"></a>
<a id="material_assetto_json"></a>
<a id="material_assetwrite_blob"></a>
<a id="material_desc"></a>
<a id="material_descalphacutoff"></a>
<a id="material_descalphamode"></a>
<a id="material_descbasecolourfactor"></a>
<a id="material_descdoublesided"></a>
<a id="material_descemissivefactor"></a>
<a id="material_descemissivestrength"></a>
<a id="material_descmetallicfactor"></a>
<a id="material_descnormalscale"></a>
<a id="material_descocclusionstrength"></a>
<a id="material_descoperator"></a>
<a id="material_descroughnessfactor"></a>
<a id="material_desctextures"></a>
<a id="material_desctextureuvsets"></a>
<a id="material_descunlit"></a>
<a id="material_extensions"></a>
<a id="material_importer"></a>
<a id="material_importer_id"></a>
<a id="material_importer_version"></a>
<a id="material_importerextensions"></a>
<a id="material_importerid"></a>
<a id="material_importerimport"></a>
<a id="material_importermaterial_importer"></a>
<a id="material_importermaterial_importer_1"></a>
<a id="material_importerversion"></a>
<a id="material_loader"></a>
<a id="material_loadercreate_fallback_typed"></a>
<a id="material_loaderfinalise_typed"></a>
<a id="material_loaderload_cpu_typed"></a>
<a id="material_loadermaterial_loader"></a>
<a id="material_loadermaterial_loader_1"></a>
<a id="material_texture"></a>
<a id="material_texturebase_colour"></a>
<a id="material_textureemissive"></a>
<a id="material_texturemetallic_roughness"></a>
<a id="material_texturenormal"></a>
<a id="material_textureocclusion"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
