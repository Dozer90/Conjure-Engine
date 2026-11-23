---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# result.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/result.h` |
| API ↗ | [Errors and results](../../../api/core/errors-and-results.md) |

<p>Either a value or an error. The engine&#x27;s replacement for exceptions. Check with <code>has_value()</code> (or operator bool) before calling <code>value()</code>.</p>



## Classes

| File | Description |
|---|---|
| [`result`](result/result.md) | Either a value or an error. The engine&#x27;s replacement for exceptions. Check with <code>has_value()</code> (or operator bool) before calling <code>value()</code>. |
| [`result<void>`](result/result-void.md) | <code>A</code> <code><a href="result/result.html">result</a></code> that carries no value: success, or an error. |

## Type Aliases

### `status` {#symbol-status}

```cpp
using status = result<void>;
```

Convenience alias for operations that only report success or failure.

## Functions

### Public

#### `detail::wrong_result_alternative` {#function-detail-wrong-result-alternative}

<div class="function-declaration" id="function-detail-wrong-result-alternative-1"><pre><code>[[<span class="na">noreturn</span>]] <span class="k">inline</span> <span class="k">void</span> wrong_result_alternative(<span class="k">const</span> <span class="k">char</span> *what);</code></pre></div>

<p>Accessing the wrong alternative of a <code><a href="result/result.html">result</a></code> is a programmer error. It asserts, and because continuing would dereference nothing, it terminates even when the assertion handler asks to continue.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailwrong_result_alternative"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="result"></a>
<a id="result_1"></a>
<a id="resultget_error"></a>
<a id="resultget_error_1"></a>
<a id="resulthas_value"></a>
<a id="resulthas_value_1"></a>
<a id="resultmerror"></a>
<a id="resultmstorage"></a>
<a id="resultoperator"></a>
<a id="resultoperator-"></a>
<a id="resultoperator-bool"></a>
<a id="resultoperator-bool_1"></a>
<a id="resultoperator_1"></a>
<a id="resultoperator_2"></a>
<a id="resultresult"></a>
<a id="resultresult_1"></a>
<a id="resultresult_2"></a>
<a id="resultresult_3"></a>
<a id="resultsuccess"></a>
<a id="resultvalue"></a>
<a id="resultvalue_or"></a>
<a id="resultvoid"></a>
<a id="resultvoidget_error"></a>
<a id="resultvoidhas_value"></a>
<a id="resultvoidmerror"></a>
<a id="resultvoidoperator"></a>
<a id="resultvoidoperator-bool"></a>
<a id="resultvoidresult"></a>
<a id="resultvoidresult_1"></a>
<a id="resultvoidsuccess"></a>
<a id="status"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="wrong_result_alternative"></a>
<!-- doc-legacy-anchors:end -->
