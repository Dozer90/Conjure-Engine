---
kind: struct
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [file_system.h](../file-system.md)

# vfs::file_stat
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::vfs` |
| Include File Path | `engine/include/conjure/core/vfs/file_system.h` |
| Source File Path | `engine/src/core/vfs/file_system.cpp` |
| API ↗ | [Files and the virtual file system](../../../../../api/core/files.md) |

## Syntax

```cpp
struct file_stat
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">isDirectory</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">isRegular</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">size</code></td><td><code class="signature"><a href="../../types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">write<wbr>Time<wbr>Nanoseconds</code></td><td><code class="signature"><a href="../../types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
