---
kind: struct
---

[Editor](../../../../editor.md) → [User Interface](../../../user-interface.md) → [hierarchy_cache.h](../hierarchy-cache.md)

# ui::hierarchy_row
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/hierarchy_cache.h` |
| Source File Path | `engine/src/editor/ui/hierarchy_cache.cpp` |
| API ↗ | [The workspace UI](../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
struct hierarchy_row
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">depth</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u16">u16</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">entity</code></td><td><code class="signature"><a href="../../../../engine/ecs/entity/entity.html">ecs::entity</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flags</code></td><td><code class="signature"><a href="../hierarchy-cache.html#symbol-ui-hierarchy-row-flags">hierarchy_row_flags</a></code></td><td><code class="signature">hierarchy_<wbr>row_<wbr>flags::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">nameId</code></td><td><code class="signature"><a href="../../../../engine/core/string-id/string-id.html">core::string_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">padding</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u8">u8</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
