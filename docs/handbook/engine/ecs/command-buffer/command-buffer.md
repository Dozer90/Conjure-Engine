---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [command_buffer.h](../command-buffer.md)

# command_buffer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/command_buffer.h` |
| Source File Path | `engine/src/ecs/command_buffer.cpp` |
| API ↗ | [Command buffers and observers](../../../../api/ecs/command-buffers-and-observers.md) |

## Syntax

```cpp
class CONJURE_API command_buffer
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> command_buffer(
    <a href="../world/world.html">world</a> &amp;target);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">command_buffer(
    <a href="command-buffer.html">command_buffer</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~command_buffer();</code></td><td>Discards pending commands with a <code>WARN</code> (payloads are destroyed) and returns unused ids. The ids of a pending instantiation are released with a bumped generation, as in <code>clear()</code>.</td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mArena</code></td><td><code class="signature"><a href="../../core/memory/arena/memory-arena.html">core::memory::arena</a></code></td><td>—</td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFirst</code></td><td><code class="signature"><a href="command-buffer-record.html">record</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Instantiates</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="command-buffer-instantiate-entry.html">instantiate_<wbr>entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLast</code></td><td><code class="signature"><a href="command-buffer-record.html">record</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRegistry</code></td><td><code class="signature"><span class="k">const</span> <a href="../component-registry/component-registry.html">component_<wbr>registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mReserved</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>entity,<wbr> RESERVE_<wbr>BLOCK&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Reserved<wbr>Count</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWorld</code></td><td><code class="signature"><a href="../world/world.html">world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">RESERVE_<wbr>BLOCK</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">64</span></code></td><td>Ids reserved per refill.</td></tr>
</tbody>
</table>

## Functions

### Public

#### `add` {#function-add}

<div class="function-declaration" id="function-add-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; <span class="k">void</span> add(
    <a href="../entity/entity.html">entity</a> target,
    ComponentType value = {});</code></pre></div>

<p>Records adding a component with an initial value (moved in). On an entity that already has it the value is applied like <code><a href="#function-set-1">set</a>()</code>. An unregistered type asserts and records nothing.</p>

#### `add_raw` {#function-add-raw}

<div class="function-declaration" id="function-add-raw-1"><pre><code><span class="k">void</span> add_raw(
    <a href="../entity/entity.html">entity</a> target,
    <a href="../component/component-id.html">component_id</a> id,
    std::span&lt;<span class="k">const</span> std::byte&gt; payload = {});</code></pre></div>

<p>Untyped add: the payload bytes are copy-constructed into the record (<code>ops.copy</code> or memcpy). An empty payload records no value: an entity that gains the component at flush gets its default value; one that holds it at flush (a <code>remove_raw</code> earlier in the same buffer included) keeps its current value, and no observer fires for it. Pass the default bytes to reset a held component.</p>

#### `append` {#function-append}

<div class="function-declaration" id="function-append-1"><pre><code><span class="k">void</span> append(
    <a href="command-buffer.html">command_buffer</a> &amp;&amp;other);</code></pre></div>

<p>Moves every command of <code>other</code> behind this buffer&#x27;s commands, in order. <code>other</code> keeps its reserved ids and is empty afterwards.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Drops every command, destroying payloads that were not applied. The ids reserved by an instantiation that was never applied (cleared, or skipped by a failed flush) are released with a bumped generation, so the root <code><a href="#function-instantiate-1">instantiate</a>()</code> handed out stays dead and never aliases a later entity. That writes the world&#x27;s record table, so clear a buffer holding an instantiation on the world&#x27;s owner thread: off it, <code><a href="#function-clear-1">clear</a>()</code> asserts and leaves those ids reserved for good (the root still never aliases). Unused reserved ids are kept, and so is room for the records: a recording that outgrew the arena&#x27;s 16 KiB block makes the buffer keep one block spanning the blocks that recording used (its bytes rounded up to whole 16 KiB blocks, like a vector&#x27;s capacity: at most one block more than it needed), so repeating a recording of the same size allocates nothing after the next one. The room is kept for the buffer&#x27;s lifetime unless <code><a href="#function-shrink-to-fit-1">shrink_to_fit</a>()</code> returns it.</p>

#### `command_count` {#function-command-count}

<div class="function-declaration" id="function-command-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> command_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of recorded commands.</p>

#### `create_entity` {#function-create-entity}

<div class="function-declaration" id="function-create-entity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../entity/entity.html">entity</a> create_entity();</code></pre></div>

<p>Reserves a real entity id that becomes alive at flush. The id may be used in later commands of any buffer of the same world and handed to scripts at once. Returns <code>NULL_ENTITY</code> (and logs) when the id space is exhausted.</p>

#### `destroy_entity` {#function-destroy-entity}

<div class="function-declaration" id="function-destroy-entity-1"><pre><code><span class="k">void</span> destroy_entity(
    <a href="../entity/entity.html">entity</a> target);</code></pre></div>

<p>Records the destruction of an entity. Wins over every other command on it in this buffer.</p>

#### `empty` {#function-empty}

<div class="function-declaration" id="function-empty-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> empty() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when no command is recorded.</p>

#### `instantiate` {#function-instantiate}

<div class="function-declaration" id="function-instantiate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../entity/entity.html">entity</a>&gt; instantiate(
    std::shared_ptr&lt;<span class="k">const</span> <a href="../entity-archive/entity-archive.html">entity_archive</a>&gt; archive);</code></pre></div>

<p>Records instantiating one copy of an archive at flush; ids are reserved now and the root is returned now. <code>ARCHIVE_CORRUPT</code> when the archive does not validate, <code>ENTITY_LIMIT_REACHED</code> when no ids are left.</p>

#### `remove` {#function-remove}

<div class="function-declaration" id="function-remove-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; <span class="k">void</span> remove(
    <a href="../entity/entity.html">entity</a> target);</code></pre></div>

<p>Records removing a component. Cancels a pending add of the same component in this buffer. An unregistered type asserts and records nothing.</p>

#### `remove_raw` {#function-remove-raw}

<div class="function-declaration" id="function-remove-raw-1"><pre><code><span class="k">void</span> remove_raw(
    <a href="../entity/entity.html">entity</a> target,
    <a href="../component/component-id.html">component_id</a> id);</code></pre></div>

<p>Untyped remove.</p>

#### `reserved_count` {#function-reserved-count}

<div class="function-declaration" id="function-reserved-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> reserved_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Ids reserved and not yet handed out by <code><a href="#function-create-entity-1">create_entity</a>()</code>.</p>

#### `set` {#function-set}

<div class="function-declaration" id="function-set-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; <span class="k">void</span> set(
    <a href="../entity/entity.html">entity</a> target,
    ComponentType value);</code></pre></div>

<p>Records replacing the value of a component the entity has at flush (a missing component is skipped and counted). An unregistered type asserts and records nothing.</p>

#### `set_raw` {#function-set-raw}

<div class="function-declaration" id="function-set-raw-1"><pre><code><span class="k">void</span> set_raw(
    <a href="../entity/entity.html">entity</a> target,
    <a href="../component/component-id.html">component_id</a> id,
    std::span&lt;<span class="k">const</span> std::byte&gt; payload);</code></pre></div>

<p>Untyped set.</p>

#### `shrink_to_fit` {#function-shrink-to-fit}

<div class="function-declaration" id="function-shrink-to-fit-1"><pre><code><span class="k">void</span> shrink_to_fit() <span class="k">noexcept</span>;</code></pre></div>

<p>Returns the room an earlier recording left behind to the heap: the arena&#x27;s blocks and the capacity of the instantiation list. Only an empty buffer has room to give back; with commands pending the call changes nothing. Reserved ids are kept either way. The next recording acquires a fresh block, so call it when the buffer goes idle for good (a removed system, a cleared world), not between frames.</p>

#### `target` {#function-target}

<div class="function-declaration" id="function-target-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../world/world.html">world</a> &amp;target() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The world this buffer targets.</p>

### Private

#### `destroy_payloads` {#function-destroy-payloads}

<div class="function-declaration" id="function-destroy-payloads-1"><pre><code><span class="k">void</span> destroy_payloads() <span class="k">noexcept</span>;</code></pre></div>

#### `payload_size` {#function-payload-size}

<div class="function-declaration" id="function-payload-size-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a> payload_size() <span class="k">noexcept</span>;</code></pre></div>

#### `push_instantiate` {#function-push-instantiate}

<div class="function-declaration" id="function-push-instantiate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;std::span&lt;<span class="k">const</span> <a href="../entity/entity.html">entity</a>&gt;&gt; push_instantiate(
    std::shared_ptr&lt;<span class="k">const</span> <a href="../entity-archive/entity-archive.html">entity_archive</a>&gt; archive,
    <a href="../../core/types.html#symbol-u32">u32</a> copies);</code></pre></div>

#### `push_plain_record` {#function-push-plain-record}

<div class="function-declaration" id="function-push-plain-record-1"><pre><code><a href="command-buffer-record.html">record</a> *push_plain_record(
    <a href="../command-buffer.html#symbol-command-buffer-opcode">opcode</a> op,
    <a href="../entity/entity.html">entity</a> target,
    <a href="../component/component-id.html">component_id</a> id);</code></pre></div>

#### `push_record` {#function-push-record}

<div class="function-declaration" id="function-push-record-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">void</span> *push_record(
    <a href="../command-buffer.html#symbol-command-buffer-opcode">opcode</a> op,
    <a href="../entity/entity.html">entity</a> target,
    <a href="../component/component-id.html">component_id</a> id,
    <a href="../../core/types.html#symbol-u32">u32</a> payloadSize,
    <a href="../../core/types.html#symbol-u32">u32</a> alignment);</code></pre></div>

#### `recycle_arena` {#function-recycle-arena}

<div class="function-declaration" id="function-recycle-arena-1"><pre><code><span class="k">void</span> recycle_arena() <span class="k">noexcept</span>;</code></pre></div>

#### `refill_reserved` {#function-refill-reserved}

<div class="function-declaration" id="function-refill-reserved-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> refill_reserved();</code></pre></div>

#### `return_reserved` {#function-return-reserved}

<div class="function-declaration" id="function-return-reserved-1"><pre><code><span class="k">void</span> return_reserved() <span class="k">noexcept</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="command-buffer.html">command_buffer</a> &amp;<span class="k">operator</span>=(
    <a href="command-buffer.html">command_buffer</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
