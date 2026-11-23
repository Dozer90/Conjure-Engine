---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [scheduler.h](../scheduler.md)

# scheduler
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/scheduler.h` |
| Source File Path | `engine/src/ecs/scheduler.cpp` |
| API ↗ | [Command buffers and observers](../../../../api/ecs/command-buffers-and-observers.md), [Queries, views and systems](../../../../api/ecs/queries-views-and-systems.md) |

## Syntax

```cpp
class CONJURE_API scheduler
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">scheduler(
    <a href="../world/world.html">ecs::world</a> &amp;target,
    <a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *jobs,
    <a href="scheduler-desc.html">scheduler_desc</a> desc = {});</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted. Move constructors have been deleted. Move assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~scheduler();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mDesc</code></td><td><code class="signature"><a href="scheduler-desc.html">scheduler_desc</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFreeRecords</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mJobs</code></td><td><code class="signature"><a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Next<wbr>Generation</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPhases</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr>std::<wbr>array&lt;<wbr>phase_<wbr>state,<wbr> SYSTEM_<wbr>PHASE_<wbr>COUNT&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Registration<wbr>Counter</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Resolved<wbr>Component<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Running<wbr>Frame</code></td><td><code class="signature"><span class="k">const</span> <a href="../system-phase/frame-info.html">frame_info</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSystems</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../scheduler.html">system_<wbr>record</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWorld</code></td><td><code class="signature"><a href="../world/world.html">ecs::world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add_system` {#function-add-system}

<div class="function-declaration" id="function-add-system-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="system-id.html">system_id</a>&gt; add_system(
    <a href="../system/system-desc.html">system_desc</a> desc);</code></pre></div>

<p>Adds a system; its phase&#x27;s stages are rebuilt lazily. <code>A</code> name is unique within its phase: <code>NAME_CONFLICT</code> (and the system is not added) when a live system of the same phase already carries the same name. Unnamed systems (the <code>null</code> <code><a href="../../core/string-id/string-id.html">string_id</a></code>) never conflict, and the same name may be used again in another phase or after <code>remove_system</code>. <code>SYSTEM_CYCLE</code> (and the system is not added) when its after/before edges close a cycle; unknown names in those lists, the <code>null</code> name always among them, are ignored with one <code>DEBUG</code> log each, written here and never again by later additions, <code><a href="#function-finalise-1">finalise</a>()</code> or the lazy rebuilds. <code>A</code> resolveAccess hook is called here against the world&#x27;s registry, and again by <code><a href="#function-finalise-1">finalise</a>()</code>, <code>run_phase</code> and <code><a href="#function-stages-1">stages</a>()</code> once the registry has gained component types, so the access set of a system added before a component it names is completed when that component is registered (the pass waits while the world is locked for iteration). Owner thread.</p>

#### `desc` {#function-desc}

<div class="function-declaration" id="function-desc-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="scheduler-desc.html">scheduler_desc</a> &amp;desc() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The parallelism switches in effect.</p>

#### `finalise` {#function-finalise}

<div class="function-declaration" id="function-finalise-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; finalise();</code></pre></div>

<p>Builds every phase&#x27;s stages now and reports <code>SYSTEM_CYCLE</code> and Debug access conflicts. Resolves every system&#x27;s access again first when the registry has gained component types. Optional: <code>run_phase</code> builds lazily; the engine calls it at startup step 18 and after every module load so configuration errors surface before the first frame instead of inside it. Owner thread, outside a running phase.</p>

#### `find_system` {#function-find-system}

<div class="function-declaration" id="function-find-system-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="system-id.html">system_id</a> find_system(
    <a href="../system-phase.html#symbol-system-phase">system_phase</a> target,
    <a href="../../core/string-id/string-id.html">core::string_id</a> name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The live system of a phase registered under <code>name</code> (disabled ones included), which is the one an after/before edge to that name binds to. An invalid id when the phase holds no such system, for the <code>null</code> name (unnamed systems cannot be looked up) and for an out-of-range phase.</p>

#### `last_timings` {#function-last-timings}

<div class="function-declaration" id="function-last-timings-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="system-timing.html">system_timing</a>&gt; last_timings(
    <a href="../system-phase.html#symbol-system-phase">system_phase</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Per-system timings of the last run of a phase, in stage order.</p>

#### `remove_system` {#function-remove-system}

<div class="function-declaration" id="function-remove-system-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; remove_system(
    <a href="system-id.html">system_id</a> id);</code></pre></div>

<p>Removes a system (its pending commands are discarded). <code>SYSTEM_NOT_FOUND</code> for a stale id.</p>

#### `run_phase` {#function-run-phase}

<div class="function-declaration" id="function-run-phase-1"><pre><code><span class="k">void</span> run_phase(
    <a href="../system-phase.html#symbol-system-phase">system_phase</a> target,
    <span class="k">const</span> <a href="../system-phase/frame-info.html">frame_info</a> &amp;frame);</code></pre></div>

