---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [job_handle.h](../job-handle.md)

# jobs::job_fence
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::jobs` |
| Include File Path | `engine/include/conjure/core/jobs/job_handle.h` |
| Source File Path | `engine/src/core/jobs/job_handle.cpp` |
| API ↗ | [Jobs and threading](../../../../../api/core/jobs.md) |

## Syntax

```cpp
class CONJURE_API job_fence
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">job_fence() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~job_fence();</code></td><td>Asserts when jobs are still pending: a batch that outlives its fence would signal freed memory.</td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mCounter</code></td><td><code class="signature"><a href="jobs-detail-job-counter.html">detail::job_counter</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `is_complete` {#function-is-complete}

<div class="function-declaration" id="function-is-complete-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_complete() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when every job scheduled on this fence has finished executing (a fresh fence is complete).</p>

#### `pending` {#function-pending}

<div class="function-declaration" id="function-pending-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u64">u64</a> pending() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of jobs scheduled on this fence that have not finished yet.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
