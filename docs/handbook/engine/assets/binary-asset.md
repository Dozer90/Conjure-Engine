---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# binary_asset.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/binary_asset.h` |
| API ↗ | [Cooking, caches and packs](../../../api/assets/cooking-and-packs.md), [Database, importers and hot reload](../../../api/assets/database-and-import.md), [Assets](../../../api/assets/index.md), [Referencing and loading assets](../../../api/assets/referencing-and-loading.md) |

<p>The published payload: the source bytes as imported.</p>

## Structs

| File | Description |
|---|---|
| [`binary_asset`](binary-asset/binary-asset.md) | The published payload: the source bytes as imported. |

## Classes

| File | Description |
|---|---|
| [`binary_importer`](binary-asset/binary-importer.md) | Imports .dat / .bytes / .raw files verbatim; raw data files use .bytes. It does not claim .bin, the extension of glTF&#x27;s external buffers, which are side files of the .gltf that reads them. So a .bin file whose .meta names no importer is recorded <code>NO_IMPORTER</code> when no other registered importer claims .bin: it gets a meta and a browser row, but no import and no blob. <code>A</code> .meta that names this importer (one written while it still claimed .bin, or set through <code>asset_database::set_importer</code>) still imports the file. |
| [`binary_loader`](binary-asset/binary-loader.md) | Copies the payload bytes; finalise is trivial. The fallback is an empty asset. |

## Macros

### `CONJURE_DECLARE_ASSET_TYPE` {#symbol-conjure-declare-asset-type}

```cpp
CONJURE_DECLARE_ASSET_TYPE(conjure::assets::binary_asset, "conjure.assets.binary")
```

## Functions

### Public

#### `register_binary_asset_type` {#function-register-binary-asset-type}

<div class="function-declaration" id="function-register-binary-asset-type-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_binary_asset_type(
    <a href="asset-type-registry/asset-type-registry.html">asset_type_registry</a> &amp;types,
    <a href="asset-importer/importer-registry.html">importer_registry</a> *importers = nullptr);</code></pre></div>

<p>Registers the type and, when a registry is given, the importer.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="binary_asset"></a>
<a id="binary_assetbytes"></a>
<a id="binary_importer"></a>
<a id="binary_importerbinary_importer"></a>
<a id="binary_importerbinary_importer_1"></a>
<a id="binary_importerextensions"></a>
<a id="binary_importerid"></a>
<a id="binary_importerid_1"></a>
<a id="binary_importerimport"></a>
<a id="binary_importerversion"></a>
<a id="binary_importerversion_1"></a>
<a id="binary_loader"></a>
<a id="binary_loaderbinary_loader"></a>
<a id="binary_loaderbinary_loader_1"></a>
<a id="binary_loadercreate_fallback_typed"></a>
<a id="binary_loaderfinalise_typed"></a>
<a id="binary_loaderload_cpu_typed"></a>
<a id="conjure_declare_asset_type"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="register_binary_asset_type"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
