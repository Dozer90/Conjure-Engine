---
kind: struct
---

[Engine](../../../engine.md) → [Bindings](../../bindings.md) → [native_api_entries.h](../native-api-entries.md)

# stubs::stub_thunk<Id, Ret(Args...)>
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::jobs` |
| Include File Path | `engine/include/conjure/bindings/native_api_entries.h` |
| Source File Path | N/A |
| API ↗ | [The C# API](../../../../api/bindings/csharp-api.md), [Scripting from C#: the bindings](../../../../api/bindings/index.md), [The native table](../../../../api/bindings/native-table.md) |

## Syntax

```cpp
template <native_function_id Id, typename Ret, typename... Args> struct stub_thunk<Id, Ret(Args...)>
```









## Functions

### Public

#### `invoke` {#function-invoke}

<div class="function-declaration" id="function-invoke-1"><pre><code><span class="k">static</span> Ret invoke(
    Args...) <span class="k">noexcept</span>;</code></pre></div>

<p>The failure value of the entry: -<code>NOT_SUPPORTED</code> for <code>int32_t</code>, 0 (<code>NULL_ENTITY</code>) for <code>uint64_t</code>.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
