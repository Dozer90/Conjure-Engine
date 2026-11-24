---
kind: class
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_runtime.h](../script-runtime.md)

# script_runtime
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_runtime.h` |
| Source File Path | `engine/src/scripting/script_runtime.cpp` |
| API ↗ | [Scripting](../../../../api/scripting/index.md), [Scripts, components and the runtime](../../../../api/scripting/scripts-and-components.md) |

## Syntax

```cpp
class CONJURE_API script_runtime
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> script_runtime(
    std::unique_ptr&lt;<a href="../script-host/script-host.html">script_host</a>&gt; host);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~script_runtime();</code></td><td><code>destroy_all(false)</code> when instances are still alive, then shuts the host down.</td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">m<wbr>Added<wbr>Observer</code></td><td><code class="signature"><a href="../../ecs/observer/observer-id.html">ecs::observer_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Assembly<wbr>Loaded</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Assembly<wbr>Path</code></td><td><code class="signature">std::filesystem::path</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>FLUSH_<wbr>ITERATIONS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">8</span></code></td><td>Loops <code>flush_pending()</code> runs for creates and destroys requested from inside OnCreate/OnDestroy.</td></tr>
<tr><td>private</td><td><code class="signature">mBlobBuffer</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u8">u8</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mClasses</code></td><td><code class="signature"><a href="../script-class-registry/script-class-registry.html">script_class_registry</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Collision<wbr>Records</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../native-api/conjure-collision-record.html">conjure_<wbr>collision_<wbr>record</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Collision<wbr>Source</code></td><td><code class="signature">std::<wbr>function&lt;<wbr>std::<wbr>span&lt;<wbr><span class="k">const</span> <a href="collision-event.html">collision_<wbr>event</a>&gt;()&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Creating<wbr>Instances</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Current<wbr>Phase</code></td><td><code class="signature"><a href="../native-api.html#symbol-script-phase">script_phase</a></code></td><td><code class="signature">script_<wbr>phase::<wbr>COUNT</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Deferred<wbr>Commands</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../../ecs/command-buffer/command-buffer.html">ecs::<wbr>command_<wbr>buffer</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Deferred<wbr>Reserved</code></td><td><code class="signature"><a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::<wbr>flat_<wbr>hash_<wbr>map</a>&lt;<wbr>ecs::<wbr>entity,<wbr> <a href="script-runtime-deferred-reservation.html">deferred_<wbr>reservation</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Destroyed<wbr>Observer</code></td><td><code class="signature"><a href="../../ecs/observer/observer-id.html">ecs::observer_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEntityNames</code></td><td><code class="signature">std::<wbr>unordered_<wbr>map&lt;<wbr>ecs::<wbr>entity,<wbr> std::<wbr>string&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Faulted<wbr>Before<wbr>Mapped</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="script-runtime-faulted-slot.html">faulted_<wbr>slot</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFrameBlock</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../native-api/conjure-frame-block.html">conjure_<wbr>frame_<wbr>block</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Free<wbr>Query<wbr>Scopes</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="script-runtime-query-scope-state.html">query_<wbr>scope_<wbr>state</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mHost</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../script-host/script-host.html">script_<wbr>host</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mInFlush</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMode</code></td><td><code class="signature"><a href="../script-runtime.html#symbol-script-mode">script_mode</a></code></td><td><code class="signature">script_<wbr>mode::<wbr>EDIT</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNativeApi</code></td><td><code class="signature"><a href="../native-api/conjure-native-api.html">conjure_native_api</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNextScopeId</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pending<wbr>Creates</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="script-runtime-pending-create.html">pending_<wbr>create</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pending<wbr>Destroy<wbr>Slots</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-i32">i32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pending<wbr>Entity<wbr>Destroys</code></td><td><code class="signature">std::vector&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mQueryCache</code></td><td><code class="signature"><a href="../../core/containers/flat-map/flat-map.html">core::<wbr>flat_<wbr>map</a>&lt;<wbr>query_<wbr>cache_<wbr>key,<wbr> <a href="../../ecs/query/query.html">ecs::<wbr>query</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mQueryScopes</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="script-runtime-query-scope-state.html">query_<wbr>scope_<wbr>state</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRegistry</code></td><td><code class="signature"><a href="../../ecs/component-registry/component-registry.html">ecs::<wbr>component_<wbr>registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mReloadCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Removed<wbr>Observer</code></td><td><code class="signature"><a href="../../ecs/observer/observer-id.html">ecs::observer_id</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mScratch</code></td><td><code class="signature"><a href="../../core/memory/frame-allocator/memory-frame-allocator.html">core::<wbr>memory::<wbr>frame_<wbr>allocator</a></code></td><td>—</td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Script<wbr>Component<wbr>Id</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStatus</code></td><td><code class="signature"><a href="../script-host.html#symbol-script-host-status">script_host_status</a></code></td><td><code class="signature">script_<wbr>host_<wbr>status::<wbr>UNINITIALISED</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStore</code></td><td><code class="signature"><a href="../script-component/script-store.html">script_store</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Warned<wbr>Mirror<wbr>Names</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u64">u64</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Warned<wbr>Missing<wbr>Classes</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u64">u64</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWorld</code></td><td><code class="signature"><a href="../../ecs/world/world.html">ecs::world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Zombie<wbr>Assemblies</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">noexcept</code></td><td><code class="signature">[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::<wbr>function&lt;<wbr>std::<wbr>span&lt;<wbr><span class="k">const</span> <a href="collision-event.html">collision_<wbr>event</a>&gt;()&gt; &amp;collision_<wbr>source() <span class="k">const</span></code></td><td><code class="signature">{ return m<wbr>Collision<wbr>Source; } <span class="k">void</span> set_<wbr>entity_<wbr>name(<a href="../../ecs/entity/entity.html">ecs::<wbr>entity</a> entity,<wbr> std::<wbr>string_<wbr>view name)</code></td><td>The collision source set with <code>set_collision_source()</code>, or an empty function.</td></tr>
<tr><td>public</td><td><code class="signature">QUERY_<wbr>SCOPE_<wbr>INLINE_<wbr>CHUNKS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">16</span></code></td><td>Chunk views a query scope holds without touching the heap: 10,000 rows of a 12-<code><a href="../../core/types.html#symbol-byte">byte</a></code> component are 13 chunks of 819 rows. <code>A</code> larger scope spills its state once per size step; finished states are reused best-fit, so the spilled one keeps serving the large scope.</td></tr>
<tr><td>public</td><td><code class="signature">QUERY_<wbr>SCOPE_<wbr>RESERVE</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">4</span></code></td><td>Scope states (open and reusable) reserved up front: the nesting depth scripts realistically reach.</td></tr>
</tbody>
</table>

