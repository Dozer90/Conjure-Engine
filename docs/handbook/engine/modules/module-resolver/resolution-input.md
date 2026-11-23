---
kind: struct
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_resolver.h](../module-resolver.md)

# resolution_input
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_resolver.h` |
| Source File Path | `engine/src/modules/module_resolver.cpp` |
| API ↗ | [Loading and managing modules](../../../../api/modules/loading-and-managing.md) |

## Syntax

```cpp
struct resolution_input
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">candidates</code></td><td><code class="signature">std::<wbr>span&lt;<wbr><span class="k">const</span> <a href="../module-types/module-id.html">module_<wbr>id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">engine<wbr>Features</code></td><td><code class="signature"><a href="../module-types.html#symbol-module-feature-flags">module_feature_flags</a></code></td><td><code class="signature">module_<wbr>feature_<wbr>flags::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">engine<wbr>Version</code></td><td><code class="signature"><a href="../module-version/semantic-version.html">semantic_version</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">loaded</code></td><td><code class="signature">std::<wbr>span&lt;<wbr><span class="k">const</span> <a href="../module-types/module-id.html">module_<wbr>id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">registry</code></td><td><code class="signature"><span class="k">const</span> <a href="../module-registry/module-registry.html">module_registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
