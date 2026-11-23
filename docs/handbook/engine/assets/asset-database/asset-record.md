---
kind: struct
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_database.h](../asset-database.md)

# asset_record
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_database.h` |
| Source File Path | `engine/src/assets/asset_database.cpp` |
| API ↗ | [Database, importers and hot reload](../../../../api/assets/database-and-import.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
struct asset_record
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">displayName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-id">asset_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">importerId</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">importer<wbr>Version</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">mountIndex</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">parentId</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-id">asset_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">payloadHash</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">relativePath</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">settingsHash</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">sourceHash</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">sourceSize</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">source<wbr>Write<wbr>Time</code></td><td><code class="signature"><a href="../../core/types.html#symbol-i64">i64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">status</code></td><td><code class="signature"><a href="../asset-database.html#symbol-asset-record-status">asset_record_status</a></code></td><td><code class="signature">asset_<wbr>record_<wbr>status::<wbr>OK</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">subPath</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">type</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a></code></td><td><code class="signature">asset_<wbr>type_<wbr>id::<wbr>INVALID</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `is_sub_asset` {#function-is-sub-asset}

<div class="function-declaration" id="function-is-sub-asset-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_sub_asset() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for a sub-asset.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
