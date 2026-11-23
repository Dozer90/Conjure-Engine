---
kind: struct
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_registration_set.h](../module-registration-set.md)

# module_registration_set
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_registration_set.h` |
| Source File Path | N/A |
| API ↗ | [Writing a module](../../../../api/modules/writing-a-module.md) |

## Syntax

```cpp
struct module_registration_set
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">errors</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">extensions</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="extension-record.html">extension_<wbr>record</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">extension<wbr>Storage</code></td><td><code class="signature">std::vector&lt;std::byte&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">lifecycle</code></td><td><code class="signature"><a href="module-lifecycle.html">module_lifecycle</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">managed<wbr>Assemblies</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