## Functions

### Public

#### `active` {#function-active}

<div class="function-declaration" id="function-active-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="script-runtime.html">script_runtime</a> *active() <span class="k">noexcept</span>;</code></pre></div>

<p>The single live runtime of the process, or <code>nullptr</code>.</p>

#### `add_script` {#function-add-script}

<div class="function-declaration" id="function-add-script-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../script-component/script-entry-id.html">script_entry_id</a> add_script(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity,
    <a href="../../core/types.html#symbol-u64">u64</a> classHash,
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;fields);</code></pre></div>

<p>Adds a <code><a href="../script-component/script-component.html">script_component</a></code> when missing, appends an entry with the given JSON fields (an object; invalid = defaults) and, in <code>PLAY</code> mode, queues its creation for the next flush. An invalid id when the entity is dead, the world detached or the entity already carries <code>MAX_SCRIPTS_PER_ENTITY</code> scripts, and (with a <code>WARN</code>) when the entity has no <code><a href="../script-component/script-component.html">script_component</a></code> yet while the world is locked for iteration: adding that component is a structural change. An id <code><a href="#function-is-deferred-reserved-1">is_deferred_reserved</a>()</code> answers <code>true</code> for takes the script anyway: the entry is staged in a set of its own and the <code><a href="../script-component/script-component.html">script_component</a></code> is recorded in <code><a href="#function-deferred-commands-1">deferred_commands</a>()</code> behind the earlier commands on the id, so the flush places the entity with the set and queues the creation (a destroy or a removal of <code><a href="../script-component/script-component.html">script_component</a></code> recorded later drops the staged scripts, as it drops the component). The returned id names the entry in that set; when the entity already holds a <code><a href="../script-component/script-component.html">script_component</a></code> at the flush (a deferred instantiation root whose archive carries scripts), the staged entries are appended to its set instead and the id goes stale; entries that would take that set past <code>MAX_SCRIPTS_PER_ENTITY</code> are dropped at the flush with a <code>WARN</code>. On a reserved id the result is invalid only when the staged set already holds <code>MAX_SCRIPTS_PER_ENTITY</code> scripts or the registry has no <code><a href="../script-component/script-component.html">script_component</a></code> type.</p>

#### `assembly_path` {#function-assembly-path}

<div class="function-declaration" id="function-assembly-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;assembly_path() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `attach_registry` {#function-attach-registry}

<div class="function-declaration" id="function-attach-registry-1"><pre><code><span class="k">void</span> attach_registry(
    <a href="../../ecs/component-registry/component-registry.html">ecs::component_registry</a> *registry);</code></pre></div>

<p>The registry [EcsComponent] types register into (non-owning). Needed for <code>component_register_managed</code> and <code>component_lookup_type</code>; defaults to none.</p>

#### `attach_world` {#function-attach-world}

<div class="function-declaration" id="function-attach-world-1"><pre><code><span class="k">void</span> attach_world(
    <a href="../../ecs/world/world.html">ecs::world</a> *world);</code></pre></div>

<p>The world scripts act on (non-owning; <code>nullptr</code> detaches). Registers the <code><a href="../script-component/script-component.html">script_component</a></code> observers on it and adopts the script sets its entities already carry (queuing their creates in <code>PLAY</code> mode); the owner must <code><a href="#function-destroy-all-1">destroy_all</a>()</code> before destroying the world. Swapping to another world destroys the live instances of the old one without OnDestroy (<code>WARN</code> when any exist): instances are bound to the ids of the world they were created on.</p>

#### `begin_query_scope` {#function-begin-query-scope}

<div class="function-declaration" id="function-begin-query-scope-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt; begin_query_scope(
    std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-u32">u32</a>&gt; typeIds,
    <a href="../native-api/conjure-query-scope.html">conjure_query_scope</a> &amp;out);</code></pre></div>

<p>Opens a scope over the archetypes holding every type: locks the world, fills the chunk views (columns in <code>typeIds</code> order, <code>null</code> for an absent id), bumps the change versions of the written columns. <code>WRONG_THREAD</code>, <code>NOT_IN_PHASE</code>, <code>NOT_INITIALISED</code>, <code>INVALID_ARGUMENT</code> (no world, bad id; id 0 is never valid). Allocation-free once warm: the <code><a href="../../ecs/query/query.html">ecs::query</a></code> behind a component set is cached for the life of the attached world (<code><a href="#function-cached-query-count-1">cached_query_count</a>()</code>) and finished scope states are reused, so the <code>scripting.update_10k_dod</code> gate of holds.</p>

