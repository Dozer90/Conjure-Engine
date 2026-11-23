---
kind: header
---

[Editor](../../../../editor.md) → [User Interface](../../../user-interface.md)

# inspector_panel.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/ui/panels/inspector_panel.h` |
| API ↗ | [The workspace UI](../../../../../api/editor/workspace-ui.md) |

<p>The inspector: one <code>component.get</code> document per component of the primary selection per frame, drawn through the property drawers; edits go back through <code>component.set</code> with a coalesce key per field so a drag is one undo entry. Add and remove component live in the panel&#x27;s menus.</p>



## Classes

| File | Description |
|---|---|
| [`ui::inspector_panel`](inspector-panel/ui-inspector-panel.md) |  |

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
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="inspector_panel"></a>
<a id="inspector_panelcomponents_drawn"></a>
<a id="inspector_paneldraw_add_component"></a>
<a id="inspector_paneldraw_component"></a>
<a id="inspector_paneldraw_contents"></a>
<a id="inspector_paneldraw_entity_header"></a>
<a id="inspector_panelinspector_panel"></a>
<a id="inspector_panelinspector_panel_1"></a>
<a id="inspector_panelmaddcomponentfilter"></a>
<a id="inspector_panelmcomponentsdrawn"></a>
<a id="inspector_panelmdrawers"></a>
<a id="inspector_panelmnamebuffer"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="uiinspector_panel"></a>
<a id="uiinspector_panelcomponents_drawn"></a>
<a id="uiinspector_paneldraw_add_component"></a>
<a id="uiinspector_paneldraw_component"></a>
<a id="uiinspector_paneldraw_contents"></a>
<a id="uiinspector_paneldraw_entity_header"></a>
<a id="uiinspector_panelinspector_panel"></a>
<a id="uiinspector_panelinspector_panel_1"></a>
<a id="uiinspector_panelmaddcomponentfilter"></a>
<a id="uiinspector_panelmcomponentsdrawn"></a>
<a id="uiinspector_panelmdrawers"></a>
<a id="uiinspector_panelmnamebuffer"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
