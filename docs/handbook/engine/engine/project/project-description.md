---
kind: struct
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [project.h](../project.md)

# project_description
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/project.h` |
| Source File Path | `engine/src/engine/project.cpp` |
| API ↗ | [Engine: the application and the frame](../../../../api/engine/index.md), [Running the engine](../../../../api/engine/running-the-engine.md) |

## Syntax

```cpp
struct project_description
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">assets</code></td><td><code class="signature"><a href="../engine-config/assets-settings.html">assets_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">assetsRoot</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">std::<wbr>filesystem::<wbr>path(<span class="s">"assets"</span>)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">audio</code></td><td><code class="signature"><a href="../engine-config/audio-config.html">audio_config</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">cooked</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">cookedRoot</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">std::<wbr>filesystem::<wbr>path(<span class="s">"cooked"</span>)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">engine<wbr>Version</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">extensions</code></td><td><code class="signature"><a href="../../core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">core::<wbr>json::<wbr>document::<wbr>object()</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">layers</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">modules</code></td><td><code class="signature"><a href="../engine-config/modules-settings.html">modules_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">physics</code></td><td><code class="signature"><a href="../engine-config/physics-config.html">physics_config</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">physics<wbr>Section</code></td><td><code class="signature"><a href="../../core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">core::<wbr>json::<wbr>document::<wbr>object()</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">rendering</code></td><td><code class="signature"><a href="../engine-config/rendering-settings.html">rendering_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">schema<wbr>Version</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature">PROJECT_<wbr>SCHEMA_<wbr>VERSION</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scripting</code></td><td><code class="signature"><a href="../engine-config/scripting-settings.html">scripting_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">startupScene</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">time</code></td><td><code class="signature"><a href="../engine-config/time-settings.html">time_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">version</code></td><td><code class="signature">std::string</code></td><td><code class="signature"><span class="s">"0.1.0"</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">window</code></td><td><code class="signature"><a href="../engine-config/window-settings.html">window_settings</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
