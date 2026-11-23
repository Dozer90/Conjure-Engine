---
kind: struct
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md) → [screenshot_service.h](../screenshot-service.md)

# screenshot_request
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/screenshot_service.h` |
| Source File Path | `engine/src/editor/core/screenshot_service.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
struct screenshot_request
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">camera</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">"editor"</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">format</code></td><td><code class="signature"><a href="../screenshot-service.html#symbol-screenshot-format">screenshot_format</a></code></td><td><code class="signature">screenshot_<wbr>format::<wbr>PNG</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">height</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">720</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">path</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">waitFrames</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">width</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1280</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
