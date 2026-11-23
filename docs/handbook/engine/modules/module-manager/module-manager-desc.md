---
kind: struct
---

[Engine](../../../engine.md) → [Modules](../../modules.md) → [module_manager.h](../module-manager.md)

# module_manager_desc
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::modules` |
| Include File Path | `engine/include/conjure/modules/module_manager.h` |
| Source File Path | `engine/src/modules/module_manager.cpp` |
| API ↗ | [Loading and managing modules](../../../../api/modules/loading-and-managing.md) |

## Syntax

```cpp
struct module_manager_desc
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">allow<wbr>Shared<wbr>Libraries</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">binaryConfig</code></td><td><code class="signature"><a href="../module-types.html#symbol-build-config">build_config</a></code></td><td><code class="signature">host_<wbr>build_<wbr>config()</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">default<wbr>Unload<wbr>Policy</code></td><td><code class="signature"><a href="../module-types.html#symbol-unload-policy">unload_policy</a></code></td><td><code class="signature">unload_<wbr>policy::<wbr>REFUSE_<wbr>IF_<wbr>IN_<wbr>USE</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">engine<wbr>Features</code></td><td><code class="signature"><a href="../module-types.html#symbol-module-feature-flags">module_feature_flags</a></code></td><td><code class="signature">host_<wbr>feature_<wbr>flags()</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">engine<wbr>Version</code></td><td><code class="signature"><a href="../module-version/semantic-version.html">semantic_version</a></code></td><td><code class="signature">host_<wbr>engine_<wbr>version()</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">host</code></td><td><code class="signature"><a href="../module-types.html#symbol-module-host-kind">module_host_kind</a></code></td><td><code class="signature">module_<wbr>host_<wbr>kind::<wbr>RUNTIME</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">is<wbr>Inside<wbr>Frame</code></td><td><code class="signature"><span class="k">bool</span> (*)()</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">keep<wbr>Binaries<wbr>Resident</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">shadow<wbr>Copy<wbr>Binaries</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">shadow<wbr>Copy<wbr>Directory</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
