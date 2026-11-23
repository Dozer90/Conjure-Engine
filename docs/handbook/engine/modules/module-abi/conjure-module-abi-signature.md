---
kind: struct
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_abi.h](../module-abi.md)

# conjure_module_abi_signature
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_abi.h` |
| Source File Path | `engine/src/modules/module_abi.cpp` |
| API ↗ | [Loading and managing modules](../../../../api/modules/loading-and-managing.md) |

## Syntax

```cpp
struct conjure_module_abi_signature
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">abiVersion</code></td><td><code class="signature">std::uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">architecture</code></td><td><code class="signature">std::uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">buildConfig</code></td><td><code class="signature">std::uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">compilerId</code></td><td><code class="signature">std::uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">compiler<wbr>Major</code></td><td><code class="signature">std::uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">engine<wbr>Version<wbr>Major</code></td><td><code class="signature">std::uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">engine<wbr>Version<wbr>Minor</code></td><td><code class="signature">std::uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">engine<wbr>Version<wbr>Patch</code></td><td><code class="signature">std::uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">featureFlags</code></td><td><code class="signature">std::uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">headerHash</code></td><td><code class="signature">std::uint64_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">magic</code></td><td><code class="signature">std::uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">runtimeFlags</code></td><td><code class="signature">std::uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">standard<wbr>Library<wbr>Id</code></td><td><code class="signature">std::uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">structSize</code></td><td><code class="signature">std::uint32_t</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
