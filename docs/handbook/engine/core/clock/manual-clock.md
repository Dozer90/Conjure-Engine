---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [clock.h](../clock.md)

# manual_clock
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/clock.h` |
| Source File Path | `engine/src/core/time/clock.cpp` |
| API ↗ | [Time, events and random](../../../../api/core/time-and-events.md) |

## Syntax

```cpp
class CONJURE_API manual_clock final : public clock
```

## Inheritance Hierarchy


[`clock`](clock.md) → `manual_clock`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">manual_clock();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> manual_clock(
    <a href="../types.html#symbol-f64">f64</a> startSeconds);</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~manual_clock() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mNowSeconds</code></td><td><code class="signature"><a href="../types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `advance` {#function-advance}

<div class="function-declaration" id="function-advance-1"><pre><code><span class="k">void</span> advance(
    <a href="../types.html#symbol-f64">f64</a> seconds) <span class="k">noexcept</span>;</code></pre></div>

<p>Moves the <code><a href="clock.html">clock</a></code> forward by the given non-negative amount. Negative and NaN amounts assert and are ignored.</p>

#### `now_seconds` {#function-now-seconds}

<div class="function-declaration" id="function-now-seconds-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-f64">f64</a> now_seconds() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

<p>The current simulated time.</p>

#### `set` {#function-set}

<div class="function-declaration" id="function-set-1"><pre><code><span class="k">void</span> set(
    <a href="../types.html#symbol-f64">f64</a> seconds) <span class="k">noexcept</span>;</code></pre></div>

<p>Sets the <code><a href="clock.html">clock</a></code> to an absolute time; may move it backwards.</p>

#### `sleep_until` {#function-sleep-until}

<div class="function-declaration" id="function-sleep-until-1"><pre><code><span class="k">void</span> sleep_until(
    <a href="../types.html#symbol-f64">f64</a> seconds) <span class="k">override</span>;</code></pre></div>

<p>Jumps the <code><a href="clock.html">clock</a></code> forward to the deadline. <code>A</code> deadline in the past leaves the <code><a href="clock.html">clock</a></code> where it is: a manual <code><a href="clock.html">clock</a></code> never runs backwards through <code>sleep_until</code>.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
