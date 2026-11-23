---
kind: struct
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_pack.h](../asset-pack.md)

# pack_toc_entry
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_pack.h` |
| Source File Path | `engine/src/assets/asset_pack.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
struct pack_toc_entry
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">blobOffset</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">blobSize</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">dependency<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">dependency<wbr>Start</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-id">asset_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">nameLength</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">nameOffset</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">payloadHash</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">SIZE</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">72</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">type</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-type-id">asset_type_id</a></code></td><td><code class="signature">asset_<wbr>type_<wbr>id::<wbr>INVALID</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">typeVersion</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
