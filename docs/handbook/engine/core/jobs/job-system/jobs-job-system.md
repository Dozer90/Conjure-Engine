---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [job_system.h](../job-system.md)

# jobs::job_system
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::jobs` |
| Include File Path | `engine/include/conjure/core/jobs/job_system.h` |
| Source File Path | `engine/src/core/jobs/job_system.cpp` |
| API ↗ | [Jobs and threading](../../../../../api/core/jobs.md) |

## Syntax

```cpp
class CONJURE_API job_system
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">job_system();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~job_system();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mState</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../../event/event-state.html">state</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `current` {#function-current}

<div class="function-declaration" id="function-current-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="jobs-job-system.html">job_system</a> *current() <span class="k">noexcept</span>;</code></pre></div>

<p>The system whose worker the calling thread is, or <code>nullptr</code> on every other thread.</p>

#### `is_running` {#function-is-running}

<div class="function-declaration" id="function-is-running-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_running() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True between a successful <code><a href="#function-start-1">start</a>()</code> and <code><a href="#function-stop-1">stop</a>()</code>.</p>

#### `parallel_for` {#function-parallel-for}

<div class="function-declaration" id="function-parallel-for-1"><pre><code><span class="k">void</span> parallel_for(
    <a href="../../types.html#symbol-usize">usize</a> count,
    <a href="../../types.html#symbol-usize">usize</a> grain,
    <span class="k">const</span> std::function&lt;<span class="k">void</span>(<a href="../../types.html#symbol-usize">usize</a> begin, <a href="../../types.html#symbol-usize">usize</a> end)&gt; &amp;body);</code></pre></div>

<p>Splits [0, count) into ranges of at most <code>grain</code> elements (a grain of 0 counts as 1), runs them across the workers and the caller, and returns when every range has finished. Runs the whole range inline when there are no workers or count &lt;= grain. The body is referenced, not copied, and the dispatch does not allocate once the block pool has warmed up. Nesting from inside a job or another <code>parallel_for</code> is fine.</p>

<div class="function-declaration" id="function-parallel-for-2"><pre><code><span class="k">void</span> parallel_for(
    <a href="../../types.html#symbol-usize">usize</a> count,
    <span class="k">const</span> std::function&lt;<span class="k">void</span>(<a href="../../types.html#symbol-usize">usize</a> index)&gt; &amp;body);</code></pre></div>

<p>Per-index variant with grain = <code>max(1, count / (workerCount * 4))</code>.</p>

#### `schedule` {#function-schedule}

<div class="function-declaration" id="function-schedule-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../job-handle/jobs-job-handle.html">job_handle</a> schedule(
    std::function&lt;<span class="k">void</span>()&gt; job,
    <a href="../job-system.html#symbol-jobs-job-priority">job_priority</a> priority = job_priority::NORMAL);</code></pre></div>

<p>Queues one job and returns its handle. An empty job asserts and yields an invalid handle. When the system is not running, has no workers or the queue is full, the job executes on the caller before this returns.</p>

<div class="function-declaration" id="function-schedule-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../job-handle/jobs-job-handle.html">job_handle</a> schedule(
    std::span&lt;<span class="k">const</span> std::function&lt;<span class="k">void</span>()&gt;&gt; jobs,
    <a href="../job-system.html#symbol-jobs-job-priority">job_priority</a> priority = job_priority::NORMAL);</code></pre></div>

<p>Queues a batch and returns one handle that completes when every job in it has run. Empty entries are skipped (asserting); an empty batch yields an invalid handle.</p>

<div class="function-declaration" id="function-schedule-3"><pre><code><span class="k">void</span> schedule(
    std::span&lt;<span class="k">const</span> std::function&lt;<span class="k">void</span>()&gt;&gt; jobs,
    <a href="../job-handle/jobs-job-fence.html">job_fence</a> &amp;fence,
    <a href="../job-system.html#symbol-jobs-job-priority">job_priority</a> priority = job_priority::NORMAL);</code></pre></div>

<p>Queues a batch that signals a caller-owned fence instead of returning a handle: the handle&#x27;s shared counter is the one heap allocation of <code><a href="#function-schedule-1">schedule</a>()</code>, so this overload allocates nothing once the block pool has warmed up, which per-frame dispatch (the ECS scheduler&#x27;s stages) relies on. The batch is added to whatever the fence already counts; <code><a href="#function-wait-1">wait</a>(fence)</code> returns when all of it has run. The fence must stay alive and in place until then. Empty entries are skipped (asserting); an empty batch leaves the fence unchanged. Inline execution follows the same rules as the handle overload.</p>

#### `start` {#function-start}

<div class="function-declaration" id="function-start-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../result/result.html">result</a>&lt;<span class="k">void</span>&gt; start(
    <span class="k">const</span> <a href="jobs-job-system-desc.html">job_system_desc</a> &amp;desc);</code></pre></div>

<p>Creates <code>desc.workerCount</code> named worker threads and returns once every one of them is running and waiting for work. The thread names are prepared on the calling thread, so a worker allocates nothing from the moment <code><a href="#function-start-1">start</a>()</code> returns: an allocation gate opened right after <code><a href="#function-start-1">start</a>()</code> is never charged a worker&#x27;s start-up. Fails with <code>INVALID_STATE</code> when already running and with <code>INVALID_ARGUMENT</code> for a queue capacity of zero or above <code>MAX_QUEUED_JOBS</code>, a worker count above <code>MAX_WORKER_COUNT</code> or a <code>null</code>/empty thread name prefix. Statistics are reset. Resource exhaustion is not one of the reported failures: if the operating system refuses to create a worker thread, the <code>std::system_error</code> that <code>std::thread</code> throws is not caught (engine code never catches) and ends the process like an allocation failure.</p>

#### `stats` {#function-stats}

<div class="function-declaration" id="function-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="jobs-job-system-stats.html">job_system_stats</a> stats() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Snapshot of the counters accumulated since <code><a href="#function-start-1">start</a>()</code>. Sums one per-thread slot per worker plus one for every other thread (a few hundred bytes of loads), so reading it every frame is fine.</p>

#### `stop` {#function-stop}

<div class="function-declaration" id="function-stop-1"><pre><code><span class="k">void</span> stop();</code></pre></div>

<p>Stops accepting jobs from non-worker threads, lets the workers finish everything that is queued, then joins them. Safe to call twice and from the destructor. Must not be called from one of the system&#x27;s own workers.</p>

#### `thread_index` {#function-thread-index}

<div class="function-declaration" id="function-thread-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../types.html#symbol-u32">u32</a> thread_index() <span class="k">noexcept</span>;</code></pre></div>

<p>0 on the main thread and on any thread that is not a job system worker; 1..<code>N</code> on the workers of the system that owns them.</p>

#### `wait` {#function-wait}

<div class="function-declaration" id="function-wait-1"><pre><code><span class="k">void</span> wait(
    <span class="k">const</span> <a href="../job-handle/jobs-job-handle.html">job_handle</a> &amp;handle);</code></pre></div>

<p>Blocks until the handle is complete. While waiting the caller executes queued jobs, so this is safe from inside a job and from a worker. Returns immediately for an invalid or complete handle.</p>

<div class="function-declaration" id="function-wait-2"><pre><code><span class="k">void</span> wait(
    <span class="k">const</span> <a href="../job-handle/jobs-job-fence.html">job_fence</a> &amp;fence);</code></pre></div>

<p>Blocks until every job scheduled on the fence has run, helping with queued jobs meanwhile like <code><a href="#function-wait-1">wait</a>(handle)</code>. Returns immediately for a complete fence.</p>

#### `wait_idle` {#function-wait-idle}

<div class="function-declaration" id="function-wait-idle-1"><pre><code><span class="k">void</span> wait_idle();</code></pre></div>

<p>Blocks until no job is queued or running, helping with queued jobs meanwhile. From inside a job it cannot wait for the jobs the calling thread is itself executing, nor for jobs that are themselves blocked in <code><a href="#function-wait-idle-1">wait_idle</a>()</code> (two such jobs would wait for each other forever); it returns once every other job has finished. Only a call made outside every job therefore guarantees that the system is completely idle when it returns.</p>

#### `worker_count` {#function-worker-count}

<div class="function-declaration" id="function-worker-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u32">u32</a> worker_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of worker threads, 0 when everything runs inline (not started or started with <code>workerCount</code> 0).</p>

### Private

#### `parallel_for_impl` {#function-parallel-for-impl}

<div class="function-declaration" id="function-parallel-for-impl-1"><pre><code><span class="k">void</span> parallel_for_impl(
    <a href="../../types.html#symbol-usize">usize</a> count,
    <a href="../../types.html#symbol-usize">usize</a> grain,
    <a href="../job-system.html#symbol-jobs-detail-range-invoke-fn">detail::range_invoke_fn</a> invoke,
    <span class="k">const</span> <span class="k">void</span> *context);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
