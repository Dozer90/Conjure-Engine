---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [world.h](../world.md)

# world
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/world.h` |
| Source File Path | `engine/src/ecs/world.cpp` |
| API ↗ | [Archives and serialisation](../../../../api/ecs/archives-and-serialization.md), [Command buffers and observers](../../../../api/ecs/command-buffers-and-observers.md), [Entities and components](../../../../api/ecs/entities-and-components.md), [Entities, components and systems](../../../../api/ecs/index.md), [Queries, views and systems](../../../../api/ecs/queries-views-and-systems.md) |

## Syntax

```cpp
class CONJURE_API world
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> world(
    <span class="k">const</span> <a href="../component-registry/component-registry.html">component_registry</a> &amp;registry,
    <a href="world-desc.html">world_desc</a> desc = {});</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted. Move constructors have been deleted. Move assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~world();</code></td><td>Destroys every entity without firing observers and returns every chunk.</td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">FLUSH_<wbr>INLINE_<wbr>PAYLOADS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">4</span></code></td><td>Payload records a flush plan entry stores inline: a spawn with up to this many add/set payloads costs the plan no heap allocation.</td></tr>
<tr><td>private</td><td><code class="signature">mAliveCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mAllocator</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../world.html">entity_<wbr>allocator</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Archetype<wbr>By<wbr>Mask</code></td><td><code class="signature">std::<wbr>unordered_<wbr>map&lt;<wbr>component_<wbr>mask,<wbr> <a href="../archetype/archetype.html">archetype</a> *,<wbr> <a href="../component/component-mask-hasher.html">component_<wbr>mask::<wbr>hasher</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Archetype<wbr>Pointers</code></td><td><code class="signature">std::vector&lt;<a href="../archetype/archetype.html">archetype</a> *&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mArchetypes</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../archetype/archetype.html">archetype</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mChunkPool</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../world.html">chunk_<wbr>pool</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDeferred</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../command-buffer/command-buffer.html">command_<wbr>buffer</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Deferred<wbr>Flushing</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../command-buffer/command-buffer.html">command_<wbr>buffer</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Empty<wbr>Archetype</code></td><td><code class="signature"><a href="../archetype/archetype.html">archetype</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFlushPlan</code></td><td><code class="signature">std::vector&lt;<a href="world-flush-entry.html">flush_entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFlushSlots</code></td><td><code class="signature">std::vector&lt;<a href="world-flush-slot.html">flush_slot</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFlushStamp</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mInObserver</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLockDepth</code></td><td><code class="signature">std::atomic&lt;<a href="../../core/types.html#symbol-i32">i32</a>&gt;</code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mObservers</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../world.html">observer_<wbr>table</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOwnerThread</code></td><td><code class="signature">std::thread::id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Placed<wbr>Scratch</code></td><td><code class="signature">std::vector&lt;<a href="../entity/entity.html">entity</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRecords</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="world-entity-record.html">entity_<wbr>record</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRegistry</code></td><td><code class="signature"><span class="k">const</span> <a href="../component-registry/component-registry.html">component_<wbr>registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Structural<wbr>Depth</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mVersion</code></td><td><code class="signature">std::atomic&lt;<a href="../../core/types.html#symbol-u64">u64</a>&gt;</code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add` {#function-add}

<div class="function-declaration" id="function-add-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;ComponentType *&gt; add(
    <a href="../entity/entity.html">entity</a> target,
    ComponentType value = {});</code></pre></div>

<p>Adds a component with an initial value and fires <code>ADDED</code> observers once the value is in place. Returns the stored component: <code>nullptr</code> for a tag, and also when an <code>ADDED</code> observer&#x27;s deferred work (drained before add returns) removed the component again or destroyed the entity; check the pointer when observers may do that. <code>ENTITY_NOT_ALIVE</code>, <code>COMPONENT_ALREADY_PRESENT</code>, <code>UNKNOWN_COMPONENT</code>, structural errors.</p>

#### `add_raw` {#function-add-raw}

<div class="function-declaration" id="function-add-raw-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span> *&gt; add_raw(
    <a href="../entity/entity.html">entity</a> target,
    <a href="../component/component-id.html">component_id</a> id,
    std::span&lt;<span class="k">const</span> std::byte&gt; initial = {});</code></pre></div>

