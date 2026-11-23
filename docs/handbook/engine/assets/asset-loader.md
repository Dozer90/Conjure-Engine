---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# asset_loader.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/asset_loader.h` |
| API ↗ | [Database, importers and hot reload](../../../api/assets/database-and-import.md), [Assets](../../../api/assets/index.md) |

<p>Type-erased loader interface. One instance per registered type, owned by the registry, stateless with respect to individual loads.</p>

## Structs

| File | Description |
|---|---|
| [`finalise_context`](asset-loader/finalise-context.md) | What finalise knows. Every dependency handle is terminal (<code>LOADED</code> or <code>FAILED</code>) when finalise runs; a <code>FAILED</code> dependency reads <code>nullptr</code> (use the manager&#x27;s fallback). |
| [`load_context`](asset-loader/load-context.md) | What <code>load_cpu</code> knows about the asset being loaded. |

## Classes

| File | Description |
|---|---|
| [`asset_loader`](asset-loader/asset-loader.md) | Type-erased loader interface. One instance per registered type, owned by the registry, stateless with respect to individual loads. |
| [`typed_asset_loader`](asset-loader/typed-asset-loader.md) | Typed convenience base: implements <code>type()</code>, <code>load_cpu()</code>, <code>finalise()</code> and <code>create_fallback()</code> as final overrides that erase through <code>make_payload()</code>; implementers write the typed functions. |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="asset_loader"></a>
<a id="asset_loaderasset_loader"></a>
<a id="asset_loaderasset_loader_1"></a>
<a id="asset_loadercreate_fallback"></a>
<a id="asset_loaderfinalise"></a>
<a id="asset_loaderload_cpu"></a>
<a id="asset_loaderoperator"></a>
<a id="asset_loadertype"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="finalise_context"></a>
<a id="finalise_contextdependencies"></a>
<a id="finalise_contextframeindex"></a>
<a id="finalise_contextid"></a>
<a id="finalise_contextisreload"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="load_context"></a>
<a id="load_contextdependencies"></a>
<a id="load_contextid"></a>
<a id="load_contexttype"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="typed_asset_loader"></a>
<a id="typed_asset_loadercreate_fallback"></a>
<a id="typed_asset_loadercreate_fallback_typed"></a>
<a id="typed_asset_loaderfinalise"></a>
<a id="typed_asset_loaderfinalise_typed"></a>
<a id="typed_asset_loaderload_cpu"></a>
<a id="typed_asset_loaderload_cpu_typed"></a>
<a id="typed_asset_loadertype"></a>
<a id="typed_asset_loadertyped_asset_loader"></a>
<a id="typed_asset_loadertyped_asset_loader_1"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
