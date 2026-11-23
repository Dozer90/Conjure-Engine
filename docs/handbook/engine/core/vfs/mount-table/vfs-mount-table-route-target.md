---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [mount_table.h](../mount-table.md)

# vfs::mount_table::route_target
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::vfs` |
| Include File Path | `engine/include/conjure/core/vfs/mount_table.h` |
| Source File Path | `engine/src/core/vfs/mount_table.cpp` |
| API ↗ | [Files and the virtual file system](../../../../../api/core/files.md) |

## Syntax

```cpp
struct route_target
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">backend</code></td><td><code class="signature">std::<wbr>shared_<wbr>ptr&lt;<wbr><a href="../file-system/vfs-file-system.html">file_<wbr>system</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">path</code></td><td><code class="signature">std::string_view</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scheme</code></td><td><code class="signature">std::string_view</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
