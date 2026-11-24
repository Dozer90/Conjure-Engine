---
kind: class
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [export.h](../export.md)

# export_pipeline
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/export.h` |
| Source File Path | N/A |
| API ↗ | [Export and diagnostics](../../../../api/engine/export-and-diagnostics.md), [Engine: the application and the frame](../../../../api/engine/index.md) |

## Syntax

```cpp
class CONJURE_API export_pipeline
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> export_pipeline(
    <a href="../engine-context/engine-context.html">engine_context</a> &amp;context);</code></td><td>Binds the pipeline to a running application&#x27;s context: it needs the project, the asset database and manager, the module manager and the job system.</td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~export_pipeline();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">MANAGED_<wbr>SDK_<wbr>MANIFEST_<wbr>NAME</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code></td><td><code class="signature"><span class="s">"conjure_<wbr>managed_<wbr>sdk.json"</span></code></td><td>The file that marks a folder as the prebuilt SDK of <code>export_settings::prebuiltManagedDirectory</code>. The build target <code>conjure_managed_sdk_export</code> writes it after its publish succeeded: a JSON object with &quot;<code>schema_version</code>&quot; 1, &quot;kind&quot; &quot;<code>conjure_managed_sdk_export</code>&quot;, &quot;<code><a href="../../scripting/script-host.html#symbol-host-mode">host_mode</a></code>&quot; &quot;<code>framework_dependent</code>&quot; and &quot;<code>native_api_hash</code>&quot;, the <code>CONJURE_NATIVE_API_HASH</code> (conjure/scripting/<code>native_api_hash.h</code>) of the build, as hexadecimal text (&quot;0x...&quot;).</td></tr>
<tr><td>private</td><td><code class="signature">mContext</code></td><td><code class="signature"><a href="../engine-context/engine-context.html">engine_context</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">NOTICES_<wbr>FILE_<wbr>NAME</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code></td><td><code class="signature"><span class="s">"THIRD_<wbr>PARTY_<wbr>NOTICES.txt"</span></code></td><td>The file of licence notices a build writes next to <code>conjure_runtime</code> and step 5 copies into the output: Conjure&#x27;s own notice and licence, then those of the third-party software that build links.</td></tr>
<tr><td>public</td><td><code class="signature">STEP_NAMES</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>array&lt;<wbr>std::<wbr>string_<wbr>view,<wbr> <span class="mi">6</span>&gt;</code></td><td><code class="signature">{ <span class="s">"build_<wbr>game_<wbr>module"</span>,<wbr> <span class="s">"cook_<wbr>assets"</span>,<wbr> <span class="s">"publish_<wbr>managed"</span>,<wbr> <span class="s">"export_<wbr>modules"</span>,<wbr> <span class="s">"copy_<wbr>runtime"</span>,<wbr> <span class="s">"write_<wbr>project"</span>,<wbr> }</code></td><td>The step names reported through <code><a href="../export.html#symbol-progress-fn">progress_fn</a></code> and <code>export_report::steps</code>, in order.</td></tr>
</tbody>
</table>

## Functions

### Public

#### `run` {#function-run}

<div class="function-declaration" id="function-run-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="export-report.html">export_report</a>&gt; run(
    <span class="k">const</span> <a href="export-settings.html">export_settings</a> &amp;settings,
    <span class="k">const</span> <a href="../export.html#symbol-progress-fn">progress_fn</a> &amp;progress = {});</code></pre></div>

<p>Runs the six steps. <code>EXPORT_FAILED</code> names the step and the cause; the steps before it keep their output, and step 5, which copies file by file, keeps the files it copied before the one that failed. <code>INVALID_STATE</code> (core) when the context has no project or no asset database (a cooked project cannot be exported again). <code>BAD_ARGUMENTS</code> for an empty output directory, the project directory itself or a folder inside the assets root, and for an artifacts directory inside the output directory or the assets root (checked once the output directory exists, before step 1; the refusal removes the folders the export created for the output again, so it writes nothing). <code>EXPORT_FAILED</code> naming <code>publish_managed</code>, before the output directory is created, when <code>publishManaged</code> is set and the project enables scripting but the engine&#x27;s configuration disables it (--no-scripting): the exported runtime would start the .NET host and find no managed code. <code>EXPORT_FAILED</code> naming <code>write_project</code>, before the output directory is created, when <code>project.json</code> &quot;<code>rendering.defaults</code>&quot; is not valid (<code>renderer::render_settings_from_json</code>; the message carries the first problem&#x27;s slash path and reason; unknown keys are not problems): the exported runtime reads it strictly and would refuse to start. <code>A</code> build without the renderer checks nothing. <code>BAD_ARGUMENTS</code>, before the output directory is created, when the settings name both <code>bootstrapProject</code> and <code>prebuiltManagedDirectory</code>, and, before step 1, when step 3 would copy a prebuilt SDK that lies inside the output directory (an existing one, then, which keeps what it held). <code>EXPORT_FAILED</code> naming <code>publish_managed</code>, before the output directory is created, when step 3 would copy a prebuilt SDK whose folder lacks <code>MANAGED_SDK_MANIFEST_NAME</code>, holds one of another kind, schema, host mode or native API hash than this engine&#x27;s, misses <code>Conjure.Bootstrap.dll</code>, its <code>runtimeconfig.json</code> or <code>Conjure.Managed.dll</code>, or when the export is self-contained. <code>EXPORT_FAILED</code> naming <code>publish_managed</code> and carrying <code>missing_dotnet_sdk_message()</code> when step 3 is about to run dotnet and <code>find_dotnet_sdk()</code> finds no usable SDK; steps 1 and 2 keep their output, and so does the copy of a prebuilt SDK in &lt;dir&gt;/managed. <code>A</code> distribution directory without Visual <code>C</code>++ runtime DLLs is not an error: a pipeline built with MSVC against the DLL runtime reports it as a warning. Debug runtime DLLs in it are not copied and are named in a warning. Neither is a directory without <code>NOTICES_FILE_NAME</code> an error: the report warns that the game carries no licence notices.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
