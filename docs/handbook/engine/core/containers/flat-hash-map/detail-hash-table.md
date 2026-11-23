---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [flat_hash_map.h](../flat-hash-map.md)

# detail::hash_table
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/containers/flat_hash_map.h` |
| Source File Path | N/A |
| API ↗ | [Containers](../../../../../api/core/containers.md) |

## Syntax

```cpp
template <typename Value, typename KeyOf, typename Hash, typename Eq> class hash_table
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">hash_table() = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> hash_table(
    <a href="../../types.html#symbol-usize">usize</a> initialCapacity);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">hash_table(
    <span class="k">const</span> <a href="detail-hash-table.html">hash_table</a> &amp;other);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">hash_table(
    <a href="detail-hash-table.html">hash_table</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~hash_table();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">HASH_MASK</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span>x7FFFFFFFu</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mAnchor</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAX_CAPACITY</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a>{<span class="mi">1</span>} &lt;&lt; <span class="mi">31</span></code></td><td>Largest number of buckets: the stored hash keeps 31 bits, so the home bucket is addressable for every capacity up to 2^31.</td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>LOAD_<wbr>FACTOR</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.875</span>f</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mCapacity</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEqual</code></td><td><code class="signature">Eq</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mGrowAt</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mHash</code></td><td><code class="signature">Hash</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MIN_CAPACITY</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">8</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMask</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMeta</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSize</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mValues</code></td><td><code class="signature">Value *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">NO_SLOT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature">std::<wbr>numeric_<wbr>limits&lt;<wbr><a href="../../types.html#symbol-usize">usize</a>&gt;::<wbr>max()</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">OCCUPIED_BIT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span>x80000000u</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin` {#function-begin}

<div class="function-declaration" id="function-begin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-detail-hash-table-iterator">iterator</a> begin() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-begin-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-detail-hash-table-const-iterator">const_iterator</a> begin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `capacity` {#function-capacity}

<div class="function-declaration" id="function-capacity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> capacity() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Buckets (a power of two, 0 before the first insertion).</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Destroys every element; keeps the table so <code><a href="#function-capacity-1">capacity</a>()</code> is unchanged.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> <a href="../flat-hash-map.html#symbol-detail-hash-table-key-type">key_type</a> &amp;key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `empty` {#function-empty}

<div class="function-declaration" id="function-empty-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> empty() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `end` {#function-end}

<div class="function-declaration" id="function-end-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-detail-hash-table-iterator">iterator</a> end() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-end-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-detail-hash-table-const-iterator">const_iterator</a> end() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `erase` {#function-erase}

<div class="function-declaration" id="function-erase-1"><pre><code><span class="k">bool</span> erase(
    <span class="k">const</span> <a href="../flat-hash-map.html#symbol-detail-hash-table-key-type">key_type</a> &amp;key) <span class="k">noexcept</span>;</code></pre></div>

<p>Removes the element with <code>key</code>. Returns <code>false</code> when absent. Never rehashes.</p>

<div class="function-declaration" id="function-erase-2"><pre><code><a href="../flat-hash-map.html#symbol-detail-hash-table-iterator">iterator</a> erase(
    <a href="../flat-hash-map.html#symbol-detail-hash-table-const-iterator">const_iterator</a> position) <span class="k">noexcept</span>;</code></pre></div>

