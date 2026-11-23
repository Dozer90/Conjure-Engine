---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [slot_map.h](../slot-map.md)

# slot_map
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/containers/slot_map.h` |
| Source File Path | N/A |
| API ↗ | [Containers](../../../../../api/core/containers.md) |

## Syntax

```cpp
template <typename T, slot_key_type Key = slot_key> class slot_map
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">slot_map() = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">slot_map(
    <span class="k">const</span> <a href="slot-map.html">slot_map</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">slot_map(
    <a href="slot-map.html">slot_map</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td>Takes the elements, slots and keys of <code>other</code>, which is left empty (no slots) and reusable. Keys issued by <code>other</code> address the new map.</td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~slot_map() = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">INVALID_<wbr>INDEX</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature">std::<wbr>numeric_<wbr>limits&lt;<wbr><a href="../../types.html#symbol-u32">u32</a>&gt;::<wbr>max()</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>SLOT_<wbr>COUNT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature">static_<wbr>cast&lt;<wbr><a href="../../types.html#symbol-usize">usize</a>&gt;( std::<wbr>numeric_<wbr>limits&lt;<wbr><a href="../slot-map.html#symbol-slot-map-index-type">index_<wbr>type</a>&gt;::<wbr>max() &lt;<wbr> std::<wbr>numeric_<wbr>limits&lt;<wbr><a href="../../types.html#symbol-u32">u32</a>&gt;::<wbr>max() ? static_<wbr>cast&lt;<wbr><a href="../../types.html#symbol-u64">u64</a>&gt;(std::<wbr>numeric_<wbr>limits&lt;<wbr><a href="../slot-map.html#symbol-slot-map-index-type">index_<wbr>type</a>&gt;::<wbr>max()) + <span class="mi">1</span> : static_<wbr>cast&lt;<wbr><a href="../../types.html#symbol-u64">u64</a>&gt;(std::<wbr>numeric_<wbr>limits&lt;<wbr><a href="../../types.html#symbol-u32">u32</a>&gt;::<wbr>max()) )</code></td><td>Largest number of slots the key&#x27;s index type can address.</td></tr>
<tr><td>private</td><td><code class="signature">mEntries</code></td><td><code class="signature">std::vector&lt;<a href="../slot-map.html#symbol-slot-map-entry">entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFreeCount</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFreeHead</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature">INVALID_INDEX</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Retired<wbr>Count</code></td><td><code class="signature"><a href="../../types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSlots</code></td><td><code class="signature">std::vector&lt;<a href="slot-map-slot-entry.html">slot_entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `begin` {#function-begin}

<div class="function-declaration" id="function-begin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../slot-map.html#symbol-slot-map-iterator">iterator</a> begin() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-begin-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../slot-map.html#symbol-slot-map-const-iterator">const_iterator</a> begin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `capacity` {#function-capacity}

<div class="function-declaration" id="function-capacity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> capacity() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Elements the dense storage can hold without reallocating.</p>

#### `cbegin` {#function-cbegin}

<div class="function-declaration" id="function-cbegin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../slot-map.html#symbol-slot-map-const-iterator">const_iterator</a> cbegin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `cend` {#function-cend}

<div class="function-declaration" id="function-cend-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../slot-map.html#symbol-slot-map-const-iterator">const_iterator</a> cend() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear();</code></pre></div>

<p>Erases every element. Every outstanding key becomes stale (generations are bumped); slots and capacity are kept for reuse.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    Key key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>key</code> addresses a live element.</p>

#### `emplace` {#function-emplace}

<div class="function-declaration" id="function-emplace-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... Args&gt; [[<span class="na">nodiscard</span>]] Key emplace(
    Args &amp;&amp;...args);</code></pre></div>

<p>Constructs an element in place and returns its key. Returns the <code>null</code> key (and asserts) when the key&#x27;s index type cannot address another slot.</p>

#### `empty` {#function-empty}

<div class="function-declaration" id="function-empty-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> empty() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `end` {#function-end}

<div class="function-declaration" id="function-end-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../slot-map.html#symbol-slot-map-iterator">iterator</a> end() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-end-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../slot-map.html#symbol-slot-map-const-iterator">const_iterator</a> end() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `entries` {#function-entries}

<div class="function-declaration" id="function-entries-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<a href="../slot-map.html#symbol-slot-map-entry">entry</a>&gt; entries() <span class="k">noexcept</span>;</code></pre></div>

<p>The live {key, value} entries in dense storage order (the order iteration uses).</p>

<div class="function-declaration" id="function-entries-2"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../slot-map.html#symbol-slot-map-entry">entry</a>&gt; entries() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `erase` {#function-erase}

<div class="function-declaration" id="function-erase-1"><pre><code><span class="k">bool</span> erase(
    Key key);</code></pre></div>

<p>Destroys the element addressed by <code>key</code>. Returns <code>false</code> for <code>null</code>, stale or unknown keys. The slot&#x27;s generation is bumped so every outstanding copy of the key becomes stale; a generation that would wrap to 0 retires the slot for good.</p>

<div class="function-declaration" id="function-erase-2"><pre><code><a href="../slot-map.html#symbol-slot-map-iterator">iterator</a> erase(
    <a href="../slot-map.html#symbol-slot-map-const-iterator">const_iterator</a> position);</code></pre></div>

<p>Erases the element an iterator points at. Returns the iterator to the element that now occupies that storage position (or <code><a href="#function-end-1">end</a>()</code>), so erase-while-iterating works. The iterator must point at an element of this map; <code><a href="#function-end-1">end</a>()</code> and iterators of other maps assert and leave the map unchanged.</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] T *find(
    Key key) <span class="k">noexcept</span>;</code></pre></div>

<p>The element addressed by <code>key</code>, or <code>nullptr</code> when the key is <code>null</code>, stale or unknown. Allocation-free, <code>O(1)</code>.</p>

<div class="function-declaration" id="function-find-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> T *find(
    Key key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `free_slot_count` {#function-free-slot-count}

<div class="function-declaration" id="function-free-slot-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> free_slot_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Slots on the free list, reused by the next insertions.</p>

#### `insert` {#function-insert}

<div class="function-declaration" id="function-insert-1"><pre><code>[[<span class="na">nodiscard</span>]] Key insert(
    T &amp;&amp;value);</code></pre></div>

<p>Moves an element in and returns its key.</p>

<div class="function-declaration" id="function-insert-2"><pre><code>[[<span class="na">nodiscard</span>]] Key insert(
    <span class="k">const</span> T &amp;value);</code></pre></div>

<p>Copies an element in and returns its key.</p>

#### `key_at` {#function-key-at}

<div class="function-declaration" id="function-key-at-1"><pre><code>[[<span class="na">nodiscard</span>]] Key key_at(
    <a href="../../types.html#symbol-usize">usize</a> denseIndex) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Key of the element at dense position <code>denseIndex</code> (the order iteration uses). Asserts and returns the <code>null</code> key when the position is out of range.</p>

#### `reserve` {#function-reserve}

<div class="function-declaration" id="function-reserve-1"><pre><code><span class="k">void</span> reserve(
    <a href="../../types.html#symbol-usize">usize</a> count);</code></pre></div>

<p>Reserves dense storage and slots for <code>count</code> elements.</p>

#### `retired_slot_count` {#function-retired-slot-count}

<div class="function-declaration" id="function-retired-slot-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> retired_slot_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Slots whose generation wrapped; they are never reused.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of live elements.</p>

#### `slot_count` {#function-slot-count}

<div class="function-declaration" id="function-slot-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> slot_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Slots in the indirection table (live + free + retired).</p>

### Private

#### `erase_at` {#function-erase-at}

<div class="function-declaration" id="function-erase-at-1"><pre><code><span class="k">void</span> erase_at(
    <a href="../../types.html#symbol-u32">u32</a> denseIndex);</code></pre></div>

#### `live_slot` {#function-live-slot}

<div class="function-declaration" id="function-live-slot-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="slot-map-slot-entry.html">slot_entry</a> *live_slot(
    Key key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `make_key` {#function-make-key}

<div class="function-declaration" id="function-make-key-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> Key make_key(
    <a href="../../types.html#symbol-u32">u32</a> slotIndex,
    <a href="../../types.html#symbol-u32">u32</a> generation) <span class="k">noexcept</span>;</code></pre></div>

<p>Builds a key field by field so key types that declare their members in any order (or add more) work.</p>

#### `release_slot` {#function-release-slot}

<div class="function-declaration" id="function-release-slot-1"><pre><code><span class="k">void</span> release_slot(
    <a href="../../types.html#symbol-u32">u32</a> slotIndex) <span class="k">noexcept</span>;</code></pre></div>

<p>Marks a slot free (or retired when its generation wraps). The caller has already removed the element from the dense storage.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="slot-map.html">slot_map</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="slot-map.html">slot_map</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="slot-map.html">slot_map</a> &amp;<span class="k">operator</span>=(
    <a href="slot-map.html">slot_map</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

<p>Replaces the contents with those of <code>other</code>, which is left empty (no slots) and reusable.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
