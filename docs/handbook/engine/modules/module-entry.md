---
kind: header
---

[Engine](../../engine.md) → [Modules](../modules.md)

# module_entry.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/modules/module_entry.h` |
| API ↗ | [Writing a module](../../../api/modules/writing-a-module.md) |

<p>Declaring a module.</p>

## Structs

| File | Description |
|---|---|
| [`static_module_desc`](module-entry/static-module-desc.md) | <code>A</code> module compiled into the host: its manifest as JSON text and its entry (or <code>nullptr</code>). The manager keeps a pointer to the descriptor, so it must outlive the manager; the macro below returns a function-local static. |

## Type Aliases

### `module_entry_function` {#symbol-module-entry-function}

```cpp
using module_entry_function = module_entry_status (*)(module_context &context);
```

The entry signature module authors write; the <code>C</code> thunk adapts it to <code><a href="module-abi.html#symbol-conjure-module-entry-fn">conjure_module_entry_fn</a></code>.

## Forward Declarations

### `module_context` {#symbol-module-context}

```cpp
class module_context;
```

## Functions

### Public

#### `detail::entry_thunk` {#function-detail-entry-thunk}

<div class="function-declaration" id="function-detail-entry-thunk-1"><pre><code><span class="k">template</span> &lt;<span class="k">auto</span> EntryFunction&gt; std::uint32_t entry_thunk(<a href="module-context/module-context.html">module_context</a> *context);</code></pre></div>

<p>Adapts a <code>C</code>++ entry function to the <code>C</code> ABI.</p>

#### `detail::static_entry` {#function-detail-static-entry}

<div class="function-declaration" id="function-detail-static-entry-1"><pre><code><span class="k">template</span> &lt;<span class="k">auto</span> EntryFunction&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="module-abi.html#symbol-conjure-module-entry-fn">conjure_module_entry_fn</a> static_entry() <span class="k">noexcept</span>;</code></pre></div>

<p>The <code>C</code> entry for a <code>C</code>++ entry function, or <code>nullptr</code> for a manifest-only module.</p>

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
<a id="detailentry_thunk"></a>
<a id="detailstatic_entry"></a>
<a id="direct-header-dependencies"></a>
<a id="entry_thunk"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="module_context"></a>
<a id="module_entry_function"></a>
<a id="other-declarations"></a>
<a id="static_entry"></a>
<a id="static_module_desc"></a>
<a id="static_module_descentry"></a>
<a id="static_module_descidentifier"></a>
<a id="static_module_descmanifestjson"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
