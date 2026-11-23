---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_manager.h](../asset-manager.md)

# asset_load_batch
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_manager.h` |
| Source File Path | `engine/src/assets/asset_manager.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md), [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md), [Referencing and loading assets](../../../../api/assets/referencing-and-loading.md) |

## Syntax

```cpp
class CONJURE_API asset_load_batch
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">asset_load_batch();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> asset_load_batch(
    std::vector&lt;<a href="../asset-ref/asset-handle.html">asset_handle</a>&gt; handles);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">asset_load_batch(
    <span class="k">const</span> <a href="asset-load-batch.html">asset_load_batch</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">asset_load_batch(
    <a href="asset-load-batch.html">asset_load_batch</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~asset_load_batch();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mHandles</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../asset-ref/asset-handle.html">asset_<wbr>handle</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `handles` {#function-handles}

<div class="function-declaration" id="function-handles-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../asset-ref/asset-handle.html">asset_handle</a>&gt; handles() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_complete` {#function-is-complete}

<div class="function-declaration" id="function-is-complete-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_complete() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when every handle has settled: <code>LOADED</code>, <code>RELOADING</code> (its previous payload stays published) or <code>FAILED</code>; unbound handles count as settled and an empty batch is complete.</p>

#### `loaded_count` {#function-loaded-count}

<div class="function-declaration" id="function-loaded-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> loaded_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of handles with a published payload (<code>LOADED</code> or <code>RELOADING</code>).</p>

#### `progress` {#function-progress}

<div class="function-declaration" id="function-progress-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-f32">f32</a> progress() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Settled handles divided by total, 0..1 (1 for an empty batch).</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="asset-load-batch.html">asset_load_batch</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="asset-load-batch.html">asset_load_batch</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="asset-load-batch.html">asset_load_batch</a> &amp;<span class="k">operator</span>=(
    <a href="asset-load-batch.html">asset_load_batch</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
