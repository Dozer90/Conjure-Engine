---
kind: struct
---

[Engine](../../../engine.md) → [Core](../../core.md) → [log.h](../log.md)

# log_message
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/log.h` |
| Source File Path | `engine/src/core/foundation/log.cpp` |
| API ↗ | [Errors and results](../../../../api/core/errors-and-results.md), [Logging and assertions](../../../../api/core/logging-and-assertions.md) |

## Syntax

```cpp
struct log_message
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">category</code></td><td><code class="signature">std::string_view</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">file</code></td><td><code class="signature"><span class="k">const</span> <span class="k">char</span> *</code></td><td><code class="signature"><span class="s">""</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">level</code></td><td><code class="signature"><a href="../log.html#symbol-log-level">log_level</a></code></td><td><code class="signature">log_level::INFO</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">line</code></td><td><code class="signature"><span class="k">int</span></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">text</code></td><td><code class="signature">std::string_view</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">threadId</code></td><td><code class="signature">std::thread::id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">timestamp</code></td><td><code class="signature">std::<wbr>chrono::<wbr>system_<wbr>clock::<wbr>time_<wbr>point</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