<p>Adds a component from bytes (<code>ops.copy</code> or memcpy; empty = default value) and returns the stored data: <code>nullptr</code> for a tag, and also when an <code>ADDED</code> observer&#x27;s deferred work removed the component again or destroyed the entity, as with <code><a href="#function-add-1">add</a>()</code>. Same errors as <code><a href="#function-add-1">add</a>()</code>.</p>

#### `archetypes` {#function-archetypes}

<div class="function-declaration" id="function-archetypes-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<a href="../archetype/archetype.html">archetype</a> *<span class="k">const</span>&gt; archetypes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Every archetype in creation order. Pointers stay valid for the <code><a href="world.html">world</a></code>&#x27;s lifetime.</p>

#### `bump_version` {#function-bump-version}

<div class="function-declaration" id="function-bump-version-1"><pre><code><a href="../../core/types.html#symbol-u64">u64</a> bump_version() <span class="k">noexcept</span>;</code></pre></div>

<p>Increments the version and returns the new value. Any thread.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear();</code></pre></div>

<p>Destroys every entity (firing observers) and returns every chunk, the parked chunk metadata and the structural scratch (flush plan, placement undo list, the deferred buffers&#x27; room) to the heap. Archetypes, observers and the id space are kept: destroyed ids come back with a bumped generation, so handles into the cleared <code><a href="world.html">world</a></code> stay stale and ids reserved by command buffers stay valid. Owner thread, unlocked.</p>

#### `create_entities` {#function-create-entities}

<div class="function-declaration" id="function-create-entities-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; create_entities(
    <span class="k">const</span> <a href="../component/component-mask.html">component_mask</a> &amp;mask,
    std::span&lt;<a href="../entity/entity.html">entity</a>&gt; out);</code></pre></div>

<p>Creates <code>out.size()</code> entities of one archetype chunk by chunk and writes their ids.</p>

#### `create_entity` {#function-create-entity}

<div class="function-declaration" id="function-create-entity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../entity/entity.html">entity</a>&gt; create_entity(
    <span class="k">const</span> <a href="../component/component-mask.html">component_mask</a> &amp;mask = {});</code></pre></div>

<p>Creates one entity in the archetype of <code>mask</code> (default-constructed components) and fires <code>ADDED</code> observers. <code>ARCHETYPE_TOO_LARGE</code>, <code>OUT_OF_MEMORY</code>, <code>ENTITY_LIMIT_REACHED</code>, <code>STRUCTURE_LOCKED</code>, <code>WRONG_THREAD</code>, <code>UNKNOWN_COMPONENT</code>.</p>

#### `create_query` {#function-create-query}

<div class="function-declaration" id="function-create-query-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../query/query.html">query</a> create_query(
    <span class="k">const</span> <a href="../query/query-desc.html">query_desc</a> &amp;desc);</code></pre></div>

<p>An untyped query.</p>

#### `destroy_entity` {#function-destroy-entity}

<div class="function-declaration" id="function-destroy-entity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; destroy_entity(
    <a href="../entity/entity.html">entity</a> target);</code></pre></div>

<p>Fires <code>REMOVED</code> and destroyed observers, destroys the components and frees the id. The deferred buffer observers filled is drained before returning. <code>ENTITY_NOT_ALIVE</code> for a dead or stale entity. Never cascades.</p>

#### `entity_count` {#function-entity-count}

<div class="function-declaration" id="function-entity-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> entity_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of alive entities.</p>

#### `flush` {#function-flush}

<div class="function-declaration" id="function-flush-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../command-buffer/flush-report.html">flush_report</a> flush(
    <a href="../command-buffer/command-buffer.html">command_buffer</a> &amp;commands);</code></pre></div>

<p>Applies a command buffer: one archetype move per touched entity, destroys last, observers for the mask delta only. Commands on dead or stale entities are skipped and counted. The buffer is empty afterwards. <code>A</code> structural violation leaves the buffer untouched and reports nothing applied.</p>

#### `get` {#function-get}

<div class="function-declaration" id="function-get-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] ComponentType *get(
    <a href="../entity/entity.html">entity</a> target) <span class="k">noexcept</span>;</code></pre></div>

<p>The component; a non-const <code>T</code> is for writing and stamps its chunk column, a const <code>T</code> never stamps. <code>nullptr</code> when dead or absent.</p>

