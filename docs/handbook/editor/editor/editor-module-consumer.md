---
kind: header
---

[Editor](../../editor.md) → [Applications](../applications.md)

# editor_module_consumer.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/editor/editor_module_consumer.h` |
| API ↗ | [Headless mode and the command model](../../../api/editor/headless-and-commands.md), [The editor](../../../api/editor/index.md) |

<p>The editor&#x27;s module consumer: turns <code>editor.command</code> records into <code>command_descriptors</code> owned by the module and keeps the module&#x27;s <code>editor.panel</code> records for the UI&#x27;s panel registry; detach removes both. It vetoes an unload with <code>UNLOAD_BLOCKED</code> while the editor plays, because scripts and systems of the module may be live.</p>

## Structs

| File | Description |
|---|---|
| [`module_panel_entry`](editor-module-consumer/module-panel-entry.md) | <code>A</code> panel a loaded module provides. |

## Classes

| File | Description |
|---|---|
| [`editor_module_consumer`](editor-module-consumer/editor-module-consumer.md) |  |

## Forward Declarations

### `play_controller` {#symbol-play-controller}

```cpp
class play_controller;
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
<a id="editor_module_consumer"></a>
<a id="editor_module_consumerattach"></a>
<a id="editor_module_consumerattached"></a>
<a id="editor_module_consumercan_detach"></a>
<a id="editor_module_consumerdetach"></a>
<a id="editor_module_consumereditor_module_consumer"></a>
<a id="editor_module_consumereditor_module_consumer_1"></a>
<a id="editor_module_consumerlive_use_count"></a>
<a id="editor_module_consumermattached"></a>
<a id="editor_module_consumermmodulenames"></a>
<a id="editor_module_consumermonpanelschanged"></a>
<a id="editor_module_consumermpanels"></a>
<a id="editor_module_consumermplay"></a>
<a id="editor_module_consumermregistry"></a>
<a id="editor_module_consumername"></a>
<a id="editor_module_consumeron_panels_changed"></a>
<a id="editor_module_consumerpanels"></a>
<a id="editor_module_consumerrestore"></a>
<a id="editor_module_consumervalidate"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="module_panel_entry"></a>
<a id="module_panel_entrydesc"></a>
<a id="module_panel_entrymodule"></a>
<a id="module_panel_entryowner"></a>
<a id="module_panel_entrytitle"></a>
<a id="play_controller"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
