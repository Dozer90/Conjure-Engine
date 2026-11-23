---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# asset_extension_desc.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/asset_extension_desc.h` |
| API ↗ | [Cooking, caches and packs](../../../api/assets/cooking-and-packs.md) |

<p>Trivially copyable extension records a plugin module registers. The engine&#x27;s <code>assets_module_consumer</code> calls the factories and forwards the results to <code>register_importer</code> / <code>register_type</code> on attach.</p>

## Structs

| File | Description |
|---|---|
| [`asset_type_extension_desc`](asset-extension-desc/asset-type-extension-desc.md) | One asset type offered by a module. |
| [`importer_extension_desc`](asset-extension-desc/importer-extension-desc.md) | One importer offered by a module. |

## Functions

### Public

#### `register_asset_type_extension` {#function-register-asset-type-extension}

<div class="function-declaration" id="function-register-asset-type-extension-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_asset_type_extension(
    <a href="asset-type-registry/asset-type-registry.html">asset_type_registry</a> &amp;types,
    <span class="k">const</span> <a href="asset-extension-desc/asset-type-extension-desc.html">asset_type_extension_desc</a> &amp;desc);</code></pre></div>

<p>Instantiates the loader of a record and registers the type. <code>INVALID_ARGUMENT</code> when the factory is missing, returns nothing or the loader&#x27;s type differs from the record&#x27;s name.</p>

#### `register_importer_extension` {#function-register-importer-extension}

<div class="function-declaration" id="function-register-importer-extension-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_importer_extension(
    <a href="asset-importer/importer-registry.html">importer_registry</a> &amp;importers,
    <span class="k">const</span> <a href="asset-extension-desc/importer-extension-desc.html">importer_extension_desc</a> &amp;desc);</code></pre></div>

<p>Instantiates the importer of a record and registers it. <code>INVALID_ARGUMENT</code> when the factory is missing, returns nothing or the created importer&#x27;s id differs from the record&#x27;s.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="asset_type_extension_desc"></a>
<a id="asset_type_extension_desccreateloader"></a>
<a id="asset_type_extension_descextension_point"></a>
<a id="asset_type_extension_descflags"></a>
<a id="asset_type_extension_descname"></a>
<a id="asset_type_extension_descversion"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="importer_extension_desc"></a>
<a id="importer_extension_desccreate"></a>
<a id="importer_extension_descextension_point"></a>
<a id="importer_extension_descid"></a>
<a id="internals-and-algorithms"></a>
<a id="register_asset_type_extension"></a>
<a id="register_importer_extension"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
