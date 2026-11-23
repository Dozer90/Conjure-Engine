---
kind: header
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md)

# editor_panel.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/ui/editor_panel.h` |
| API ↗ | [The workspace UI](../../../../api/editor/workspace-ui.md) |

<p>Panels: one ImGui window each, drawn by the workspace every frame while open. Panels own only UI state and submit <code>command_requests</code> through the context&#x27;s dispatcher. The registry lists the built-in panels and the ones loaded modules provide (<code><a href="../panel-desc/panel-desc.html">panel_desc</a></code> records).</p>

## Structs

| File | Description |
|---|---|
| [`ui::panel_descriptor`](editor-panel/ui-panel-descriptor.md) |  |

## Classes

| File | Description |
|---|---|
| [`ui::editor_panel`](editor-panel/ui-editor-panel.md) |  |
| [`ui::panel_registry`](editor-panel/ui-panel-registry.md) |  |

## Type Aliases

### `ui::panel_factory` {#symbol-ui-panel-factory}

```cpp
using panel_factory = std::function<std::unique_ptr<editor_panel>()>;
```

Creates a panel instance.

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
<a id="editor_panel"></a>
<a id="editor_paneldraw"></a>
<a id="editor_paneldraw_contents"></a>
<a id="editor_paneleditor_panel"></a>
<a id="editor_paneleditor_panel_1"></a>
<a id="editor_panelis_open"></a>
<a id="editor_panelmopen"></a>
<a id="editor_panelmtitle"></a>
<a id="editor_paneloperator"></a>
<a id="editor_panelset_open"></a>
<a id="editor_paneltitle"></a>
<a id="editor_panelupdate"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="panel_descriptor"></a>
<a id="panel_descriptorcreate"></a>
<a id="panel_descriptorname"></a>
<a id="panel_descriptoropenbydefault"></a>
<a id="panel_descriptorowner"></a>
<a id="panel_descriptortitle"></a>
<a id="panel_factory"></a>
<a id="panel_registry"></a>
<a id="panel_registryadd"></a>
<a id="panel_registryadd_module_panel"></a>
<a id="panel_registrydescriptors"></a>
<a id="panel_registryfind"></a>
<a id="panel_registrymdescriptors"></a>
<a id="panel_registryoperator"></a>
<a id="panel_registrypanel_registry"></a>
<a id="panel_registrypanel_registry_1"></a>
<a id="panel_registryremove_owned_by"></a>
<a id="panel_registrysize"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="uieditor_panel"></a>
<a id="uieditor_paneldraw"></a>
<a id="uieditor_paneldraw_contents"></a>
<a id="uieditor_paneleditor_panel"></a>
<a id="uieditor_paneleditor_panel_1"></a>
<a id="uieditor_panelis_open"></a>
<a id="uieditor_panelmopen"></a>
<a id="uieditor_panelmtitle"></a>
<a id="uieditor_paneloperator"></a>
<a id="uieditor_panelset_open"></a>
<a id="uieditor_paneltitle"></a>
<a id="uieditor_panelupdate"></a>
<a id="uipanel_descriptor"></a>
<a id="uipanel_descriptorcreate"></a>
<a id="uipanel_descriptorname"></a>
<a id="uipanel_descriptoropenbydefault"></a>
<a id="uipanel_descriptorowner"></a>
<a id="uipanel_descriptortitle"></a>
<a id="uipanel_factory"></a>
<a id="uipanel_registry"></a>
<a id="uipanel_registryadd"></a>
<a id="uipanel_registryadd_module_panel"></a>
<a id="uipanel_registrydescriptors"></a>
<a id="uipanel_registryfind"></a>
<a id="uipanel_registrymdescriptors"></a>
<a id="uipanel_registryoperator"></a>
<a id="uipanel_registrypanel_registry"></a>
<a id="uipanel_registrypanel_registry_1"></a>
<a id="uipanel_registryremove_owned_by"></a>
<a id="uipanel_registrysize"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