#### `cached_query_count` {#function-cached-query-count}

<div class="function-declaration" id="function-cached-query-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> cached_query_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Distinct component sets <code>begin_query_scope</code> has cached a query for since the world was attached.</p>

#### `classes` {#function-classes}

<div class="function-declaration" id="function-classes-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../script-class-registry/script-class-registry.html">script_class_registry</a> &amp;classes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `classes_mutable` {#function-classes-mutable}

<div class="function-declaration" id="function-classes-mutable-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../script-class-registry/script-class-registry.html">script_class_registry</a> &amp;classes_mutable() <span class="k">noexcept</span>;</code></pre></div>

<p>The registry the native <code>script_register_class</code>/field entries fill (staging while a load runs).</p>

#### `clone_set_for_copy` {#function-clone-set-for-copy}

<div class="function-declaration" id="function-clone-set-for-copy-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> clone_set_for_copy(
    <a href="../../core/types.html#symbol-u32">u32</a> handle);</code></pre></div>

<p><code>ops.copy</code>: a deep copy of the set for a new component instance (owner unknown until <code>ADDED</code>).</p>

#### `collect_entry_fields` {#function-collect-entry-fields}

<div class="function-declaration" id="function-collect-entry-fields-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../core/json/document/json-document.html">core::json::document</a>&gt; collect_entry_fields(
    <a href="../script-component/script-entry-id.html">script_entry_id</a> id);</code></pre></div>

<p>The entry&#x27;s fields; for a live entry the JSON is refreshed from the instance first (unknown JSON keys preserved). <code>INSTANCE_NOT_FOUND</code>.</p>

#### `current_phase` {#function-current-phase}

<div class="function-declaration" id="function-current-phase-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../native-api.html#symbol-script-phase">script_phase</a> current_phase() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Phase being run, or <code>COUNT</code> outside a script phase (<code>query_begin</code> is refused with <code>NOT_IN_PHASE</code> then; the bindings entries check the thread only).</p>

#### `deferred_commands` {#function-deferred-commands}

<div class="function-declaration" id="function-deferred-commands-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../ecs/command-buffer/command-buffer.html">ecs::command_buffer</a> &amp;deferred_commands() <span class="k">noexcept</span>;</code></pre></div>

<p>The buffer bindings record structural requests into while <code>world::is_locked()</code>; flushed at <code>query_end</code> and at the phase-end force-close, when the world is not locked by anything else then. Valid while a world is attached. An id reserved here is passed to <code>note_deferred_reserved</code> / <code>note_deferred_instance</code> so later requests on it are recorded too.</p>

#### `deserialize_entity_scripts` {#function-deserialize-entity-scripts}

<div class="function-declaration" id="function-deserialize-entity-scripts-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; deserialize_entity_scripts(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity,
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;scripts);</code></pre></div>

<p>Replaces the entity&#x27;s scripts from the <code>serialize_entity_scripts</code> form (live instances destroyed first, new ones queued in <code>PLAY</code> mode). An entry with &quot;created&quot; is recreated with OnReload instead of OnCreate, one with &quot;started&quot; without OnStart; without them (older JSON) both run. <code>INVALID_ARGUMENT</code> for malformed JSON.</p>

#### `destroy_all` {#function-destroy-all}

<div class="function-declaration" id="function-destroy-all-1"><pre><code><span class="k">void</span> destroy_all(
    <span class="k">bool</span> callOnDestroy);</code></pre></div>

<p>Destroys every live instance (OnDestroy when asked) and clears the pending queues; entries keep their JSON with <code>INVALID_SLOT</code> and lose their <code>CREATED</code>, <code>STARTED</code> and <code>FAULTED</code> flags.</p>

#### `dispatch_collision_events` {#function-dispatch-collision-events}

<div class="function-declaration" id="function-dispatch-collision-events-1"><pre><code><span class="k">void</span> dispatch_collision_events(
    std::span&lt;<span class="k">const</span> <a href="collision-event.html">collision_event</a>&gt; events);</code></pre></div>

<p>Builds one <code><a href="../native-api/conjure-collision-record.html">conjure_collision_record</a></code> per participating entry whose class overrides the callback and sends them in one <code>dispatch_collisions</code> call. The record buffer is kept between calls, so a warm dispatch allocates nothing.</p>

#### `end_query_scope` {#function-end-query-scope}

<div class="function-declaration" id="function-end-query-scope-1"><pre><code><span class="k">void</span> end_query_scope(
    <a href="../../core/types.html#symbol-u32">u32</a> scopeId);</code></pre></div>

<p>Closes a scope (unknown ids are ignored with a <code>DEBUG</code> log) and flushes <code><a href="#function-deferred-commands-1">deferred_commands</a>()</code> once no scope stays open.</p>

#### `entity_name` {#function-entity-name}

<div class="function-declaration" id="function-entity-name-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view entity_name(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The remembered name of a live entity, or &quot;&quot;.</p>

#### `entity_name_count` {#function-entity-name-count}

<div class="function-declaration" id="function-entity-name-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> entity_name_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Entities with a remembered name (diagnostics and tests).</p>

#### `entry` {#function-entry}

