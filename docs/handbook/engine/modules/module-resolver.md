---
kind: header
---

[Engine](../../engine.md) → [Modules](../modules.md)

# module_resolver.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/modules/module_resolver.h` |
| API ↗ | [Loading and managing modules](../../../api/modules/loading-and-managing.md) |

<p>Dependency resolution: a pure function from registered modules to a deterministic load order plus per-module failures.</p>

## Structs

| File | Description |
|---|---|
| [`resolution_failure`](module-resolver/resolution-failure.md) |  |
| [`resolution_input`](module-resolver/resolution-input.md) |  |
| [`resolution_result`](module-resolver/resolution-result.md) |  |

## Forward Declarations

### `module_registry` {#symbol-module-registry}

```cpp
class module_registry;
```

## Functions

### Public

#### `resolve_load_order` {#function-resolve-load-order}

<div class="function-declaration" id="function-resolve-load-order-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="module-resolver/resolution-result.html">resolution_result</a> resolve_load_order(
    <span class="k">const</span> <a href="module-resolver/resolution-input.html">resolution_input</a> &amp;input);</code></pre></div>

<p>Resolves the load order of the candidates. Pure: reads the registry, changes nothing.</p>

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
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="module_registry"></a>
<a id="resolution_failure"></a>
<a id="resolution_failureerror"></a>
<a id="resolution_failureid"></a>
<a id="resolution_input"></a>
<a id="resolution_inputcandidates"></a>
<a id="resolution_inputenginefeatures"></a>
<a id="resolution_inputengineversion"></a>
<a id="resolution_inputloaded"></a>
<a id="resolution_inputregistry"></a>
<a id="resolution_result"></a>
<a id="resolution_resultfailures"></a>
<a id="resolution_resultorder"></a>
<a id="resolve_load_order"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
