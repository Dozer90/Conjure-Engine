---
kind: header
---

[Engine](../../engine.md) → [Scripting](../scripting.md)

# managed_build.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scripting/managed_build.h` |
| API ↗ | [Hosting, the native API and hot reload](../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../api/scripting/index.md) |

<p>Building managed projects at runtime. <code><a href="managed-build/managed-build-runner.html">managed_build_runner</a></code> spawns <code>dotnet build</code> through <code><a href="../core/process/process.html">core::process</a></code> on a job, parses the MSBuild diagnostics and posts the report to the main thread with <code>core::jobs::post_to_main_thread</code>; the caller drains that queue (the engine does so before <code>FRAME_BEGIN</code>) and the editor shows the diagnostics. Lives here so <code>conjure_editor</code> --headless can rebuild too.</p>

## Structs

| File | Description |
|---|---|
| [`build_diagnostic`](managed-build/build-diagnostic.md) | One <code>path(line,col): error\|warning CSxxxx: message</code> line of the build output. |
| [`build_report`](managed-build/build-report.md) |  |
| [`build_request`](managed-build/build-request.md) | Everything a build needs. Property values are UTF-8 and literal: <code>command_line</code> escapes them with <code>msbuild_escape()</code>, so a path holding &#x27;%&#x27;, &#x27;;&#x27;, &#x27;,&#x27; or &#x27;&quot;&#x27; reaches MSBuild as itself. |
| [`managed_build_runner::shared_state`](managed-build/managed-build-runner-shared-state.md) |  |

## Classes

| File | Description |
|---|---|
| [`managed_build_runner`](managed-build/managed-build-runner.md) |  |

## Functions

### Public

#### `msbuild_escape` {#function-msbuild-escape}

<div class="function-declaration" id="function-msbuild-escape-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string msbuild_escape(
    std::string_view value);</code></pre></div>

<p><code>value</code> with the characters MSBuild treats specially in a command-line property value written as the %XX escapes MSBuild decodes: &#x27;%&#x27; (which starts an escape itself), &#x27;;&#x27; and &#x27;,&#x27; (which separate properties) and &#x27;&quot;&#x27; (which quotes). dotnet hands -o to MSBuild as a property too. Every other <code><a href="../core/types.html#symbol-byte">byte</a></code>, UTF-8 included, is kept.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="build_diagnostic"></a>
<a id="build_diagnosticcode"></a>
<a id="build_diagnosticcolumn"></a>
<a id="build_diagnosticfile"></a>
<a id="build_diagnosticiserror"></a>
<a id="build_diagnosticline"></a>
<a id="build_diagnosticmessage"></a>
<a id="build_diagnosticoperator"></a>
<a id="build_report"></a>
<a id="build_reportdiagnostics"></a>
<a id="build_reportelapsedseconds"></a>
<a id="build_reporterror_count"></a>
<a id="build_reportexitcode"></a>
<a id="build_reportlaunch"></a>
<a id="build_reportoutput"></a>
<a id="build_reportsucceeded"></a>
<a id="build_reporttimedout"></a>
<a id="build_reportwarning_count"></a>
<a id="build_request"></a>
<a id="build_requestconfiguration"></a>
<a id="build_requestoutputdirectory"></a>
<a id="build_requestprojectpath"></a>
<a id="build_requestproperties"></a>
<a id="build_requesttimeoutseconds"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="managed_build_runner"></a>
<a id="managed_build_runnerbuild"></a>
<a id="managed_build_runnerbuild_count"></a>
<a id="managed_build_runnercommand_line"></a>
<a id="managed_build_runnerdotnet_executable"></a>
<a id="managed_build_runneris_building"></a>
<a id="managed_build_runnermanaged_build_runner"></a>
<a id="managed_build_runnermanaged_build_runner_1"></a>
<a id="managed_build_runnermbuildcount"></a>
<a id="managed_build_runnermdotnetexecutable"></a>
<a id="managed_build_runnermjob"></a>
<a id="managed_build_runnermjobs"></a>
<a id="managed_build_runnermstate"></a>
<a id="managed_build_runneroperator"></a>
<a id="managed_build_runnerparse_output"></a>
<a id="managed_build_runnerrun_blocking"></a>
<a id="managed_build_runnerset_dotnet_executable"></a>
<a id="managed_build_runnershared_state"></a>
<a id="managed_build_runnershared_statebuilding"></a>
<a id="managed_build_runnerwait"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
