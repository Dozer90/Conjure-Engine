---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# clock.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/clock.h` |
| API ↗ | [Time, events and random](../../../api/core/time-and-events.md) |

<p><code>A</code> monotonic time source measured in seconds. The epoch is implementation defined: only differences between two readings of the same <code><a href="clock/clock.html">clock</a></code> are meaningful.</p>



## Classes

| File | Description |
|---|---|
| [`clock`](clock/clock.md) | <code>A</code> monotonic time source measured in seconds. The epoch is implementation defined: only differences between two readings of the same <code><a href="clock/clock.html">clock</a></code> are meaningful. |
| [`manual_clock`](clock/manual-clock.md) | <code>A</code> <code><a href="clock/clock.html">clock</a></code> that only moves when told to. Starts at zero. Used by tests and by tools that step the engine deterministically. |
| [`steady_clock`](clock/steady-clock.md) | Real time from <code>std::chrono::steady_clock</code>. Seconds count from the first use of any <code><a href="clock/steady-clock.html">steady_clock</a></code> in the process, so readings start near zero and keep full double precision. Every instance reads the same underlying time. |
| [`stopwatch`](clock/stopwatch.md) | Measures elapsed time on a <code><a href="clock/clock.html">clock</a></code>. Runs from construction; <code>start()</code> and <code>restart()</code> reset the origin. The <code><a href="clock/clock.html">clock</a></code> is referenced, not owned, and must outlive the <code><a href="clock/stopwatch.html">stopwatch</a></code>. |

## Functions

### Public

#### `default_clock` {#function-default-clock}

<div class="function-declaration" id="function-default-clock-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="clock/clock.html">clock</a> &amp;default_clock() <span class="k">noexcept</span>;</code></pre></div>

<p>The process-wide steady <code><a href="clock/clock.html">clock</a></code>. Use it when no <code><a href="clock/clock.html">clock</a></code> was injected.</p>

#### `ticks_now` {#function-ticks-now}

<div class="function-declaration" id="function-ticks-now-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="types.html#symbol-u64">u64</a> ticks_now() <span class="k">noexcept</span>;</code></pre></div>

<p>Raw steady-<code><a href="clock/clock.html">clock</a></code> ticks for profiling. Convert differences with <code>ticks_to_seconds</code>.</p>

#### `ticks_per_second` {#function-ticks-per-second}

<div class="function-declaration" id="function-ticks-per-second-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="types.html#symbol-u64">u64</a> ticks_per_second() <span class="k">noexcept</span>;</code></pre></div>

<p>Number of ticks in one second of the steady <code><a href="clock/clock.html">clock</a></code>.</p>

#### `ticks_to_seconds` {#function-ticks-to-seconds}

<div class="function-declaration" id="function-ticks-to-seconds-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="types.html#symbol-f64">f64</a> ticks_to_seconds(
    <a href="types.html#symbol-u64">u64</a> ticks) <span class="k">noexcept</span>;</code></pre></div>

<p>Converts a tick count (or a difference of two) from <code>ticks_now</code> into seconds.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="clock"></a>
<a id="clockclock"></a>
<a id="clockclock_1"></a>
<a id="clocknow_seconds"></a>
<a id="clockoperator"></a>
<a id="clocksleep_until"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="default_clock"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="manual_clock"></a>
<a id="manual_clockadvance"></a>
<a id="manual_clockmanual_clock"></a>
<a id="manual_clockmanual_clock_1"></a>
<a id="manual_clockmnowseconds"></a>
<a id="manual_clocknow_seconds"></a>
<a id="manual_clockset"></a>
<a id="manual_clocksleep_until"></a>
<a id="steady_clock"></a>
<a id="steady_clocknow_seconds"></a>
<a id="steady_clocksleep_until"></a>
<a id="steady_clockspin_window_seconds"></a>
<a id="steady_clocksteady_clock"></a>
<a id="steady_clocksteady_clock_1"></a>
<a id="stopwatch"></a>
<a id="stopwatchelapsed_seconds"></a>
<a id="stopwatchmclock"></a>
<a id="stopwatchmstartseconds"></a>
<a id="stopwatchoperator"></a>
<a id="stopwatchrestart"></a>
<a id="stopwatchsource"></a>
<a id="stopwatchstart"></a>
<a id="stopwatchstopwatch"></a>
<a id="stopwatchstopwatch_1"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="ticks_now"></a>
<a id="ticks_per_second"></a>
<a id="ticks_to_seconds"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
