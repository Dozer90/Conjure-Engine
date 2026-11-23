---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# main_thread.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/jobs/main_thread.h` |
| API ↗ | [Jobs and threading](../../../../api/core/jobs.md) |

<p>The main thread and its task queue. Worker threads and dedicated threads post work that must run on the main thread (window, GPU submission, editor state); the engine drains the queue once per frame before <code>FRAME_BEGIN</code>.</p>

## Functions

### Public

#### `jobs::drain_main_thread_queue` {#function-jobs-drain-main-thread-queue}

<div class="function-declaration" id="function-jobs-drain-main-thread-queue-1"><pre><code><span class="na">CONJURE_API</span> <a href="../types.html#symbol-usize">usize</a> drain_main_thread_queue();</code></pre></div>

<p>Runs every task queued so far, once, in posting order, and returns how many ran. Tasks posted while draining run on the next drain. Called off the designated main thread (or before one is designated) it asserts, runs nothing, leaves the tasks queued and returns 0; it never claims the main-thread designation for the caller.</p>

#### `jobs::is_main_thread` {#function-jobs-is-main-thread}

<div class="function-declaration" id="function-jobs-is-main-thread-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_main_thread() <span class="k">noexcept</span>;</code></pre></div>

<p>True on the main thread: the thread that first called <code>set_main_thread()</code> or constructed the first <code><a href="job-system/jobs-job-system.html">job_system</a></code>. When neither has happened, the first thread to ask becomes the main thread.</p>

#### `jobs::main_thread_id` {#function-jobs-main-thread-id}

<div class="function-declaration" id="function-jobs-main-thread-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::thread::id main_thread_id() <span class="k">noexcept</span>;</code></pre></div>

<p>Identifier of the designated main thread, or a default-constructed id when none is set. Does not claim the calling thread.</p>

#### `jobs::main_thread_queue_size` {#function-jobs-main-thread-queue-size}

<div class="function-declaration" id="function-jobs-main-thread-queue-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../types.html#symbol-usize">usize</a> main_thread_queue_size();</code></pre></div>

<p>Number of tasks waiting for the next drain.</p>

#### `jobs::post_to_main_thread` {#function-jobs-post-to-main-thread}

<div class="function-declaration" id="function-jobs-post-to-main-thread-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> post_to_main_thread(std::function&lt;<span class="k">void</span>()&gt; task);</code></pre></div>

<p>Queues a task for the next <code>drain_main_thread_queue()</code>. Thread-safe. Empty tasks assert and are dropped.</p>

#### `jobs::reset_main_thread` {#function-jobs-reset-main-thread}

<div class="function-declaration" id="function-jobs-reset-main-thread-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> reset_main_thread() <span class="k">noexcept</span>;</code></pre></div>

<p>Forgets the designation so the next <code>set_main_thread()</code> / <code>is_main_thread()</code> claims it again. Intended for tests and for restarting the engine inside one process.</p>

#### `jobs::set_main_thread` {#function-jobs-set-main-thread}

<div class="function-declaration" id="function-jobs-set-main-thread-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> set_main_thread() <span class="k">noexcept</span>;</code></pre></div>

<p>Designates the calling thread as the main thread. The engine calls this at startup; tests call it before using the main-thread queue.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="drain_main_thread_queue"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="is_main_thread"></a>
<a id="jobsdrain_main_thread_queue"></a>
<a id="jobsis_main_thread"></a>
<a id="jobsmain_thread_id"></a>
<a id="jobsmain_thread_queue_size"></a>
<a id="jobspost_to_main_thread"></a>
<a id="jobsreset_main_thread"></a>
<a id="jobsset_main_thread"></a>
<a id="main_thread_id"></a>
<a id="main_thread_queue_size"></a>
<a id="post_to_main_thread"></a>
<a id="reset_main_thread"></a>
<a id="set_main_thread"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
