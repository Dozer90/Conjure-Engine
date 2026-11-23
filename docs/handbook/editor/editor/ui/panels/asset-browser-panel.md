---
kind: header
---

[Editor](../../../../editor.md) → [User Interface](../../../user-interface.md)

# asset_browser_panel.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/ui/panels/asset_browser_panel.h` |
| API ↗ | [The workspace UI](../../../../../api/editor/workspace-ui.md) |

<p>The asset browser: the database&#x27;s records as a filterable table (path, type, status), import by path, reimport, delete, open scenes and instantiate prefabs; all through asset.*, <code>scene.open</code> and <code>prefab.instantiate</code>. Deleting the file behind the open scene asks first: a confirmation dialog closes the scene and deletes its file; every other asset is deleted at once and comes back with undo.</p>



## Classes

| File | Description |
|---|---|
| [`ui::asset_browser_panel`](asset-browser-panel/ui-asset-browser-panel.md) |  |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="asset_browser_panel"></a>
<a id="asset_browser_panelasset_browser_panel"></a>
<a id="asset_browser_panelasset_browser_panel_1"></a>
<a id="asset_browser_panelcancel_pending_delete"></a>
<a id="asset_browser_panelconfirm_pending_delete"></a>
<a id="asset_browser_paneldraw_contents"></a>
<a id="asset_browser_paneldraw_delete_confirmation"></a>
<a id="asset_browser_paneldraw_table"></a>
<a id="asset_browser_panelhas_pending_delete"></a>
<a id="asset_browser_panelmconfirmopenrequested"></a>
<a id="asset_browser_panelmfilter"></a>
<a id="asset_browser_panelmimportpath"></a>
<a id="asset_browser_panelmpendingdeletepath"></a>
<a id="asset_browser_panelmpendingdeleteuuid"></a>
<a id="asset_browser_panelmrowsdrawn"></a>
<a id="asset_browser_panelmselectedpath"></a>
<a id="asset_browser_panelpending_delete_path"></a>
<a id="asset_browser_panelrequest_delete"></a>
<a id="asset_browser_panelrows_drawn"></a>
<a id="asset_browser_panelselected_path"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="uiasset_browser_panel"></a>
<a id="uiasset_browser_panelasset_browser_panel"></a>
<a id="uiasset_browser_panelasset_browser_panel_1"></a>
<a id="uiasset_browser_panelcancel_pending_delete"></a>
<a id="uiasset_browser_panelconfirm_pending_delete"></a>
<a id="uiasset_browser_paneldraw_contents"></a>
<a id="uiasset_browser_paneldraw_delete_confirmation"></a>
<a id="uiasset_browser_paneldraw_table"></a>
<a id="uiasset_browser_panelhas_pending_delete"></a>
<a id="uiasset_browser_panelmconfirmopenrequested"></a>
<a id="uiasset_browser_panelmfilter"></a>
<a id="uiasset_browser_panelmimportpath"></a>
<a id="uiasset_browser_panelmpendingdeletepath"></a>
<a id="uiasset_browser_panelmpendingdeleteuuid"></a>
<a id="uiasset_browser_panelmrowsdrawn"></a>
<a id="uiasset_browser_panelmselectedpath"></a>
<a id="uiasset_browser_panelpending_delete_path"></a>
<a id="uiasset_browser_panelrequest_delete"></a>
<a id="uiasset_browser_panelrows_drawn"></a>
<a id="uiasset_browser_panelselected_path"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
