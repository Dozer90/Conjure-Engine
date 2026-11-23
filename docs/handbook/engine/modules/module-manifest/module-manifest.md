---
kind: struct
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_manifest.h](../module-manifest.md)

# module_manifest
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_manifest.h` |
| Source File Path | `engine/src/modules/module_manifest.cpp` |
| API ↗ | [Writing a module](../../../../api/modules/writing-a-module.md) |

## Syntax

```cpp
struct module_manifest
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">binary</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">content<wbr>Directories</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">dependencies</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="module-dependency.html">module_<wbr>dependency</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">description</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">displayName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">editorOnly</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">engine<wbr>Version</code></td><td><code class="signature"><a href="../module-version/version-range.html">version_range</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">kind</code></td><td><code class="signature"><a href="../module-types.html#symbol-module-kind">module_kind</a></code></td><td><code class="signature">module_<wbr>kind::<wbr>PLUGIN</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">managed<wbr>Assemblies</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">platforms</code></td><td><code class="signature"><a href="../module-types.html#symbol-module-platform-flags">module_platform_flags</a></code></td><td><code class="signature">module_<wbr>platform_<wbr>flags::<wbr>ALL</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">required<wbr>Features</code></td><td><code class="signature"><a href="../module-types.html#symbol-module-feature-flags">module_feature_flags</a></code></td><td><code class="signature">module_<wbr>feature_<wbr>flags::<wbr>NONE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">schema<wbr>Version</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature">MODULE_<wbr>MANIFEST_<wbr>SCHEMA_<wbr>VERSION</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">version</code></td><td><code class="signature"><a href="../module-version/semantic-version.html">semantic_version</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>



## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="module-manifest.html">module_manifest</a> &amp;) <span class="k">const</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
