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

## Functions

### Public

#### `dotnet_sdk_download_page` {#function-dotnet-sdk-download-page}

<div class="function-declaration" id="function-dotnet-sdk-download-page-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string dotnet_sdk_download_page();</code></pre></div>

<p>Microsoft&#x27;s official download page of the release that holds <code><a href="#function-required-dotnet-sdk-version-1">required_dotnet_sdk_version</a>()</code>, named by its major and minor version (&quot;https://<code>dotnet.microsoft.com</code>/download/dotnet/10.0&quot;).</p>

#### `find_dotnet_sdk` {#function-find-dotnet-sdk}

<div class="function-declaration" id="function-find-dotnet-sdk-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;std::string&gt; find_dotnet_sdk(
    std::string_view dotnet,
    <span class="k">const</span> std::filesystem::path &amp;workingDirectory,
    <a href="../core/types.html#symbol-f64">f64</a> timeoutSeconds);</code></pre></div>

<p>Runs <code>&lt;dotnet&gt; --version</code> in <code>workingDirectory</code> (dotnet reads the <code>global.json</code> in it or above it from there) and returns the version of the SDK it selects. <code>DEPENDENCY_FAILURE</code> (core) carrying <code><a href="#function-missing-dotnet-sdk-message-1">missing_dotnet_sdk_message</a>()</code> when dotnet could not be started, did not answer within <code>timeoutSeconds</code> (0 waits without a limit), exited with a code other than 0 (no SDK installed, or none that such a <code>global.json</code> accepts) or printed a version that <code><a href="#function-is-usable-dotnet-sdk-version-1">is_usable_dotnet_sdk_version</a>()</code> refuses. <code>workingDirectory</code> must exist; empty runs in the current directory. Blocks the calling thread while dotnet runs.</p>

#### `is_usable_dotnet_sdk_version` {#function-is-usable-dotnet-sdk-version}

<div class="function-declaration" id="function-is-usable-dotnet-sdk-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_usable_dotnet_sdk_version(
    std::string_view version) <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>version</code>, the text <code>dotnet --version</code> prints (&quot;10.0.103&quot;, &quot;11.0.100-preview.1&quot;), names <code><a href="#function-required-dotnet-sdk-version-1">required_dotnet_sdk_version</a>()</code> or a later SDK. Major, minor and patch compare as numbers, and a prerelease of the required version itself is older than it. Whitespace around the text is ignored; text that does not start with three dot-separated numbers, or that continues after them with anything but a &#x27;-&#x27; or &#x27;+&#x27; suffix, is <code>false</code>.</p>

#### `missing_dotnet_sdk_message` {#function-missing-dotnet-sdk-message}

<div class="function-declaration" id="function-missing-dotnet-sdk-message-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string missing_dotnet_sdk_message(
    std::string_view detail);</code></pre></div>

<p>The message for a build that found no usable SDK: <code>detail</code> (what was found instead), the version required as <code>global.json</code> names it and the download page. It never asks for anything to be fetched automatically.</p>

#### `required_dotnet_sdk_version` {#function-required-dotnet-sdk-version}

<div class="function-declaration" id="function-required-dotnet-sdk-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view required_dotnet_sdk_version() <span class="k">noexcept</span>;</code></pre></div>

<p>The SDK version <code>global.json</code> at the root of the engine&#x27;s source names (&quot;10.0.100&quot;), as it read when the engine was configured.</p>

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
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="progress_fn"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
