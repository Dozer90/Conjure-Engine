---
kind: class
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [guid_index.h](../guid-index.md)

# guid_index
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/guid_index.h` |
| Source File Path | `engine/src/scene/guid_index.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scenes and the hierarchy](../../../../api/scene/scenes-and-hierarchy.md) |

## Syntax

```cpp
class CONJURE_API guid_index
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">guid_index();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~guid_index();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">INITIAL_<wbr>CAPACITY</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1024</span></code></td><td>Entries reserved by the constructor so small scenes never rehash.</td></tr>
<tr><td>private</td><td><code class="signature">mEntries</code></td><td><code class="signature"><a href="../../core/containers/flat-hash-map/flat-hash-map.html">core::<wbr>flat_<wbr>hash_<wbr>map</a>&lt;<wbr>core::<wbr>uuid,<wbr> <a href="../../ecs/entity/entity.html">ecs::<wbr>entity</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `capacity` {#function-capacity}

<div class="function-declaration" id="function-capacity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> capacity() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Buckets of the underlying table (for the allocation tests).</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Drops every mapping; the capacity stays.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> <a href="../../core/uuid/uuid.html">core::uuid</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the guid is mapped.</p>

#### `erase` {#function-erase}

<div class="function-declaration" id="function-erase-1"><pre><code><span class="k">void</span> erase(
    <span class="k">const</span> <a href="../../core/uuid/uuid.html">core::uuid</a> &amp;id,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> expected) <span class="k">noexcept</span>;</code></pre></div>

<p>Removes the mapping when it points at <code>expected</code>; any other mapping is left alone.</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../ecs/entity/entity.html">ecs::entity</a> find(
    <span class="k">const</span> <a href="../../core/uuid/uuid.html">core::uuid</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The entity mapped to the guid, or <code>NULL_ENTITY</code>.</p>

#### `insert` {#function-insert}

<div class="function-declaration" id="function-insert-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> insert(
    <span class="k">const</span> <a href="../../core/uuid/uuid.html">core::uuid</a> &amp;id,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target);</code></pre></div>

<p>Maps the guid to the entity. Returns <code>false</code> (and keeps the existing mapping) when the guid is already mapped; a <code>null</code> guid is never inserted.</p>

#### `rebind` {#function-rebind}

<div class="function-declaration" id="function-rebind-1"><pre><code><span class="k">void</span> rebind(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/uuid/uuid.html">core::uuid</a> &amp;oldId,
    <span class="k">const</span> <a href="../../core/uuid/uuid.html">core::uuid</a> &amp;newId);</code></pre></div>

<p>Moves an entity from one guid to another: erases <code>oldId</code> when it maps to the entity and inserts <code>newId</code> (first mapping wins when <code>newId</code> is taken).</p>

#### `reserve` {#function-reserve}

<div class="function-declaration" id="function-reserve-1"><pre><code><span class="k">void</span> reserve(
    <a href="../../core/types.html#symbol-u32">u32</a> count);</code></pre></div>

<p>Makes room for <code>count</code> mappings so the next insertions never rehash.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of mappings.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
