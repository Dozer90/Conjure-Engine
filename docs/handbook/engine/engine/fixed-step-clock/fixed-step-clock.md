---
kind: class
---

[Engine](../../../engine.md) → [Engine](../../engine.md) → [fixed_step_clock.h](../fixed-step-clock.md)

# fixed_step_clock
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::engine` |
| Include File Path | `engine/include/conjure/engine/fixed_step_clock.h` |
| Source File Path | `engine/src/engine/fixed_step_clock.cpp` |
| API ↗ | [The frame, layers and time](../../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../../api/engine/index.md) |

## Syntax

```cpp
class CONJURE_API fixed_step_clock
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">fixed_step_clock();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">fixed_step_clock(
    <a href="../../core/types.html#symbol-f32">f32</a> fixedDeltaSeconds,
    <a href="../../core/types.html#symbol-u32">u32</a> maxStepsPerFrame);</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~fixed_step_clock();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">m<wbr>Accumulator<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Fixed<wbr>Delta<wbr>Seconds</code></td><td><code class="signature"><a href="../../core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0</span>f / <span class="mi">60.0</span>f</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Max<wbr>Steps<wbr>Per<wbr>Frame</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">8</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `accumulator` {#function-accumulator}

<div class="function-declaration" id="function-accumulator-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-f64">f64</a> accumulator() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Seconds accumulated and not yet consumed by a step.</p>

#### `advance` {#function-advance}

<div class="function-declaration" id="function-advance-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="fixed-step-result.html">fixed_step_result</a> advance(
    <a href="../../core/types.html#symbol-f32">f32</a> scaledDeltaSeconds);</code></pre></div>

<p>Adds a scaled frame delta and returns how many fixed steps to run this frame plus the alpha to use after them. When the step cap is hit the leftover is discarded so a slow frame cannot snowball (acc += delta; steps = <code>floor(acc / dt)</code>; if steps &gt; max { steps = max; acc = 0; dropped } else { acc -= steps * dt }; alpha = acc / dt). Negative or non-finite deltas add nothing.</p>

#### `fixed_delta` {#function-fixed-delta}

<div class="function-declaration" id="function-fixed-delta-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-f32">f32</a> fixed_delta() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `max_steps_per_frame` {#function-max-steps-per-frame}

<div class="function-declaration" id="function-max-steps-per-frame-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> max_steps_per_frame() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `reset` {#function-reset}

<div class="function-declaration" id="function-reset-1"><pre><code><span class="k">void</span> reset() <span class="k">noexcept</span>;</code></pre></div>

<p>Zeroes the accumulator; the fixed delta and step cap are kept.</p>

#### `set_fixed_delta` {#function-set-fixed-delta}

<div class="function-declaration" id="function-set-fixed-delta-1"><pre><code><span class="k">void</span> set_fixed_delta(
    <a href="../../core/types.html#symbol-f32">f32</a> fixedDeltaSeconds);</code></pre></div>

<p>Changes the fixed delta (must be &gt; 0; asserted and ignored otherwise). The accumulator is kept, so time is conserved across a rate change.</p>

#### `set_max_steps_per_frame` {#function-set-max-steps-per-frame}

<div class="function-declaration" id="function-set-max-steps-per-frame-1"><pre><code><span class="k">void</span> set_max_steps_per_frame(
    <a href="../../core/types.html#symbol-u32">u32</a> maxStepsPerFrame);</code></pre></div>

<p>Changes the step cap (must be &gt; 0; asserted and ignored otherwise).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
