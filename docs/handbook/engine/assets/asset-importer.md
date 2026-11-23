---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# asset_importer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/asset_importer.h` |
| API ↗ | [Database, importers and hot reload](../../../api/assets/database-and-import.md), [Assets](../../../api/assets/index.md) |

<p>Converts source bytes into cooked payloads. Called on worker threads; must be deterministic (no clocks, no hash-map iteration order in the output) and touch no state beyond the context.</p>

## Structs

| File | Description |
|---|---|
| [`import_output`](asset-importer/import-output.md) | Result of an import: exactly one entry has an empty sub path. |
| [`imported_asset`](asset-importer/imported-asset.md) | One cooked asset produced by an import: the primary (empty sub path) or a sub-asset. |

## Classes

| File | Description |
|---|---|
| [`asset_importer`](asset-importer/asset-importer.md) | Converts source bytes into cooked payloads. Called on worker threads; must be deterministic (no clocks, no hash-map iteration order in the output) and touch no state beyond the context. |
| [`import_context`](asset-importer/import-context.md) | Everything an importer may read or ask for during one import. |
| [`importer_registry`](asset-importer/importer-registry.md) | The registered importers. Registration on the main thread; find* from any thread between registrations. Pointers stay valid until the importer is unregistered. |
| [`sub_asset_table`](asset-importer/sub-asset-table.md) | Thread-safe table of sub-asset uuids for one source: ids already in the .meta are returned as recorded, new sub paths are allocated under a mutex and merged back on the main thread. |

## Type Aliases

### `asset_path_resolver` {#symbol-asset-path-resolver}

```cpp
using asset_path_resolver = std::function<std::optional<asset_id>(std::string_view assetPath)>;
```

Resolves a mount-relative source path to the uuid of its primary asset.

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="asset_importer"></a>
<a id="asset_importerasset_importer"></a>
<a id="asset_importerasset_importer_1"></a>
<a id="asset_importerdefault_settings"></a>
<a id="asset_importerextensions"></a>
<a id="asset_importerid"></a>
<a id="asset_importerimport"></a>
<a id="asset_importeroperator"></a>
<a id="asset_importerversion"></a>
<a id="asset_path_resolver"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="import_context"></a>
<a id="import_contextadd_warning"></a>
<a id="import_contextfind_asset_by_path"></a>
<a id="import_contextimport_context"></a>
<a id="import_contextimport_context_1"></a>
<a id="import_contextmassetpath"></a>
<a id="import_contextmfilepath"></a>
<a id="import_contextmfilesystem"></a>
<a id="import_contextmplatform"></a>
<a id="import_contextmprimaryid"></a>
<a id="import_contextmresolver"></a>
<a id="import_contextmsettings"></a>
<a id="import_contextmsidefiles"></a>
<a id="import_contextmsourcebytes"></a>
<a id="import_contextmsubassets"></a>
<a id="import_contextmwarnings"></a>
<a id="import_contextoperator"></a>
<a id="import_contextplatform"></a>
<a id="import_contextprimary_id"></a>
<a id="import_contextread_side_file"></a>
<a id="import_contextsettings"></a>
<a id="import_contextside_files"></a>
<a id="import_contextsource_bytes"></a>
<a id="import_contextsource_path"></a>
<a id="import_contextsub_asset_id"></a>
<a id="import_contextwarnings"></a>
<a id="import_output"></a>
<a id="import_outputassets"></a>
<a id="imported_asset"></a>
<a id="imported_assetdependencies"></a>
<a id="imported_assetdisplayname"></a>
<a id="imported_assetpayload"></a>
<a id="imported_assetsubpath"></a>
<a id="imported_assettype"></a>
<a id="importer_registry"></a>
<a id="importer_registrybegin_imports"></a>
<a id="importer_registryend_imports"></a>
<a id="importer_registryfind"></a>
<a id="importer_registryfind_for_extension"></a>
<a id="importer_registryfind_for_path"></a>
<a id="importer_registryimporter_registry"></a>
<a id="importer_registryimporter_registry_1"></a>
<a id="importer_registryimporters"></a>
<a id="importer_registryimports_in_flight"></a>
<a id="importer_registrymimporters"></a>
<a id="importer_registrymimportsinflight"></a>
<a id="importer_registrymview"></a>
<a id="importer_registryoperator"></a>
<a id="importer_registryregister_importer"></a>
<a id="importer_registryunregister_importer"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="sub_asset_table"></a>
<a id="sub_asset_tablechanged"></a>
<a id="sub_asset_tableentries"></a>
<a id="sub_asset_tableid_for"></a>
<a id="sub_asset_tablemchanged"></a>
<a id="sub_asset_tablementries"></a>
<a id="sub_asset_tablemmutex"></a>
<a id="sub_asset_tableoperator"></a>
<a id="sub_asset_tablesub_asset_table"></a>
<a id="sub_asset_tablesub_asset_table_1"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
