---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# frame_stats.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/frame_stats.h` |
| API ↗ | [The frame, layers and time](../../../api/engine/frame-layers-and-time.md), [Engine: the application and the frame](../../../api/engine/index.md) |

<p>Frame statistics: one <code><a href="frame-stats/frame-sample.html">frame_sample</a></code> per frame committed at step 15 into a ring of <code>HISTORY_SIZE</code> samples. The editor profiler panel and <code>diagnostics_frame_stats</code> read it through <code>engine_context::stats</code>; written only by the main thread between phases.</p>

## Structs

| File | Description |
|---|---|
| [`frame_sample`](frame-stats/frame-sample.md) |  |

## Classes

| File | Description |
|---|---|
| [`frame_stats`](frame-stats/frame-stats.md) |  |

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
<a id="frame_sample"></a>
<a id="frame_sampledroppedfixedtime"></a>
<a id="frame_samplefixedsteps"></a>
<a id="frame_sampleframememoryhighwater"></a>
<a id="frame_sampleframemilliseconds"></a>
<a id="frame_samplejobsexecuted"></a>
<a id="frame_sampleoperator"></a>
<a id="frame_samplepacersleepmilliseconds"></a>
<a id="frame_samplephasemilliseconds"></a>
<a id="frame_samplesubmitmilliseconds"></a>
<a id="frame_stats"></a>
<a id="frame_statsaverage_frame_milliseconds"></a>
<a id="frame_statsclear"></a>
<a id="frame_statscommit"></a>
<a id="frame_statsframe_stats"></a>
<a id="frame_statsframe_stats_1"></a>
<a id="frame_statshistory"></a>
<a id="frame_statshistory_size"></a>
<a id="frame_statslatest"></a>
<a id="frame_statsmcount"></a>
<a id="frame_statsmsamples"></a>
<a id="frame_statsmtotalcommitted"></a>
<a id="frame_statsmtotalframeswithdroppedtime"></a>
<a id="frame_statsmwriteindex"></a>
<a id="frame_statsoperator"></a>
<a id="frame_statssample_at"></a>
<a id="frame_statssample_count"></a>
<a id="frame_statstotal_committed"></a>
<a id="frame_statstotal_frames_with_dropped_time"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
