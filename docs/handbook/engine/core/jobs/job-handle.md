---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# job_handle.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/jobs/job_handle.h` |
| API ↗ | [Jobs and threading](../../../../api/core/jobs.md) |

<p>Declarations for <code><a href="job-handle/jobs-job-handle.html">job_handle</a>.h</code>.</p>

## Structs

| File | Description |
|---|---|
| [`jobs::detail::job_counter`](job-handle/jobs-detail-job-counter.md) | Number of jobs still outstanding for one handle. Shared between the handle, the job system and every queued job block that belongs to the handle. 64 bits wide so a <code>parallel_for</code> over a huge range can never truncate its range count. |

## Classes

| File | Description |
|---|---|
| [`jobs::job_fence`](job-handle/jobs-job-fence.md) | Caller-owned completion state of one or more batches: the allocation-free counterpart of a <code><a href="job-handle/jobs-job-handle.html">job_handle</a></code> for code that dispatches the same batch every frame (the ECS scheduler&#x27;s stages). <code>job_system::schedule(jobs, fence)</code> adds a batch, <code>job_system::wait(fence)</code> returns once every job added so far has run. Queued jobs hold the fence&#x27;s address, so the owner keeps it alive and in place until it is complete; it can then be reused. Neither copyable nor movable for that reason. |
| [`jobs::job_handle`](job-handle/jobs-job-handle.md) | Reference to the completion state of one scheduled job or of a batch scheduled together. Handles are cheap to copy; every copy observes the same counter. <code>A</code> default-constructed handle is invalid and reports itself complete, so waiting on it is a no-op. |

## Forward Declarations

### `jobs::job_system` {#symbol-jobs-job-system}

```cpp
class job_system;
```

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
<a id="internals-and-algorithms"></a>
<a id="job_counter"></a>
<a id="job_counterremaining"></a>
<a id="job_fence"></a>
<a id="job_fenceis_complete"></a>
<a id="job_fencejob_fence"></a>
<a id="job_fencejob_fence_1"></a>
<a id="job_fencejob_system"></a>
<a id="job_fencemcounter"></a>
<a id="job_fenceoperator"></a>
<a id="job_fencepending"></a>
<a id="job_handle"></a>
<a id="job_handleis_complete"></a>
<a id="job_handlejob_handle"></a>
<a id="job_handlejob_handle_1"></a>
<a id="job_handlejob_system"></a>
<a id="job_handlemcounter"></a>
<a id="job_handleoperator"></a>
<a id="job_handleoperator_1"></a>
<a id="job_handlepending"></a>
<a id="job_handlereset"></a>
<a id="job_handlevalid"></a>
<a id="job_system"></a>
<a id="jobsdetailjob_counter"></a>
<a id="jobsdetailjob_counterremaining"></a>
<a id="jobsjob_fence"></a>
<a id="jobsjob_fenceis_complete"></a>
<a id="jobsjob_fencejob_fence"></a>
<a id="jobsjob_fencejob_fence_1"></a>
<a id="jobsjob_fencejob_system"></a>
<a id="jobsjob_fencemcounter"></a>
<a id="jobsjob_fenceoperator"></a>
<a id="jobsjob_fencepending"></a>
<a id="jobsjob_handle"></a>
<a id="jobsjob_handleis_complete"></a>
<a id="jobsjob_handlejob_handle"></a>
<a id="jobsjob_handlejob_handle_1"></a>
<a id="jobsjob_handlejob_system"></a>
<a id="jobsjob_handlemcounter"></a>
<a id="jobsjob_handleoperator"></a>
<a id="jobsjob_handleoperator_1"></a>
<a id="jobsjob_handlepending"></a>
<a id="jobsjob_handlereset"></a>
<a id="jobsjob_handlevalid"></a>
<a id="jobsjob_system"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
