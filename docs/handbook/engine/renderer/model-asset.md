---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# model_asset.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/model_asset.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Renderer assets](../../../api/renderer/renderer-assets.md) |

<p>The glTF node tree: what the scene module turns into a prefab. Nodes reference meshes and materials by index into the asset id lists, so the model is self-describing once its dependencies are loaded. Cooked as JSON (.cmodel).</p>

## Structs

| File | Description |
|---|---|
| [`model_node`](model-asset/model-node.md) |  |

## Classes

| File | Description |
|---|---|
| [`model_asset`](model-asset/model-asset.md) |  |
| [`model_loader`](model-asset/model-loader.md) | The <code>conjure.renderer.model</code> loader. No fallback: a missing model instantiates nothing. |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MODEL_ASSET_TYPE_NAME"></a><code>MODEL_ASSET_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.renderer.model"</span></code> |  |
| <a id="variable-MODEL_FORMAT_VERSION"></a><code>MODEL_FORMAT_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> |  |

## Macros

### `CONJURE_DECLARE_ASSET_TYPE` {#symbol-conjure-declare-asset-type}

```cpp
CONJURE_DECLARE_ASSET_TYPE(conjure::renderer::model_asset, "conjure.renderer.model")
```

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="conjure_declare_asset_type"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="model_asset"></a>
<a id="model_asset_type_name"></a>
<a id="model_assetadd_material"></a>
<a id="model_assetadd_mesh"></a>
<a id="model_assetadd_node"></a>
<a id="model_assetchildren_of"></a>
<a id="model_assetmaterials"></a>
<a id="model_assetmeshes"></a>
<a id="model_assetmmaterials"></a>
<a id="model_assetmmeshes"></a>
<a id="model_assetmname"></a>
<a id="model_assetmnodes"></a>
<a id="model_assetmodel_asset"></a>
<a id="model_assetmodel_asset_1"></a>
<a id="model_assetname"></a>
<a id="model_assetnodes"></a>
<a id="model_assetoperator"></a>
<a id="model_assetoperator_1"></a>
<a id="model_assetparse_blob"></a>
<a id="model_assetparse_json"></a>
<a id="model_assetroot_nodes"></a>
<a id="model_assetset_name"></a>
<a id="model_assetto_json"></a>
<a id="model_assetworld_transform"></a>
<a id="model_assetwrite_blob"></a>
<a id="model_format_version"></a>
<a id="model_loader"></a>
<a id="model_loaderfinalise_typed"></a>
<a id="model_loaderload_cpu_typed"></a>
<a id="model_loadermodel_loader"></a>
<a id="model_loadermodel_loader_1"></a>
<a id="model_node"></a>
<a id="model_nodecameradata"></a>
<a id="model_nodehascamera"></a>
<a id="model_nodehaslight"></a>
<a id="model_nodelightdata"></a>
<a id="model_nodelocaltransform"></a>
<a id="model_nodematerials"></a>
<a id="model_nodemesh"></a>
<a id="model_nodemodel_node"></a>
<a id="model_nodemodel_node_1"></a>
<a id="model_nodename"></a>
<a id="model_nodeoperator"></a>
<a id="model_nodeoperator_1"></a>
<a id="model_nodeparent"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
