---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [job_handle.h](../job-handle.md)

# jobs::job_handle
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::jobs` |
| Include File Path | `engine/include/conjure/core/jobs/job_handle.h` |
| Source File Path | `engine/src/core/jobs/job_handle.cpp` |
| API ↗ | [Jobs and threading](../../../../../api/core/jobs.md) |

## Syntax

```cpp
class CONJURE_API job_handle
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">job_handle() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> job_handle(
    std::shared_ptr&lt;<a href="jobs-detail-job-counter.html">detail::job_counter</a>&gt; counter) <span class="k">noexcept</span>;</code></td><td>Wraps an existing counter. Used by the job system; callers obtain handles from <code>job_system::schedule()</code>.</td></tr>
<tr><td>public</td><td><code class="signature">job_handle(
    <span class="k">const</span> <a href="jobs-job-handle.html">job_handle</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">job_handle(
    <a href="jobs-job-handle.html">job_handle</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~job_handle() = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mCounter</code></td><td><code class="signature">std::<wbr>shared_<wbr>ptr&lt;<wbr><a href="jobs-detail-job-counter.html">detail::<wbr>job_<wbr>counter</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `is_complete` {#function-is-complete}

<div class="function-declaration" id="function-is-complete-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_complete() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when every job behind this handle has finished executing. An invalid handle is always complete.</p>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when this handle refers to scheduled work. False for a default-constructed or reset handle.</p>

#### `pending` {#function-pending}

<div class="function-declaration" id="function-pending-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u64">u64</a> pending() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of jobs behind this handle that have not finished yet. 0 for an invalid handle.</p>

#### `reset` {#function-reset}

<div class="function-declaration" id="function-reset-1"><pre><code><span class="k">void</span> reset() <span class="k">noexcept</span>;</code></pre></div>

<p>Drops the reference and makes the handle invalid. The jobs themselves still run.</p>

#### `valid` {#function-valid}

<div class="function-declaration" id="function-valid-1"><pre><code>[[<span class="na">deprecated</span>(<span class="s">"use is_valid()"</span>)]] [[<span class="na">nodiscard</span>]] <span class="k">bool</span> valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The former name of <code><a href="#function-is-valid-1">is_valid</a>()</code>, kept for one wave; returns <code><a href="#function-is-valid-1">is_valid</a>()</code>.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="jobs-job-handle.html">job_handle</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="jobs-job-handle.html">job_handle</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="jobs-job-handle.html">job_handle</a> &amp;<span class="k">operator</span>=(
    <a href="jobs-job-handle.html">job_handle</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="jobs-job-handle.html">job_handle</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Two handles are equal when they observe the same counter (or are both invalid).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
