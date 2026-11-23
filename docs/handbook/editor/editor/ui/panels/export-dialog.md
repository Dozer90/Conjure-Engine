---
kind: header
---

[Editor](../../../../editor.md) → [User Interface](../../../user-interface.md)

# export_dialog.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/ui/panels/export_dialog.h` |
| API ↗ | [The workspace UI](../../../../../api/editor/workspace-ui.md) |

<p>The export dialog: the output directory, the <code>project.export</code> command and the progress events it emits. An export is always a Distribution export, so there is no configuration to choose. Each dialog owns its export&#x27;s state; the event client and response sink it registers are released when the export completes or the dialog is destroyed, whichever comes first.</p>



## Classes

| File | Description |
|---|---|
| [`ui::export_dialog`](export-dialog/ui-export-dialog.md) |  |

## Forward Declarations

### `ui::export_dialog::export_state` {#symbol-ui-export-dialog-export-state}

```cpp
struct export_state;
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
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="export_dialog"></a>
<a id="export_dialogdraw_contents"></a>
<a id="export_dialogexport_dialog"></a>
<a id="export_dialogexport_dialog_1"></a>
<a id="export_dialogexport_state"></a>
<a id="export_dialogis_running"></a>
<a id="export_dialogmcontext"></a>
<a id="export_dialogmdirectory"></a>
<a id="export_dialogmprogress"></a>
<a id="export_dialogmrunning"></a>
<a id="export_dialogmstate"></a>
<a id="export_dialogmstatus"></a>
<a id="export_dialogrelease_callbacks"></a>
<a id="export_dialogset_directory"></a>
<a id="export_dialogstart_export"></a>
<a id="export_dialogstatus"></a>
<a id="export_dialogupdate"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="uiexport_dialog"></a>
<a id="uiexport_dialogdraw_contents"></a>
<a id="uiexport_dialogexport_dialog"></a>
<a id="uiexport_dialogexport_dialog_1"></a>
<a id="uiexport_dialogexport_state"></a>
<a id="uiexport_dialogis_running"></a>
<a id="uiexport_dialogmcontext"></a>
<a id="uiexport_dialogmdirectory"></a>
<a id="uiexport_dialogmprogress"></a>
<a id="uiexport_dialogmrunning"></a>
<a id="uiexport_dialogmstate"></a>
<a id="uiexport_dialogmstatus"></a>
<a id="uiexport_dialogrelease_callbacks"></a>
<a id="uiexport_dialogset_directory"></a>
<a id="uiexport_dialogstart_export"></a>
<a id="uiexport_dialogstatus"></a>
<a id="uiexport_dialogupdate"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