<div class="function-declaration" id="function-get-2"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <span class="k">const</span> ComponentType *get(
    <a href="../entity/entity.html">entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The component for reading. <code>nullptr</code> when dead or absent. Never stamps.</p>

#### `get_raw` {#function-get-raw}

<div class="function-declaration" id="function-get-raw-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *get_raw(
    <a href="../entity/entity.html">entity</a> target,
    <a href="../component/component-id.html">component_id</a> id,
    <span class="k">bool</span> forWrite) <span class="k">noexcept</span>;</code></pre></div>

<p>Component data by id; <code>forWrite</code> stamps the column. <code>nullptr</code> when dead, absent or a tag.</p>

<div class="function-declaration" id="function-get-raw-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <span class="k">void</span> *get_raw(
    <a href="../entity/entity.html">entity</a> target,
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Read-only component data by id. <code>nullptr</code> when dead, absent or a tag. Safe from grain jobs while the <code><a href="world.html">world</a></code> is locked (an <code><a href="iteration-scope.html">iteration_scope</a></code>, a system stage, <code>parallel_for_chunks</code>): no structural change can move a row meanwhile, so a job may read another entity&#x27;s component this way (the scene&#x27;s transform pass reads parent columns through it) as long as no grain is writing that column at the same time.</p>

#### `handle` {#function-handle}

<div class="function-declaration" id="function-handle-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../entity-handle/entity-handle.html">entity_handle</a> handle(
    <a href="../entity/entity.html">entity</a> target) <span class="k">noexcept</span>;</code></pre></div>

<p>An OOP handle to an entity (<code><a href="../entity-handle/entity-handle.html">entity_handle</a>.h</code>). Defined in <code><a href="../entity-handle/entity-handle.html">entity_handle</a>.h</code>.</p>

#### `has` {#function-has}

<div class="function-declaration" id="function-has-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <span class="k">bool</span> has(
    <a href="../entity/entity.html">entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the entity is alive and holds the component.</p>

#### `has_raw` {#function-has-raw}

<div class="function-declaration" id="function-has-raw-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_raw(
    <a href="../entity/entity.html">entity</a> target,
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the entity is alive and its archetype holds <code>id</code>.</p>

#### `instance_count` {#function-instance-count}

<div class="function-declaration" id="function-instance-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> instance_count(
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of alive entities holding <code>id</code>.</p>

#### `instantiate` {#function-instantiate}

<div class="function-declaration" id="function-instantiate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../entity/entity.html">entity</a>&gt; instantiate(
    <span class="k">const</span> std::shared_ptr&lt;<span class="k">const</span> <a href="../entity-archive/entity-archive.html">entity_archive</a>&gt; &amp;archive,
    <a href="../command-buffer/command-buffer.html">command_buffer</a> *commands = nullptr);</code></pre></div>

<p>Instantiates one copy of an archive and returns its root (entry 0). With a command buffer the copy is recorded and applied at flush; the root id is still returned now. The buffer must target this <code><a href="world.html">world</a></code> (<code>INVALID_ARGUMENT</code> otherwise, as <code>instantiate_batch</code>).</p>

#### `instantiate_batch` {#function-instantiate-batch}

<div class="function-declaration" id="function-instantiate-batch-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; instantiate_batch(
    <span class="k">const</span> std::shared_ptr&lt;<span class="k">const</span> <a href="../entity-archive/entity-archive.html">entity_archive</a>&gt; &amp;archive,
    std::span&lt;<a href="../entity/entity.html">entity</a>&gt; outRoots,
    <a href="../command-buffer/command-buffer.html">command_buffer</a> *commands = nullptr,
    std::vector&lt;<a href="../entity/entity.html">entity</a>&gt; *outEntities = nullptr);</code></pre></div>

<p>Instantiates <code>outRoots.size()</code> copies of an archive, placing entities directly in their final archetypes and rewiring internal entity references. <code>ARCHIVE_CORRUPT</code> before anything is created when the archive does not validate against this <code><a href="world.html">world</a></code>&#x27;s registry. When given, <code>outEntities</code> receives every created id, copy-major in entry order (copies x <code><a href="#function-entity-count-1">entity_count</a>()</code> entries, entries without components included); with a command buffer those are the ids reserved for the flush. After a failed call <code>outEntities</code> is empty. <code>INVALID_ARGUMENT</code> (asserted in Debug and Development) when the command buffer targets another <code><a href="world.html">world</a></code>; nothing is reserved then.</p>