<p>Removes the element at <code>position</code>. Returns the iterator following it, which is the next element an iteration from <code><a href="#function-begin-1">begin</a>()</code> has not visited yet: deletion only shifts elements towards the start of their probe run and no run crosses the anchor the iteration order starts at.</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-detail-hash-table-iterator">iterator</a> find(
    <span class="k">const</span> <a href="../flat-hash-map.html#symbol-detail-hash-table-key-type">key_type</a> &amp;key) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-find-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-detail-hash-table-const-iterator">const_iterator</a> find(
    <span class="k">const</span> <a href="../flat-hash-map.html#symbol-detail-hash-table-key-type">key_type</a> &amp;key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `hash_function` {#function-hash-function}

<div class="function-declaration" id="function-hash-function-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> Hash &amp;hash_function() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `insert` {#function-insert}

<div class="function-declaration" id="function-insert-1"><pre><code>std::pair&lt;iterator, <span class="k">bool</span>&gt; insert(
    <span class="k">const</span> <a href="../flat-hash-map.html#symbol-detail-hash-table-value-type">value_type</a> &amp;value);</code></pre></div>

<div class="function-declaration" id="function-insert-2"><pre><code>std::pair&lt;iterator, <span class="k">bool</span>&gt; insert(
    <a href="../flat-hash-map.html#symbol-detail-hash-table-value-type">value_type</a> &amp;&amp;value);</code></pre></div>

#### `insert_with` {#function-insert-with}

<div class="function-declaration" id="function-insert-with-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Builder&gt; std::pair&lt;iterator, <span class="k">bool</span>&gt; insert_with(
    <span class="k">const</span> <a href="../flat-hash-map.html#symbol-detail-hash-table-key-type">key_type</a> &amp;key,
    Builder &amp;&amp;build);</code></pre></div>

<p>Inserts when the key is absent. <code>build</code> receives the uninitialised slot and must construct the value there. Returns the element and whether it was inserted. Arguments captured by <code>build</code> must not refer to elements of this table.</p>

#### `key_eq` {#function-key-eq}

<div class="function-declaration" id="function-key-eq-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> Eq &amp;key_eq() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `load_factor` {#function-load-factor}

<div class="function-declaration" id="function-load-factor-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-f32">f32</a> load_factor() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code><a href="#function-size-1">size</a>()</code> / <code><a href="#function-capacity-1">capacity</a>()</code>; 0 for an empty table.</p>

#### `reserve` {#function-reserve}

<div class="function-declaration" id="function-reserve-1"><pre><code><span class="k">void</span> reserve(
    <a href="../../types.html#symbol-usize">usize</a> count);</code></pre></div>

