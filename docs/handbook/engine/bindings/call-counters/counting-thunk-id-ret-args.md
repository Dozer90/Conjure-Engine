---
kind: struct
---

[Engine](../../../engine.md) → [Bindings](../../bindings.md) → [call_counters.h](../call-counters.md)

# counting_thunk<Id, Ret(Args...)>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::bindings` |
| Include File Path | `engine/include/conjure/bindings/call_counters.h` |
| Source File Path | `engine/src/bindings/call_counters.cpp` |
| API ↗ | [The C# API](../../../../api/bindings/csharp-api.md), [Scripting from C#: the bindings](../../../../api/bindings/index.md), [The native table](../../../../api/bindings/native-table.md) |

## Syntax

```cpp
template <native_function_id Id, typename Ret, typename... Args> struct counting_thunk<Id, Ret(Args...)>
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">sTarget</code></td><td><code class="signature"><span class="k">static</span> <span class="k">inline</span> <a href="../call-counters.html#symbol-counting-thunk-id-ret-args-function-pointer">function_<wbr>pointer</a></code></td><td><code class="signature">nullptr</code></td><td>The slot&#x27;s original pointer (process-wide: one native table per process, like <code><a href="../../scripting/script-runtime/script-runtime.html">script_runtime</a></code>).</td></tr>
</tbody>
</table>

## Functions

### Public

#### `invoke` {#function-invoke}

<div class="function-declaration" id="function-invoke-1"><pre><code><span class="k">static</span> Ret invoke(
    Args... args);</code></pre></div>

<p>Increments the counter and forwards every argument by value.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
