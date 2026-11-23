---
kind: header
---

[Engine](../../engine.md) → [Platform](../platform.md)

# input.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/platform/input.h` |
| API ↗ | [Input](../../../api/platform/input.md) |

<p>The backend-independent <code><a href="input/input.html">input</a></code> accumulator. <code>apply()</code> (engine frame step 3) and <code>inject()</code> (editor, tests) run the same state machine; <code>end_frame()</code> publishes the snapshot by copy and clears the edges. Main thread only, except <code>snapshot()</code>, which any thread may read between two <code>end_frame()</code> calls.</p>

## Structs

| File | Description |
|---|---|
| [`input_stats`](input/input-stats.md) | Counters kept across the lifetime of an <code><a href="input/input.html">input</a></code> instance (diagnostics overlay, tests). |

## Classes

| File | Description |
|---|---|
| [`input`](input/input.md) |  |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="input"></a>
<a id="input_stats"></a>
<a id="input_statseventsapplied"></a>
<a id="input_statseventsinjected"></a>
<a id="input_statsignoredtransitions"></a>
<a id="input_statstextcodepointsdropped"></a>
<a id="inputapply"></a>
<a id="inputapply_cursor_enter"></a>
<a id="inputapply_gamepad_axis"></a>
<a id="inputapply_gamepad_button"></a>
<a id="inputapply_gamepad_connection"></a>
<a id="inputapply_input"></a>
<a id="inputapply_key"></a>
<a id="inputapply_mouse_button"></a>
<a id="inputapply_mouse_move"></a>
<a id="inputapply_mouse_scroll"></a>
<a id="inputapply_text"></a>
<a id="inputgamepad_for"></a>
<a id="inputinject"></a>
<a id="inputinput"></a>
<a id="inputinput_1"></a>
<a id="inputmframecounter"></a>
<a id="inputmpending"></a>
<a id="inputmpublished"></a>
<a id="inputmskipnextmousedelta"></a>
<a id="inputmstats"></a>
<a id="inputoperator"></a>
<a id="inputrelease_all"></a>
<a id="inputskip_next_mouse_delta"></a>
<a id="inputsnapshot"></a>
<a id="inputstats"></a>
<a id="inputvalue"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
