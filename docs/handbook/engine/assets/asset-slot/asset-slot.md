---
kind: struct
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_slot.h](../asset-slot.md)

# asset_slot
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_slot.h` |
| Source File Path | N/A |
| API ↗ | [Referencing and loading assets](../../../../api/assets/referencing-and-loading.md) |

## Syntax

```cpp
struct asset_slot
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">id</code></td><td><code class="signature"><a href="../asset-types.html#symbol-asset-id">asset_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">index</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">object</code></td><td><code class="signature">std::atomic&lt;<span class="k">void</span> *&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">state</code></td><td><code class="signature">std::atomic&lt;<a href="../asset-types.html#symbol-asset-state">asset_state</a>&gt;</code></td><td><code class="signature">asset_<wbr>state::<wbr>UNLOADED</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">strongRefs</code></td><td><code class="signature">std::atomic&lt;<a href="../../core/types.html#symbol-i32">i32</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">type</code></td><td><code class="signature">std::<wbr>atomic&lt;<wbr><a href="../asset-types.html#symbol-asset-type-id">asset_<wbr>type_<wbr>id</a>&gt;</code></td><td><code class="signature">asset_<wbr>type_<wbr>id::<wbr>INVALID</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">version</code></td><td><code class="signature">std::atomic&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
