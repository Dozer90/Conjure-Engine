---
kind: header
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md)

# editor_workspace.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/ui/editor_workspace.h` |
| API ↗ | [The workspace UI](../../../../api/editor/workspace-ui.md) |

<p>The docking workspace: the full-window dockspace with a pass-through central node (the scene renders underneath), the main menu bar, the global shortcuts (Ctrl+<code>Z</code>, Ctrl+<code>Y</code>, Ctrl+<code>S</code>, <code>F5</code>, Shift+<code>F5</code>, handled here so they work whichever panels are open), the Save Scene As dialog, the project trust question (decision: &quot;Do you trust the authors of this project?&quot;, shown while the context&#x27;s <code>trust_prompt()</code> names a project), the panel instances and the layout file &lt;project&gt;/.conjure/<code>editor_layout.ini</code>.</p>

## Structs

| File | Description |
|---|---|
| [`ui::editor_workspace::panel_instance`](editor-workspace/ui-editor-workspace-panel-instance.md) |  |
| [`ui::menu_request`](editor-workspace/ui-menu-request.md) | <code>A</code> command the menu bar fires with a fixed argument set. |

## Classes

| File | Description |
|---|---|
| [`ui::editor_workspace`](editor-workspace/ui-editor-workspace.md) |  |

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
<a id="editor_workspace"></a>
<a id="editor_workspaceanswer_trust_prompt"></a>
<a id="editor_workspaceclose_panel"></a>
<a id="editor_workspacecreate_default_panels"></a>
<a id="editor_workspacedraw"></a>
<a id="editor_workspacedraw_menu_bar"></a>
<a id="editor_workspacedraw_save_as_modal"></a>
<a id="editor_workspacedraw_trust_prompt"></a>
<a id="editor_workspacedrawers"></a>
<a id="editor_workspaceeditor_workspace"></a>
<a id="editor_workspaceeditor_workspace_1"></a>
<a id="editor_workspacefind_panel"></a>
<a id="editor_workspaceframes_drawn"></a>
<a id="editor_workspacehandle_global_shortcuts"></a>
<a id="editor_workspacelayout_file"></a>
<a id="editor_workspaceload_layout_once"></a>
<a id="editor_workspacemdrawers"></a>
<a id="editor_workspacemenu_requests"></a>
<a id="editor_workspacemframesdrawn"></a>
<a id="editor_workspaceminstances"></a>
<a id="editor_workspacemlayoutfile"></a>
<a id="editor_workspacemlayoutloaded"></a>
<a id="editor_workspacempanels"></a>
<a id="editor_workspacemsaveaserror"></a>
<a id="editor_workspacemsaveasopenrequested"></a>
<a id="editor_workspacemsaveaspath"></a>
<a id="editor_workspacemsaveaspending"></a>
<a id="editor_workspacemshowdemo"></a>
<a id="editor_workspacemtrustprompterror"></a>
<a id="editor_workspaceopen_panel"></a>
<a id="editor_workspaceopen_panel_count"></a>
<a id="editor_workspaceoperator"></a>
<a id="editor_workspacepanel_instance"></a>
<a id="editor_workspacepanel_instancename"></a>
<a id="editor_workspacepanel_instancepanel"></a>
<a id="editor_workspacepanels"></a>
<a id="editor_workspacerequest_save_scene"></a>
<a id="editor_workspacerequest_save_scene_as"></a>
<a id="editor_workspacesave_as_error"></a>
<a id="editor_workspacesave_as_path"></a>
<a id="editor_workspacesave_as_pending"></a>
<a id="editor_workspacesave_layout"></a>
<a id="editor_workspacesave_scene_as"></a>
<a id="editor_workspaceset_layout_file"></a>
<a id="editor_workspacesync_module_panels"></a>
<a id="editor_workspacetrust_prompt_error"></a>
<a id="editor_workspaceupdate"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="menu_request"></a>
<a id="menu_requestargs"></a>
<a id="menu_requestcommand"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="uieditor_workspace"></a>
<a id="uieditor_workspaceanswer_trust_prompt"></a>
<a id="uieditor_workspaceclose_panel"></a>
<a id="uieditor_workspacecreate_default_panels"></a>
<a id="uieditor_workspacedraw"></a>
<a id="uieditor_workspacedraw_menu_bar"></a>
<a id="uieditor_workspacedraw_save_as_modal"></a>
<a id="uieditor_workspacedraw_trust_prompt"></a>
<a id="uieditor_workspacedrawers"></a>
<a id="uieditor_workspaceeditor_workspace"></a>
<a id="uieditor_workspaceeditor_workspace_1"></a>
<a id="uieditor_workspacefind_panel"></a>
<a id="uieditor_workspaceframes_drawn"></a>
<a id="uieditor_workspacehandle_global_shortcuts"></a>
<a id="uieditor_workspacelayout_file"></a>
<a id="uieditor_workspaceload_layout_once"></a>
<a id="uieditor_workspacemdrawers"></a>
<a id="uieditor_workspacemenu_requests"></a>
<a id="uieditor_workspacemframesdrawn"></a>
<a id="uieditor_workspaceminstances"></a>
<a id="uieditor_workspacemlayoutfile"></a>
<a id="uieditor_workspacemlayoutloaded"></a>
<a id="uieditor_workspacempanels"></a>
<a id="uieditor_workspacemsaveaserror"></a>
<a id="uieditor_workspacemsaveasopenrequested"></a>
<a id="uieditor_workspacemsaveaspath"></a>
<a id="uieditor_workspacemsaveaspending"></a>
<a id="uieditor_workspacemshowdemo"></a>
<a id="uieditor_workspacemtrustprompterror"></a>
<a id="uieditor_workspaceopen_panel"></a>
<a id="uieditor_workspaceopen_panel_count"></a>
<a id="uieditor_workspaceoperator"></a>
<a id="uieditor_workspacepanel_instance"></a>
<a id="uieditor_workspacepanel_instancename"></a>
<a id="uieditor_workspacepanel_instancepanel"></a>
<a id="uieditor_workspacepanels"></a>
<a id="uieditor_workspacerequest_save_scene"></a>
<a id="uieditor_workspacerequest_save_scene_as"></a>
<a id="uieditor_workspacesave_as_error"></a>
<a id="uieditor_workspacesave_as_path"></a>
<a id="uieditor_workspacesave_as_pending"></a>
<a id="uieditor_workspacesave_layout"></a>
<a id="uieditor_workspacesave_scene_as"></a>
<a id="uieditor_workspaceset_layout_file"></a>
<a id="uieditor_workspacesync_module_panels"></a>
<a id="uieditor_workspacetrust_prompt_error"></a>
<a id="uieditor_workspaceupdate"></a>
<a id="uimenu_request"></a>
<a id="uimenu_requestargs"></a>
<a id="uimenu_requestcommand"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
