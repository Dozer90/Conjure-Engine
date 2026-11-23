---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [managed_build.h](../managed-build.md)

# build_report
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/managed_build.h` |
| Source File Path | `engine/src/scripting/managed_build.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
struct CONJURE_API build_report
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">diagnostics</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="build-diagnostic.html">build_<wbr>diagnostic</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">elapsed<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">exitCode</code></td><td><code class="signature"><a href="../../core/types.html#symbol-i32">i32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">launch</code></td><td><code class="signature"><a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt;</code></td><td><code class="signature"><a href="../../core/result/result.html">core::<wbr>result</a>&lt;<wbr><span class="k">void</span>&gt;::<wbr>success()</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">output</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">succeeded</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">timedOut</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `error_count` {#function-error-count}

<div class="function-declaration" id="function-error-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> error_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Diagnostics with <code>isError</code> set.</p>

#### `warning_count` {#function-warning-count}

<div class="function-declaration" id="function-warning-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> warning_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
