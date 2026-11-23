---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_source.h](../asset-source.md)

# asset_source
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_source.h` |
| Source File Path | `engine/src/assets/asset_source.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md), [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
class CONJURE_API asset_source
```

## Derived Classes


<details><summary>Classes that use this as a base</summary>

<ul>

<li><a href="cache-asset-source.html">cache_asset_source</a></li>

<li><a href="composite-asset-source.html">composite_asset_source</a></li>

<li><a href="memory-asset-source.html">memory_asset_source</a></li>

<li><a href="pack-asset-source.html">pack_asset_source</a></li>

</ul>

</details>

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">asset_source();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~asset_source();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <span class="k">bool</span> contains(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

#### `find_by_path` {#function-find-by-path}

<div class="function-declaration" id="function-find-by-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> std::optional&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; find_by_path(
    std::string_view assetPath) <span class="k">const</span> = <span class="mi">0</span>;</code></pre></div>

#### `read` {#function-read}

<div class="function-declaration" id="function-read-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="../cooked-blob/cooked-blob.html">cooked_blob</a>&gt; read(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span> = <span class="mi">0</span>;</code></pre></div>

<p>Any thread. Returns the validated blob (header, dependencies, payload). <code>NOT_FOUND</code> for an unknown uuid, <code>CACHE_MISS</code> / <code>CACHE_CORRUPT</code> / <code>PACK_CORRUPT</code> as the backend reports.</p>

#### `type_of` {#function-type-of}

<div class="function-declaration" id="function-type-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> std::optional&lt;<a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a>&gt; type_of(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
