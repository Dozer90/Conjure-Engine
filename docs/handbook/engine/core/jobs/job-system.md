---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# job_system.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/jobs/job_system.h` |
| API ↗ | [Jobs and threading](../../../../api/core/jobs.md) |

<p>Declarations for <code><a href="job-system/jobs-job-system.html">job_system</a>.h</code>.</p>

## Structs

| File | Description |
|---|---|
| [`jobs::job_system_desc`](job-system/jobs-job-system-desc.md) | Configuration passed to <code>job_system::start()</code>. |
| [`jobs::job_system_stats`](job-system/jobs-job-system-stats.md) | Counters accumulated since the last <code>start()</code>. Reads are approximate while jobs are running. stolen + helped + <code>inlineExecutions</code> == executed holds only while no worker executes a job it queued itself: such a job is neither stolen nor helped and is counted in executed alone. |

## Classes

| File | Description |
|---|---|
| [`jobs::job_system`](job-system/jobs-job-system.md) | Thread pool with a shared, priority-laned job queue. Jobs are <code>std::function</code> objects; a system started with zero workers (or one that is not running) executes everything inline on the calling thread, which keeps every caller deterministic and headless-testable. <code>wait()</code> executes queued jobs on the waiting thread, so waiting from inside a job never deadlocks. Not copyable or movable: workers hold the address of the system. |

## Enumerations

### `jobs::job_priority` {#symbol-jobs-job-priority}

```cpp
enum class job_priority : u8 {
    HIGH = 0,
    NORMAL,
    LOW
};
```

Scheduling lane. Workers always take the highest priority job available.

## Type Aliases

### `jobs::detail::range_invoke_fn` {#symbol-jobs-detail-range-invoke-fn}

```cpp
using range_invoke_fn = void (*)(const void *context, usize begin, usize end);
```

Executes one <code>parallel_for</code> range against an opaque body. Lets both <code>parallel_for</code> overloads share one allocation-free dispatch path.

## Forward Declarations

### `jobs::job_system::state` {#symbol-jobs-job-system-state}

```cpp
class state;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-jobs::JOB_PRIORITY_COUNT"></a><code>jobs::JOB_PRIORITY_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a></code> | <code class="signature"><span class="mi">3</span></code> | Number of priority lanes. |
| <a id="variable-jobs::MAX_QUEUED_JOBS"></a><code>jobs::MAX_QUEUED_JOBS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a></code> | <code class="signature"><a href="../types.html#symbol-usize">usize</a>{<span class="mi">1</span>} &lt;&lt; <span class="mi">20</span></code> | Upper bound accepted for <code>job_system_desc::maxQueuedJobs</code>. <code>start()</code> reserves one pointer per slot in each priority lane (24 bytes per unit), so the bound keeps a stray value from exhausting memory. |
| <a id="variable-jobs::MAX_WORKER_COUNT"></a><code>jobs::MAX_WORKER_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">256</span></code> | Upper bound accepted for <code>job_system_desc::workerCount</code>. |

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
<a id="job_priority"></a>
<a id="job_priority_count"></a>
<a id="job_priorityhigh"></a>
<a id="job_prioritylow"></a>
<a id="job_prioritynormal"></a>
<a id="job_system"></a>
<a id="job_system_desc"></a>
<a id="job_system_descmaxqueuedjobs"></a>
<a id="job_system_descthreadnameprefix"></a>
<a id="job_system_descworkercount"></a>
<a id="job_system_stats"></a>
<a id="job_system_statsexecuted"></a>
<a id="job_system_statshelped"></a>
<a id="job_system_statsinlineexecutions"></a>
<a id="job_system_statsmaxqueuedepth"></a>
<a id="job_system_statspoolcapacity"></a>
<a id="job_system_statspoolgrowths"></a>
<a id="job_system_statsqueuednow"></a>
<a id="job_system_statsqueuefullfallbacks"></a>
<a id="job_system_statsscheduled"></a>
<a id="job_system_statsstolen"></a>
<a id="job_systemcurrent"></a>
<a id="job_systemis_running"></a>
<a id="job_systemjob_system"></a>
<a id="job_systemjob_system_1"></a>
<a id="job_systemmstate"></a>
<a id="job_systemoperator"></a>
<a id="job_systemparallel_for"></a>
<a id="job_systemparallel_for_impl"></a>
<a id="job_systemschedule"></a>
<a id="job_systemstart"></a>
<a id="job_systemstate"></a>
<a id="job_systemstats"></a>
<a id="job_systemstop"></a>
<a id="job_systemthread_index"></a>
<a id="job_systemwait"></a>
<a id="job_systemwait_idle"></a>
<a id="job_systemworker_count"></a>
<a id="jobsdetailrange_invoke_fn"></a>
<a id="jobsjob_priority"></a>
<a id="jobsjob_priority_count"></a>
<a id="jobsjob_priorityhigh"></a>
<a id="jobsjob_prioritylow"></a>
<a id="jobsjob_prioritynormal"></a>
<a id="jobsjob_system"></a>
<a id="jobsjob_system_desc"></a>
<a id="jobsjob_system_descmaxqueuedjobs"></a>
<a id="jobsjob_system_descthreadnameprefix"></a>
<a id="jobsjob_system_descworkercount"></a>
<a id="jobsjob_system_stats"></a>
<a id="jobsjob_system_statsexecuted"></a>
<a id="jobsjob_system_statshelped"></a>
<a id="jobsjob_system_statsinlineexecutions"></a>
<a id="jobsjob_system_statsmaxqueuedepth"></a>
<a id="jobsjob_system_statspoolcapacity"></a>
<a id="jobsjob_system_statspoolgrowths"></a>
<a id="jobsjob_system_statsqueuednow"></a>
<a id="jobsjob_system_statsqueuefullfallbacks"></a>
<a id="jobsjob_system_statsscheduled"></a>
<a id="jobsjob_system_statsstolen"></a>
<a id="jobsjob_systemcurrent"></a>
<a id="jobsjob_systemis_running"></a>
<a id="jobsjob_systemjob_system"></a>
<a id="jobsjob_systemjob_system_1"></a>
<a id="jobsjob_systemmstate"></a>
<a id="jobsjob_systemoperator"></a>
<a id="jobsjob_systemparallel_for"></a>
<a id="jobsjob_systemparallel_for_impl"></a>
<a id="jobsjob_systemschedule"></a>
<a id="jobsjob_systemstart"></a>
<a id="jobsjob_systemstate"></a>
<a id="jobsjob_systemstats"></a>
<a id="jobsjob_systemstop"></a>
<a id="jobsjob_systemthread_index"></a>
<a id="jobsjob_systemwait"></a>
<a id="jobsjob_systemwait_idle"></a>
<a id="jobsjob_systemworker_count"></a>
<a id="jobsmax_queued_jobs"></a>
<a id="jobsmax_worker_count"></a>
<a id="max_queued_jobs"></a>
<a id="max_worker_count"></a>
<a id="other-declarations"></a>
<a id="range_invoke_fn"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