<p>Makes room for <code>count</code> elements without a rehash during the next insertions. <code><a href="#function-reserve-1">reserve</a>(0)</code> does nothing, so an empty table stays unallocated.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `swap` {#function-swap}

<div class="function-declaration" id="function-swap-1"><pre><code><span class="k">void</span> swap(
    <a href="detail-hash-table.html">hash_table</a> &amp;other) <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `allocate_table` {#function-allocate-table}

<div class="function-declaration" id="function-allocate-table-1"><pre><code><span class="k">void</span> allocate_table(
    <a href="../../types.html#symbol-usize">usize</a> capacity);</code></pre></div>

#### `capacity_for` {#function-capacity-for}

<div class="function-declaration" id="function-capacity-for-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../types.html#symbol-usize">usize</a> capacity_for(
    <a href="../../types.html#symbol-usize">usize</a> count) <span class="k">noexcept</span>;</code></pre></div>

<p>Smallest power-of-two capacity (at least <code>MIN_CAPACITY</code>) whose grow threshold admits <code>count</code> elements.</p>

#### `clone_from` {#function-clone-from}

<div class="function-declaration" id="function-clone-from-1"><pre><code><span class="k">void</span> clone_from(
    <span class="k">const</span> <a href="detail-hash-table.html">hash_table</a> &amp;other);</code></pre></div>

<p>Copies another table slot for slot, which keeps its iteration order.</p>

#### `deallocate_table` {#function-deallocate-table}

<div class="function-declaration" id="function-deallocate-table-1"><pre><code><span class="k">static</span> <span class="k">void</span> deallocate_table(
    Value *values,
    <a href="../../types.html#symbol-u32">u32</a> *meta,
    <a href="../../types.html#symbol-usize">usize</a> capacity) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-deallocate-table-2"><pre><code><span class="k">void</span> deallocate_table() <span class="k">noexcept</span>;</code></pre></div>

#### `destroy_elements` {#function-destroy-elements}

<div class="function-declaration" id="function-destroy-elements-1"><pre><code><span class="k">void</span> destroy_elements() <span class="k">noexcept</span>;</code></pre></div>

#### `erase_index` {#function-erase-index}

<div class="function-declaration" id="function-erase-index-1"><pre><code><span class="k">void</span> erase_index(
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">noexcept</span>;</code></pre></div>

<p>Backward-shift deletion: later elements of the run whose home is at or before the hole move into it, so no tombstone is needed and lookups stay short after many erases. The anchor is empty and therefore never part of the run.</p>

#### `find_index` {#function-find-index}

<div class="function-declaration" id="function-find-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> find_index(
    <span class="k">const</span> <a href="../flat-hash-map.html#symbol-detail-hash-table-key-type">key_type</a> &amp;key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-find-index-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> find_index(
    <a href="../../types.html#symbol-u64">u64</a> hash,
    <span class="k">const</span> <a href="../flat-hash-map.html#symbol-detail-hash-table-key-type">key_type</a> &amp;key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Slot of <code>key</code> or <code>NO_SLOT</code>. The probe stops at the first empty slot (there always is one: the load factor is below one); keys are only compared when the stored hash matches.</p>

#### `forget_table` {#function-forget-table}

<div class="function-declaration" id="function-forget-table-1"><pre><code><span class="k">void</span> forget_table() <span class="k">noexcept</span>;</code></pre></div>

#### `grow_threshold` {#function-grow-threshold}

<div class="function-declaration" id="function-grow-threshold-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../types.html#symbol-usize">usize</a> grow_threshold(
    <a href="../../types.html#symbol-usize">usize</a> capacity) <span class="k">noexcept</span>;</code></pre></div>

#### `hash_of` {#function-hash-of}

<div class="function-declaration" id="function-hash-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-u64">u64</a> hash_of(
    <span class="k">const</span> <a href="../flat-hash-map.html#symbol-detail-hash-table-key-type">key_type</a> &amp;key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `home_of` {#function-home-of}

<div class="function-declaration" id="function-home-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> home_of(
    <a href="../../types.html#symbol-u64">u64</a> hash) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `home_of_slot` {#function-home-of-slot}

<div class="function-declaration" id="function-home-of-slot-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> home_of_slot(
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `insert_new` {#function-insert-new}

<div class="function-declaration" id="function-insert-new-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Builder&gt; <a href="../../types.html#symbol-usize">usize</a> insert_new(
    <a href="../../types.html#symbol-u64">u64</a> hash,
    Builder &amp;&amp;build);</code></pre></div>

<p>Places a value whose key is known to be absent in the first empty slot of its probe sequence. The caller has made sure the table is below its grow threshold.</p>

#### `is_occupied` {#function-is-occupied}

<div class="function-declaration" id="function-is-occupied-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_occupied(
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `iterator_at` {#function-iterator-at}

<div class="function-declaration" id="function-iterator-at-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-detail-hash-table-iterator">iterator</a> iterator_at(
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-iterator-at-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-detail-hash-table-const-iterator">const_iterator</a> iterator_at(
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `next_empty_after` {#function-next-empty-after}

<div class="function-declaration" id="function-next-empty-after-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> next_empty_after(
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The first empty slot after <code>index</code> in probe order. Exists because the table is never full.</p>

#### `rehash` {#function-rehash}

<div class="function-declaration" id="function-rehash-1"><pre><code><span class="k">void</span> rehash(
    <a href="../../types.html#symbol-usize">usize</a> newCapacity);</code></pre></div>

#### `relocate` {#function-relocate}

<div class="function-declaration" id="function-relocate-1"><pre><code><span class="k">static</span> <span class="k">void</span> relocate(
    Value *source,
    Value *destination) <span class="k">noexcept</span>;</code></pre></div>

#### `tag_of` {#function-tag-of}

<div class="function-declaration" id="function-tag-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../types.html#symbol-u32">u32</a> tag_of(
    <a href="../../types.html#symbol-u64">u64</a> hash) <span class="k">noexcept</span>;</code></pre></div>

<p>What an occupied slot stores for <code>hash</code>. Its low bits are the home bucket, so erase can recompute homes without re-hashing keys.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="detail-hash-table.html">hash_table</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="detail-hash-table.html">hash_table</a> &amp;other);</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="detail-hash-table.html">hash_table</a> &amp;<span class="k">operator</span>=(
    <a href="detail-hash-table.html">hash_table</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
