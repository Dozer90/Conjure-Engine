---
kind: class
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [null_script_host.h](../null-script-host.md)

# null_host_recorder
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/null_script_host.h` |
| Source File Path | `engine/src/scripting/null_script_host.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
class CONJURE_API null_host_recorder
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">null_host_recorder();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">null_host_recorder(
    <span class="k">const</span> <a href="null-host-recorder.html">null_host_recorder</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~null_host_recorder();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mActivities</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="null-host-recorder-activity-record.html">activity_<wbr>record</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mApplies</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="null-host-recorder-apply-record.html">apply_<wbr>record</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAX_RECORDS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">65536</span></code></td><td>Entries each list keeps; later records are dropped and counted.</td></tr>
<tr><td>private</td><td><code class="signature">mCalls</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Collect<wbr>Fields<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Collect<wbr>Garbage<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCollisions</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="null-host-recorder-collision-record.html">collision_<wbr>record</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCreates</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="null-host-recorder-create-record.html">create_<wbr>record</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDestroys</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="null-host-recorder-destroy-record.html">destroy_<wbr>record</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Dropped<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEnabled</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Loaded<wbr>Assemblies</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPhases</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="null-host-recorder-phase-record.html">phase_<wbr>record</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Sample<wbr>Memory<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Shutdown<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mUnloadCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `activities` {#function-activities}

<div class="function-declaration" id="function-activities-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::vector&lt;<a href="null-host-recorder-activity-record.html">activity_record</a>&gt; &amp;activities() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Every slot of every <code>set_active</code> call, in call order.</p>

#### `applies` {#function-applies}

<div class="function-declaration" id="function-applies-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::vector&lt;<a href="null-host-recorder-apply-record.html">apply_record</a>&gt; &amp;applies() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `calls` {#function-calls}

<div class="function-declaration" id="function-calls-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::vector&lt;std::string&gt; &amp;calls() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Every <code>managed_api</code> function called, in order, by name (&quot;<code>run_phase</code>&quot;, &quot;<code>create_instances</code>&quot;, ...).</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear();</code></pre></div>

<p>Forgets everything recorded; whether recording is enabled is kept.</p>

#### `collect_fields_count` {#function-collect-fields-count}

<div class="function-declaration" id="function-collect-fields-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> collect_fields_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `collect_garbage_count` {#function-collect-garbage-count}

<div class="function-declaration" id="function-collect-garbage-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> collect_garbage_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `collisions` {#function-collisions}

<div class="function-declaration" id="function-collisions-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::vector&lt;<a href="null-host-recorder-collision-record.html">collision_record</a>&gt; &amp;collisions() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `count` {#function-count}

<div class="function-declaration" id="function-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> count(
    std::string_view call) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of recorded calls with this name (a scan of <code><a href="#function-calls-1">calls</a>()</code>: at most <code>MAX_RECORDS</code> are kept, and none while recording is disabled).</p>

#### `creates` {#function-creates}

<div class="function-declaration" id="function-creates-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::vector&lt;<a href="null-host-recorder-create-record.html">create_record</a>&gt; &amp;creates() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `destroys` {#function-destroys}

<div class="function-declaration" id="function-destroys-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::vector&lt;<a href="null-host-recorder-destroy-record.html">destroy_record</a>&gt; &amp;destroys() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `dropped_count` {#function-dropped-count}

<div class="function-declaration" id="function-dropped-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> dropped_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Records a full list turned away since the last <code><a href="#function-clear-1">clear</a>()</code>.</p>

#### `is_enabled` {#function-is-enabled}

<div class="function-declaration" id="function-is-enabled-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_enabled() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>False after <code><a href="#function-set-enabled-1">set_enabled</a>(false)</code>: the lists stay as they are.</p>

#### `loaded_assemblies` {#function-loaded-assemblies}

<div class="function-declaration" id="function-loaded-assemblies-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::vector&lt;std::string&gt; &amp;loaded_assemblies() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Paths handed to <code>load_game_assembly</code>, in order.</p>

#### `phases` {#function-phases}

<div class="function-declaration" id="function-phases-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::vector&lt;<a href="null-host-recorder-phase-record.html">phase_record</a>&gt; &amp;phases() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `record_activity` {#function-record-activity}

<div class="function-declaration" id="function-record-activity-1"><pre><code><span class="k">void</span> record_activity(
    <span class="k">const</span> <a href="null-host-recorder-activity-record.html">activity_record</a> &amp;record);</code></pre></div>

#### `record_apply` {#function-record-apply}

<div class="function-declaration" id="function-record-apply-1"><pre><code><span class="k">void</span> record_apply(
    <span class="k">const</span> <a href="null-host-recorder-apply-record.html">apply_record</a> &amp;record);</code></pre></div>

#### `record_call` {#function-record-call}

<div class="function-declaration" id="function-record-call-1"><pre><code><span class="k">void</span> record_call(
    std::string_view call);</code></pre></div>

#### `record_collect_fields` {#function-record-collect-fields}

<div class="function-declaration" id="function-record-collect-fields-1"><pre><code><span class="k">void</span> record_collect_fields();</code></pre></div>

#### `record_collect_garbage` {#function-record-collect-garbage}

<div class="function-declaration" id="function-record-collect-garbage-1"><pre><code><span class="k">void</span> record_collect_garbage();</code></pre></div>

#### `record_collision` {#function-record-collision}

<div class="function-declaration" id="function-record-collision-1"><pre><code><span class="k">void</span> record_collision(
    <span class="k">const</span> <a href="null-host-recorder-collision-record.html">collision_record</a> &amp;record);</code></pre></div>

#### `record_create` {#function-record-create}

<div class="function-declaration" id="function-record-create-1"><pre><code><span class="k">void</span> record_create(
    <span class="k">const</span> <a href="null-host-recorder-create-record.html">create_record</a> &amp;record);</code></pre></div>

#### `record_destroy` {#function-record-destroy}

<div class="function-declaration" id="function-record-destroy-1"><pre><code><span class="k">void</span> record_destroy(
    <span class="k">const</span> <a href="null-host-recorder-destroy-record.html">destroy_record</a> &amp;record);</code></pre></div>

#### `record_load` {#function-record-load}

<div class="function-declaration" id="function-record-load-1"><pre><code><span class="k">void</span> record_load(
    std::string path);</code></pre></div>

#### `record_phase` {#function-record-phase}

<div class="function-declaration" id="function-record-phase-1"><pre><code><span class="k">void</span> record_phase(
    <span class="k">const</span> <a href="null-host-recorder-phase-record.html">phase_record</a> &amp;record);</code></pre></div>

#### `record_sample_memory` {#function-record-sample-memory}

<div class="function-declaration" id="function-record-sample-memory-1"><pre><code><span class="k">void</span> record_sample_memory();</code></pre></div>

#### `record_shutdown` {#function-record-shutdown}

<div class="function-declaration" id="function-record-shutdown-1"><pre><code><span class="k">void</span> record_shutdown();</code></pre></div>

#### `record_unload` {#function-record-unload}

<div class="function-declaration" id="function-record-unload-1"><pre><code><span class="k">void</span> record_unload();</code></pre></div>

#### `sample_memory_count` {#function-sample-memory-count}

<div class="function-declaration" id="function-sample-memory-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> sample_memory_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>sample_memory</code> calls. The engine samples once per frame, so these calls are counted here only and never appear in <code><a href="#function-calls-1">calls</a>()</code>.</p>

#### `set_enabled` {#function-set-enabled}

<div class="function-declaration" id="function-set-enabled-1"><pre><code><span class="k">void</span> set_enabled(
    <span class="k">bool</span> enabled) <span class="k">noexcept</span>;</code></pre></div>

<p>False stops filling the lists, so recording allocates nothing (the <code>null</code> host then builds no record either); the counters keep counting.</p>

#### `shutdown_count` {#function-shutdown-count}

<div class="function-declaration" id="function-shutdown-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> shutdown_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `unload_count` {#function-unload-count}

<div class="function-declaration" id="function-unload-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> unload_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `admits` {#function-admits}

<div class="function-declaration" id="function-admits-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> admits(
    <a href="../../core/types.html#symbol-usize">usize</a> listSize) <span class="k">noexcept</span>;</code></pre></div>

<p>True when a list of this size may take one more record; counts a drop when it is full.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="null-host-recorder.html">null_host_recorder</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="null-host-recorder.html">null_host_recorder</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
