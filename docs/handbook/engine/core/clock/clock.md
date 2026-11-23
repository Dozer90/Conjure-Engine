---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [clock.h](../clock.md)

# clock
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/clock.h` |
| Source File Path | `engine/src/core/time/clock.cpp` |
| API ↗ | [Time, events and random](../../../../api/core/time-and-events.md) |

## Syntax

```cpp
class CONJURE_API clock
```

## Derived Classes


<details><summary>Classes that use this as a base</summary>

<ul>

<li><a href="manual-clock.html">manual_clock</a></li>

<li><a href="steady-clock.html">steady_clock</a></li>

</ul>

</details>

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">clock();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">virtual</span> ~clock();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `now_seconds` {#function-now-seconds}

<div class="function-declaration" id="function-now-seconds-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">virtual</span> <a href="../types.html#symbol-f64">f64</a> now_seconds() <span class="k">const</span> <span class="k">noexcept</span> = <span class="mi">0</span>;</code></pre></div>

<p>Current time in seconds. Never decreases between two calls on the same <code><a href="clock.html">clock</a></code>.</p>

#### `sleep_until` {#function-sleep-until}

<div class="function-declaration" id="function-sleep-until-1"><pre><code><span class="k">virtual</span> <span class="k">void</span> sleep_until(
    <a href="../types.html#symbol-f64">f64</a> seconds) = <span class="mi">0</span>;</code></pre></div>

<p>Blocks (or, for simulated clocks, advances time) until <code><a href="#function-now-seconds-1">now_seconds</a>()</code> reaches the given deadline. Returns immediately when the deadline has already passed.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
