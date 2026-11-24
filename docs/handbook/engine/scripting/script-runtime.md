---
kind: header
---

[Engine](../../engine.md) → [Scripting](../scripting.md)

# script_runtime.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scripting/script_runtime.h` |
| API ↗ | [Scripting](../../../api/scripting/index.md), [Scripts, components and the runtime](../../../api/scripting/scripts-and-components.md) |

<p>The native side of script execution. One <code><a href="script-runtime/script-runtime.html">script_runtime</a></code> per process (asserted; <code>script_runtime::active()</code> is what the component hooks and the core table entries reach). It owns the host, the class registry, the store, the frame block, the pending queues and the DOD query scopes; <code>attach_world()</code> wires the <code>ADDED</code>/<code>REMOVED</code> observers of <code><a href="script-component/script-component.html">script_component</a></code> and of the activity tags (<code>set_activity_tags</code>). Every method runs on the main thread.</p>

## Structs

| File | Description |
|---|---|
| [`collision_event`](script-runtime/collision-event.md) | One contact the engine forwards from <code>physics_world::contact_events()</code>; dispatched to both participants&#x27; scripts with the normal flipped for b. |
| [`managed_memory_delta`](script-runtime/managed-memory-delta.md) | The managed heap figures of one frame (<code>script_runtime::sample_managed_memory</code>). <code>allocatedBytes</code> and the collections are what happened since the previous sample on the main thread&#x27;s managed heap; <code>heapBytes</code> is the heap size now. Threads that scripts start are not counted. |
| [`script_runtime::deferred_reservation`](script-runtime/script-runtime-deferred-reservation.md) | An id the scripting entries reserved through <code>mDeferredCommands</code> that no flush has applied yet. <code>held</code> follows the components the commands recorded so far give it, so <code>component_remove</code> can answer whether there is something to remove; an instantiation root gets its archive&#x27;s components at the flush, which the runtime cannot see (<code>heldKnown</code> <code>false</code>). |
| [`script_runtime::faulted_slot`](script-runtime/script-runtime-faulted-slot.md) | <code>A</code> fault reported inside <code>create_instances</code>, before its slot was mapped (OnCreate or OnReload threw). |
| [`script_runtime::pending_create`](script-runtime/script-runtime-pending-create.md) |  |
| [`script_runtime::query_cache_key`](script-runtime/script-runtime-query-cache-key.md) | The component ids a scope requires (<code>with</code>) and excludes (<code>without</code>), each sorted ascending, without repeats and padded with <code>component_id::INVALID</code>: the same sets in any order, {a, a} included, find the same cached query (columns still follow the request order), and {a} without {b} is a different query from {a}. |
| [`script_runtime::query_scope_state`](script-runtime/script-runtime-query-scope-state.md) |  |
| [`script_runtime_config`](script-runtime/script-runtime-config.md) |  |
| [`script_runtime_stats`](script-runtime/script-runtime-stats.md) | Snapshot of the runtime&#x27;s size for diagnostics and tests. |

## Classes

| File | Description |
|---|---|
| [`script_runtime`](script-runtime/script-runtime.md) |  |

## Enumerations

### `script_mode` {#symbol-script-mode}

```cpp
enum class script_mode : u8 {
    EDIT,       // JSON is authoritative, no managed instances exist
    PLAY        // live instances are authoritative, JSON is a snapshot
};
```

## Functions

### Public

#### `fill_core_native_entries` {#function-fill-core-native-entries}

<div class="function-declaration" id="function-fill-core-native-entries-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> fill_core_native_entries(
    <a href="native-api/conjure-native-api.html">conjure_native_api</a> &amp;api) <span class="k">noexcept</span>;</code></pre></div>

