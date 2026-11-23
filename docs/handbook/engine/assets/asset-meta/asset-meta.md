---
kind: struct
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_meta.h](../asset-meta.md)

# asset_meta
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_meta.h` |
| Source File Path | `engine/src/assets/asset_meta.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md) |

## Syntax

```cpp
struct CONJURE_API asset_meta
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-id">asset_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">importerId</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">importer<wbr>Version</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">settings</code></td><td><code class="signature"><a href="../../core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">core::<wbr>json::<wbr>document::<wbr>object()</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">subAssets</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="sub-asset-entry.html">sub_<wbr>asset_<wbr>entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">unknown</code></td><td><code class="signature"><a href="../../core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">core::<wbr>json::<wbr>document::<wbr>object()</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `find_sub_asset` {#function-find-sub-asset}

<div class="function-declaration" id="function-find-sub-asset-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="sub-asset-entry.html">sub_asset_entry</a> *find_sub_asset(
    std::string_view subPath) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The uuid recorded for a sub path, or nullopt.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
