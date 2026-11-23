---
kind: struct
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [engine_context.h](../engine-context.md)

# engine_context
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/engine/engine_context.h` |
| Source File Path | N/A |
| API ↗ | [Engine: the application and the frame](../../../../api/engine/index.md), [Running the engine](../../../../api/engine/running-the-engine.md) |

## Syntax

```cpp
struct engine_context
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">app</code></td><td><code class="signature"><a href="../application/application.html">application</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">asset<wbr>Database</code></td><td><code class="signature"><a href="../../assets/asset-database/asset-database.html">conjure::<wbr>assets::<wbr>asset_<wbr>database</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">assets</code></td><td><code class="signature"><a href="../../assets/asset-manager/asset-manager.html">conjure::<wbr>assets::<wbr>asset_<wbr>manager</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">audio</code></td><td><code class="signature"><a href="../../audio/audio-engine/audio-engine.html">conjure::<wbr>audio::<wbr>audio_<wbr>engine</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">checks</code></td><td><code class="signature"><a href="../diagnostics/check-registry/diagnostics-check-registry.html">diagnostics::<wbr>check_<wbr>registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">components</code></td><td><code class="signature"><a href="../../ecs/component-registry/component-registry.html">ecs::<wbr>component_<wbr>registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">config</code></td><td><code class="signature"><span class="k">const</span> <a href="../engine-config/engine-config.html">engine_config</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">device</code></td><td><code class="signature"><a href="../../rhi/device/device.html">rhi::device</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">filesystem</code></td><td><code class="signature"><a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">frameMemory</code></td><td><code class="signature"><a href="../frame-memory/frame-memory.html">frame_memory</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">input</code></td><td><code class="signature"><a href="../../platform/input/input.html">platform::input</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">jobs</code></td><td><code class="signature"><a href="../../core/jobs/job-system/jobs-job-system.html">core::jobs::job_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">modules</code></td><td><code class="signature"><a href="../../modules/module-manager/module-manager.html">conjure::<wbr>modules::<wbr>module_<wbr>manager</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">physics</code></td><td><code class="signature"><a href="../../physics/physics-world/physics-world.html">conjure::<wbr>physics::<wbr>physics_<wbr>world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">project</code></td><td><code class="signature"><span class="k">const</span> <span class="k">class</span> <a href="../project/project.html">project</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">renderer</code></td><td><code class="signature">conjure::<wbr>renderer::<wbr>renderer *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scenes</code></td><td><code class="signature"><a href="../../scene/scene-manager/scene-manager.html">scene::scene_manager</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scheduler</code></td><td><code class="signature"><a href="../../ecs/scheduler/scheduler.html">ecs::scheduler</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scripting</code></td><td><code class="signature"><a href="../../scripting/script-runtime/script-runtime.html">conjure::<wbr>scripting::<wbr>script_<wbr>runtime</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">stats</code></td><td><code class="signature"><span class="k">const</span> <a href="../frame-stats/frame-stats.html">frame_stats</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">time</code></td><td><code class="signature"><span class="k">const</span> <a href="../engine-time/engine-time.html">engine_time</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">window</code></td><td><code class="signature"><a href="../../platform/window/window.html">platform::window</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">world</code></td><td><code class="signature"><a href="../../ecs/world/world.html">ecs::world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="functions"></a>
<!-- doc-legacy-anchors:end -->
