---
kind: struct
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [cooked_blob.h](../cooked-blob.md)

# cooked_blob
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/cooked_blob.h` |
| Source File Path | `engine/src/assets/cooked_blob.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md) |

## Syntax

```cpp
struct cooked_blob
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">dependencies</code></td><td><code class="signature">std::vector&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">header</code></td><td><code class="signature"><a href="cooked-blob-header.html">cooked_blob_header</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">payload</code></td><td><code class="signature">std::vector&lt;std::byte&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
