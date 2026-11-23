---
kind: header
---

[Editor](../../../../editor.md) → [User Interface](../../../user-interface.md)

# hierarchy_panel.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/ui/panels/hierarchy_panel.h` |
| API ↗ | [The workspace UI](../../../../../api/editor/workspace-ui.md) |

<p>The scene tree: flat rows from <code><a href="../hierarchy-cache/ui-hierarchy-cache.html">hierarchy_cache</a></code> drawn with a list clipper, click, ctrl-click (toggle) and shift-click (range from the last clicked row) selection, drag and drop reparenting (<code>entity.set_parent</code>, dropping below the rows moves to the root), a context menu for create, duplicate, rename, enable and destroy, inline rename (<code>F2</code> or the menu; Enter or clicking away commits, Escape cancels) and a name filter. <code>A</code> selection made elsewhere (a viewport pick) expands its ancestors and scrolls its row into view.</p>



## Classes

| File | Description |
|---|---|
| [`ui::hierarchy_panel`](hierarchy-panel/ui-hierarchy-panel.md) |  |

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
<a id="hierarchy_panel"></a>
<a id="hierarchy_panelbegin_rename"></a>
<a id="hierarchy_panelcache"></a>
<a id="hierarchy_paneldraw_contents"></a>
<a id="hierarchy_paneldraw_context_menu"></a>
<a id="hierarchy_paneldraw_rename_field"></a>
<a id="hierarchy_paneldraw_row"></a>
<a id="hierarchy_panelhierarchy_panel"></a>
<a id="hierarchy_panelhierarchy_panel_1"></a>
<a id="hierarchy_panelis_renaming"></a>
<a id="hierarchy_panelmanchor"></a>
<a id="hierarchy_panelmcache"></a>
<a id="hierarchy_panelmlastprimary"></a>
<a id="hierarchy_panelmrenamebuffer"></a>
<a id="hierarchy_panelmrenamefocuspending"></a>
<a id="hierarchy_panelmrenaming"></a>
<a id="hierarchy_panelmrowsdrawn"></a>
<a id="hierarchy_panelmscrolltarget"></a>
<a id="hierarchy_panelmseenrevision"></a>
<a id="hierarchy_panelmselectedhere"></a>
<a id="hierarchy_panelrows_drawn"></a>
<a id="hierarchy_panelscroll_to_target"></a>
<a id="hierarchy_panelselect_range"></a>
<a id="hierarchy_panelselect_row"></a>
<a id="hierarchy_panelupdate"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="uihierarchy_panel"></a>
<a id="uihierarchy_panelbegin_rename"></a>
<a id="uihierarchy_panelcache"></a>
<a id="uihierarchy_paneldraw_contents"></a>
<a id="uihierarchy_paneldraw_context_menu"></a>
<a id="uihierarchy_paneldraw_rename_field"></a>
<a id="uihierarchy_paneldraw_row"></a>
<a id="uihierarchy_panelhierarchy_panel"></a>
<a id="uihierarchy_panelhierarchy_panel_1"></a>
<a id="uihierarchy_panelis_renaming"></a>
<a id="uihierarchy_panelmanchor"></a>
<a id="uihierarchy_panelmcache"></a>
<a id="uihierarchy_panelmlastprimary"></a>
<a id="uihierarchy_panelmrenamebuffer"></a>
<a id="uihierarchy_panelmrenamefocuspending"></a>
<a id="uihierarchy_panelmrenaming"></a>
<a id="uihierarchy_panelmrowsdrawn"></a>
<a id="uihierarchy_panelmscrolltarget"></a>
<a id="uihierarchy_panelmseenrevision"></a>
<a id="uihierarchy_panelmselectedhere"></a>
<a id="uihierarchy_panelrows_drawn"></a>
<a id="uihierarchy_panelscroll_to_target"></a>
<a id="uihierarchy_panelselect_range"></a>
<a id="uihierarchy_panelselect_row"></a>
<a id="uihierarchy_panelupdate"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