<p>Runs every enabled system of the phase, stage by stage, flushing command buffers at each stage boundary. Owner thread, world unlocked. When the registry has gained component types since the access sets were last resolved, every resolveAccess hook runs again first and the phases whose systems gained access are rebuilt before their next run; otherwise that check is one atomic load. The per-stage bookkeeping lives in the phase (kept at capacity) and stage jobs complete on a stack-held <code><a href="../../core/jobs/job-handle/jobs-job-fence.html">core::jobs::job_fence</a></code>, so a phase that has run once allocates nothing of its own afterwards, with <code>parallelSystems</code> and a job system included (tests/ecs/<code>test_scheduler.cpp</code> gates it at zero in both modes).</p>

#### `run_phase_filtered` {#function-run-phase-filtered}

<div class="function-declaration" id="function-run-phase-filtered-1"><pre><code><span class="k">void</span> run_phase_filtered(
    <a href="../system-phase.html#symbol-system-phase">system_phase</a> target,
    <span class="k">const</span> <a href="../system-phase/frame-info.html">frame_info</a> &amp;frame,
    <a href="../system.html#symbol-system-flags">system_flags</a> required);</code></pre></div>

<p>Runs only the systems of the phase whose flags contain every bit of required.</p>

#### `set_enabled` {#function-set-enabled}

<div class="function-declaration" id="function-set-enabled-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_enabled(
    <a href="system-id.html">system_id</a> id,
    <span class="k">bool</span> enabled);</code></pre></div>

<p>Enables or disables a system (<code>DISABLED</code> flag). <code>SYSTEM_NOT_FOUND</code> for a stale id.</p>

#### `stages` {#function-stages}

<div class="function-declaration" id="function-stages-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="stage-info.html">stage_info</a>&gt; stages(
    <a href="../system-phase.html#symbol-system-phase">system_phase</a> target);</code></pre></div>

<p>The stages of a phase. <code>A</code> phase left dirty by <code>add_system</code>, <code>remove_system</code> or <code>set_enabled</code>, or by an access set that grew with the registry, is rebuilt here first, which allocates and can fail, so the method is non-const by design. Empty for an out-of-range phase or a failed rebuild. Owner thread, outside a running phase.</p>

#### `system` {#function-system}

<div class="function-declaration" id="function-system-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../system/system-desc.html">system_desc</a> *system(
    <a href="system-id.html">system_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The description of a system; <code>nullptr</code> for a stale id. Its reads and writes are the access set as last resolved: they grow when <code><a href="#function-finalise-1">finalise</a>()</code>, <code>run_phase</code> or <code><a href="#function-stages-1">stages</a>()</code> resolves it again.</p>

#### `system_count` {#function-system-count}

<div class="function-declaration" id="function-system-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> system_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Registered systems.</p>

### Private

#### `build_stages` {#function-build-stages}

<div class="function-declaration" id="function-build-stages-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; build_stages(
    <a href="../system-phase.html#symbol-system-phase">system_phase</a> target);</code></pre></div>

#### `find_record` {#function-find-record}

<div class="function-declaration" id="function-find-record-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../scheduler.html">system_record</a> *find_record(
    <a href="system-id.html">system_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-find-record-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../scheduler.html">system_record</a> *find_record(
    <a href="system-id.html">system_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `mark_dirty` {#function-mark-dirty}

<div class="function-declaration" id="function-mark-dirty-1"><pre><code><span class="k">void</span> mark_dirty(
    <a href="../system-phase.html#symbol-system-phase">system_phase</a> target);</code></pre></div>

#### `refresh_access` {#function-refresh-access}

<div class="function-declaration" id="function-refresh-access-1"><pre><code><span class="k">void</span> refresh_access();</code></pre></div>

#### `resolve_edges` {#function-resolve-edges}

<div class="function-declaration" id="function-resolve-edges-1"><pre><code><span class="k">void</span> resolve_edges(
    <a href="../system-phase.html#symbol-system-phase">system_phase</a> target,
    std::vector&lt;std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;&gt; &amp;predecessors,
    <span class="k">const</span> <a href="../scheduler.html">system_record</a> *logFor);</code></pre></div>

#### `run_system` {#function-run-system}

<div class="function-declaration" id="function-run-system-1"><pre><code><span class="k">void</span> run_system(
    <a href="../scheduler.html">system_record</a> &amp;record,
    <span class="k">const</span> <a href="../system-phase/frame-info.html">frame_info</a> &amp;frame);</code></pre></div>

#### `topological_order` {#function-topological-order}

<div class="function-declaration" id="function-topological-order-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;&gt; topological_order(
    <a href="../system-phase.html#symbol-system-phase">system_phase</a> target,
    <span class="k">const</span> std::vector&lt;std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;&gt; &amp;predecessors);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
