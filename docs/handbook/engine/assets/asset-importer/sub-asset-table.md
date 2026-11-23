---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_importer.h](../asset-importer.md)

# sub_asset_table
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_importer.h` |
| Source File Path | `engine/src/assets/asset_importer.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
class CONJURE_API sub_asset_table
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">sub_asset_table();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> sub_asset_table(
    std::vector&lt;<a href="../asset-meta/sub-asset-entry.html">sub_asset_entry</a>&gt; existing);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~sub_asset_table();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mChanged</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEntries</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../asset-meta/sub-asset-entry.html">sub_<wbr>asset_<wbr>entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMutex</code></td><td><code class="signature"><span class="k">mutable</span> std::mutex</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `changed` {#function-changed}

<div class="function-declaration" id="function-changed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> changed() <span class="k">const</span>;</code></pre></div>

<p>True when <code>id_for</code> allocated at least one new uuid.</p>

#### `entries` {#function-entries}

<div class="function-declaration" id="function-entries-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="../asset-meta/sub-asset-entry.html">sub_asset_entry</a>&gt; entries() <span class="k">const</span>;</code></pre></div>

<p>Every entry, sorted by sub path.</p>

#### `id_for` {#function-id-for}

<div class="function-declaration" id="function-id-for-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../asset-types.html#symbol-asset-id">asset_id</a> id_for(
    std::string_view subPath);</code></pre></div>

<p>The uuid for a sub path, allocating a new one on first use.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