#### `is_alive` {#function-is-alive}

<div class="function-declaration" id="function-is-alive-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_alive(
    <a href="../entity/entity.html">entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the entity is alive with this generation. Any thread while locked.</p>

#### `is_locked` {#function-is-locked}

<div class="function-declaration" id="function-is-locked-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_locked() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while at least one <code><a href="iteration-scope.html">iteration_scope</a></code> is alive.</p>

#### `lock_iteration` {#function-lock-iteration}

<div class="function-declaration" id="function-lock-iteration-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="iteration-scope.html">iteration_scope</a> lock_iteration() <span class="k">noexcept</span>;</code></pre></div>

<p>Locks the <code><a href="world.html">world</a></code> for iteration until the returned scope dies. Any thread.</p>

#### `mask_of` {#function-mask-of}

<div class="function-declaration" id="function-mask-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../component/component-mask.html">component_mask</a> mask_of(
    <a href="../entity/entity.html">entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The component set of an entity; empty for a dead one.</p>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::string &amp;name() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The <code><a href="world.html">world</a></code>&#x27;s name from <code><a href="world-desc.html">world_desc</a></code>.</p>

#### `observe` {#function-observe}

<div class="function-declaration" id="function-observe-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../observer/observer-id.html">observer_id</a> observe(
    <a href="../component/component-id.html">component_id</a> id,
    <a href="../observer.html#symbol-component-event">component_event</a> event,
    <a href="../observer.html#symbol-component-observer-fn">component_observer_fn</a> fn);</code></pre></div>

<p>Registers a component observer. Observers run on the owner thread inside the structural operation and may only enqueue into the deferred buffer they receive. Off the owner thread, an unregistered id, an event past <code>REMOVED</code> or an empty callable asserts and returns an invalid <code><a href="../observer/observer-id.html">observer_id</a></code>.</p>

#### `observe_destroyed` {#function-observe-destroyed}

<div class="function-declaration" id="function-observe-destroyed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../observer/observer-id.html">observer_id</a> observe_destroyed(
    <a href="../observer.html#symbol-entity-observer-fn">entity_observer_fn</a> fn);</code></pre></div>

<p>Registers an observer called before an entity is destroyed.</p>

#### `owner_thread` {#function-owner-thread}

<div class="function-declaration" id="function-owner-thread-1"><pre><code>[[<span class="na">nodiscard</span>]] std::thread::id owner_thread() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The thread structural operations must run on.</p>

#### `registry` {#function-registry}

<div class="function-declaration" id="function-registry-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../component-registry/component-registry.html">component_registry</a> &amp;registry() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The registry this <code><a href="world.html">world</a></code> was created on.</p>

#### `remove` {#function-remove}

<div class="function-declaration" id="function-remove-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; remove(
    <a href="../entity/entity.html">entity</a> target);</code></pre></div>

<p>Removes a component after firing <code>REMOVED</code> observers. <code>COMPONENT_NOT_PRESENT</code> when absent.</p>

#### `remove_from_all` {#function-remove-from-all}

<div class="function-declaration" id="function-remove-from-all-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; remove_from_all(
    <a href="../component/component-id.html">component_id</a> id);</code></pre></div>

<p>Removes <code>id</code> from every entity holding it (module unload, dynamic type redefinition).</p>

#### `remove_raw` {#function-remove-raw}

<div class="function-declaration" id="function-remove-raw-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; remove_raw(
    <a href="../entity/entity.html">entity</a> target,
    <a href="../component/component-id.html">component_id</a> id);</code></pre></div>

<p>Removes a component by id.</p>

#### `set` {#function-set}

<div class="function-declaration" id="function-set-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set(
    <a href="../entity/entity.html">entity</a> target,
    ComponentType value);</code></pre></div>

<p>Replaces the value of an existing component and stamps its column. Not structural. <code>ENTITY_NOT_ALIVE</code>, <code>COMPONENT_NOT_PRESENT</code>.</p>

#### `set_owner_thread` {#function-set-owner-thread}

<div class="function-declaration" id="function-set-owner-thread-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_owner_thread(
    std::thread::id thread);</code></pre></div>

<p>Hands ownership to another thread. Only while unlocked (<code>STRUCTURE_LOCKED</code> otherwise).</p>

