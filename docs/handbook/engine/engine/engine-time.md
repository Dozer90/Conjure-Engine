---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# engine_time.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/engine_time.h` |
| API ↗ | [The frame, layers and time](../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../api/engine/index.md) |

<p>Frame time. Written only by the main thread between phases and read from any thread during a phase. Frame 0 uses <code>unscaledDeltaTime</code> = <code>fixedDeltaTime</code> so the first frame is deterministic and runs exactly one fixed step; a time scale of zero yields no fixed steps and a <code>deltaTime</code> of zero while <code>UPDATE</code> still runs.</p>

## Structs

| File | Description |
|---|---|
| [`engine_time`](engine-time/engine-time.md) |  |

## Functions

### Public

#### `advance_fixed_step` {#function-advance-fixed-step}

<div class="function-declaration" id="function-advance-fixed-step-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> advance_fixed_step(
    <a href="engine-time/engine-time.html">engine_time</a> &amp;time) <span class="k">noexcept</span>;</code></pre></div>

<p>Advances <code>fixedTime</code> and <code>fixedStepIndex</code> by one fixed step.</p>

#### `begin_frame` {#function-begin-frame}

<div class="function-declaration" id="function-begin-frame-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> begin_frame(
    <a href="engine-time/engine-time.html">engine_time</a> &amp;time,
    <span class="k">const</span> <a href="engine-config/time-settings.html">time_settings</a> &amp;settings,
    <a href="../core/types.html#symbol-f64">f64</a> rawDeltaSeconds) <span class="k">noexcept</span>;</code></pre></div>

<p>Step 1 of the frame: clamps the raw delta to maxFrameDeltaSeconds, applies the time scale, advances time and <code>unscaledTime</code>, increments <code>frameIndex</code> and resets the per-frame fields (<code>fixedStepsThisFrame</code>, <code>inFixedStep</code>, <code>droppedFixedTime</code>). The first frame (<code>frameIndex</code> 0 on entry) ignores <code>rawDeltaSeconds</code> and uses <code>fixedDeltaTime</code>; negative or non-finite deltas count as zero, and so does a negative or non-finite time scale.</p>

#### `make_frame_info` {#function-make-frame-info}

<div class="function-declaration" id="function-make-frame-info-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../ecs/system-phase/frame-info.html">ecs::frame_info</a> make_frame_info(
    <span class="k">const</span> <a href="engine-time/engine-time.html">engine_time</a> &amp;time) <span class="k">noexcept</span>;</code></pre></div>

<p>The <code><a href="../ecs/system-phase/frame-info.html">ecs::frame_info</a></code> handed to <code>scheduler::run_phase</code>: <code>deltaTime</code> is the fixed delta inside a fixed phase and the frame delta elsewhere.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="advance_fixed_step"></a>
<a id="begin_frame"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="engine_time"></a>
<a id="engine_timedeltatime"></a>
<a id="engine_timedroppedfixedtime"></a>
<a id="engine_timefixeddeltatime"></a>
<a id="engine_timefixedstepindex"></a>
<a id="engine_timefixedstepsthisframe"></a>
<a id="engine_timefixedtime"></a>
<a id="engine_timeframeindex"></a>
<a id="engine_timeinfixedstep"></a>
<a id="engine_timeinterpolationalpha"></a>
<a id="engine_timeoperator"></a>
<a id="engine_timetime"></a>
<a id="engine_timetimescale"></a>
<a id="engine_timeunscaleddeltatime"></a>
<a id="engine_timeunscaledtime"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="make_frame_info"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
