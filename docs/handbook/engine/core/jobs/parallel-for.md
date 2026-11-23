---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# parallel_for.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/jobs/parallel_for.h` |
| API ↗ | [Jobs and threading](../../../../api/core/jobs.md) |

<p>Convenience wrappers around <code>job_system::parallel_for</code> that accept any callable instead of a <code>std::function</code>. The wrapper <code>std::function</code> is built once per call and holds only references, so it stays within the small-object buffer and the dispatch remains allocation-free.</p>

## Functions

### Public

#### `jobs::default_grain` {#function-jobs-default-grain}

<div class="function-declaration" id="function-jobs-default-grain-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a> default_grain(<a href="../types.html#symbol-usize">usize</a> count, <a href="../types.html#symbol-u32">u32</a> workerCount) <span class="k">noexcept</span>;</code></pre></div>

<p>Grain used by the per-index <code>parallel_for</code>: <code>max(1, count / (workerCount * 4))</code>, treating zero workers as one.</p>

#### `jobs::parallel_for_each` {#function-jobs-parallel-for-each}

<div class="function-declaration" id="function-jobs-parallel-for-each-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ElementType, <span class="k">typename</span> Body&gt; <span class="k">void</span> parallel_for_each(<a href="job-system/jobs-job-system.html">job_system</a> &amp;system, std::span&lt;ElementType&gt; items, <a href="../types.html#symbol-usize">usize</a> grain, Body &amp;&amp;body);</code></pre></div>

<p>Runs <code>body(item)</code> for every element of the span, <code>grain</code> elements per job.</p>

<div class="function-declaration" id="function-jobs-parallel-for-each-2"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ElementType, <span class="k">typename</span> Body&gt; <span class="k">void</span> parallel_for_each(<a href="job-system/jobs-job-system.html">job_system</a> &amp;system, std::span&lt;ElementType&gt; items, Body &amp;&amp;body);</code></pre></div>

<p>Runs <code>body(item)</code> for every element of the span with the default grain.</p>

#### `jobs::parallel_for_range` {#function-jobs-parallel-for-range}

<div class="function-declaration" id="function-jobs-parallel-for-range-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Body&gt; <span class="k">void</span> parallel_for_range(<a href="job-system/jobs-job-system.html">job_system</a> &amp;system, <a href="../types.html#symbol-usize">usize</a> count, <a href="../types.html#symbol-usize">usize</a> grain, Body &amp;&amp;body);</code></pre></div>

<p>Runs <code>body(begin, end)</code> over [0, count) in ranges of at most <code>grain</code> elements.</p>

#### `jobs::range_count` {#function-jobs-range-count}

<div class="function-declaration" id="function-jobs-range-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a> range_count(<a href="../types.html#symbol-usize">usize</a> count, <a href="../types.html#symbol-usize">usize</a> grain) <span class="k">noexcept</span>;</code></pre></div>

<p>Number of ranges [0, count) splits into with the given grain (a grain of 0 counts as 1).</p>

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
<a id="default_grain"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="jobsdefault_grain"></a>
<a id="jobsparallel_for_each"></a>
<a id="jobsparallel_for_range"></a>
<a id="jobsrange_count"></a>
<a id="parallel_for_each"></a>
<a id="parallel_for_range"></a>
<a id="range_count"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
