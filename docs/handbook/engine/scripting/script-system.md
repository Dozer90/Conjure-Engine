---
kind: header
---

[Engine](../../engine.md) → [Scripting](../scripting.md)

# script_system.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scripting/script_system.h` |
| API ↗ | [Scripting](../../../api/scripting/index.md), [Scripts, components and the runtime](../../../api/scripting/scripts-and-components.md) |

<p>The six <code>EXCLUSIVE</code> scripting systems. All managed execution happens inside them on the main thread, so scripts may mutate the world directly except inside a DOD query scope.</p>

## Forward Declarations

### `hot_reload_controller` {#symbol-hot-reload-controller}

```cpp
class hot_reload_controller;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-SYSTEM_COLLISION_DISPATCH"></a><code>SYSTEM_COLLISION_DISPATCH</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"scripting.collision_<wbr>dispatch"</span></code> |  |
| <a id="variable-SYSTEM_FIXED_UPDATE"></a><code>SYSTEM_FIXED_UPDATE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"scripting.fixed_update"</span></code> |  |
| <a id="variable-SYSTEM_FRAME_BEGIN"></a><code>SYSTEM_FRAME_BEGIN</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"scripting.frame_begin"</span></code> |  |
| <a id="variable-SYSTEM_FRAME_END"></a><code>SYSTEM_FRAME_END</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"scripting.frame_end"</span></code> |  |
| <a id="variable-SYSTEM_LATE_UPDATE"></a><code>SYSTEM_LATE_UPDATE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"scripting.late_update"</span></code> |  |
| <a id="variable-SYSTEM_UPDATE"></a><code>SYSTEM_UPDATE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"scripting.update"</span></code> |  |

## Functions

### Public

#### `register_script_systems` {#function-register-script-systems}

<div class="function-declaration" id="function-register-script-systems-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; register_script_systems(
    <a href="../ecs/scheduler/scheduler.html">ecs::scheduler</a> &amp;scheduler,
    <a href="script-runtime/script-runtime.html">script_runtime</a> &amp;runtime,
    <a href="hot-reload/hot-reload-controller.html">hot_reload_controller</a> *reload = nullptr);</code></pre></div>

<p>Registers <code>scripting.frame_begin</code> (<code>FRAME_BEGIN</code>: reload tick, flush), <code>scripting.collision_dispatch</code> and <code>scripting.fixed_update</code> (<code>FIXED_UPDATE</code>, dispatch before update), <code>scripting.update</code> (<code>UPDATE</code>), <code>scripting.late_update</code> (<code>LATE_UPDATE</code>) and <code>scripting.frame_end</code> (<code>FRAME_END</code>: flush). <code>reload</code> may be <code>null</code>; when set, <code>frame_begin</code> ticks it with the frame time. The runtime and controller must outlive the scheduler. Call once per scheduler: a second call returns <code>NAME_CONFLICT</code> from its first system and adds nothing.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="hot_reload_controller"></a>
<a id="internals-and-algorithms"></a>
<a id="register_script_systems"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="system_collision_dispatch"></a>
<a id="system_fixed_update"></a>
<a id="system_frame_begin"></a>
<a id="system_frame_end"></a>
<a id="system_late_update"></a>
<a id="system_update"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
