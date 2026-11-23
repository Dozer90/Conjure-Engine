---
kind: class
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [managed_build.h](../managed-build.md)

# managed_build_runner
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/managed_build.h` |
| Source File Path | `engine/src/scripting/managed_build.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
class CONJURE_API managed_build_runner
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">managed_build_runner(
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *jobs = nullptr);</code></td><td><code>jobs</code> runs the build off the main thread; <code>nullptr</code> runs it inline inside <code>build()</code> (the report is still posted to the main-thread queue so callers behave the same either way).</td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~managed_build_runner();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mBuildCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Dotnet<wbr>Executable</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">"dotnet"</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mJob</code></td><td><code class="signature"><a href="../../core/jobs/job-handle/jobs-job-handle.html">core::jobs::job_handle</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mJobs</code></td><td><code class="signature"><a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature">std::<wbr>shared_<wbr>ptr&lt;<wbr><a href="managed-build-runner-shared-state.html">shared_<wbr>state</a>&gt;</code></td><td><code class="signature">std::<wbr>make_<wbr>shared&lt;<wbr><a href="managed-build-runner-shared-state.html">shared_<wbr>state</a>&gt;()</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `build` {#function-build}

<div class="function-declaration" id="function-build-1"><pre><code><span class="k">void</span> build(
    <span class="k">const</span> std::filesystem::path &amp;projectPath,
    std::string_view configuration,
    std::function&lt;<span class="k">void</span>(<span class="k">const</span> <a href="build-report.html">build_report</a> &amp;)&gt; onComplete);</code></pre></div>

<p>Spawns <code>dotnet build &lt;project&gt; -c &lt;configuration&gt;</code> on a job; <code>onComplete</code> runs on the main thread at the next <code>drain_main_thread_queue()</code>. Asserts and ignores the call while a build runs.</p>

<div class="function-declaration" id="function-build-2"><pre><code><span class="k">void</span> build(
    <span class="k">const</span> <a href="build-request.html">build_request</a> &amp;request,
    std::function&lt;<span class="k">void</span>(<span class="k">const</span> <a href="build-report.html">build_report</a> &amp;)&gt; onComplete);</code></pre></div>

<p>Same with every option of <code><a href="build-request.html">build_request</a></code>.</p>

#### `build_count` {#function-build-count}

<div class="function-declaration" id="function-build-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> build_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Builds issued so far.</p>

#### `command_line` {#function-command-line}

<div class="function-declaration" id="function-command-line-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/process/process-desc.html">core::process_desc</a> command_line(
    <span class="k">const</span> <a href="build-request.html">build_request</a> &amp;request,
    std::string_view dotnet);</code></pre></div>

<p>The process description a request turns into: dotnet build &lt;project&gt; -c &lt;configuration&gt; [-o &lt;output&gt;] --nologo -clp:NoSummary [-p:Name=Value...]. It never adds ContinuousIntegrationBuild, so the debug information keeps the real source paths and the built scripts can be debugged; a caller that wants it passes it in <code>build_request::properties</code>.</p>

#### `dotnet_executable` {#function-dotnet-executable}

<div class="function-declaration" id="function-dotnet-executable-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;dotnet_executable() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_building` {#function-is-building}

<div class="function-declaration" id="function-is-building-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_building() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `parse_output` {#function-parse-output}

<div class="function-declaration" id="function-parse-output-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> std::vector&lt;<a href="build-diagnostic.html">build_diagnostic</a>&gt; parse_output(
    std::string_view msbuildOutput);</code></pre></div>

<p>Parses MSBuild output: <code>file(line,col): error CS1002: message</code> and the <code>warning</code> form; other lines are ignored. Paths with spaces and project suffixes <code>[Game.csproj]</code> are handled.</p>

#### `run_blocking` {#function-run-blocking}

<div class="function-declaration" id="function-run-blocking-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="build-report.html">build_report</a> run_blocking(
    <span class="k">const</span> <a href="build-request.html">build_request</a> &amp;request,
    std::string_view dotnet);</code></pre></div>

<p>Runs a request to completion on the calling thread.</p>

#### `set_dotnet_executable` {#function-set-dotnet-executable}

<div class="function-declaration" id="function-set-dotnet-executable-1"><pre><code><span class="k">void</span> set_dotnet_executable(
    std::string executable);</code></pre></div>

<p>The dotnet program to spawn (&quot;dotnet&quot; from PATH by default).</p>

#### `wait` {#function-wait}

<div class="function-declaration" id="function-wait-1"><pre><code><span class="k">void</span> wait();</code></pre></div>

<p>Blocks until the running build (if any) finished; the report is then in the main-thread queue.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
