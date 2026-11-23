---
kind: struct
---

[Engine](../../../engine.md) → [Physics](../../physics.md) → [physics_settings.h](../physics-settings.md)

# physics_settings
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::physics` |
| Include File Path | `engine/include/conjure/physics/physics_settings.h` |
| Source File Path | `engine/src/physics/physics_settings.cpp` |
| API ↗ | [Physics](../../../../api/physics/index.md), [The physics world and queries](../../../../api/physics/physics-world-and-queries.md) |

## Syntax

```cpp
struct CONJURE_API physics_settings
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">allow<wbr>Sleeping</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">collision<wbr>Steps</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">deterministic</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">estimate<wbr>Contact<wbr>Impulses</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">true</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">gravity</code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../core/math/glm-types.html#symbol-vec3">math::<wbr>vec3</a>(<span class="mi">0.0</span>f,<wbr> -<span class="mi">9.81</span>f,<wbr> <span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">layers</code></td><td><code class="signature"><a href="../physics-layers/physics-layer-table.html">physics_layer_table</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">maxBarriers</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">8</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">maxBodies</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">65536</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">maxBodyPairs</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">65536</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">max<wbr>Characters</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">256</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">max<wbr>Contact<wbr>Constraints</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">16384</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">max<wbr>Contact<wbr>Events<wbr>Per<wbr>Step</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">16384</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">maxJobs</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">2048</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">max<wbr>Queued<wbr>Commands</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">8192</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">num<wbr>Body<wbr>Mutexes</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">num<wbr>Position<wbr>Steps</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">2</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">num<wbr>Velocity<wbr>Steps</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">10</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">penetration<wbr>Slop</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.005</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">point<wbr>Velocity<wbr>Sleep<wbr>Threshold</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.03</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">speculative<wbr>Contact<wbr>Distance</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.02</span>f</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">temp<wbr>Allocator<wbr>Bytes</code></td><td><code class="signature"><a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">32</span> * MIB</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">time<wbr>Before<wbr>Sleep<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.5</span>f</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `from_json` {#function-from-json}

<div class="function-declaration" id="function-from-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="physics-settings.html">physics_settings</a>&gt; from_json(
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;physicsSection,
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;layersSection);</code></pre></div>

<p>Reads the <code>project.json</code> &quot;physics&quot; object (<code>snake_case</code> keys matching the fields above; &quot;enabled&quot; is the engine&#x27;s and is ignored; unknown keys log one <code>WARN</code> each) and the &quot;<code>extensions.physics_layers</code>&quot; object (an invalid or missing value keeps the default table). Fails with <code>INVALID_SETTINGS</code> for a value of the wrong type and with the layer errors of <code>physics_layer_table::from_json</code>. The result is not validated; call <code><a href="#function-validate-1">validate</a>()</code>.</p>

#### `validate` {#function-validate}

<div class="function-declaration" id="function-validate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate() <span class="k">const</span>;</code></pre></div>

<p>Rejects <code>maxBodies</code> == 0 or above 1 &lt;&lt; 23, a <code>maxBodyPairs</code> or <code>maxContactConstraints</code> below 4 or above 1 &lt;&lt; 24, <code>collisionSteps</code> == 0, <code>tempAllocatorBytes</code> below 1 MiB, a <code>maxJobs</code> below 64 or not a power of two, <code>maxBarriers</code> == 0, <code>maxContactEventsPerStep</code> == 0, <code>maxQueuedCommands</code> == 0, <code>maxCharacters</code> == 0, <code>numVelocitySteps</code> == 0, <code>numPositionSteps</code> == 0, a negative <code>timeBeforeSleepSeconds</code>, <code>pointVelocitySleepThreshold</code>, <code>speculativeContactDistance</code> or <code>penetrationSlop</code>, a non-finite gravity, and whatever <code>layers.<a href="#function-validate-1">validate</a>()</code> rejects. The <code>INVALID_SETTINGS</code> message names the field.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
