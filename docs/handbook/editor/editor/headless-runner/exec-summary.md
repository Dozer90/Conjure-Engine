---
kind: struct
---

[Editor](../../../editor.md) → [Applications](../../applications.md) → [headless_runner.h](../headless-runner.md)

# exec_summary
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/headless_runner.h` |
| Source File Path | `engine/src/editor/core/headless_runner.cpp` |
| API ↗ | [The AI control protocol](../../../../api/editor/ai-protocol.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct CONJURE_API exec_summary
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">executed</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">exitCode</code></td><td><code class="signature"><span class="k">int</span></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">failed</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">first<wbr>Failed<wbr>Index</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-i32">i32</a></code></td><td><code class="signature">-<span class="mi">1</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">succeeded</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `record` {#function-record}

<div class="function-declaration" id="function-record-1"><pre><code><span class="k">void</span> record(
    <span class="k">bool</span> ok,
    <a href="../../../engine/core/types.html#symbol-u32">u32</a> index) <span class="k">noexcept</span>;</code></pre></div>

<p>Counts one answered request: executed, then succeeded or failed; the first failure keeps its index in <code>firstFailedIndex</code>.</p>

#### `to_json` {#function-to-json}

<div class="function-declaration" id="function-to-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> to_json() <span class="k">const</span>;</code></pre></div>

<p>{executed, succeeded, failed, <code>firstFailedIndex</code>, <code>exitCode</code>}.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
