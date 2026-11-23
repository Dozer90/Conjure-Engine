---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# export.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/export.h` |
| API ↗ | [Export and diagnostics](../../../api/engine/export-and-diagnostics.md), [Engine: the application and the frame](../../../api/engine/index.md) |

<p>The export pipeline: the one owner of <code>conjure_editor --export &lt;dir&gt;</code> and the editor&#x27;s <code>project.export</code> command. Synchronous, main thread, outside a frame. Steps, each logged; the first that fails ends the export with <code>EXPORT_FAILED</code>, the steps before it keep their output, and step 5 (a copy file by file) keeps the files it had copied:</p>

## Structs

| File | Description |
|---|---|
| [`export_report`](export/export-report.md) |  |
| [`export_settings`](export/export-settings.md) |  |

## Classes

| File | Description |
|---|---|
| [`export_pipeline`](export/export-pipeline.md) |  |

## Type Aliases

### `progress_fn` {#symbol-progress-fn}

```cpp
using progress_fn = std::function<void(std::string_view step, f32 fraction)>;
```

Called before each step with its name and the fraction of steps completed so far.

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
<a id="export_pipeline"></a>
<a id="export_pipelineexport_pipeline"></a>
<a id="export_pipelineexport_pipeline_1"></a>
<a id="export_pipelinemcontext"></a>
<a id="export_pipelinenotices_file_name"></a>
<a id="export_pipelineoperator"></a>
<a id="export_pipelinerun"></a>
<a id="export_pipelinestep_names"></a>
<a id="export_report"></a>
<a id="export_reportassetcount"></a>
<a id="export_reportcontenthash"></a>
<a id="export_reportcopiedfiles"></a>
<a id="export_reportelapsedseconds"></a>
<a id="export_reportmanagedpublished"></a>
<a id="export_reportmodulesexported"></a>
<a id="export_reportnativemodulebuilt"></a>
<a id="export_reportpackbytes"></a>
<a id="export_reportsteps"></a>
<a id="export_reportwarnings"></a>
<a id="export_settings"></a>
<a id="export_settingsalwaysinclude"></a>
<a id="export_settingsbootstrapproject"></a>
<a id="export_settingsbuildnativemodule"></a>
<a id="export_settingscmakeexecutable"></a>
<a id="export_settingscopyruntime"></a>
<a id="export_settingscopyshaders"></a>
<a id="export_settingsdistributiondirectory"></a>
<a id="export_settingsdotnetexecutable"></a>
<a id="export_settingsoutputdirectory"></a>
<a id="export_settingsprocesstimeoutseconds"></a>
<a id="export_settingspublishmanaged"></a>
<a id="export_settingsruntimeidentifier"></a>
<a id="export_settingsscriptsproject"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="progress_fn"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
