---
kind: header
---

[Engine](../../engine.md) → [RHI](../rhi.md)

# null_device_stats.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/rhi/null_device_stats.h` |
| API ↗ | [Devices and swapchains](../../../api/rhi/devices-and-swapchains.md) |

<p>What the <code>null</code> backend recorded. Resources are counted when they are created, command-list work when it is recorded, submissions when executeCommandLists runs, and debug markers keep their names in recording order so renderer tests can assert on the pass stream. Read a snapshot with <code>device::null_stats()</code>.</p>

## Structs

| File | Description |
|---|---|
| [`null_device_stats`](null-device-stats/null-device-stats.md) |  |

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MAX_RECORDED_MARKER_NAMES"></a><code>MAX_RECORDED_MARKER_NAMES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">4096</span></code> | Marker names kept per <code>reset_null_stats()</code>; beginMarker calls beyond this are counted in <code>markersBegun</code> and <code>markerNamesDropped</code> but their names are not stored. |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="max_recorded_marker_names"></a>
<a id="null_device_stats"></a>
<a id="null_device_statsacquires"></a>
<a id="null_device_statsbindinglayoutscreated"></a>
<a id="null_device_statsbindingsetscreated"></a>
<a id="null_device_statsbufferclears"></a>
<a id="null_device_statsbuffercopies"></a>
<a id="null_device_statsbufferscreated"></a>
<a id="null_device_statsbufferwritebytes"></a>
<a id="null_device_statsbufferwrites"></a>
<a id="null_device_statscommandlistsclosed"></a>
<a id="null_device_statscommandlistscreated"></a>
<a id="null_device_statscommandlistsopened"></a>
<a id="null_device_statscommandlistssubmitted"></a>
<a id="null_device_statscomputepipelinescreated"></a>
<a id="null_device_statscomputestatechanges"></a>
<a id="null_device_statsdepthclears"></a>
<a id="null_device_statsdescriptortablescreated"></a>
<a id="null_device_statsdispatches"></a>
<a id="null_device_statsdispatchesindirect"></a>
<a id="null_device_statsdraws"></a>
<a id="null_device_statsdrawsindirect"></a>
<a id="null_device_statseventqueriescreated"></a>
<a id="null_device_statsframebufferscreated"></a>
<a id="null_device_statsgarbagecollections"></a>
<a id="null_device_statsgraphicspipelinescreated"></a>
<a id="null_device_statsgraphicsstatechanges"></a>
<a id="null_device_statsheapscreated"></a>
<a id="null_device_statsinputlayoutscreated"></a>
<a id="null_device_statsinstancesdrawn"></a>
<a id="null_device_statsmarkernames"></a>
<a id="null_device_statsmarkernamesdropped"></a>
<a id="null_device_statsmarkersbegun"></a>
<a id="null_device_statsmarkersended"></a>
<a id="null_device_statsmeshdispatches"></a>
<a id="null_device_statsmeshletpipelinescreated"></a>
<a id="null_device_statspresents"></a>
<a id="null_device_statspushconstantwrites"></a>
<a id="null_device_statsreset"></a>
<a id="null_device_statsresizes"></a>
<a id="null_device_statssamplerscreated"></a>
<a id="null_device_statsshaderscreated"></a>
<a id="null_device_statsstagingtexturescreated"></a>
<a id="null_device_statsswapchainscreated"></a>
<a id="null_device_statstextureclears"></a>
<a id="null_device_statstexturecopies"></a>
<a id="null_device_statstexturescreated"></a>
<a id="null_device_statstexturewrites"></a>
<a id="null_device_statstimerqueriesbegun"></a>
<a id="null_device_statstimerqueriescreated"></a>
<a id="null_device_statswaitsforidle"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
