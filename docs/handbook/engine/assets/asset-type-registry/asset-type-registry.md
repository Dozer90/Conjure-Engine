---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_type_registry.h](../asset-type-registry.md)

# asset_type_registry
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_type_registry.h` |
| Source File Path | `engine/src/assets/asset_type_registry.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
class CONJURE_API asset_type_registry
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">asset_type_registry();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~asset_type_registry();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mHooks</code></td><td><code class="signature"><a href="asset-type-registry-hooks.html">asset_<wbr>type_<wbr>registry_<wbr>hooks</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mIndexById</code></td><td><code class="signature"><a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::<wbr>flat_<wbr>hash_<wbr>map</a>&lt;<wbr>asset_<wbr>type_<wbr>id,<wbr> <a href="../../core/types.html#symbol-usize">usize</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTypes</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="asset-type-desc.html">asset_<wbr>type_<wbr>desc</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mView</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><span class="k">const</span> <a href="asset-type-desc.html">asset_<wbr>type_<wbr>desc</a> *&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="asset-type-desc.html">asset_type_desc</a> *find(
    <a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-find-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="asset-type-desc.html">asset_type_desc</a> *find(
    std::string_view name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `find_loader` {#function-find-loader}

<div class="function-declaration" id="function-find-loader-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../asset-loader/asset-loader.html">asset_loader</a> *find_loader(
    <a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `register_type` {#function-register-type}

<div class="function-declaration" id="function-register-type-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_type(
    <a href="asset-type-desc.html">asset_type_desc</a> desc);</code></pre></div>

<p>Main thread, before systems run. Fails with <code>TYPE_DUPLICATE</code> if the id is taken, with <code>INVALID_ARGUMENT</code> for an <code>INVALID</code> id, a name whose hash is not the id, or a missing loader whose type differs from the id, and with <code>OUT_OF_RANGE</code> past <code>MAX_ASSET_TYPES</code>.</p>

#### `set_hooks` {#function-set-hooks}

<div class="function-declaration" id="function-set-hooks-1"><pre><code><span class="k">void</span> set_hooks(
    <a href="asset-type-registry-hooks.html">asset_type_registry_hooks</a> hooks);</code></pre></div>

<p>Installs the manager&#x27;s callbacks. Engine plumbing, not for general use.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `types` {#function-types}

<div class="function-declaration" id="function-types-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="asset-type-desc.html">asset_type_desc</a> *<span class="k">const</span>&gt; types() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Every registered type in registration order. The span is valid until the next registration or removal; the descriptions it points at follow the <code><a href="#function-find-1">find</a>()</code> rule.</p>

#### `unregister_type` {#function-unregister-type}

<div class="function-declaration" id="function-unregister-type-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; unregister_type(
    <a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a> id);</code></pre></div>

<p>Plugin unload. <code>TYPE_UNKNOWN</code> for an unregistered id; <code>INVALID_STATE</code> while any slot of the type is loaded or in flight. Only pointers to the removed description die.</p>

### Private

#### `rebuild_index` {#function-rebuild-index}

<div class="function-declaration" id="function-rebuild-index-1"><pre><code><span class="k">void</span> rebuild_index();</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
