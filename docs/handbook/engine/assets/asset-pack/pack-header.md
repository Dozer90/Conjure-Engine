---
kind: struct
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_pack.h](../asset-pack.md)

# pack_header
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_pack.h` |
| Source File Path | `engine/src/assets/asset_pack.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
struct pack_header
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">blobsOffset</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">contentHash</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">dependency<wbr>Table<wbr>Offset</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">entryCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">format<wbr>Version</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature">PACK_<wbr>FORMAT_<wbr>VERSION</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAGIC</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> std::<wbr>array&lt;<wbr><span class="k">char</span>,<wbr> <span class="mi">4</span>&gt;</code></td><td><code class="signature">{<span class="s">'C'</span>,<wbr> <span class="s">'P'</span>,<wbr> <span class="s">'A'</span>,<wbr> <span class="s">'K'</span>}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">PAYLOAD_<wbr>ALIGNMENT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">16</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">SIZE</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">64</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">string<wbr>Table<wbr>Offset</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">tocOffset</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
