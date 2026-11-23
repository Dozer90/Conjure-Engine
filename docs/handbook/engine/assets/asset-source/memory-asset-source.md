---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_source.h](../asset-source.md)

# memory_asset_source
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_source.h` |
| Source File Path | `engine/src/assets/asset_source.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md), [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
class CONJURE_API memory_asset_source final : public asset_source
```

## Inheritance Hierarchy


[`asset_source`](asset-source.md) → `memory_asset_source`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">memory_asset_source();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~memory_asset_source() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mBlobs</code></td><td><code class="signature"><a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::<wbr>flat_<wbr>hash_<wbr>map</a>&lt;<wbr>asset_<wbr>id,<wbr> <a href="memory-asset-source-entry.html">entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mIdsByPath</code></td><td><code class="signature"><a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::<wbr>flat_<wbr>hash_<wbr>map</a>&lt;<wbr>std::<wbr>string,<wbr> <a href="../asset-types.html#symbol-asset-id">asset_<wbr>id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMutex</code></td><td><code class="signature"><span class="k">mutable</span> std::mutex</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add` {#function-add}

<div class="function-declaration" id="function-add-1"><pre><code><span class="k">void</span> add(
    <a href="../cooked-blob/cooked-blob.html">cooked_blob</a> blob,
    std::string_view assetPath = {});</code></pre></div>

<p>Adds or replaces the blob of its header&#x27;s uuid, optionally under an asset path.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `find_by_path` {#function-find-by-path}

<div class="function-declaration" id="function-find-by-path-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; find_by_path(
    std::string_view assetPath) <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

#### `read` {#function-read}

<div class="function-declaration" id="function-read-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../cooked-blob/cooked-blob.html">cooked_blob</a>&gt; read(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span> <span class="k">override</span>;</code></pre></div>

#### `remove` {#function-remove}

<div class="function-declaration" id="function-remove-1"><pre><code><span class="k">bool</span> remove(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id);</code></pre></div>

<p>Removes a blob. Returns <code>false</code> when there was none.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> size() <span class="k">const</span>;</code></pre></div>

#### `type_of` {#function-type-of}

<div class="function-declaration" id="function-type-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a>&gt; type_of(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
