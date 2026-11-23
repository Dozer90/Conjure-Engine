---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [system.h](../system.md)

# system_context
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/system.h` |
| Source File Path | `engine/src/ecs/system.cpp` |
| API ↗ | [Command buffers and observers](../../../../api/ecs/command-buffers-and-observers.md), [Queries, views and systems](../../../../api/ecs/queries-views-and-systems.md) |

## Syntax

```cpp
class CONJURE_API system_context
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">system_context(
    <a href="../world/world.html">ecs::world</a> &amp;target,
    <span class="k">const</span> <a href="../system-phase/frame-info.html">frame_info</a> &amp;frame,
    <a href="../../core/types.html#symbol-u64">u64</a> sinceVersion,
    <a href="../../core/types.html#symbol-u64">u64</a> runVersion,
    <a href="../command-buffer/command-buffer.html">command_buffer</a> &amp;commands,
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *jobs,
    <span class="k">const</span> <a href="system-desc.html">system_desc</a> &amp;desc,
    <a href="system-scratch.html">system_scratch</a> &amp;scratch,
    <span class="k">bool</span> parallelChunks,
    <a href="../../core/types.html#symbol-u32">u32</a> minEntitiesPerJob) <span class="k">noexcept</span>;</code></td><td>Builds a context. <code>sinceVersion</code> is the version observed at the start of the previous run, <code>runVersion</code> the version chunk views created through this context stamp.</td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~system_context();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mCommands</code></td><td><code class="signature"><a href="../command-buffer/command-buffer.html">command_buffer</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDesc</code></td><td><code class="signature"><span class="k">const</span> <a href="system-desc.html">system_desc</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFrame</code></td><td><code class="signature"><span class="k">const</span> <a href="../system-phase/frame-info.html">frame_info</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mJobs</code></td><td><code class="signature"><a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Last<wbr>Grain<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Min<wbr>Entities<wbr>Per<wbr>Job</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1024</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Parallel<wbr>Chunks</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRunVersion</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mScratch</code></td><td><code class="signature"><a href="system-scratch.html">system_scratch</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Since<wbr>Version</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWorld</code></td><td><code class="signature"><a href="../world/world.html">ecs::world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `commands` {#function-commands}

<div class="function-declaration" id="function-commands-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../command-buffer/command-buffer.html">command_buffer</a> &amp;commands() <span class="k">noexcept</span>;</code></pre></div>

<p>The system&#x27;s command buffer, flushed when the stage ends. System thread only; jobs of <code>parallel_for_chunks</code> receive their own grain buffer.</p>

#### `delta_time` {#function-delta-time}

<div class="function-declaration" id="function-delta-time-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-f32">f32</a> delta_time() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code><a href="#function-frame-1">frame</a>()</code>.deltaTime.</p>

#### `desc` {#function-desc}

<div class="function-declaration" id="function-desc-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="system-desc.html">system_desc</a> &amp;desc() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The running system&#x27;s description.</p>

#### `frame` {#function-frame}

<div class="function-declaration" id="function-frame-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../system-phase/frame-info.html">frame_info</a> &amp;frame() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The full per-frame timing block.</p>

#### `jobs` {#function-jobs}

<div class="function-declaration" id="function-jobs-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *jobs() <span class="k">noexcept</span>;</code></pre></div>

<p>The job system, or <code>nullptr</code> when everything runs inline.</p>

#### `last_grain_count` {#function-last-grain-count}

<div class="function-declaration" id="function-last-grain-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> last_grain_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of grains the last <code>parallel_for_chunks</code> dispatch used (for tests and profiling).</p>

#### `parallel_for_chunks` {#function-parallel-for-chunks}

<div class="function-declaration" id="function-parallel-for-chunks-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Callable&gt; <span class="k">void</span> parallel_for_chunks(
    <a href="../query/query.html">query</a> &amp;target,
    Callable &amp;&amp;callable);</code></pre></div>

<p>Runs <code>callable(<a href="../chunk-view/chunk-view.html">chunk_view</a> &amp;, <a href="../command-buffer/command-buffer.html">command_buffer</a> &amp;)</code> over the chunks matched by <code>target</code>. Chunks are grouped into grains of <code>max(1, minEntitiesPerJob / capacity)</code> chunks, each grain runs as one job (inline without a job system or with chunk parallelism disabled) with its own command buffer, and the grain buffers are appended to <code><a href="#function-commands-1">commands</a>()</code> in grain order after the join. The callable must not touch state shared between grains without synchronisation.</p>

#### `run_version` {#function-run-version}

<div class="function-declaration" id="function-run-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> run_version() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The version this run stamps.</p>

#### `since_version` {#function-since-version}

<div class="function-declaration" id="function-since-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> since_version() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The world version at the start of this system&#x27;s previous run: a <code>changed&lt;T&gt;</code> filter against it sees every write since, but never the system&#x27;s own previous writes made through <code><a href="#function-view-1">view</a>()</code> or <code>parallel_for_chunks</code> (those stamp <code><a href="#function-run-version-1">run_version</a>()</code>). Writes through <code><a href="#function-world-1">world</a>()</code>.<code>get&lt;T&gt;()</code>, <code>set()</code> or <code><a href="../entity-handle/component-ref.html">component_ref</a></code> stamp the current world version, which a sibling&#x27;s run or a structural call may have bumped past <code><a href="#function-run-version-1">run_version</a>()</code>, so the next run can see those.</p>

#### `view` {#function-view}

<div class="function-declaration" id="function-view-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... ComponentTypes&gt; [[<span class="na">nodiscard</span>]] <a href="../view/view.html">ecs::view</a>&lt;ComponentTypes...&gt; view();</code></pre></div>

<p><code>A</code> typed view validated against the declared access in Debug and Development: every required or optional component must be in reads | writes and every written one in writes. Violations assert naming the system. The chunk views the result hands out carry this context, so a non-const span of an undeclared component asserts too; the view must not outlive the system run. The checks compile out in Distribution; the view is bound to this context in every configuration, so its writes stamp <code><a href="#function-run-version-1">run_version</a>()</code>.</p>

#### `world` {#function-world}

<div class="function-declaration" id="function-world-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../world/world.html">ecs::world</a> &amp;world() <span class="k">noexcept</span>;</code></pre></div>

<p>The world the system runs on.</p>

<div class="function-declaration" id="function-world-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../world/world.html">ecs::world</a> &amp;world() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `dispatch_chunks` {#function-dispatch-chunks}

<div class="function-declaration" id="function-dispatch-chunks-1"><pre><code><span class="k">void</span> dispatch_chunks(
    <a href="../query/query.html">query</a> &amp;target,
    <span class="k">void</span> (*visit)(<span class="k">void</span> *context, <a href="../chunk-view/chunk-view.html">chunk_view</a> &amp;view, <a href="../command-buffer/command-buffer.html">command_buffer</a> &amp;commands),
    <span class="k">void</span> *context);</code></pre></div>

#### `validate_access` {#function-validate-access}

<div class="function-declaration" id="function-validate-access-1"><pre><code><span class="k">void</span> validate_access(
    <span class="k">const</span> <a href="../view/view-access.html">view_access</a> &amp;access) <span class="k">const</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