<div class="function-declaration" id="function-entry-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../script-component/script-entry.html">script_entry</a> *entry(
    <a href="../script-component/script-entry-id.html">script_entry_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The entry behind an id, or <code>nullptr</code>.</p>

#### `entry_of_slot` {#function-entry-of-slot}

<div class="function-declaration" id="function-entry-of-slot-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../script-component/script-entry-id.html">script_entry_id</a> entry_of_slot(
    <a href="../../core/types.html#symbol-i32">i32</a> slot) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The entry a managed slot belongs to, or an invalid id.</p>

#### `find_entity_by_name` {#function-find-entity-by-name}

<div class="function-declaration" id="function-find-entity-by-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../ecs/entity/entity.html">ecs::entity</a> find_entity_by_name(
    std::string_view name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The first live entity remembered under <code>name</code>, or <code>NULL_ENTITY</code>.</p>

#### `flush_pending` {#function-flush-pending}

<div class="function-declaration" id="function-flush-pending-1"><pre><code><span class="k">void</span> flush_pending();</code></pre></div>

<p>Processes the pending destroys (OnDestroy, entity destruction) and creates (one <code>create_instances</code> call: OnCreate, or OnReload for an entry a hot reload recreates) until the queues are empty or <code>MAX_FLUSH_ITERATIONS</code> passed.</p>

#### `frame_block` {#function-frame-block}

<div class="function-declaration" id="function-frame-block-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../native-api/conjure-frame-block.html">conjure_frame_block</a> &amp;frame_block() <span class="k">noexcept</span>;</code></pre></div>

#### `free_set_from_destruct` {#function-free-set-from-destruct}

<div class="function-declaration" id="function-free-set-from-destruct-1"><pre><code><span class="k">void</span> free_set_from_destruct(
    <a href="../../core/types.html#symbol-u32">u32</a> handle);</code></pre></div>

<p><code>ops.destruct</code>: frees the set; live slots are detached and their OnDestroy deferred.</p>

#### `host` {#function-host}

<div class="function-declaration" id="function-host-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../script-host/script-host.html">script_host</a> &amp;host() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-host-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../script-host/script-host.html">script_host</a> &amp;host() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `initialise` {#function-initialise}

<div class="function-declaration" id="function-initialise-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; initialise(
    <span class="k">const</span> <a href="script-runtime-config.html">script_runtime_config</a> &amp;config);</code></pre></div>

<p>Bindings must have filled <code><a href="#function-native-api-1">native_api</a>()</code> before this call; the core entries are filled here. Initialises the host and loads <code>config.gameAssemblyPath</code> when set. <code>A</code> host start or table validation failure leaves the runtime <code>DISABLED</code>: entries keep their JSON with <code>INVALID_SLOT</code> and every phase is a no-op. <code>A</code> failure to load <code>config.gameAssemblyPath</code> is returned (<code>ASSEMBLY_LOAD_FAILED</code>) but leaves the runtime <code>READY</code> without an assembly (scenes still load); call <code>load_game_assembly</code> again once it is built.</p>

#### `is_assembly_loaded` {#function-is-assembly-loaded}

<div class="function-declaration" id="function-is-assembly-loaded-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_assembly_loaded() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_deferred_instance` {#function-is-deferred-instance}

<div class="function-declaration" id="function-is-deferred-instance-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_deferred_instance(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for a root <code>note_deferred_instance</code> remembered that no flush has applied yet. Such a root takes component, script and destroy requests like any reserved id; <code>record_reserved_remove</code> cannot tell what its archive holds, so it always records for it.</p>

#### `is_deferred_reserved` {#function-is-deferred-reserved}

<div class="function-declaration" id="function-is-deferred-reserved-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_deferred_reserved(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for an id <code>note_deferred_reserved</code> or <code>note_deferred_instance</code> remembered that no flush of <code><a href="#function-deferred-commands-1">deferred_commands</a>()</code> has applied yet (the world&#x27;s <code>is_alive</code> answers <code>false</code> for it).</p>

#### `load_game_assembly` {#function-load-game-assembly}

<div class="function-declaration" id="function-load-game-assembly-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; load_game_assembly(
    <span class="k">const</span> std::filesystem::path &amp;assemblyPath);</code></pre></div>

<p>Loads the game assembly: new load context, classes staged then committed, every entry whose class now exists is created in <code>PLAY</code> mode. With an assembly already loaded this is a reload. <code>NOT_INITIALISED</code>, <code>ASSEMBLY_LOAD_FAILED</code> (nothing changes).</p>

#### `mark_faulted` {#function-mark-faulted}

<div class="function-declaration" id="function-mark-faulted-1"><pre><code><span class="k">void</span> mark_faulted(
    <a href="../../core/types.html#symbol-i32">i32</a> slot,
    <a href="../native-api.html#symbol-script-callback">script_callback</a> callback);</code></pre></div>

<p><code>script_mark_faulted</code>: records the <code>FAULTED</code> flag on the entry of a slot with a <code>WARN</code> (an OnCreate or OnReload fault disables every callback of the instance, any other callback is disabled after three consecutive faults). <code>A</code> fault reported inside <code>create_instances</code>, before the slot is mapped, is applied and logged once the slot is.</p>

#### `mode` {#function-mode}

