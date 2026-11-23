---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# asset_type_registry.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/asset_type_registry.h` |
| API ↗ | [Database, importers and hot reload](../../../api/assets/database-and-import.md), [Assets](../../../api/assets/index.md) |

<p>The set of registered asset types. Registration and removal happen on the main thread before systems run (or at plugin attach/detach); lookups are safe from any thread between those calls. Every description lives in its own heap block, so a pointer returned by <code>find()</code> stays valid until that type is unregistered; unregistering one type never moves another description.</p>

## Structs

| File | Description |
|---|---|
| [`asset_type_desc`](asset-type-registry/asset-type-desc.md) | One registered type: identity, version, flags and the loader that produces its payloads. |
| [`asset_type_registry_hooks`](asset-type-registry/asset-type-registry-hooks.md) | Callbacks the owning <code><a href="asset-manager/asset-manager.html">asset_manager</a></code> installs so the registry can refuse to unregister a type that is in use and can prepare pinned fallbacks at registration. |

## Classes

| File | Description |
|---|---|
| [`asset_type_registry`](asset-type-registry/asset-type-registry.md) | The set of registered asset types. Registration and removal happen on the main thread before systems run (or at plugin attach/detach); lookups are safe from any thread between those calls. Every description lives in its own heap block, so a pointer returned by <code>find()</code> stays valid until that type is unregistered; unregistering one type never moves another description. |

## Functions

### Public

#### `make_asset_type_desc` {#function-make-asset-type-desc}

<div class="function-declaration" id="function-make-asset-type-desc-1"><pre><code><span class="k">template</span> &lt;<a href="asset-types.html#symbol-declared-asset-type">declared_asset_type</a> AssetType&gt; [[<span class="na">nodiscard</span>]] <a href="asset-type-registry/asset-type-desc.html">asset_type_desc</a> make_asset_type_desc(
    std::unique_ptr&lt;<a href="asset-loader/asset-loader.html">asset_loader</a>&gt; loader,
    <a href="../core/types.html#symbol-u32">u32</a> version = <span class="mi">1</span>,
    <a href="asset-types.html#symbol-asset-type-flags">asset_type_flags</a> flags = asset_type_flags::NONE);</code></pre></div>

<p>Builds a description from the loader&#x27;s type and the traits name; the id comes from the name.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="asset_type_desc"></a>
<a id="asset_type_descflags"></a>
<a id="asset_type_descid"></a>
<a id="asset_type_descloader"></a>
<a id="asset_type_descname"></a>
<a id="asset_type_descversion"></a>
<a id="asset_type_registry"></a>
<a id="asset_type_registry_hooks"></a>
<a id="asset_type_registry_hooksbool"></a>
<a id="asset_type_registry_hooksvoid"></a>
<a id="asset_type_registryasset_type_registry"></a>
<a id="asset_type_registryasset_type_registry_1"></a>
<a id="asset_type_registryfind"></a>
<a id="asset_type_registryfind_loader"></a>
<a id="asset_type_registrymhooks"></a>
<a id="asset_type_registrymindexbyid"></a>
<a id="asset_type_registrymtypes"></a>
<a id="asset_type_registrymview"></a>
<a id="asset_type_registryoperator"></a>
<a id="asset_type_registryrebuild_index"></a>
<a id="asset_type_registryregister_type"></a>
<a id="asset_type_registryset_hooks"></a>
<a id="asset_type_registrysize"></a>
<a id="asset_type_registrytypes"></a>
<a id="asset_type_registryunregister_type"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="make_asset_type_desc"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
