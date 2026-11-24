---
kind: header
---

[Engine](../../engine.md) → [Scripting](../scripting.md)

# null_script_host.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scripting/null_script_host.h` |
| API ↗ | [Hosting, the native API and hot reload](../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../api/scripting/index.md) |

<p>The <code>C</code>++ fake of the managed side. It implements <code><a href="native-api/conjure-managed-api.html">conjure_managed_api</a></code> with recording functions: slots come from a LIFO free list, <code>apply_fields</code> stores the blob per slot and <code>collect_fields</code> echoes it, <code>run_phase</code>, <code>dispatch_collisions</code> and <code>set_active</code> are logged, <code>sample_memory</code> reports the figures <code>set_managed_memory</code> gave it, <code>load_game_assembly</code> registers fake classes (<code>add_fake_class</code> and an optional &lt;assembly&gt;.<code>classes.json</code> fixture) through the native table so the registry, blob and reload paths all run without .NET. Every scripting, scene, engine and editor test uses it; one <code>null</code> host is active (initialised) at a time.</p>

## Structs

| File | Description |
|---|---|
| [`null_host_recorder::activity_record`](null-script-host/null-host-recorder-activity-record.md) | One slot of a <code>set_active</code> call: the state asked for and whether the slot was live and changed. |
| [`null_host_recorder::apply_record`](null-script-host/null-host-recorder-apply-record.md) |  |
| [`null_host_recorder::collision_record`](null-script-host/null-host-recorder-collision-record.md) |  |
| [`null_host_recorder::create_record`](null-script-host/null-host-recorder-create-record.md) |  |
| [`null_host_recorder::destroy_record`](null-script-host/null-host-recorder-destroy-record.md) |  |
| [`null_host_recorder::phase_record`](null-script-host/null-host-recorder-phase-record.md) |  |
| [`null_script_host::slot_state`](null-script-host/null-script-host-slot-state.md) |  |

## Classes

| File | Description |
|---|---|
| [`null_host_recorder`](null-script-host/null-host-recorder.md) | What the <code>null</code> host recorded. Cleared by <code>null_script_host::reset_recorder()</code>. Each list keeps at most <code>MAX_RECORDS</code> entries and counts the rest in <code>dropped_count()</code>, so a long-running <code>null</code> host (the headless editor runs one by default) stays bounded; <code>set_enabled(false)</code> records nothing at all (the counters keep counting). |
| [`null_script_host`](null-script-host/null-script-host.md) |  |

## Type Aliases

### `null_script_host::activity_hook` {#symbol-null-script-host-activity-hook}

```cpp
using activity_hook = std::function<void(i32 slot, bool active)>;
```

Runs inside <code>set_active</code> for every live slot whose state changed, after the change: what OnEnable (active) or OnDisable (inactive) would do.

### `null_script_host::collision_hook` {#symbol-null-script-host-collision-hook}

```cpp
using collision_hook = std::function<void(const conjure_collision_record &record)>;
```

Runs inside <code>dispatch_collisions</code> for every record: what OnCollisionEnter and friends would do.

### `null_script_host::create_hook` {#symbol-null-script-host-create-hook}

```cpp
using create_hook = std::function<void(const conjure_create_request &request, i32 slot)>;
```

Runs inside <code>create_instances</code> once a request&#x27;s slot is assigned. As the create hook, for a request without <code>CREATE_FLAG_ALREADY_CREATED</code>: what OnCreate would do. As the reload hook, for a request with it: what OnReload would do.

### `null_script_host::destroy_hook` {#symbol-null-script-host-destroy-hook}

```cpp
using destroy_hook = std::function<void(i32 slot)>;
```

Runs inside <code>destroy_instances</code> for every live slot before it is freed when <code>callOnDestroy</code> is set: what OnDestroy would do.

### `null_script_host::phase_hook` {#symbol-null-script-host-phase-hook}

```cpp
using phase_hook = std::function<void(script_phase phase, f32 deltaTime)>;
```

