---
kind: class
---

[Engine](../../../engine.md) → [Scripting](../../scripting.md) → [script_component.h](../script-component.md)

# script_store
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scripting` |
| Include File Path | `engine/include/conjure/scripting/script_component.h` |
| Source File Path | `engine/src/scripting/script_component.cpp` |
| API ↗ | [Scripting](../../../../api/scripting/index.md), [Scripts, components and the runtime](../../../../api/scripting/scripts-and-components.md) |

## Syntax

```cpp
class CONJURE_API script_store
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">script_store();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">script_store(
    <a href="script-store.html">script_store</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~script_store();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">GENERATION_<wbr>MASK</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span>xFFFu</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">INDEX_BITS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">20</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAX_SETS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1</span>u &lt;&lt; INDEX_BITS</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFreeList</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLiveCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMappedCount</code></td><td><code class="signature"><a href="../../core/types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSlots</code></td><td><code class="signature">std::vector&lt;<a href="script-store-set-slot.html">set_slot</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSlotToEntry</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="script-entry-id.html">script_<wbr>entry_<wbr>id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add_entry` {#function-add-entry}

<div class="function-declaration" id="function-add-entry-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="script-entry-id.html">script_entry_id</a> add_entry(
    <a href="../../core/types.html#symbol-u32">u32</a> handle,
    <a href="script-entry.html">script_entry</a> entry);</code></pre></div>

<p>Appends an entry; an invalid id when the set is unknown or holds <code>MAX_SCRIPTS_PER_ENTITY</code>.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear();</code></pre></div>

<p>Frees every set and clears the reverse map.</p>

#### `clone_set` {#function-clone-set}

<div class="function-declaration" id="function-clone-set-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> clone_set(
    <a href="../../core/types.html#symbol-u32">u32</a> handle,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> owner = {});</code></pre></div>

<p>Deep-copies a set into a new one: same class hashes and JSON, no managed slots, flags reset to <code>CLASS_MISSING</code> where it was set. <code>INVALID_SET_HANDLE</code> for an invalid source.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <a href="../../core/types.html#symbol-u32">u32</a> handle) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `create_set` {#function-create-set}

<div class="function-declaration" id="function-create-set-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> create_set(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> owner);</code></pre></div>

<p>Creates an empty set and returns its handle; <code>INVALID_SET_HANDLE</code> (and an assert) when <code>MAX_SETS</code> sets are live.</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="script-set.html">script_set</a> *find(
    <a href="../../core/types.html#symbol-u32">u32</a> handle) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-find-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="script-set.html">script_set</a> *find(
    <a href="../../core/types.html#symbol-u32">u32</a> handle) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `find_entry` {#function-find-entry}

<div class="function-declaration" id="function-find-entry-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="script-entry.html">script_entry</a> *find_entry(
    <a href="script-entry-id.html">script_entry_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-find-entry-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="script-entry.html">script_entry</a> *find_entry(
    <a href="script-entry-id.html">script_entry_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `for_each_set` {#function-for-each-set}

<div class="function-declaration" id="function-for-each-set-1"><pre><code><span class="k">void</span> for_each_set(
    <span class="k">const</span> std::function&lt;<span class="k">void</span>(u32, <a href="script-set.html">script_set</a> &amp;)&gt; &amp;callable);</code></pre></div>

<p>Invokes <code>callable(<a href="../../core/types.html#symbol-u32">u32</a> handle, <a href="script-set.html">script_set</a> &amp;)</code> for every live set.</p>

#### `free_set` {#function-free-set}

<div class="function-declaration" id="function-free-set-1"><pre><code><span class="k">bool</span> free_set(
    <a href="../../core/types.html#symbol-u32">u32</a> handle);</code></pre></div>

<p>Frees a set and unmaps its live slots. False for a <code>null</code>, stale or unknown handle.</p>

#### `handle_generation` {#function-handle-generation}

<div class="function-declaration" id="function-handle-generation-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a> handle_generation(
    <a href="../../core/types.html#symbol-u32">u32</a> handle) <span class="k">noexcept</span>;</code></pre></div>

#### `handle_index` {#function-handle-index}

<div class="function-declaration" id="function-handle-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a> handle_index(
    <a href="../../core/types.html#symbol-u32">u32</a> handle) <span class="k">noexcept</span>;</code></pre></div>

#### `make_handle` {#function-make-handle}

<div class="function-declaration" id="function-make-handle-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a> make_handle(
    <a href="../../core/types.html#symbol-u32">u32</a> index,
    <a href="../../core/types.html#symbol-u32">u32</a> generation) <span class="k">noexcept</span>;</code></pre></div>

<p>Handle arithmetic.</p>

#### `map_slot` {#function-map-slot}

<div class="function-declaration" id="function-map-slot-1"><pre><code><span class="k">void</span> map_slot(
    <a href="../../core/types.html#symbol-i32">i32</a> slot,
    <a href="script-entry-id.html">script_entry_id</a> id);</code></pre></div>

<p>Binds a managed slot to an entry (and writes the entry&#x27;s <code>managedSlot</code>).</p>

#### `mapped_slot_count` {#function-mapped-slot-count}

<div class="function-declaration" id="function-mapped-slot-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> mapped_slot_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of mapped slots: a counter the mapping functions keep, so <code>stats()</code> costs nothing.</p>

#### `remove_entry` {#function-remove-entry}

<div class="function-declaration" id="function-remove-entry-1"><pre><code><span class="k">bool</span> remove_entry(
    <a href="script-entry-id.html">script_entry_id</a> id);</code></pre></div>

<p>Removes an entry (later entries shift down; the reverse map follows). False when unknown.</p>

#### `resolve_slot` {#function-resolve-slot}

<div class="function-declaration" id="function-resolve-slot-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="script-entry-id.html">script_entry_id</a>&gt; resolve_slot(
    <a href="../../core/types.html#symbol-i32">i32</a> slot) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The entry a managed slot belongs to, or nullopt.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Live sets.</p>

#### `unmap_slot` {#function-unmap-slot}

<div class="function-declaration" id="function-unmap-slot-1"><pre><code><span class="k">void</span> unmap_slot(
    <a href="../../core/types.html#symbol-i32">i32</a> slot);</code></pre></div>

<p>Clears the mapping of a slot (and the entry&#x27;s <code>managedSlot</code> when it still points at the slot).</p>

### Private

#### `live_slot` {#function-live-slot}

<div class="function-declaration" id="function-live-slot-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="script-store-set-slot.html">set_slot</a> *live_slot(
    <a href="../../core/types.html#symbol-u32">u32</a> handle) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-live-slot-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="script-store-set-slot.html">set_slot</a> *live_slot(
    <a href="../../core/types.html#symbol-u32">u32</a> handle) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="script-store.html">script_store</a> &amp;<span class="k">operator</span>=(
    <a href="script-store.html">script_store</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
