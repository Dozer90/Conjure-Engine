---
kind: struct
---

[Engine](../../../../engine.md) → [Engine](../../../engine.md) → [check_registry.h](../check-registry.md)

# diagnostics::check_record
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::engine::diagnostics` |
| Include File Path | `engine/include/conjure/engine/diagnostics/check_registry.h` |
| Source File Path | `engine/src/engine/check_registry.cpp` |
| API ↗ | [Export and diagnostics](../../../../../api/engine/export-and-diagnostics.md), [Engine: the application and the frame](../../../../../api/engine/index.md) |

## Syntax

```cpp
struct check_record
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">detail</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">frame</code></td><td><code class="signature"><a href="../../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">passed</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="diagnostics-check-record.html">check_record</a> &amp;) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