Runs inside <code>run_phase</code> after the counters were bumped: what the phase callbacks would do.

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
<a id="null_host_recorder"></a>
<a id="null_host_recorderadmits"></a>
<a id="null_host_recorderapplies"></a>
<a id="null_host_recorderapply_record"></a>
<a id="null_host_recorderapply_recordblob"></a>
<a id="null_host_recorderapply_recordslot"></a>
<a id="null_host_recordercalls"></a>
<a id="null_host_recorderclear"></a>
<a id="null_host_recordercollect_fields_count"></a>
<a id="null_host_recordercollect_garbage_count"></a>
<a id="null_host_recordercollision_record"></a>
<a id="null_host_recordercollision_recordimpulse"></a>
<a id="null_host_recordercollision_recordkind"></a>
<a id="null_host_recordercollision_recordother"></a>
<a id="null_host_recordercollision_recordslot"></a>
<a id="null_host_recordercollisions"></a>
<a id="null_host_recordercount"></a>
<a id="null_host_recordercreate_record"></a>
<a id="null_host_recordercreate_recordclasshash"></a>
<a id="null_host_recordercreate_recordentity"></a>
<a id="null_host_recordercreate_recordfields"></a>
<a id="null_host_recordercreate_recordflags"></a>
<a id="null_host_recordercreate_recordslot"></a>
<a id="null_host_recordercreates"></a>
<a id="null_host_recorderdestroy_record"></a>
<a id="null_host_recorderdestroy_recordcallondestroy"></a>
<a id="null_host_recorderdestroy_recordslot"></a>
<a id="null_host_recorderdestroys"></a>
<a id="null_host_recorderdropped_count"></a>
<a id="null_host_recorderis_enabled"></a>
<a id="null_host_recorderloaded_assemblies"></a>
<a id="null_host_recordermapplies"></a>
<a id="null_host_recordermax_records"></a>
<a id="null_host_recordermcalls"></a>
<a id="null_host_recordermcollectfieldscount"></a>
<a id="null_host_recordermcollectgarbagecount"></a>
<a id="null_host_recordermcollisions"></a>
<a id="null_host_recordermcreates"></a>
<a id="null_host_recordermdestroys"></a>
<a id="null_host_recordermdroppedcount"></a>
<a id="null_host_recordermenabled"></a>
<a id="null_host_recordermloadedassemblies"></a>
<a id="null_host_recordermphases"></a>
<a id="null_host_recordermshutdowncount"></a>
<a id="null_host_recordermunloadcount"></a>
<a id="null_host_recordernull_host_recorder"></a>
<a id="null_host_recordernull_host_recorder_1"></a>
<a id="null_host_recorderoperator"></a>
<a id="null_host_recorderphase_record"></a>
<a id="null_host_recorderphase_recorddeltatime"></a>
<a id="null_host_recorderphase_recordliveslots"></a>
<a id="null_host_recorderphase_recordphase"></a>
<a id="null_host_recorderphases"></a>
<a id="null_host_recorderrecord_apply"></a>
<a id="null_host_recorderrecord_call"></a>
<a id="null_host_recorderrecord_collect_fields"></a>
<a id="null_host_recorderrecord_collect_garbage"></a>
<a id="null_host_recorderrecord_collision"></a>
<a id="null_host_recorderrecord_create"></a>
<a id="null_host_recorderrecord_destroy"></a>
<a id="null_host_recorderrecord_load"></a>
<a id="null_host_recorderrecord_phase"></a>
<a id="null_host_recorderrecord_shutdown"></a>
<a id="null_host_recorderrecord_unload"></a>
<a id="null_host_recorderset_enabled"></a>
<a id="null_host_recordershutdown_count"></a>
<a id="null_host_recorderunload_count"></a>
<a id="null_script_host"></a>
<a id="null_script_hostactive"></a>
<a id="null_script_hostadd_fake_class"></a>
<a id="null_script_hostallocate_slot"></a>
<a id="null_script_hostapply_fields"></a>
<a id="null_script_hostbackend"></a>
<a id="null_script_hostclear_fake_classes"></a>
<a id="null_script_hostcollect_fields"></a>
<a id="null_script_hostcollect_garbage"></a>
<a id="null_script_hostcollision_hook"></a>
<a id="null_script_hostcreate_hook"></a>
<a id="null_script_hostcreate_instances"></a>
<a id="null_script_hostdestroy_hook"></a>
<a id="null_script_hostdestroy_instances"></a>
<a id="null_script_hostdispatch_collisions"></a>
<a id="null_script_hostfree_list"></a>
<a id="null_script_hostfree_slot"></a>
<a id="null_script_hostinitialize"></a>
<a id="null_script_hostis_assembly_loaded"></a>
<a id="null_script_hostis_slot_live"></a>
<a id="null_script_hostis_slot_started"></a>
<a id="null_script_hostlive_slot_count"></a>
<a id="null_script_hostload_game_assembly"></a>
<a id="null_script_hostloaded_assembly"></a>
<a id="null_script_hostmanaged_api"></a>
<a id="null_script_hostmassemblyloaded"></a>
<a id="null_script_hostmcollisionhook"></a>
<a id="null_script_hostmcreatehook"></a>
<a id="null_script_hostmdestroyhook"></a>
<a id="null_script_hostmfakeclasses"></a>
<a id="null_script_hostmfreelist"></a>
<a id="null_script_hostmloadedassembly"></a>
<a id="null_script_hostmloadresult"></a>
<a id="null_script_hostmmanagedapi"></a>
<a id="null_script_hostmnativeapi"></a>
<a id="null_script_hostmphasehook"></a>
<a id="null_script_hostmrecorder"></a>
<a id="null_script_hostmslots"></a>
<a id="null_script_hostmstatus"></a>
<a id="null_script_hostmunloadresult"></a>
<a id="null_script_hostnull_script_host"></a>
<a id="null_script_hostnull_script_host_1"></a>
<a id="null_script_hostparse_classes_fixture"></a>
<a id="null_script_hostphase_hook"></a>
<a id="null_script_hostrecorder"></a>
<a id="null_script_hostregister_classes_with_native"></a>
<a id="null_script_hostremove_fake_class"></a>
<a id="null_script_hostreset_recorder"></a>
<a id="null_script_hostrun_phase"></a>
<a id="null_script_hostset_collect_override"></a>
<a id="null_script_hostset_collision_hook"></a>
<a id="null_script_hostset_create_hook"></a>
<a id="null_script_hostset_destroy_hook"></a>
<a id="null_script_hostset_load_result"></a>
<a id="null_script_hostset_phase_hook"></a>
<a id="null_script_hostset_recording"></a>
<a id="null_script_hostset_unload_result"></a>
<a id="null_script_hostshutdown"></a>
<a id="null_script_hostshutdown_managed"></a>
<a id="null_script_hostslot_class"></a>
<a id="null_script_hostslot_entity"></a>
<a id="null_script_hostslot_fields"></a>
<a id="null_script_hostslot_phase_count"></a>
<a id="null_script_hostslot_state"></a>
<a id="null_script_hostslot_stateclasshash"></a>
<a id="null_script_hostslot_statecollectoverride"></a>
<a id="null_script_hostslot_stateentity"></a>
<a id="null_script_hostslot_statefields"></a>
<a id="null_script_hostslot_statelive"></a>
<a id="null_script_hostslot_statephasecounts"></a>
<a id="null_script_hostslot_statestarted"></a>
<a id="null_script_hoststatus"></a>
<a id="null_script_hosttrampoline_apply_fields"></a>
<a id="null_script_hosttrampoline_collect_fields"></a>
<a id="null_script_hosttrampoline_collect_garbage"></a>
<a id="null_script_hosttrampoline_create_instances"></a>
<a id="null_script_hosttrampoline_destroy_instances"></a>
<a id="null_script_hosttrampoline_dispatch_collisions"></a>
<a id="null_script_hosttrampoline_load_game_assembly"></a>
<a id="null_script_hosttrampoline_run_phase"></a>
<a id="null_script_hosttrampoline_shutdown"></a>
<a id="null_script_hosttrampoline_unload_game_assembly"></a>
<a id="null_script_hostunload_game_assembly"></a>
<a id="other-declarations"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
