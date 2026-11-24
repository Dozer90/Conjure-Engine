---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# asset_manager.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/asset_manager.h` |
| API ↗ | [Cooking, caches and packs](../../../api/assets/cooking-and-packs.md), [Database, importers and hot reload](../../../api/assets/database-and-import.md), [Assets](../../../api/assets/index.md), [Referencing and loading assets](../../../api/assets/referencing-and-loading.md) |

<p>Owns every slot, payload and the load pipeline. Outlives every handle. An untyped bind (<code>bind_slot</code>, <code>make_handle</code>), load or pin of a uuid that no source can type asks the sources again on each call, until one can.</p>

## Structs

| File | Description |
|---|---|
| [`asset_event`](asset-manager/asset-event.md) | Delivered to subscribers on the main thread at the end of <code>update()</code>, in order. |
| [`asset_manager_desc`](asset-manager/asset-manager-desc.md) | Construction parameters. <code>update()</code> stops finalising once <code>finaliseBudgetSeconds</code> have passed since it began finalising (dispatch and drain are not counted); it checks after each slot, so it finalises at least one ready slot and can overrun the budget by the slot in progress. <code>blockingTimeoutSeconds</code> is how long <code>load_blocking</code> waits for a job that already owns a slot. For both, NaN and negative values count as 0 and values above 1e9 seconds (infinity included) as 1e9. |
| [`asset_manager_stats`](asset-manager/asset-manager-stats.md) | Counters for tooling. |
| [`asset_subscription`](asset-manager/asset-subscription.md) | Token returned by <code>subscribe()</code>; 0 is invalid. |

## Classes

| File | Description |
|---|---|
| [`asset_load_batch`](asset-manager/asset-load-batch.md) | The handles of one <code>load_batch()</code> call. |
| [`asset_manager`](asset-manager/asset-manager.md) | Owns every slot, payload and the load pipeline. Outlives every handle. An untyped bind (<code>bind_slot</code>, <code>make_handle</code>), load or pin of a uuid that no source can type asks the sources again on each call, until one can. |

## Enumerations

### `asset_event_kind` {#symbol-asset-event-kind}

```cpp
enum class asset_event_kind : u8 {
    LOADED = 0,
    RELOADED,
    RELOAD_FAILED,
    FAILED,
    UNLOADED
};
```

What happened to a slot.

### `blocking_load_warning` {#symbol-blocking-load-warning}

```cpp
enum class blocking_load_warning : u8 {
    WARN_INSIDE_FRAME = 0,
    EXPECTED
};
```

Whether <code>asset_manager::load_blocking</code> logs its once-per-frame hitch <code>WARN</code> when it is called inside a frame. <code>WARN_INSIDE_FRAME</code> is the default and the right choice for game code, where a blocking load is a mistake worth seeing. <code>EXPECTED</code> is for a tool whose contract is to finish inside the call (the editor&#x27;s prefab commands inside <code>editor.batch</code> and in undo and redo replays): the load still blocks the frame, it is only not reported. Outside a frame neither logs anything.

## Type Aliases

### `asset_event_callback` {#symbol-asset-event-callback}

```cpp
using asset_event_callback = std::function<void(const asset_event &event)>;
```

## Forward Declarations

### `asset_manager::state` {#symbol-asset-manager-state}

```cpp
struct state;
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
<a id="asset_event"></a>
<a id="asset_event_callback"></a>
<a id="asset_event_kind"></a>
<a id="asset_event_kindfailed"></a>
<a id="asset_event_kindloaded"></a>
<a id="asset_event_kindreload_failed"></a>
<a id="asset_event_kindreloaded"></a>
<a id="asset_event_kindunloaded"></a>
<a id="asset_eventerror"></a>
<a id="asset_eventid"></a>
<a id="asset_eventkind"></a>
<a id="asset_eventtype"></a>
<a id="asset_eventversion"></a>
<a id="asset_load_batch"></a>
<a id="asset_load_batchasset_load_batch"></a>
<a id="asset_load_batchasset_load_batch_1"></a>
<a id="asset_load_batchhandles"></a>
<a id="asset_load_batchis_complete"></a>
<a id="asset_load_batchloaded_count"></a>
<a id="asset_load_batchmhandles"></a>
<a id="asset_load_batchoperator"></a>
<a id="asset_load_batchprogress"></a>
<a id="asset_load_batchsize"></a>
<a id="asset_manager"></a>
<a id="asset_manager_desc"></a>
<a id="asset_manager_descblockingtimeout"></a>
<a id="asset_manager_descfinalisebudget"></a>
<a id="asset_manager_descjobs"></a>
<a id="asset_manager_descmaxjobsinflight"></a>
<a id="asset_manager_descsource"></a>
<a id="asset_manager_stats"></a>
<a id="asset_manager_statsbytesresident"></a>
<a id="asset_manager_statsfailed"></a>
<a id="asset_manager_statsframeindex"></a>
<a id="asset_manager_statsjobsinflight"></a>
<a id="asset_manager_statsloaded"></a>
<a id="asset_manager_statspending"></a>
<a id="asset_manager_statsretired"></a>
<a id="asset_manager_statsslots"></a>
<a id="asset_manageradd_source"></a>
<a id="asset_managerasset_manager"></a>
<a id="asset_managerasset_manager_1"></a>
<a id="asset_managerbegin_frame"></a>
<a id="asset_managerbind"></a>
<a id="asset_managerbind_slot"></a>
<a id="asset_managerbind_typed"></a>
<a id="asset_managercollect"></a>
<a id="asset_managerend_frame"></a>
<a id="asset_managerfailed_ids"></a>
<a id="asset_managerfallback"></a>
<a id="asset_managerfallback_payload"></a>
<a id="asset_managerfind"></a>
<a id="asset_managerfind_slot"></a>
<a id="asset_managerflush"></a>
<a id="asset_managerframe_index"></a>
<a id="asset_managerget_or_fallback"></a>
<a id="asset_manageris_pinned"></a>
<a id="asset_manageris_shut_down"></a>
<a id="asset_managerlast_error"></a>
<a id="asset_managerload"></a>
<a id="asset_managerload_batch"></a>
<a id="asset_managerload_blocking"></a>
<a id="asset_managermake_handle"></a>
<a id="asset_managermstate"></a>
<a id="asset_manageroperator"></a>
<a id="asset_managerpin"></a>
<a id="asset_managerreload"></a>
<a id="asset_managerretry_failed"></a>
<a id="asset_managershutdown"></a>
<a id="asset_managerstate"></a>
<a id="asset_managerstate_of"></a>
<a id="asset_managerstats"></a>
<a id="asset_managersubscribe"></a>
<a id="asset_managerswap_source"></a>
<a id="asset_managertype_of"></a>
<a id="asset_managertypes"></a>
<a id="asset_managerunsubscribe"></a>
<a id="asset_managerupdate"></a>
<a id="asset_subscription"></a>
<a id="asset_subscriptionid"></a>
<a id="asset_subscriptionis_valid"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
