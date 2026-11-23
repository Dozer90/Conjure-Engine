---
kind: struct
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_types.h](../module-types.md)

# binary_triplet
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_types.h` |
| Source File Path | `engine/src/modules/module_types.cpp` |
| API ↗ | [Writing a module](../../../../api/modules/writing-a-module.md) |

## Syntax

```cpp
struct binary_triplet
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">architecture</code></td><td><code class="signature"><a href="../module-types.html#symbol-module-architecture">module_architecture</a></code></td><td><code class="signature">module_<wbr>architecture::<wbr>X64</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">config</code></td><td><code class="signature"><a href="../module-types.html#symbol-build-config">build_config</a></code></td><td><code class="signature">build_<wbr>config::<wbr>DEVELOPMENT</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">platform</code></td><td><code class="signature"><a href="../module-types.html#symbol-module-platform">module_platform</a></code></td><td><code class="signature">module_<wbr>platform::<wbr>WINDOWS</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator<=>` {#operator-3c--3d--3e}

<div class="function-declaration" id="operator-3c--3d--3e-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">auto</span> <span class="k">operator</span>&lt;=&gt;(
    <span class="k">const</span> <a href="binary-triplet.html">binary_triplet</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
