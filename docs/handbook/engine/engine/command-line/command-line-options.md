---
kind: struct
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [command_line.h](../command-line.md)

# command_line_options
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/command_line.h` |
| Source File Path | `engine/src/engine/command_line.cpp` |
| API ↗ | [Engine: the application and the frame](../../../../api/engine/index.md), [Running the engine](../../../../api/engine/running-the-engine.md) |

## Syntax

```cpp
struct command_line_options
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">audio</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="../engine-config.html#symbol-audio-backend-choice">audio_<wbr>backend_<wbr>choice</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">audioOff</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">fixed<wbr>Frame<wbr>Delta</code></td><td><code class="signature">std::optional&lt;<a href="../../core/types.html#symbol-f32">f32</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">fixedHz</code></td><td><code class="signature">std::optional&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">frames</code></td><td><code class="signature">std::optional&lt;<a href="../../core/types.html#symbol-u64">u64</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">fullscreen</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">gpu<wbr>Validation</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="../engine-config.html#symbol-gpu-validation-level">gpu_<wbr>validation_<wbr>level</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">headless</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">height</code></td><td><code class="signature">std::optional&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">help</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">jobs</code></td><td><code class="signature">std::optional&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">logFile</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr>std::<wbr>filesystem::<wbr>path&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">logLevel</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="../../core/log.html#symbol-log-level">core::<wbr>log_<wbr>level</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">noPhysics</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">noRender</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">noScripting</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">noVsync</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">projectPath</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr>std::<wbr>filesystem::<wbr>path&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">rhi</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr><a href="../engine-config.html#symbol-rhi-backend-choice">rhi_<wbr>backend_<wbr>choice</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scene</code></td><td><code class="signature">std::<wbr>optional&lt;<wbr>std::<wbr>filesystem::<wbr>path&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scripting</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">targetHz</code></td><td><code class="signature">std::optional&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">timeScale</code></td><td><code class="signature">std::optional&lt;<a href="../../core/types.html#symbol-f32">f32</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">unrecognised<wbr>Arguments</code></td><td><code class="signature">std::vector&lt;std::string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">width</code></td><td><code class="signature">std::optional&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">std::nullopt</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
