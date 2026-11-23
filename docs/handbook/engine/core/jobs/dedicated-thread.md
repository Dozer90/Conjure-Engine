---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# dedicated_thread.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/jobs/dedicated_thread.h` |
| API ↗ | [Jobs and threading](../../../../api/core/jobs.md) |

<p>Declarations for <code><a href="dedicated-thread/jobs-dedicated-thread.html">dedicated_thread</a>.h</code>.</p>



## Classes

| File | Description |
|---|---|
| [`jobs::dedicated_thread`](dedicated-thread/jobs-dedicated-thread.md) | <code>A</code> named, long-running thread outside the worker pool (file watchers, process pipe readers, audio streaming). The body receives a stop token; destruction requests a stop and joins. Move-only. |

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
<a id="dedicated_thread"></a>
<a id="dedicated_threaddedicated_thread"></a>
<a id="dedicated_threaddedicated_thread_1"></a>
<a id="dedicated_threadget_id"></a>
<a id="dedicated_threadjoin"></a>
<a id="dedicated_threadjoinable"></a>
<a id="dedicated_threadmname"></a>
<a id="dedicated_threadmthread"></a>
<a id="dedicated_threadname"></a>
<a id="dedicated_threadoperator"></a>
<a id="dedicated_threadrequest_stop"></a>
<a id="dedicated_threadstop_requested"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="jobsdedicated_thread"></a>
<a id="jobsdedicated_threaddedicated_thread"></a>
<a id="jobsdedicated_threaddedicated_thread_1"></a>
<a id="jobsdedicated_threadget_id"></a>
<a id="jobsdedicated_threadjoin"></a>
<a id="jobsdedicated_threadjoinable"></a>
<a id="jobsdedicated_threadmname"></a>
<a id="jobsdedicated_threadmthread"></a>
<a id="jobsdedicated_threadname"></a>
<a id="jobsdedicated_threadoperator"></a>
<a id="jobsdedicated_threadrequest_stop"></a>
<a id="jobsdedicated_threadstop_requested"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