#### `stats` {#function-stats}

<div class="function-declaration" id="function-stats-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="world-stats.html">world_stats</a> stats() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Size snapshot.</p>

#### `unobserve` {#function-unobserve}

<div class="function-declaration" id="function-unobserve-1"><pre><code><span class="k">void</span> unobserve(
    <a href="../observer/observer-id.html">observer_id</a> id);</code></pre></div>

<p>Removes an observer. Stale or unknown ids are ignored.</p>

#### `validate` {#function-validate}

<div class="function-declaration" id="function-validate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate() <span class="k">const</span>;</code></pre></div>

<p>Full invariant check: records and rows agree, chunk counts sum to archetype counts, only the last chunk of an archetype is partial, free-list indices are <code>FREE</code>, every archetype mask maps back to itself, column offsets are aligned and inside the block. The error message names the first violated invariant.</p>

#### `version` {#function-version}

<div class="function-declaration" id="function-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> version() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The current change-detection version (starts at 1, never decreases).</p>

#### `view` {#function-view}

<div class="function-declaration" id="function-view-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... ComponentTypes&gt; [[<span class="na">nodiscard</span>]] <a href="../view/view.html">ecs::view</a>&lt;ComponentTypes...&gt; view();</code></pre></div>

<p><code>A</code> typed view over this <code><a href="world.html">world</a></code> (<code>view.h</code>).</p>

### Private

#### `add_storage` {#function-add-storage}

<div class="function-declaration" id="function-add-storage-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span> *&gt; add_storage(
    <a href="../entity/entity.html">entity</a> target,
    <a href="../component/component-id.html">component_id</a> id);</code></pre></div>

#### `archetype_for` {#function-archetype-for}

<div class="function-declaration" id="function-archetype-for-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../archetype/archetype.html">archetype</a> *&gt; archetype_for(
    <span class="k">const</span> <a href="../component/component-mask.html">component_mask</a> &amp;mask);</code></pre></div>

#### `archetype_with` {#function-archetype-with}

<div class="function-declaration" id="function-archetype-with-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../archetype/archetype.html">archetype</a> *&gt; archetype_with(
    <a href="../archetype/archetype.html">archetype</a> &amp;source,
    <a href="../component/component-id.html">component_id</a> id,
    <span class="k">bool</span> added);</code></pre></div>

#### `begin_structural` {#function-begin-structural}

<div class="function-declaration" id="function-begin-structural-1"><pre><code><span class="k">void</span> begin_structural() <span class="k">noexcept</span>;</code></pre></div>

#### `check_structural` {#function-check-structural}

<div class="function-declaration" id="function-check-structural-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; check_structural() <span class="k">const</span>;</code></pre></div>

#### `construct_column_default` {#function-construct-column-default}

<div class="function-declaration" id="function-construct-column-default-1"><pre><code><span class="k">void</span> construct_column_default(
    <span class="k">const</span> <a href="../component/component-info.html">component_info</a> &amp;info,
    <span class="k">void</span> *destination,
    <a href="../../core/types.html#symbol-u32">u32</a> count) <span class="k">noexcept</span>;</code></pre></div>

#### `destroy_alive` {#function-destroy-alive}

<div class="function-declaration" id="function-destroy-alive-1"><pre><code><span class="k">void</span> destroy_alive(
    <a href="../entity/entity.html">entity</a> target,
    <a href="world-entity-record.html">entity_record</a> &amp;record,
    <span class="k">bool</span> fireObservers);</code></pre></div>

#### `destruct_column` {#function-destruct-column}

<div class="function-declaration" id="function-destruct-column-1"><pre><code><span class="k">void</span> destruct_column(
    <span class="k">const</span> <a href="../component/component-info.html">component_info</a> &amp;info,
    <span class="k">void</span> *object,
    <a href="../../core/types.html#symbol-u32">u32</a> count) <span class="k">noexcept</span>;</code></pre></div>

#### `discard_placed` {#function-discard-placed}

<div class="function-declaration" id="function-discard-placed-1"><pre><code><span class="k">void</span> discard_placed(
    <a href="../entity/entity.html">entity</a> target);</code></pre></div>

#### `drain_deferred` {#function-drain-deferred}

<div class="function-declaration" id="function-drain-deferred-1"><pre><code><span class="k">void</span> drain_deferred();</code></pre></div>

