---
kind: struct
---

[Engine](../../../engine.md) → [Bindings](../../bindings.md) → [native_api_entries.h](../native-api-entries.md)

# binding_context
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::core::jobs` |
| Include File Path | `engine/include/conjure/bindings/native_api_entries.h` |
| Source File Path | N/A |
| API ↗ | [The C# API](../../../../api/bindings/csharp-api.md), [Scripting from C#: the bindings](../../../../api/bindings/index.md), [The native table](../../../../api/bindings/native-table.md) |

## Syntax

```cpp
struct CONJURE_API binding_context
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">asset<wbr>Database</code></td><td><code class="signature"><a href="../../assets/asset-database/asset-database.html">assets::asset_database</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">assets</code></td><td><code class="signature"><a href="../../assets/asset-manager/asset-manager.html">assets::asset_manager</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">audio</code></td><td><code class="signature"><a href="../../audio/audio-engine/audio-engine.html">audio::audio_engine</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">checks</code></td><td><code class="signature"><a href="../../engine/diagnostics/check-registry/diagnostics-check-registry.html">engine::<wbr>diagnostics::<wbr>check_<wbr>registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">components</code></td><td><code class="signature"><a href="../../ecs/component-registry/component-registry.html">ecs::<wbr>component_<wbr>registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">debugDraw</code></td><td><code class="signature"><a href="../../renderer/debug-draw/debug-draw.html">renderer::debug_draw</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">input</code></td><td><code class="signature"><a href="../../platform/input/input.html">platform::input</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">modules</code></td><td><code class="signature"><span class="k">const</span> <a href="../../modules/module-registry/module-registry.html">modules::<wbr>module_<wbr>registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">physics</code></td><td><code class="signature"><a href="../../physics/physics-world/physics-world.html">physics::physics_world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">renderer</code></td><td><code class="signature">renderer::renderer *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scenes</code></td><td><code class="signature"><a href="../../scene/scene-manager/scene-manager.html">scene::scene_manager</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">scripting</code></td><td><code class="signature"><a href="../../scripting/script-runtime/script-runtime.html">scripting::<wbr>script_<wbr>runtime</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">stats</code></td><td><code class="signature"><span class="k">const</span> <a href="../../engine/frame-stats/frame-stats.html">engine::<wbr>frame_<wbr>stats</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">time</code></td><td><code class="signature"><span class="k">const</span> <a href="../../engine/engine-time/engine-time.html">engine::<wbr>engine_<wbr>time</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">window</code></td><td><code class="signature"><a href="../../platform/window/window.html">platform::window</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">world</code></td><td><code class="signature"><a href="../../ecs/world/world.html">ecs::world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `from` {#function-from}

<div class="function-declaration" id="function-from-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="binding-context.html">binding_context</a> from(
    <span class="k">const</span> engine::engine_context &amp;context) <span class="k">noexcept</span>;</code></pre></div>

<p>The view of an <code><a href="../../engine/engine-context/engine-context.html">engine_context</a></code> (every pointer copied; <code>debugDraw</code> stays <code>null</code> because the engine does not publish it, so debug drawing is a silent success there).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
