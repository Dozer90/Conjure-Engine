---
kind: struct
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [managed_build.h](../managed-build.md)

# build_request
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/managed_build.h` |
| Source File Path | `engine/src/scripting/managed_build.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
struct build_request
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">configuration</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">"Debug"</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">output<wbr>Directory</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">projectPath</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">properties</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>pair&lt;<wbr>std::<wbr>string,<wbr> std::<wbr>string&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">timeout<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">600.0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">working<wbr>Directory</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
