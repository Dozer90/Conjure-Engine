---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [clock.h](../clock.md)

# steady_clock
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/clock.h` |
| Source File Path | `engine/src/core/time/clock.cpp` |
| API ↗ | [Time, events and random](../../../../api/core/time-and-events.md) |

## Syntax

```cpp
class CONJURE_API steady_clock final : public clock
```

## Inheritance Hierarchy


[`clock`](clock.md) → `steady_clock`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">steady_clock();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~steady_clock() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">SPIN_<wbr>WINDOW_<wbr>SECONDS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.002</span></code></td><td>How close to the deadline the thread stops sleeping and starts spinning.</td></tr>
</tbody>
</table>

## Functions

### Public

#### `now_seconds` {#function-now-seconds}

<div class="function-declaration" id="function-now-seconds-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-f64">f64</a> now_seconds() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

<p>Seconds since the process-wide steady epoch.</p>

#### `sleep_until` {#function-sleep-until}

<div class="function-declaration" id="function-sleep-until-1"><pre><code><span class="k">void</span> sleep_until(
    <a href="../types.html#symbol-f64">f64</a> seconds) <span class="k">override</span>;</code></pre></div>

<p>Sleeps the calling thread until 2 ms before the deadline, then spins on the <code><a href="clock.html">clock</a></code> so the wake-up lands within the scheduler jitter of the deadline. Deadlines may be arbitrarily far away (even infinite): the sleep is taken in bounded steps.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
