---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [random.h](../random.md)

# random::random_engine
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core::random` |
| Include File Path | `engine/include/conjure/core/random.h` |
| Source File Path | `engine/src/core/time/random.cpp` |
| API ↗ | [Time, events and random](../../../../api/core/time-and-events.md) |

## Syntax

```cpp
class CONJURE_API random_engine
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">random_engine();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> random_engine(
    <a href="../types.html#symbol-u64">u64</a> seedValue);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">random_engine(
    <span class="k">const</span> <a href="random-random-engine.html">random_engine</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">random_engine(
    <a href="random-random-engine.html">random_engine</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~random_engine();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mEngine</code></td><td><code class="signature"><a href="../random.html#symbol-random-engine">engine</a></code></td><td><code class="signature"><a href="../random.html#symbol-random-engine">engine</a>(DEFAULT_<wbr>SEED)</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSeed</code></td><td><code class="signature"><a href="../types.html#symbol-u64">u64</a></code></td><td><code class="signature">DEFAULT_SEED</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `chance` {#function-chance}

<div class="function-declaration" id="function-chance-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> chance(
    <a href="../types.html#symbol-f64">f64</a> probability);</code></pre></div>

<p>True with the given probability.</p>

#### `current_seed` {#function-current-seed}

<div class="function-declaration" id="function-current-seed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-u64">u64</a> current_seed() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The seed the current sequence started from.</p>

#### `discard` {#function-discard}

<div class="function-declaration" id="function-discard-1"><pre><code><span class="k">void</span> discard(
    <a href="../types.html#symbol-u64">u64</a> count) <span class="k">noexcept</span>;</code></pre></div>

<p>Advances the sequence by the given number of draws without producing values.</p>

#### `max` {#function-max}

<div class="function-declaration" id="function-max-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="../random.html#symbol-random-random-engine-result-type">result_type</a> max() <span class="k">noexcept</span>;</code></pre></div>

<p>Largest value <code>operator()</code> can return, as required by <code>std::uniform_random_bit_generator</code>.</p>

#### `min` {#function-min}

<div class="function-declaration" id="function-min-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="../random.html#symbol-random-random-engine-result-type">result_type</a> min() <span class="k">noexcept</span>;</code></pre></div>

<p>Smallest value <code>operator()</code> can return, as required by <code>std::uniform_random_bit_generator</code>.</p>

#### `raw` {#function-raw}

<div class="function-declaration" id="function-raw-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../random.html#symbol-random-engine">engine</a> &amp;raw() <span class="k">noexcept</span>;</code></pre></div>

<p>The wrapped engine, for callers that need the standard interface directly.</p>

#### `seed` {#function-seed}

<div class="function-declaration" id="function-seed-1"><pre><code><span class="k">void</span> seed(
    <a href="../types.html#symbol-u64">u64</a> seedValue) <span class="k">noexcept</span>;</code></pre></div>

<p>Restarts the sequence from the given seed.</p>

#### `uniform` {#function-uniform}

<div class="function-declaration" id="function-uniform-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T&gt; [[<span class="na">nodiscard</span>]] T uniform(
    T minimum,
    T maximum);</code></pre></div>

<p>Uniform integer in [min, max] or floating point value in [min, max).</p>

#### `unit_vector3` {#function-unit-vector3}

<div class="function-declaration" id="function-unit-vector3-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Vector3 = std::array&lt;f32, <span class="mi">3</span>&gt;&gt; [[<span class="na">nodiscard</span>]] Vector3 unit_vector3();</code></pre></div>

<p><code>A</code> uniformly distributed point on the unit sphere. Two engine draws; x and y may differ in the last bit between platforms (see the free <code>random::unit_vector3</code>).</p>

## Operator Overloads

### Public

#### `operator()` {#operator-28--29}

<div class="function-declaration" id="operator-28--29-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../random.html#symbol-random-random-engine-result-type">result_type</a> <span class="k">operator</span>()() <span class="k">noexcept</span>;</code></pre></div>

<p>The next raw 64-bit output.</p>

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="random-random-engine.html">random_engine</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="random-random-engine.html">random_engine</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="random-random-engine.html">random_engine</a> &amp;<span class="k">operator</span>=(
    <a href="random-random-engine.html">random_engine</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="random-random-engine.html">random_engine</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when both engines are in the same state and will produce the same sequence.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
