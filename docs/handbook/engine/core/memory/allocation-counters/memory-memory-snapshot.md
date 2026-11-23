---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [allocation_counters.h](../allocation-counters.md)

# memory::memory_snapshot
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::memory` |
| Include File Path | `engine/include/conjure/core/memory/allocation_counters.h` |
| Source File Path | `engine/src/core/memory/allocation_counters.cpp` |
| API ↗ | [Memory](../../../../../api/core/memory.md) |

## Syntax

```cpp
struct memory_snapshot
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">allocated<wbr>Bytes</code></td><td><code class="signature"><a href="../../types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">allocation<wbr>Count</code></td><td><code class="signature"><a href="../../types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">freeCount</code></td><td><code class="signature"><a href="../../types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `live_count` {#function-live-count}

<div class="function-declaration" id="function-live-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../../types.html#symbol-u64">u64</a> live_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Allocations that have not been freed yet (<code>allocationCount</code> - <code>freeCount</code>).</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="memory-memory-snapshot.html">memory_snapshot</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
