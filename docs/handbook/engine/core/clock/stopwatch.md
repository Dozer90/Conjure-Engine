---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [clock.h](../clock.md)

# stopwatch
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/clock.h` |
| Source File Path | `engine/src/core/time/clock.cpp` |
| API ↗ | [Time, events and random](../../../../api/core/time-and-events.md) |

## Syntax

```cpp
class CONJURE_API stopwatch
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">stopwatch() <span class="k">noexcept</span>;</code></td><td>Starts measuring on the process-wide steady <code><a href="clock.html">clock</a></code>.</td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> stopwatch(
    <span class="k">const</span> <a href="clock.html">clock</a> &amp;source) <span class="k">noexcept</span>;</code></td><td>Starts measuring on the given <code><a href="clock.html">clock</a></code>.</td></tr>
<tr><td>public</td><td><code class="signature">stopwatch(
    <span class="k">const</span> <a href="stopwatch.html">stopwatch</a> &amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~stopwatch();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mClock</code></td><td><code class="signature"><span class="k">const</span> <a href="clock.html">clock</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Start<wbr>Seconds</code></td><td><code class="signature"><a href="../types.html#symbol-f64">f64</a></code></td><td><code class="signature"><span class="mi">0.0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `elapsed_seconds` {#function-elapsed-seconds}

<div class="function-declaration" id="function-elapsed-seconds-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-f64">f64</a> elapsed_seconds() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Seconds elapsed since the last start, restart or construction.</p>

#### `restart` {#function-restart}

<div class="function-declaration" id="function-restart-1"><pre><code><a href="../types.html#symbol-f64">f64</a> restart() <span class="k">noexcept</span>;</code></pre></div>

<p>Returns the elapsed seconds and resets the origin to the current time in one reading, so consecutive intervals add up without gaps.</p>

#### `source` {#function-source}

<div class="function-declaration" id="function-source-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="clock.html">clock</a> &amp;source() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The <code><a href="clock.html">clock</a></code> being measured.</p>

#### `start` {#function-start}

<div class="function-declaration" id="function-start-1"><pre><code><span class="k">void</span> start() <span class="k">noexcept</span>;</code></pre></div>

<p>Resets the origin to the current time.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="stopwatch.html">stopwatch</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="stopwatch.html">stopwatch</a> &amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