<p>Fills the core-owned entries (<code>log_</code>*, <code>script_</code>*, the five <code>entity_</code>*, <code>component_</code>*, <code>query_</code>*) of a table with functions that act on <code>script_runtime::active()</code>. Called by <code>script_runtime::initialise</code>.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="collision_event"></a>
<a id="collision_eventa"></a>
<a id="collision_eventb"></a>
<a id="collision_eventimpulse"></a>
<a id="collision_eventkind"></a>
<a id="collision_eventnormal"></a>
<a id="collision_eventpoint"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="fill_core_native_entries"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="script_mode"></a>
<a id="script_modeedit"></a>
<a id="script_modeplay"></a>
<a id="script_runtime"></a>
<a id="script_runtime_config"></a>
<a id="script_runtime_configgameassemblypath"></a>
<a id="script_runtime_confighost"></a>
<a id="script_runtime_configinitialmode"></a>
<a id="script_runtime_stats"></a>
<a id="script_runtime_statscachedqueries"></a>
<a id="script_runtime_statsliveinstances"></a>
<a id="script_runtime_statsopenqueryscopes"></a>
<a id="script_runtime_statspendingcreates"></a>
<a id="script_runtime_statspendingdestroys"></a>
<a id="script_runtime_statspendingentitydestroys"></a>
<a id="script_runtime_statsreloadcount"></a>
<a id="script_runtime_statszombieassemblies"></a>
<a id="script_runtimeactive"></a>
<a id="script_runtimeadd_script"></a>
<a id="script_runtimeadopt_world_sets"></a>
<a id="script_runtimeapply_entry_fields"></a>
<a id="script_runtimeapply_entry_fields_with"></a>
<a id="script_runtimeassembly_path"></a>
<a id="script_runtimeattach_registry"></a>
<a id="script_runtimeattach_world"></a>
<a id="script_runtimebegin_query_scope"></a>
<a id="script_runtimecached_query"></a>
<a id="script_runtimecached_query_count"></a>
<a id="script_runtimeclasses"></a>
<a id="script_runtimeclasses_mutable"></a>
<a id="script_runtimeclone_set_for_copy"></a>
<a id="script_runtimecollect_entry_fields"></a>
<a id="script_runtimecurrent_phase"></a>
<a id="script_runtimedeferred_commands"></a>
<a id="script_runtimedeserialize_entity_scripts"></a>
<a id="script_runtimedestroy_all"></a>
<a id="script_runtimedestroy_slots"></a>
<a id="script_runtimedispatch_collision_events"></a>
<a id="script_runtimeend_query_scope"></a>
<a id="script_runtimeenter_phase"></a>
<a id="script_runtimeentity_name"></a>
<a id="script_runtimeentity_name_count"></a>
<a id="script_runtimeentry"></a>
<a id="script_runtimeentry_of_slot"></a>
<a id="script_runtimefaulted_slot"></a>
<a id="script_runtimefaulted_slotcallback"></a>
<a id="script_runtimefaulted_slotslot"></a>
<a id="script_runtimefind_entity_by_name"></a>
<a id="script_runtimeflush_deferred_commands"></a>
<a id="script_runtimeflush_pending"></a>
<a id="script_runtimeforce_close_query_scopes"></a>
<a id="script_runtimeframe_block"></a>
<a id="script_runtimefree_set_from_destruct"></a>
<a id="script_runtimehost"></a>
<a id="script_runtimeinitialize"></a>
<a id="script_runtimeis_assembly_loaded"></a>
<a id="script_runtimeis_ready"></a>
<a id="script_runtimeleave_phase"></a>
<a id="script_runtimeload_game_assembly"></a>
<a id="script_runtimelog_faulted"></a>
<a id="script_runtimemaddedobserver"></a>
<a id="script_runtimemark_faulted"></a>
<a id="script_runtimemark_started_entries"></a>
<a id="script_runtimemassemblyloaded"></a>
<a id="script_runtimemassemblypath"></a>
<a id="script_runtimemax_flush_iterations"></a>
<a id="script_runtimemblobbuffer"></a>
<a id="script_runtimemclasses"></a>
<a id="script_runtimemcollisionrecords"></a>
<a id="script_runtimemcollisionsource"></a>
<a id="script_runtimemcreatinginstances"></a>
<a id="script_runtimemcurrentphase"></a>
<a id="script_runtimemdeferredcommands"></a>
<a id="script_runtimemdestroyedobserver"></a>
<a id="script_runtimementitynames"></a>
<a id="script_runtimemfaultedbeforemapped"></a>
<a id="script_runtimemframeblock"></a>
<a id="script_runtimemfreequeryscopes"></a>
<a id="script_runtimemhost"></a>
<a id="script_runtimeminflush"></a>
<a id="script_runtimemmode"></a>
<a id="script_runtimemnativeapi"></a>
<a id="script_runtimemnextscopeid"></a>
<a id="script_runtimemode"></a>
<a id="script_runtimempendingcreates"></a>
<a id="script_runtimempendingdestroyslots"></a>
<a id="script_runtimempendingentitydestroys"></a>
<a id="script_runtimemquerycache"></a>
<a id="script_runtimemqueryscopes"></a>
<a id="script_runtimemregistry"></a>
<a id="script_runtimemreloadcount"></a>
<a id="script_runtimemremovedobserver"></a>
<a id="script_runtimemscratch"></a>
<a id="script_runtimemscriptcomponentid"></a>
<a id="script_runtimemstatus"></a>
<a id="script_runtimemstore"></a>
<a id="script_runtimemwarnedmirrornames"></a>
<a id="script_runtimemwarnedmissingclasses"></a>
<a id="script_runtimemworld"></a>
<a id="script_runtimemzombieassemblies"></a>
<a id="script_runtimenative_api"></a>
<a id="script_runtimenoexcept"></a>
<a id="script_runtimenote_mirror_mismatch"></a>
<a id="script_runtimeon_component_added"></a>
<a id="script_runtimeon_component_removed"></a>
<a id="script_runtimeopen_query_scope_count"></a>
<a id="script_runtimeoperator"></a>
<a id="script_runtimepending_create"></a>
<a id="script_runtimepending_createid"></a>
<a id="script_runtimeprocess_pending_creates"></a>
<a id="script_runtimeprocess_pending_destroys"></a>
<a id="script_runtimequery_cache_key"></a>
<a id="script_runtimequery_scope_inline_chunks"></a>
<a id="script_runtimequery_scope_reserve"></a>
<a id="script_runtimequery_scope_state"></a>
<a id="script_runtimequery_scope_statechunks"></a>
<a id="script_runtimequery_scope_stateid"></a>
<a id="script_runtimequery_scope_statelock"></a>
<a id="script_runtimequery_scope_statetypes"></a>
<a id="script_runtimequeue_create"></a>
<a id="script_runtimequeue_creates_of_set"></a>
<a id="script_runtimeread_set_json"></a>
<a id="script_runtimerefresh_live_fields"></a>
<a id="script_runtimeregistry"></a>
<a id="script_runtimereload_game_assembly"></a>
<a id="script_runtimereload_internal"></a>
<a id="script_runtimeremap_entity_fields"></a>
<a id="script_runtimeremap_set_entities"></a>
<a id="script_runtimeremove_script"></a>
<a id="script_runtimereplace_set_entries"></a>
<a id="script_runtimerequest_entity_destroy"></a>
<a id="script_runtimerun_phase"></a>
<a id="script_runtimescript_component_id"></a>
<a id="script_runtimescript_runtime"></a>
<a id="script_runtimescript_runtime_1"></a>
<a id="script_runtimescripts_of"></a>
<a id="script_runtimeserialize_entity_scripts"></a>
<a id="script_runtimeset_collision_source"></a>
<a id="script_runtimeset_field"></a>
<a id="script_runtimeset_mode"></a>
<a id="script_runtimeshutdown"></a>
<a id="script_runtimestats"></a>
<a id="script_runtimestatus"></a>
<a id="script_runtimestore"></a>
<a id="script_runtimetake_scope_state"></a>
<a id="script_runtimevoid"></a>
<a id="script_runtimewarn_missing_class_once"></a>
<a id="script_runtimeworld"></a>
<a id="script_runtimewrite_set_entries_json"></a>
<a id="script_runtimewrite_set_json"></a>
<a id="structs-and-types"></a>
<a id="symbol-script-runtime-query-cache-key"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="type-aliases"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