<div class="function-declaration" id="function-mode-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../script-runtime.html#symbol-script-mode">script_mode</a> mode() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `native_api` {#function-native-api}

<div class="function-declaration" id="function-native-api-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../native-api/conjure-native-api.html">conjure_native_api</a> &amp;native_api() <span class="k">noexcept</span>;</code></pre></div>

#### `note_deferred_instance` {#function-note-deferred-instance}

<div class="function-declaration" id="function-note-deferred-instance-1"><pre><code><span class="k">void</span> note_deferred_instance(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> root);</code></pre></div>

<p>Remembers the root <code><a href="#function-deferred-commands-1">deferred_commands</a>()</code>.<code>instantiate()</code> returned, until the flush. Its archive gives it components the runtime does not see, so <code>record_reserved_remove</code> always records for it. <code>A</code> <code>null</code> id is ignored.</p>

#### `note_deferred_reserved` {#function-note-deferred-reserved}

<div class="function-declaration" id="function-note-deferred-reserved-1"><pre><code><span class="k">void</span> note_deferred_reserved(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity,
    std::span&lt;<span class="k">const</span> ecs::component_id&gt; recorded = {});</code></pre></div>

<p>Remembers an id <code>create_entity()</code> of <code><a href="#function-deferred-commands-1">deferred_commands</a>()</code> reserved, until the flush that makes it alive or releases it. <code>recorded</code> are the components the caller already added to it in the buffer. <code>A</code> <code>null</code> id is ignored; noting an id twice adds the components to the first note.</p>

#### `note_mirror_mismatch` {#function-note-mirror-mismatch}

<div class="function-declaration" id="function-note-mirror-mismatch-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> note_mirror_mismatch(
    <a href="../../core/types.html#symbol-u64">u64</a> nameHash);</code></pre></div>

<p><code>component_lookup_type</code>: <code>true</code> the first time a mirror name mismatches since the last assembly load, shutdown or world attach (the caller logs it then); <code>false</code> after that.</p>

#### `on_component_added` {#function-on-component-added}

<div class="function-declaration" id="function-on-component-added-1"><pre><code><span class="k">void</span> on_component_added(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity,
    <a href="../script-component/script-component.html">script_component</a> &amp;component);</code></pre></div>

<p>Called by the <code>ADDED</code> observer: adopts the set (owner = entity; for a zero handle the scripts <code>add_script</code> staged on a reserved id, or else an empty set) and queues creates in <code>PLAY</code> mode.</p>

#### `on_component_removed` {#function-on-component-removed}

<div class="function-declaration" id="function-on-component-removed-1"><pre><code><span class="k">void</span> on_component_removed(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity,
    <span class="k">const</span> <a href="../script-component/script-component.html">script_component</a> &amp;component);</code></pre></div>

<p>Called by the <code>REMOVED</code> observer on <code><a href="../script-component/script-component.html">script_component</a></code> (and by <code>ops.destruct</code> for sets freed with live instances): detaches the set&#x27;s slots and defers their OnDestroy to the next flush.</p>

#### `open_query_scope_count` {#function-open-query-scope-count}

<div class="function-declaration" id="function-open-query-scope-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> open_query_scope_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `read_set_json` {#function-read-set-json}

<div class="function-declaration" id="function-read-set-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt; read_set_json(
    <a href="../../core/types.html#symbol-u32">u32</a> handle,
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;in);</code></pre></div>

<p><code>serializer.readJson</code>: replaces the entries of a set from the {&quot;scripts&quot;:[...]} object (&quot;started&quot; and &quot;created&quot; restore their flags, as in <code>deserialize_entity_scripts</code>); creates the set when <code>handle</code> is <code>INVALID_SET_HANDLE</code> and returns the handle to store.</p>

#### `record_reserved_add` {#function-record-reserved-add}

<div class="function-declaration" id="function-record-reserved-add-1"><pre><code><span class="k">bool</span> record_reserved_add(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity,
    ecs::component_id id,
    std::span&lt;<span class="k">const</span> std::byte&gt; payload);</code></pre></div>

<p><code>component_add</code> on a reserved id: records <code>add_raw</code> in <code><a href="#function-deferred-commands-1">deferred_commands</a>()</code> and returns <code>true</code>; <code>false</code> (nothing recorded) for any other id or an unregistered component id. At the flush a second add of a component with a value overwrites the first, as a set would; an add without a payload gives the default value only when no earlier add on the id carried one, and otherwise keeps that value.</p>

#### `record_reserved_remove` {#function-record-reserved-remove}

<div class="function-declaration" id="function-record-reserved-remove-1"><pre><code><span class="k">bool</span> record_reserved_remove(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity,
    ecs::component_id id);</code></pre></div>

<p><code>component_remove</code> on a reserved id: records <code>remove_raw</code> in <code><a href="#function-deferred-commands-1">deferred_commands</a>()</code> (cancelling the earlier adds and sets of the component on it) and returns <code>true</code> when the commands recorded so far give it the component, and always for an instantiation root. False, with nothing recorded, when there is nothing to remove, for any other id and for an unregistered component id. Removing <code><a href="../script-component/script-component.html">script_component</a></code> also drops the scripts <code>add_script</code> staged on the id.</p>

#### `record_reserved_set` {#function-record-reserved-set}

<div class="function-declaration" id="function-record-reserved-set-1"><pre><code><span class="k">bool</span> record_reserved_set(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity,
    ecs::component_id id,
    std::span&lt;<span class="k">const</span> std::byte&gt; payload);</code></pre></div>

<p><code>component_set</code> on a reserved id: records <code>set_raw</code> in <code><a href="#function-deferred-commands-1">deferred_commands</a>()</code> and returns <code>true</code>; <code>false</code> for any other id or an unregistered component id. <code>A</code> set the flush finds no component for (no add of the component on the id is left at the flush: none was recorded, or a later remove took it) is skipped and counted in the flush&#x27;s <code>DEBUG</code> line. <code>A</code> tag has no value to write: nothing is recorded and the call still returns <code>true</code>.</p>

#### `registry` {#function-registry}

<div class="function-declaration" id="function-registry-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../ecs/component-registry/component-registry.html">ecs::component_registry</a> *registry() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `reload_game_assembly` {#function-reload-game-assembly}

<div class="function-declaration" id="function-reload-game-assembly-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; reload_game_assembly(
    <span class="k">const</span> std::filesystem::path &amp;assemblyPath);</code></pre></div>

