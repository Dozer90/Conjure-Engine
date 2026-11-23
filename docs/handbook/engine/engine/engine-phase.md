---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# engine_phase.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/engine_phase.h` |
| API ↗ | [The frame, layers and time](../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../api/engine/index.md) |

<p>Helpers over the frame phases. The list itself lives in conjure/ecs/<code><a href="../ecs/system-phase.html#symbol-system-phase">system_phase</a>.h</code>; the engine owns its order and the fixed-step grouping.</p>

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-FIXED_PHASE_LIST"></a><code>FIXED_PHASE_LIST</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>array&lt;<wbr>ecs::<wbr>system_<wbr>phase,<wbr> <span class="mi">3</span>&gt;</code> | <code class="signature">{ ecs::<wbr>system_<wbr>phase::<wbr>FIXED_<wbr>UPDATE,<wbr> ecs::<wbr>system_<wbr>phase::<wbr>PHYSICS_<wbr>STEP,<wbr> ecs::<wbr>system_<wbr>phase::<wbr>FIXED_<wbr>END,<wbr> }</code> | The three phases that run once per fixed step, in order (<code>FIXED_UPDATE</code>, <code>PHYSICS_STEP</code>, <code>FIXED_END</code>). |
| <a id="variable-FIXED_PHASES"></a><code>FIXED_PHASES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>span&lt;<wbr><span class="k">const</span> <a href="../ecs/system-phase.html#symbol-system-phase">ecs::<wbr>system_<wbr>phase</a>&gt;</code> | <code class="signature">FIXED_PHASE_LIST</code> | The fixed-step phases as a span. |

## Functions

### Public

#### `is_fixed_phase` {#function-is-fixed-phase}

<div class="function-declaration" id="function-is-fixed-phase-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_fixed_phase(
    <a href="../ecs/system-phase.html#symbol-system-phase">ecs::system_phase</a> phase) <span class="k">noexcept</span>;</code></pre></div>

<p>True for <code>FIXED_UPDATE</code>, <code>PHYSICS_STEP</code> and <code>FIXED_END</code>.</p>

#### `phase_index` {#function-phase-index}

<div class="function-declaration" id="function-phase-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a> phase_index(
    <a href="../ecs/system-phase.html#symbol-system-phase">ecs::system_phase</a> phase) <span class="k">noexcept</span>;</code></pre></div>

<p>Index of a phase in the engine frame (0 = <code>FRAME_BEGIN</code>); <code>SYSTEM_PHASE_COUNT</code> for <code>COUNT</code> and out-of-range values.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="../ecs/system-phase.html#symbol-system-phase">ecs::system_phase</a> phase) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a phase (&quot;<code>UPDATE</code>&quot;); &quot;<code>COUNT</code>&quot; for <code>COUNT</code> and out-of-range values. Identical to <code>ecs::to_string(<a href="../ecs/system-phase.html#symbol-system-phase">system_phase</a>)</code>, re-exported so engine code needs only this header.</p>

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
<a id="fixed_phase_list"></a>
<a id="fixed_phases"></a>
<a id="internals-and-algorithms"></a>
<a id="is_fixed_phase"></a>
<a id="phase_index"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
