---
kind: header
---

[Engine](../../engine.md) → [Modules](../modules.md)

# module_registration_set.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/modules/module_registration_set.h` |
| API ↗ | [Writing a module](../../../api/modules/writing-a-module.md) |

<p>Everything one module entry registered. Owned by the manager, read by consumers.</p>

## Structs

| File | Description |
|---|---|
| [`extension_record`](module-registration-set/extension-record.md) | One descriptor inside <code>module_registration_set::extensionStorage</code>. |
| [`module_lifecycle`](module-registration-set/module-lifecycle.md) | Optional module callbacks: <code>onActivated</code> runs after every consumer attached, <code>onDeactivating</code> before any consumer detaches. Both receive the module&#x27;s context. |
| [`module_registration_set`](module-registration-set/module-registration-set.md) | Everything one module entry registered. Owned by the manager, read by consumers. |

## Forward Declarations

### `module_context` {#symbol-module-context}

```cpp
class module_context;
```

## Functions

### Public

#### `extensions_of` {#function-extensions-of}

<div class="function-declaration" id="function-extensions-of-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> DescType&gt; [[<span class="na">nodiscard</span>]] <a href="../core/containers/small-vector/small-vector.html">core::small_vector</a>&lt;<span class="k">const</span> DescType *, <span class="mi">8</span>&gt; extensions_of(
    <span class="k">const</span> <a href="module-registration-set/module-registration-set.html">module_registration_set</a> &amp;set);</code></pre></div>

<p>Every descriptor of type DescType, in registration order. Records under the same point whose size does not match DescType are skipped.</p>

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
<a id="extension_record"></a>
<a id="extension_recordalignment"></a>
<a id="extension_recordoffset"></a>
<a id="extension_recordpoint"></a>
<a id="extension_recordsize"></a>
<a id="extensions_of"></a>
<a id="internals-and-algorithms"></a>
<a id="module_context"></a>
<a id="module_lifecycle"></a>
<a id="module_lifecycleonactivated"></a>
<a id="module_lifecycleondeactivating"></a>
<a id="module_registration_set"></a>
<a id="module_registration_seterrors"></a>
<a id="module_registration_setextensions"></a>
<a id="module_registration_setextensionstorage"></a>
<a id="module_registration_setlifecycle"></a>
<a id="module_registration_setmanagedassemblies"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