<p>Hot reload: load new -&gt; collect fields -&gt; destroy -&gt; unload -&gt; swap registries -&gt; recreate. OnCreate runs once per entity lifetime: an entry whose OnCreate returned (the <code>CREATED</code> flag) is recreated with <code>CREATE_FLAG_ALREADY_CREATED</code> and runs OnReload after its fields are restored, an entry whose create was still queued, or whose OnCreate threw, runs OnCreate, and OnStart does not run again (an entry whose OnCreate threw never attempted it: it runs after that OnCreate). <code>A</code> failed load leaves the running game untouched; a leaked load context is a <code>WARN</code> (<code>ASSEMBLY_LEAKED</code> is logged, not returned).</p>

#### `remap_entity_fields` {#function-remap-entity-fields}

<div class="function-declaration" id="function-remap-entity-fields-1"><pre><code><span class="k">void</span> remap_entity_fields(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity,
    <span class="k">const</span> std::function&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>(ecs::entity)&gt; &amp;remap);</code></pre></div>

<p>Rewrites every <code>ENTITY</code> field (and <code>LIST</code> of <code>ENTITY</code>) of the entity&#x27;s scripts through <code>remap</code> using the class schema and re-applies the fields when live.</p>

#### `remap_set_entities` {#function-remap-set-entities}

<div class="function-declaration" id="function-remap-set-entities-1"><pre><code><span class="k">void</span> remap_set_entities(
    <a href="../../core/types.html#symbol-u32">u32</a> handle,
    <span class="k">const</span> <a href="../../ecs/entity/entity-remap.html">ecs::entity_remap</a> &amp;map);</code></pre></div>

<p><code>serializer.remapEntities</code>: archive-local references become real through <code>map</code>; real references that are targets of <code>map</code> become archive-local (so the same hook serves capture and instantiation).</p>

#### `remove_script` {#function-remove-script}

<div class="function-declaration" id="function-remove-script-1"><pre><code><span class="k">void</span> remove_script(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity,
    <a href="../script-component/script-entry-id.html">script_entry_id</a> id);</code></pre></div>

<p>Destroys the entry&#x27;s instance now (OnDestroy, entity alive) and removes the entry. Ignored (<code>DEBUG</code> log) when the id does not belong to the entity&#x27;s script set or no world is attached.</p>

#### `request_entity_destroy` {#function-request-entity-destroy}

<div class="function-declaration" id="function-request-entity-destroy-1"><pre><code><span class="k">void</span> request_entity_destroy(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity);</code></pre></div>

<p><code>Entity.Destroy()</code> from <code>C</code>#: an entity with live scripts is destroyed at the next flush, after OnDestroy ran with the entity alive (even when requested inside a query scope); one without is destroyed now, or through <code><a href="#function-deferred-commands-1">deferred_commands</a>()</code> while the world is locked. An id <code><a href="#function-is-deferred-reserved-1">is_deferred_reserved</a>()</code> answers <code>true</code> for (it has no instance yet) records the destroy in <code><a href="#function-deferred-commands-1">deferred_commands</a>()</code>, where it wins over every command on the id: the id never becomes alive. On a root <code><a href="#function-is-deferred-instance-1">is_deferred_instance</a>()</code> answers <code>true</code> for, the flush cancels the whole copy, so the archive&#x27;s inner entities never become alive either. Anything else that is not alive is ignored.</p>

#### `run_phase` {#function-run-phase}

<div class="function-declaration" id="function-run-phase-1"><pre><code><span class="k">void</span> run_phase(
    <a href="../native-api.html#symbol-script-phase">script_phase</a> phase,
    <a href="../../core/types.html#symbol-f32">f32</a> deltaTime);</code></pre></div>

<p>One managed call for the phase, then the open query scopes are force-closed (<code>WARN</code>), the deferred commands flushed and <code><a href="#function-flush-pending-1">flush_pending</a>()</code> run while the phase is still current (so OnCreate/OnDestroy of that flush may open query scopes). Outside <code>run_phase</code> and <code>dispatch_collision_events</code> (<code>FIXED_UPDATE</code>) a query scope is refused with <code>NOT_IN_PHASE</code>: the flushes of <code>frame_begin</code>, <code>frame_end</code> and <code>set_mode</code> run no phase.</p>

#### `script_component_id` {#function-script-component-id}

<div class="function-declaration" id="function-script-component-id-1"><pre><code>[[<span class="na">nodiscard</span>]] ecs::component_id script_component_id() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `scripts_of` {#function-scripts-of}

<div class="function-declaration" id="function-scripts-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="../script-component/script-entry-id.html">script_entry_id</a>&gt; scripts_of(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity) <span class="k">const</span>;</code></pre></div>

<p>Every entry id of the entity in set order (empty without scripts).</p>

#### `serialize_entity_scripts` {#function-serialize-entity-scripts}

<div class="function-declaration" id="function-serialize-entity-scripts-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../core/json/document/json-document.html">core::json::document</a>&gt; serialize_entity_scripts(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity);</code></pre></div>

<p>{&quot;scripts&quot;:[{&quot;class&quot;,&quot;hash&quot;,&quot;fields&quot;}]} of the entity&#x27;s set (live fields refreshed first), each entry with &quot;started&quot;: <code>true</code> and &quot;created&quot;: <code>true</code> when its <code>STARTED</code> and <code>CREATED</code> flags are set; {&quot;scripts&quot;:[]} for an entity without scripts. <code>ENTITY_NOT_ALIVE</code> through the ecs category.</p>

#### `set_collision_source` {#function-set-collision-source}

<div class="function-declaration" id="function-set-collision-source-1"><pre><code><span class="k">void</span> set_collision_source(
    std::function&lt;std::span&lt;<span class="k">const</span> <a href="collision-event.html">collision_event</a>&gt;()&gt; source);</code></pre></div>

<p>The source <code>scripting.collision_dispatch</code> reads (the engine binds <code>physics_world::contact_events</code>).</p>

#### `set_field` {#function-set-field}

<div class="function-declaration" id="function-set-field-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_field(
    <a href="../script-component/script-entry-id.html">script_entry_id</a> id,
    <a href="../../core/types.html#symbol-u64">u64</a> fieldHash,
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;value);</code></pre></div>