#### `end_structural` {#function-end-structural}

<div class="function-declaration" id="function-end-structural-1"><pre><code><span class="k">void</span> end_structural();</code></pre></div>

#### `ensure_records` {#function-ensure-records}

<div class="function-declaration" id="function-ensure-records-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; ensure_records(
    <a href="../../core/types.html#symbol-u32">u32</a> highestIndex);</code></pre></div>

#### `find_alive` {#function-find-alive}

<div class="function-declaration" id="function-find-alive-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="world-entity-record.html">entity_record</a> *find_alive(
    <a href="../entity/entity.html">entity</a> target) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-find-alive-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="world-entity-record.html">entity_record</a> *find_alive(
    <a href="../entity/entity.html">entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `finish_add` {#function-finish-add}

<div class="function-declaration" id="function-finish-add-1"><pre><code><span class="k">void</span> finish_add(
    <a href="../entity/entity.html">entity</a> target,
    <a href="../component/component-id.html">component_id</a> id);</code></pre></div>

#### `fire_added` {#function-fire-added}

<div class="function-declaration" id="function-fire-added-1"><pre><code><span class="k">void</span> fire_added(
    <a href="../entity/entity.html">entity</a> target,
    <span class="k">const</span> <a href="../component/component-mask.html">component_mask</a> &amp;added);</code></pre></div>

#### `fire_destroyed` {#function-fire-destroyed}

<div class="function-declaration" id="function-fire-destroyed-1"><pre><code><span class="k">void</span> fire_destroyed(
    <a href="../entity/entity.html">entity</a> target);</code></pre></div>

#### `fire_removed` {#function-fire-removed}

<div class="function-declaration" id="function-fire-removed-1"><pre><code><span class="k">void</span> fire_removed(
    <a href="../entity/entity.html">entity</a> target,
    <span class="k">const</span> <a href="../component/component-mask.html">component_mask</a> &amp;removed);</code></pre></div>

#### `flush_internal` {#function-flush-internal}

<div class="function-declaration" id="function-flush-internal-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../command-buffer/flush-report.html">flush_report</a> flush_internal(
    <a href="../command-buffer/command-buffer.html">command_buffer</a> &amp;commands);</code></pre></div>

#### `instantiate_now` {#function-instantiate-now}

<div class="function-declaration" id="function-instantiate-now-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; instantiate_now(
    <span class="k">const</span> <a href="../entity-archive/entity-archive.html">entity_archive</a> &amp;archive,
    std::span&lt;<span class="k">const</span> <a href="../entity/entity.html">entity</a>&gt; ids,
    <a href="../../core/types.html#symbol-u32">u32</a> copies);</code></pre></div>

#### `is_reserved` {#function-is-reserved}

<div class="function-declaration" id="function-is-reserved-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_reserved(
    <a href="../entity/entity.html">entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `move_entity` {#function-move-entity}

<div class="function-declaration" id="function-move-entity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; move_entity(
    <a href="../entity/entity.html">entity</a> target,
    <a href="world-entity-record.html">entity_record</a> &amp;record,
    <a href="../archetype/archetype.html">archetype</a> &amp;destination);</code></pre></div>

#### `place_rows` {#function-place-rows}

<div class="function-declaration" id="function-place-rows-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; place_rows(
    <a href="../archetype/archetype.html">archetype</a> &amp;target,
    std::span&lt;<span class="k">const</span> <a href="../entity/entity.html">entity</a>&gt; ids);</code></pre></div>

#### `relayout_archetypes` {#function-relayout-archetypes}

<div class="function-declaration" id="function-relayout-archetypes-1"><pre><code><span class="k">void</span> relayout_archetypes(
    <a href="../component/component-id.html">component_id</a> id);</code></pre></div>

#### `release_reserved` {#function-release-reserved}

<div class="function-declaration" id="function-release-reserved-1"><pre><code><span class="k">void</span> release_reserved(
    <a href="../entity/entity.html">entity</a> target);</code></pre></div>

#### `remove_row` {#function-remove-row}

<div class="function-declaration" id="function-remove-row-1"><pre><code><span class="k">void</span> remove_row(
    <a href="../archetype/archetype.html">archetype</a> &amp;owner,
    <a href="../../core/types.html#symbol-u32">u32</a> chunkIndex,
    <a href="../../core/types.html#symbol-u32">u32</a> row);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
