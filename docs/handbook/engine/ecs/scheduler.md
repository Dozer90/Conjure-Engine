---
kind: header
---

[Engine](../../engine.md) → [ECS](../ecs.md)

# scheduler.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/ecs/scheduler.h` |
| API ↗ | [Command buffers and observers](../../../api/ecs/command-buffers-and-observers.md), [Queries, views and systems](../../../api/ecs/queries-views-and-systems.md) |

<p>The <code><a href="scheduler/scheduler.html">scheduler</a></code>.</p>

## Structs

| File | Description |
|---|---|
| [`scheduler_desc`](scheduler/scheduler-desc.md) | Parallelism switches. Both <code>false</code> reproduces the engine&#x27;s stage semantics strictly serially. |
| [`stage_info`](scheduler/stage-info.md) | One stage of a phase: systems that run concurrently. |
| [`system_id`](scheduler/system-id.md) | Handle of a registered system. |
| [`system_timing`](scheduler/system-timing.md) | Wall-clock cost of one system in the last run of its phase. |

## Classes

| File | Description |
|---|---|
| [`scheduler`](scheduler/scheduler.md) |  |

## Forward Declarations

### `scheduler::phase_state` {#symbol-scheduler-phase-state}

```cpp
struct phase_state;
```

### `scheduler::system_record` {#symbol-scheduler-system-record}

```cpp
struct system_record;
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
<a id="scheduler"></a>
<a id="scheduler_desc"></a>
<a id="scheduler_descminentitiesperjob"></a>
<a id="scheduler_descparallelchunks"></a>
<a id="scheduler_descparallelsystems"></a>
<a id="scheduleradd_system"></a>
<a id="schedulerbuild_stages"></a>
<a id="schedulerdesc"></a>
<a id="schedulerfinalise"></a>
<a id="schedulerfind_record"></a>
<a id="schedulerfind_system"></a>
<a id="schedulerlast_timings"></a>
<a id="schedulermark_dirty"></a>
<a id="schedulermdesc"></a>
<a id="schedulermfreerecords"></a>
<a id="schedulermjobs"></a>
<a id="schedulermnextgeneration"></a>
<a id="schedulermphases"></a>
<a id="schedulermregistrationcounter"></a>
<a id="schedulermresolvedcomponentcount"></a>
<a id="schedulermrunningframe"></a>
<a id="schedulermsystems"></a>
<a id="schedulermworld"></a>
<a id="scheduleroperator"></a>
<a id="schedulerphase_state"></a>
<a id="schedulerrefresh_access"></a>
<a id="schedulerremove_system"></a>
<a id="schedulerresolve_edges"></a>
<a id="schedulerrun_phase"></a>
<a id="schedulerrun_phase_filtered"></a>
<a id="schedulerrun_system"></a>
<a id="schedulerscheduler"></a>
<a id="schedulerscheduler_1"></a>
<a id="schedulerset_enabled"></a>
<a id="schedulerstages"></a>
<a id="schedulersystem"></a>
<a id="schedulersystem_count"></a>
<a id="schedulersystem_record"></a>
<a id="schedulertopological_order"></a>
<a id="stage_info"></a>
<a id="stage_infoexclusive"></a>
<a id="stage_infosystems"></a>
<a id="structs-and-types"></a>
<a id="system_id"></a>
<a id="system_idgeneration"></a>
<a id="system_idindex"></a>
<a id="system_idis_valid"></a>
<a id="system_idoperator"></a>
<a id="system_timing"></a>
<a id="system_timingmicroseconds"></a>
<a id="system_timingname"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