<p>Rewrites one field&#x27;s JSON; in <code>PLAY</code> mode also applies it to the live instance as a single-field blob. <code>INSTANCE_NOT_FOUND</code>, <code>CLASS_NOT_FOUND</code>, <code>FIELD_NOT_FOUND</code>, <code>INVALID_ARGUMENT</code> (type mismatch).</p>

#### `set_mode` {#function-set-mode}

<div class="function-declaration" id="function-set-mode-1"><pre><code><span class="k">void</span> set_mode(
    <a href="../script-runtime.html#symbol-script-mode">script_mode</a> mode);</code></pre></div>

<p><code>EDIT</code> -&gt; <code>PLAY</code> creates every instance of the attached world&#x27;s entities (and flushes); <code>PLAY</code> -&gt; <code>EDIT</code> destroys them with OnDestroy and clears their <code>CREATED</code>, <code>STARTED</code> and <code>FAULTED</code> flags, so the next <code>PLAY</code> runs OnCreate and OnStart again (the created and started flags are for hot reload only).</p>

#### `shutdown` {#function-shutdown}

<div class="function-declaration" id="function-shutdown-1"><pre><code><span class="k">void</span> shutdown();</code></pre></div>

<p><code><a href="#function-destroy-all-1">destroy_all</a>(true)</code>, unload, host shutdown. The runtime may be initialised again.</p>

#### `stats` {#function-stats}

<div class="function-declaration" id="function-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="script-runtime-stats.html">script_runtime_stats</a> stats() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `status` {#function-status}

<div class="function-declaration" id="function-status-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../script-host.html#symbol-script-host-status">script_host_status</a> status() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `store` {#function-store}

<div class="function-declaration" id="function-store-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../script-component/script-store.html">script_store</a> &amp;store() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-store-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../script-component/script-store.html">script_store</a> &amp;store() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `void` {#function-void}

<div class="function-declaration" id="function-void-1"><pre><code><a href="../../core/delegate/delegate-return-args.html">core::delegate</a>&lt;<span class="k">void</span>()&gt; on_reloaded =;</code></pre></div>

<p>Fires after a successful reload (editor refreshes inspectors).</p>

#### `world` {#function-world}

<div class="function-declaration" id="function-world-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../ecs/world/world.html">ecs::world</a> *world() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `write_set_json` {#function-write-set-json}

<div class="function-declaration" id="function-write-set-json-1"><pre><code><span class="k">void</span> write_set_json(
    <a href="../../core/types.html#symbol-u32">u32</a> handle,
    <a href="../../core/json/writer/json-writer.html">core::json::writer</a> &amp;out);</code></pre></div>

<p><code>serializer.writeJson</code>: the {&quot;scripts&quot;:[...]} object of a set (live fields refreshed first; the <code>serialize_entity_scripts</code> form, &quot;started&quot; and &quot;created&quot; included).</p>

### Private

#### `adopt_world_sets` {#function-adopt-world-sets}

<div class="function-declaration" id="function-adopt-world-sets-1"><pre><code><span class="k">void</span> adopt_world_sets(
    <span class="k">bool</span> queueCreates);</code></pre></div>

#### `apply_entry_fields` {#function-apply-entry-fields}

<div class="function-declaration" id="function-apply-entry-fields-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; apply_entry_fields(
    <a href="../script-component/script-entry.html">script_entry</a> &amp;entryValue,
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;fields);</code></pre></div>

#### `apply_entry_fields_with` {#function-apply-entry-fields-with}

<div class="function-declaration" id="function-apply-entry-fields-with-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; apply_entry_fields_with(
    <a href="../script-component/script-entry.html">script_entry</a> &amp;entryValue,
    <span class="k">const</span> <a href="../script-class-registry/script-class-info.html">script_class_info</a> &amp;info,
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;fields);</code></pre></div>

#### `cached_query` {#function-cached-query}

<div class="function-declaration" id="function-cached-query-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../ecs/query/query.html">ecs::query</a> &amp;cached_query(
    <span class="k">const</span> <a href="../script-runtime.html#symbol-script-runtime-query-cache-key">query_cache_key</a> &amp;key,
    <span class="k">const</span> <a href="../../ecs/query/query-desc.html">ecs::query_desc</a> &amp;desc);</code></pre></div>

#### `destroy_slots` {#function-destroy-slots}

<div class="function-declaration" id="function-destroy-slots-1"><pre><code><span class="k">void</span> destroy_slots(
    std::span&lt;<span class="k">const</span> <a href="../../core/types.html#symbol-i32">i32</a>&gt; slots,
    <span class="k">bool</span> callOnDestroy);</code></pre></div>

#### `drop_deferred_reservations` {#function-drop-deferred-reservations}

<div class="function-declaration" id="function-drop-deferred-reservations-1"><pre><code><span class="k">void</span> drop_deferred_reservations();</code></pre></div>

