---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# system_phase.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/system_phase.h` |
| API ↗ | [Queries, views and systems](../../../api/ecs/queries-views-and-systems.md) |

<p>The phases of one engine frame, in execution order. The fixed-step phases run zero or more times per frame.</p>

## Structs

| File | Description |
|---|---|
| [`frame_info`](system-phase/frame-info.md) | Per-frame timing filled by the engine before every <code>scheduler::run_phase</code> call. |

## Enumerations

### `system_phase` {#symbol-system-phase}

```cpp
enum class system_phase : u8 {
    FRAME_BEGIN,
    FIXED_UPDATE,
    PHYSICS_STEP,
    FIXED_END,
    UPDATE,
    LATE_UPDATE,
    TRANSFORM,
    SYNC,
    EXTRACT,
    FRAME_END,
    COUNT
};
```

The phases of one engine frame, in execution order. The fixed-step phases run zero or more times per frame.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-SYSTEM_PHASE_COUNT"></a><code>SYSTEM_PHASE_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature">static_<wbr>cast&lt;<wbr><a href="../core/types.html#symbol-u32">u32</a>&gt;(system_<wbr>phase::<wbr>COUNT)</code> | Number of real phases (<code>COUNT</code> excluded). |

## Functions

### Public

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="system-phase.html#symbol-system-phase">system_phase</a> phase) <span class="k">noexcept</span>;</code></pre></div>

<p>The upper-case enumerator name of a phase (&quot;<code>UPDATE</code>&quot;); &quot;<code>COUNT</code>&quot; for <code>COUNT</code> and out-of-range values.</p>

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
<a id="frame_info"></a>
<a id="frame_infodeltatime"></a>
<a id="frame_infofixeddeltatime"></a>
<a id="frame_infoframeindex"></a>
<a id="frame_infoinfixedstep"></a>
<a id="frame_infointerpolationalpha"></a>
<a id="frame_infotime"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="system_phase"></a>
<a id="system_phase_count"></a>
<a id="system_phasecount"></a>
<a id="system_phaseextract"></a>
<a id="system_phasefixed_end"></a>
<a id="system_phasefixed_update"></a>
<a id="system_phaseframe_begin"></a>
<a id="system_phaseframe_end"></a>
<a id="system_phaselate_update"></a>
<a id="system_phasephysics_step"></a>
<a id="system_phasesync"></a>
<a id="system_phasetransform"></a>
<a id="system_phaseupdate"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
