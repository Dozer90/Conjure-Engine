---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [job_system.h](../job-system.md)

# jobs::job_system_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::jobs` |
| Include File Path | `engine/include/conjure/core/jobs/job_system.h` |
| Source File Path | `engine/src/core/jobs/job_system.cpp` |
| API ↗ | [Jobs and threading](../../../../../api/core/jobs.md) |

## Syntax

```cpp
struct job_system_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">max<wbr>Queued<wbr>Jobs</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">16384</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">thread<wbr>Name<wbr>Prefix</code></td><td><code class="signature"><span class="k">const</span> <span class="k">char</span> *</code></td><td><code class="signature"><span class="s">"conjure-worker"</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">workerCount</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
