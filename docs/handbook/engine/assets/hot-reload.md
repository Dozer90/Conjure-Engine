---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# hot_reload.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/hot_reload.h` |
| API ↗ | [Database, importers and hot reload](../../../api/assets/database-and-import.md), [Assets](../../../api/assets/index.md) |

<p>Hot reload: watcher changes -&gt; dirty sources -&gt; re-import -&gt; reload dependents through the normal pipeline in dependency-depth order.</p>

## Structs

| File | Description |
|---|---|
| [`hot_reload_report`](hot-reload/hot-reload-report.md) | What the last <code>update()</code> did. |

## Classes

| File | Description |
|---|---|
| [`hot_reload_controller`](hot-reload/hot-reload-controller.md) | Drives re-import and reload from file changes. Main thread, after <code>asset_manager::update()</code>. |

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
<a id="hot_reload_controller"></a>
<a id="hot_reload_controllercollect_dirty"></a>
<a id="hot_reload_controllerdeferred_count"></a>
<a id="hot_reload_controllerhot_reload_controller"></a>
<a id="hot_reload_controllerhot_reload_controller_1"></a>
<a id="hot_reload_controllerlast_report"></a>
<a id="hot_reload_controllermchanges"></a>
<a id="hot_reload_controllermdatabase"></a>
<a id="hot_reload_controllermdeferred"></a>
<a id="hot_reload_controllermjobs"></a>
<a id="hot_reload_controllermlastreport"></a>
<a id="hot_reload_controllermmanager"></a>
<a id="hot_reload_controllermwatcher"></a>
<a id="hot_reload_controlleroperator"></a>
<a id="hot_reload_controllerreload_deferred"></a>
<a id="hot_reload_controllerreload_or_defer"></a>
<a id="hot_reload_controllerreloads_this_frame"></a>
<a id="hot_reload_controllerupdate"></a>
<a id="hot_reload_report"></a>
<a id="hot_reload_reportchangesseen"></a>
<a id="hot_reload_reportdeferred"></a>
<a id="hot_reload_reportreloaded"></a>
<a id="hot_reload_reportreloads"></a>
<a id="hot_reload_reportsourcesreimported"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