#### `enter_phase` {#function-enter-phase}

<div class="function-declaration" id="function-enter-phase-1"><pre><code><span class="k">void</span> enter_phase(
    <a href="../native-api.html#symbol-script-phase">script_phase</a> phase) <span class="k">noexcept</span>;</code></pre></div>

#### `find_reservation` {#function-find-reservation}

<div class="function-declaration" id="function-find-reservation-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="script-runtime-deferred-reservation.html">deferred_reservation</a> *find_reservation(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity) <span class="k">noexcept</span>;</code></pre></div>

#### `flush_deferred_commands` {#function-flush-deferred-commands}

<div class="function-declaration" id="function-flush-deferred-commands-1"><pre><code><span class="k">void</span> flush_deferred_commands();</code></pre></div>

#### `force_close_query_scopes` {#function-force-close-query-scopes}

<div class="function-declaration" id="function-force-close-query-scopes-1"><pre><code><span class="k">void</span> force_close_query_scopes();</code></pre></div>

#### `is_ready` {#function-is-ready}

<div class="function-declaration" id="function-is-ready-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_ready() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `leave_phase` {#function-leave-phase}

<div class="function-declaration" id="function-leave-phase-1"><pre><code><span class="k">void</span> leave_phase() <span class="k">noexcept</span>;</code></pre></div>

#### `log_faulted` {#function-log-faulted}

<div class="function-declaration" id="function-log-faulted-1"><pre><code><span class="k">void</span> log_faulted(
    <a href="../../core/types.html#symbol-i32">i32</a> slot,
    <a href="../native-api.html#symbol-script-callback">script_callback</a> callback,
    <span class="k">const</span> <a href="../script-component/script-entry.html">script_entry</a> &amp;entryValue) <span class="k">const</span>;</code></pre></div>

#### `mark_started_entries` {#function-mark-started-entries}

<div class="function-declaration" id="function-mark-started-entries-1"><pre><code><span class="k">void</span> mark_started_entries();</code></pre></div>

#### `process_pending_creates` {#function-process-pending-creates}

<div class="function-declaration" id="function-process-pending-creates-1"><pre><code><span class="k">void</span> process_pending_creates();</code></pre></div>

#### `process_pending_destroys` {#function-process-pending-destroys}

<div class="function-declaration" id="function-process-pending-destroys-1"><pre><code><span class="k">void</span> process_pending_destroys();</code></pre></div>

#### `queue_create` {#function-queue-create}

<div class="function-declaration" id="function-queue-create-1"><pre><code><span class="k">void</span> queue_create(
    <a href="../script-component/script-entry-id.html">script_entry_id</a> id);</code></pre></div>

#### `queue_creates_of_set` {#function-queue-creates-of-set}

<div class="function-declaration" id="function-queue-creates-of-set-1"><pre><code><span class="k">void</span> queue_creates_of_set(
    <a href="../../core/types.html#symbol-u32">u32</a> handle);</code></pre></div>

#### `refresh_live_fields` {#function-refresh-live-fields}

<div class="function-declaration" id="function-refresh-live-fields-1"><pre><code><span class="k">void</span> refresh_live_fields(
    <a href="../script-component/script-entry.html">script_entry</a> &amp;entryValue);</code></pre></div>

#### `reload_internal` {#function-reload-internal}

<div class="function-declaration" id="function-reload-internal-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; reload_internal(
    <span class="k">const</span> std::filesystem::path &amp;assemblyPath);</code></pre></div>

#### `replace_set_entries` {#function-replace-set-entries}

<div class="function-declaration" id="function-replace-set-entries-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; replace_set_entries(
    <a href="../../core/types.html#symbol-u32">u32</a> handle,
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;in);</code></pre></div>

#### `settle_deferred_reservations` {#function-settle-deferred-reservations}

<div class="function-declaration" id="function-settle-deferred-reservations-1"><pre><code><span class="k">void</span> settle_deferred_reservations();</code></pre></div>

#### `stage_reserved_script` {#function-stage-reserved-script}

<div class="function-declaration" id="function-stage-reserved-script-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../script-component/script-entry-id.html">script_entry_id</a> stage_reserved_script(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity,
    <a href="script-runtime-deferred-reservation.html">deferred_reservation</a> &amp;reservation,
    <a href="../../core/types.html#symbol-u64">u64</a> classHash,
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;fields);</code></pre></div>

#### `take_scope_state` {#function-take-scope-state}

<div class="function-declaration" id="function-take-scope-state-1"><pre><code>[[<span class="na">nodiscard</span>]] std::unique_ptr&lt;<a href="script-runtime-query-scope-state.html">query_scope_state</a>&gt; take_scope_state(
    <a href="../../core/types.html#symbol-usize">usize</a> chunkCount);</code></pre></div>

#### `warn_missing_class_once` {#function-warn-missing-class-once}

<div class="function-declaration" id="function-warn-missing-class-once-1"><pre><code><span class="k">void</span> warn_missing_class_once(
    <a href="../../core/types.html#symbol-u64">u64</a> classHash);</code></pre></div>

#### `write_set_entries_json` {#function-write-set-entries-json}

<div class="function-declaration" id="function-write-set-entries-json-1"><pre><code><span class="k">void</span> write_set_entries_json(
    <a href="../script-component/script-set.html">script_set</a> &amp;set,
    <a href="../../core/json/writer/json-writer.html">core::json::writer</a> &amp;out);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="function-initialize"></a>
<a id="function-initialize-1"></a>
<!-- doc-legacy-anchors:end -->
