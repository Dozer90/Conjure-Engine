---
kind: class
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [null_script_host.h](../null-script-host.md)

# null_script_host
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/null_script_host.h` |
| Source File Path | `engine/src/scripting/null_script_host.cpp` |
| API ↗ | [Hosting, the native API and hot reload](../../../../api/scripting/hosting-and-hot-reload.md), [Scripting](../../../../api/scripting/index.md) |

## Syntax

```cpp
class CONJURE_API null_script_host final : public script_host
```

## Inheritance Hierarchy


[`script_host`](../script-host/script-host.md) → `null_script_host`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">null_script_host();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~null_script_host() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">m<wbr>Assembly<wbr>Loaded</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Collision<wbr>Hook</code></td><td><code class="signature"><a href="../null-script-host.html#symbol-null-script-host-collision-hook">collision_hook</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCreateHook</code></td><td><code class="signature"><a href="../null-script-host.html#symbol-null-script-host-create-hook">create_hook</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDestroyHook</code></td><td><code class="signature"><a href="../null-script-host.html#symbol-null-script-host-destroy-hook">destroy_hook</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFakeClasses</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../script-class-registry/script-class-info.html">script_<wbr>class_<wbr>info</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFreeList</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-i32">i32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Loaded<wbr>Assembly</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLoadResult</code></td><td><code class="signature"><a href="../../core/types.html#symbol-i32">i32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mManagedApi</code></td><td><code class="signature"><a href="../native-api/conjure-managed-api.html">conjure_managed_api</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNativeApi</code></td><td><code class="signature"><a href="../native-api/conjure-native-api.html">conjure_native_api</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPhaseHook</code></td><td><code class="signature"><a href="../null-script-host.html#symbol-null-script-host-phase-hook">phase_hook</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRecorder</code></td><td><code class="signature"><a href="null-host-recorder.html">null_host_recorder</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSlots</code></td><td><code class="signature">std::vector&lt;<a href="null-script-host-slot-state.html">slot_state</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStatus</code></td><td><code class="signature"><a href="../script-host.html#symbol-script-host-status">script_host_status</a></code></td><td><code class="signature">script_<wbr>host_<wbr>status::<wbr>UNINITIALISED</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Unload<wbr>Result</code></td><td><code class="signature"><a href="../../core/types.html#symbol-i32">i32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `active` {#function-active}

<div class="function-declaration" id="function-active-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="null-script-host.html">null_script_host</a> *active() <span class="k">noexcept</span>;</code></pre></div>

<p>The <code>null</code> host whose functions the managed table currently calls, or <code>nullptr</code>.</p>

#### `add_fake_class` {#function-add-fake-class}

<div class="function-declaration" id="function-add-fake-class-1"><pre><code><span class="k">void</span> add_fake_class(
    <a href="../script-class-registry/script-class-info.html">script_class_info</a> info);</code></pre></div>

<p><code>A</code> class <code>load_game_assembly</code> reports to native through <code>script_register_class</code>/field (fields may be in any order; the registry sorts them).</p>

#### `backend` {#function-backend}

<div class="function-declaration" id="function-backend-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../script-host.html#symbol-script-backend">script_backend</a> backend() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `clear_fake_classes` {#function-clear-fake-classes}

<div class="function-declaration" id="function-clear-fake-classes-1"><pre><code><span class="k">void</span> clear_fake_classes();</code></pre></div>

<p>Forgets every fake class.</p>

#### `free_list` {#function-free-list}

<div class="function-declaration" id="function-free-list-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::vector&lt;<a href="../../core/types.html#symbol-i32">i32</a>&gt; &amp;free_list() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Slots on the free list, most recently freed first.</p>

#### `initialise` {#function-initialise}

<div class="function-declaration" id="function-initialise-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; initialise(
    <span class="k">const</span> <a href="../script-host/script-host-config.html">script_host_config</a> &amp;config,
    <a href="../native-api/conjure-native-api.html">conjure_native_api</a> &amp;nativeApi) <span class="k">override</span>;</code></pre></div>

<p>Fills the managed table with the recording functions and becomes the active <code>null</code> host. Never fails; a second active <code>null</code> host in the process asserts and replaces the first.</p>

#### `is_assembly_loaded` {#function-is-assembly-loaded}

<div class="function-declaration" id="function-is-assembly-loaded-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_assembly_loaded() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_slot_live` {#function-is-slot-live}

<div class="function-declaration" id="function-is-slot-live-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_slot_live(
    <a href="../../core/types.html#symbol-i32">i32</a> slot) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_slot_started` {#function-is-slot-started}

<div class="function-declaration" id="function-is-slot-started-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_slot_started(
    <a href="../../core/types.html#symbol-i32">i32</a> slot) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the slot is live and either took part in a <code>FIXED_UPDATE</code> or <code>UPDATE</code> <code>run_phase</code> (the phases that run OnStart; <code>LATE_UPDATE</code> does not start a slot) or was created with <code>CREATE_FLAG_ALREADY_STARTED</code>. This mirrors what the managed side marks after OnStart, so a test can inspect it.</p>

#### `live_slot_count` {#function-live-slot-count}

<div class="function-declaration" id="function-live-slot-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> live_slot_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `loaded_assembly` {#function-loaded-assembly}

<div class="function-declaration" id="function-loaded-assembly-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;loaded_assembly() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `managed_api` {#function-managed-api}

<div class="function-declaration" id="function-managed-api-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../native-api/conjure-managed-api.html">conjure_managed_api</a> &amp;managed_api() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

#### `parse_classes_fixture` {#function-parse-classes-fixture}

<div class="function-declaration" id="function-parse-classes-fixture-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;std::vector&lt;<a href="../script-class-registry/script-class-info.html">script_class_info</a>&gt;&gt; parse_classes_fixture(
    std::string_view jsonText);</code></pre></div>

<p>Parses a &lt;assembly&gt;.<code>classes.json</code> fixture: {&quot;classes&quot;:[{&quot;name&quot;,&quot;callbacks&quot;:[&quot;<code>UPDATE</code>&quot;,...], &quot;fields&quot;:[{&quot;name&quot;,&quot;kind&quot;,&quot;elementKind&quot;,&quot;default&quot;,&quot;meta&quot;}]}]}.</p>

#### `recorder` {#function-recorder}

<div class="function-declaration" id="function-recorder-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="null-host-recorder.html">null_host_recorder</a> &amp;recorder() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `remove_fake_class` {#function-remove-fake-class}

<div class="function-declaration" id="function-remove-fake-class-1"><pre><code><span class="k">bool</span> remove_fake_class(
    <a href="../../core/types.html#symbol-u64">u64</a> classHash);</code></pre></div>

<p>Removes a fake class so the next load models an assembly that dropped it.</p>

#### `reset_recorder` {#function-reset-recorder}

<div class="function-declaration" id="function-reset-recorder-1"><pre><code><span class="k">void</span> reset_recorder();</code></pre></div>

#### `set_collect_override` {#function-set-collect-override}

<div class="function-declaration" id="function-set-collect-override-1"><pre><code><span class="k">void</span> set_collect_override(
    <a href="../../core/types.html#symbol-i32">i32</a> slot,
    std::vector&lt;<a href="../../core/types.html#symbol-u8">u8</a>&gt; blob);</code></pre></div>

<p>Bytes <code>collect_fields</code> returns for a slot instead of echoing the last <code>apply_fields</code> blob.</p>

#### `set_collision_hook` {#function-set-collision-hook}

<div class="function-declaration" id="function-set-collision-hook-1"><pre><code><span class="k">void</span> set_collision_hook(
    <a href="../null-script-host.html#symbol-null-script-host-collision-hook">collision_hook</a> hook);</code></pre></div>

#### `set_create_hook` {#function-set-create-hook}

<div class="function-declaration" id="function-set-create-hook-1"><pre><code><span class="k">void</span> set_create_hook(
    <a href="../null-script-host.html#symbol-null-script-host-create-hook">create_hook</a> hook);</code></pre></div>

<p>Callbacks standing in for managed script code (empty = none). They run on the main thread inside the <code>managed_api</code> call and may call back into the runtime like a script would: add scripts, destroy entities, open query scopes through the native table.</p>

#### `set_destroy_hook` {#function-set-destroy-hook}

<div class="function-declaration" id="function-set-destroy-hook-1"><pre><code><span class="k">void</span> set_destroy_hook(
    <a href="../null-script-host.html#symbol-null-script-host-destroy-hook">destroy_hook</a> hook);</code></pre></div>

#### `set_load_result` {#function-set-load-result}

<div class="function-declaration" id="function-set-load-result-1"><pre><code><span class="k">void</span> set_load_result(
    <a href="../../core/types.html#symbol-i32">i32</a> result) <span class="k">noexcept</span>;</code></pre></div>

<p>The value <code>load_game_assembly</code> returns (0 = success; a <code><a href="../scripting-error.html#symbol-script-error">script_error</a></code> code models a failed load).</p>

#### `set_phase_hook` {#function-set-phase-hook}

<div class="function-declaration" id="function-set-phase-hook-1"><pre><code><span class="k">void</span> set_phase_hook(
    <a href="../null-script-host.html#symbol-null-script-host-phase-hook">phase_hook</a> hook);</code></pre></div>

#### `set_recording` {#function-set-recording}

<div class="function-declaration" id="function-set-recording-1"><pre><code><span class="k">void</span> set_recording(
    <span class="k">bool</span> enabled) <span class="k">noexcept</span>;</code></pre></div>

<p>False makes the recorder record nothing (<code>null_host_recorder::set_enabled</code>) and the host build no record, so recording adds no allocation to a managed call: for long runs and allocation gates. Only the slot state the host keeps (a new slot, a slot&#x27;s fields) may still grow. Survives <code><a href="#function-reset-recorder-1">reset_recorder</a>()</code>. On by default.</p>

#### `set_unload_result` {#function-set-unload-result}

<div class="function-declaration" id="function-set-unload-result-1"><pre><code><span class="k">void</span> set_unload_result(
    <a href="../../core/types.html#symbol-i32">i32</a> result) <span class="k">noexcept</span>;</code></pre></div>

<p>The value <code>unload_game_assembly</code> returns: 0 collected, 1 leaked.</p>

#### `shutdown` {#function-shutdown}

<div class="function-declaration" id="function-shutdown-1"><pre><code><span class="k">void</span> shutdown() <span class="k">override</span>;</code></pre></div>

<p>Frees every slot, forgets the loaded assembly, deactivates. Recorded as &quot;shutdown&quot;.</p>

#### `slot_class` {#function-slot-class}

<div class="function-declaration" id="function-slot-class-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> slot_class(
    <a href="../../core/types.html#symbol-i32">i32</a> slot) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `slot_entity` {#function-slot-entity}

<div class="function-declaration" id="function-slot-entity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> slot_entity(
    <a href="../../core/types.html#symbol-i32">i32</a> slot) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Entity recorded for a live slot (0 otherwise).</p>

#### `slot_fields` {#function-slot-fields}

<div class="function-declaration" id="function-slot-fields-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::vector&lt;<a href="../../core/types.html#symbol-u8">u8</a>&gt; &amp;slot_fields(
    <a href="../../core/types.html#symbol-i32">i32</a> slot) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Blob last applied to a live slot (empty otherwise).</p>

#### `slot_phase_count` {#function-slot-phase-count}

<div class="function-declaration" id="function-slot-phase-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> slot_phase_count(
    <a href="../../core/types.html#symbol-i32">i32</a> slot,
    <a href="../native-api.html#symbol-script-phase">script_phase</a> phase) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>How often a live slot took part in <code><a href="#function-run-phase-1">run_phase</a>(phase)</code>.</p>

#### `status` {#function-status}

<div class="function-declaration" id="function-status-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../script-host.html#symbol-script-host-status">script_host_status</a> status() <span class="k">const</span> <span class="k">noexcept</span> <span class="k">override</span>;</code></pre></div>

### Private

#### `allocate_slot` {#function-allocate-slot}

<div class="function-declaration" id="function-allocate-slot-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-i32">i32</a> allocate_slot();</code></pre></div>

#### `apply_fields` {#function-apply-fields}

<div class="function-declaration" id="function-apply-fields-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-i32">i32</a> apply_fields(
    <a href="../../core/types.html#symbol-i32">i32</a> slot,
    <span class="k">const</span> <a href="../../core/types.html#symbol-u8">u8</a> *blob,
    <a href="../../core/types.html#symbol-i32">i32</a> size);</code></pre></div>

#### `collect_fields` {#function-collect-fields}

<div class="function-declaration" id="function-collect-fields-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-i32">i32</a> collect_fields(
    <a href="../../core/types.html#symbol-i32">i32</a> slot,
    <a href="../../core/types.html#symbol-u8">u8</a> *buffer,
    <a href="../../core/types.html#symbol-i32">i32</a> capacity);</code></pre></div>

#### `collect_garbage` {#function-collect-garbage}

<div class="function-declaration" id="function-collect-garbage-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-i32">i32</a> collect_garbage();</code></pre></div>

#### `create_instances` {#function-create-instances}

<div class="function-declaration" id="function-create-instances-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-i32">i32</a> create_instances(
    <a href="../native-api/conjure-create-request.html">conjure_create_request</a> *requests,
    <a href="../../core/types.html#symbol-i32">i32</a> count);</code></pre></div>

#### `destroy_instances` {#function-destroy-instances}

<div class="function-declaration" id="function-destroy-instances-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-i32">i32</a> destroy_instances(
    <span class="k">const</span> <a href="../../core/types.html#symbol-i32">i32</a> *slots,
    <a href="../../core/types.html#symbol-i32">i32</a> count,
    <a href="../../core/types.html#symbol-i32">i32</a> callOnDestroy);</code></pre></div>

#### `dispatch_collisions` {#function-dispatch-collisions}

<div class="function-declaration" id="function-dispatch-collisions-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-i32">i32</a> dispatch_collisions(
    <span class="k">const</span> <a href="../native-api/conjure-collision-record.html">conjure_collision_record</a> *records,
    <a href="../../core/types.html#symbol-i32">i32</a> count);</code></pre></div>

#### `free_slot` {#function-free-slot}

<div class="function-declaration" id="function-free-slot-1"><pre><code><span class="k">void</span> free_slot(
    <a href="../../core/types.html#symbol-i32">i32</a> slot);</code></pre></div>

#### `load_game_assembly` {#function-load-game-assembly}

<div class="function-declaration" id="function-load-game-assembly-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-i32">i32</a> load_game_assembly(
    std::string_view path);</code></pre></div>

#### `register_classes_with_native` {#function-register-classes-with-native}

<div class="function-declaration" id="function-register-classes-with-native-1"><pre><code><span class="k">void</span> register_classes_with_native(
    <span class="k">const</span> std::vector&lt;<a href="../script-class-registry/script-class-info.html">script_class_info</a>&gt; &amp;classes);</code></pre></div>

#### `run_phase` {#function-run-phase}

<div class="function-declaration" id="function-run-phase-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-i32">i32</a> run_phase(
    <a href="../../core/types.html#symbol-u8">u8</a> phase,
    <a href="../../core/types.html#symbol-f32">f32</a> deltaTime);</code></pre></div>

#### `shutdown_managed` {#function-shutdown-managed}

<div class="function-declaration" id="function-shutdown-managed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-i32">i32</a> shutdown_managed();</code></pre></div>

#### `trampoline_apply_fields` {#function-trampoline-apply-fields}

<div class="function-declaration" id="function-trampoline-apply-fields-1"><pre><code><span class="k">static</span> <a href="../../core/types.html#symbol-i32">i32</a> trampoline_apply_fields(
    <a href="../../core/types.html#symbol-i32">i32</a> slot,
    <span class="k">const</span> <a href="../../core/types.html#symbol-u8">u8</a> *blob,
    <a href="../../core/types.html#symbol-i32">i32</a> size);</code></pre></div>

#### `trampoline_collect_fields` {#function-trampoline-collect-fields}

<div class="function-declaration" id="function-trampoline-collect-fields-1"><pre><code><span class="k">static</span> <a href="../../core/types.html#symbol-i32">i32</a> trampoline_collect_fields(
    <a href="../../core/types.html#symbol-i32">i32</a> slot,
    <a href="../../core/types.html#symbol-u8">u8</a> *buffer,
    <a href="../../core/types.html#symbol-i32">i32</a> capacity);</code></pre></div>

#### `trampoline_collect_garbage` {#function-trampoline-collect-garbage}

<div class="function-declaration" id="function-trampoline-collect-garbage-1"><pre><code><span class="k">static</span> <a href="../../core/types.html#symbol-i32">i32</a> trampoline_collect_garbage();</code></pre></div>

#### `trampoline_create_instances` {#function-trampoline-create-instances}

<div class="function-declaration" id="function-trampoline-create-instances-1"><pre><code><span class="k">static</span> <a href="../../core/types.html#symbol-i32">i32</a> trampoline_create_instances(
    <a href="../native-api/conjure-create-request.html">conjure_create_request</a> *requests,
    <a href="../../core/types.html#symbol-i32">i32</a> count);</code></pre></div>

#### `trampoline_destroy_instances` {#function-trampoline-destroy-instances}

<div class="function-declaration" id="function-trampoline-destroy-instances-1"><pre><code><span class="k">static</span> <a href="../../core/types.html#symbol-i32">i32</a> trampoline_destroy_instances(
    <span class="k">const</span> <a href="../../core/types.html#symbol-i32">i32</a> *slots,
    <a href="../../core/types.html#symbol-i32">i32</a> count,
    <a href="../../core/types.html#symbol-i32">i32</a> callOnDestroy);</code></pre></div>

#### `trampoline_dispatch_collisions` {#function-trampoline-dispatch-collisions}

<div class="function-declaration" id="function-trampoline-dispatch-collisions-1"><pre><code><span class="k">static</span> <a href="../../core/types.html#symbol-i32">i32</a> trampoline_dispatch_collisions(
    <span class="k">const</span> <a href="../native-api/conjure-collision-record.html">conjure_collision_record</a> *records,
    <a href="../../core/types.html#symbol-i32">i32</a> count);</code></pre></div>

#### `trampoline_load_game_assembly` {#function-trampoline-load-game-assembly}

<div class="function-declaration" id="function-trampoline-load-game-assembly-1"><pre><code><span class="k">static</span> <a href="../../core/types.html#symbol-i32">i32</a> trampoline_load_game_assembly(
    <span class="k">const</span> <span class="k">char</span> *utf8Path,
    <a href="../../core/types.html#symbol-i32">i32</a> length);</code></pre></div>

#### `trampoline_run_phase` {#function-trampoline-run-phase}

<div class="function-declaration" id="function-trampoline-run-phase-1"><pre><code><span class="k">static</span> <a href="../../core/types.html#symbol-i32">i32</a> trampoline_run_phase(
    <a href="../../core/types.html#symbol-u8">u8</a> phase,
    <a href="../../core/types.html#symbol-f32">f32</a> deltaTime);</code></pre></div>

#### `trampoline_shutdown` {#function-trampoline-shutdown}

<div class="function-declaration" id="function-trampoline-shutdown-1"><pre><code><span class="k">static</span> <a href="../../core/types.html#symbol-i32">i32</a> trampoline_shutdown();</code></pre></div>

#### `trampoline_unload_game_assembly` {#function-trampoline-unload-game-assembly}

<div class="function-declaration" id="function-trampoline-unload-game-assembly-1"><pre><code><span class="k">static</span> <a href="../../core/types.html#symbol-i32">i32</a> trampoline_unload_game_assembly();</code></pre></div>

#### `unload_game_assembly` {#function-unload-game-assembly}

<div class="function-declaration" id="function-unload-game-assembly-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-i32">i32</a> unload_game_assembly();</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="function-initialize"></a>
<a id="function-initialize-1"></a>
<!-- doc-legacy-anchors:end -->
