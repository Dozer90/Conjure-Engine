---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# asset_payload.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/asset_payload.h` |
| API ↗ | [Database, importers and hot reload](../../../api/assets/database-and-import.md) |

<p>Type erasure for published payloads. This is the only erasure in the module: asset types need no base class, no <code>shared_ptr</code> and no virtual call on the hot path.</p>

## Type Aliases

### `asset_payload` {#symbol-asset-payload}

```cpp
using asset_payload = std::unique_ptr<void, void (*)(void *)>;
```

An owned, type-erased payload: the object pointer plus the deleter that knows its type.

## Functions

### Public

#### `detail::delete_nothing` {#function-detail-delete-nothing}

<div class="function-declaration" id="function-detail-delete-nothing-1"><pre><code><span class="k">inline</span> <span class="k">void</span> delete_nothing(<span class="k">void</span> *) <span class="k">noexcept</span>;</code></pre></div>

#### `detail::delete_payload` {#function-detail-delete-payload}

<div class="function-declaration" id="function-detail-delete-payload-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ValueType&gt; <span class="k">void</span> delete_payload(<span class="k">void</span> *object) <span class="k">noexcept</span>;</code></pre></div>

#### `make_null_payload` {#function-make-null-payload}

<div class="function-declaration" id="function-make-null-payload-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="asset-payload.html#symbol-asset-payload">asset_payload</a> make_null_payload() <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> payload that owns nothing. Destroying it does nothing.</p>

#### `make_payload` {#function-make-payload}

<div class="function-declaration" id="function-make-payload-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ValueType&gt; [[<span class="na">nodiscard</span>]] <a href="asset-payload.html#symbol-asset-payload">asset_payload</a> make_payload(
    ValueType &amp;&amp;value);</code></pre></div>

<p>Moves (or copies) a value onto the heap and erases its type. The payload destroys the value through its real type.</p>

#### `payload_as` {#function-payload-as}

<div class="function-declaration" id="function-payload-as-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ValueType&gt; [[<span class="na">nodiscard</span>]] ValueType *payload_as(
    <span class="k">const</span> <a href="asset-payload.html#symbol-asset-payload">asset_payload</a> &amp;payload) <span class="k">noexcept</span>;</code></pre></div>

<p>Views the payload as its real type. The caller must know the type (the slot&#x27;s type id).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="asset_payload"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="delete_nothing"></a>
<a id="delete_payload"></a>
<a id="detaildelete_nothing"></a>
<a id="detaildelete_payload"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="make_null_payload"></a>
<a id="make_payload"></a>
<a id="other-declarations"></a>
<a id="payload_as"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
