---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [small_vector.h](../small-vector.md)

# small_vector
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/containers/small_vector.h` |
| Source File Path | N/A |
| API ↗ | [Containers](../../../../../api/core/containers.md) |

## Syntax

```cpp
template <typename T, usize N> class small_vector
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">small_vector() <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> small_vector(
    <a href="../../types.html#symbol-usize">usize</a> count);</code></td><td><code>count</code> value-initialised elements.</td></tr>
<tr><td>public</td><td><code class="signature">small_vector(
    <a href="../../types.html#symbol-usize">usize</a> count,
    <span class="k">const</span> T &amp;value);</code></td><td><code>count</code> copies of <code>value</code>.</td></tr>
<tr><td>public</td><td><code class="signature">small_vector(
    std::initializer_list&lt;T&gt; values);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">template</span> &lt;std::input_iterator Iterator&gt; small_vector(
    Iterator first,
    Iterator last);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> small_vector(
    std::span&lt;<span class="k">const</span> T&gt; values);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">small_vector(
    <span class="k">const</span> <a href="small-vector.html">small_vector</a> &amp;other);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">small_vector(
    <a href="small-vector.html">small_vector</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~small_vector();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">INLINE_<wbr>CAPACITY</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature">N</code></td><td>Elements stored inside the object before the heap is used.</td></tr>
<tr><td>public</td><td><code class="signature">IS_<wbr>TRIVIALLY_<wbr>RELOCATABLE</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <span class="k">bool</span></code></td><td><code class="signature">std::<wbr>is_<wbr>trivially_<wbr>copyable_<wbr>v&lt;<wbr>T&gt;</code></td><td>True when elements are relocated with memcpy instead of move + destroy.</td></tr>
<tr><td>private</td><td><code class="signature">mCapacity</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature">N</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mData</code></td><td><code class="signature">T *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mInline</code></td><td><code class="signature"><a href="../small-vector.html#symbol-small-vector-inline-storage">inline_storage</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSize</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `assign` {#function-assign}

<div class="function-declaration" id="function-assign-1"><pre><code><span class="k">void</span> assign(
    <a href="../../types.html#symbol-usize">usize</a> count,
    <span class="k">const</span> T &amp;value);</code></pre></div>

<p>Replaces the contents with <code>count</code> copies of <code>value</code>. <code>value</code> may refer to an element of this vector.</p>

<div class="function-declaration" id="function-assign-2"><pre><code><span class="k">template</span> &lt;std::input_iterator Iterator&gt; <span class="k">void</span> assign(
    Iterator first,
    Iterator last);</code></pre></div>

<p>Replaces the contents with a copy of the range.</p>

#### `at` {#function-at}

<div class="function-declaration" id="function-at-1"><pre><code>[[<span class="na">nodiscard</span>]] T &amp;at(
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">noexcept</span>;</code></pre></div>

<p>Bounds-checked access in every configuration. An out-of-range index is a fatal programmer error (asserts, then terminates), never an exception.</p>

<div class="function-declaration" id="function-at-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> T &amp;at(
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `back` {#function-back}

<div class="function-declaration" id="function-back-1"><pre><code>[[<span class="na">nodiscard</span>]] T &amp;back() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-back-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> T &amp;back() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `begin` {#function-begin}

<div class="function-declaration" id="function-begin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../small-vector.html#symbol-small-vector-iterator">iterator</a> begin() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-begin-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../small-vector.html#symbol-small-vector-const-iterator">const_iterator</a> begin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `capacity` {#function-capacity}

<div class="function-declaration" id="function-capacity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> capacity() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `cbegin` {#function-cbegin}

<div class="function-declaration" id="function-cbegin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../small-vector.html#symbol-small-vector-const-iterator">const_iterator</a> cbegin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `cend` {#function-cend}

<div class="function-declaration" id="function-cend-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../small-vector.html#symbol-small-vector-const-iterator">const_iterator</a> cend() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Destroys every element; keeps the current storage.</p>

#### `data` {#function-data}

<div class="function-declaration" id="function-data-1"><pre><code>[[<span class="na">nodiscard</span>]] T *data() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-data-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> T *data() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `emplace` {#function-emplace}

<div class="function-declaration" id="function-emplace-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... Args&gt; <a href="../small-vector.html#symbol-small-vector-iterator">iterator</a> emplace(
    <a href="../small-vector.html#symbol-small-vector-const-iterator">const_iterator</a> position,
    Args &amp;&amp;...args);</code></pre></div>

<p>Constructs an element before <code>position</code> (which may be <code><a href="#function-end-1">end</a>()</code>). Arguments may refer to existing elements. Requires a move-assignable <code>T</code> unless <code>T</code> is trivially copyable.</p>

#### `emplace_back` {#function-emplace-back}

<div class="function-declaration" id="function-emplace-back-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... Args&gt; T &amp;emplace_back(
    Args &amp;&amp;...args);</code></pre></div>

<p>Constructs an element at the end and returns it. Arguments may refer to existing elements; they are read before any reallocation.</p>

#### `empty` {#function-empty}

<div class="function-declaration" id="function-empty-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> empty() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `end` {#function-end}

<div class="function-declaration" id="function-end-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../small-vector.html#symbol-small-vector-iterator">iterator</a> end() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-end-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../small-vector.html#symbol-small-vector-const-iterator">const_iterator</a> end() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `erase` {#function-erase}

<div class="function-declaration" id="function-erase-1"><pre><code><a href="../small-vector.html#symbol-small-vector-iterator">iterator</a> erase(
    <a href="../small-vector.html#symbol-small-vector-const-iterator">const_iterator</a> position);</code></pre></div>

<p>Erases the element at <code>position</code>. Returns the iterator following it.</p>

<div class="function-declaration" id="function-erase-2"><pre><code><a href="../small-vector.html#symbol-small-vector-iterator">iterator</a> erase(
    <a href="../small-vector.html#symbol-small-vector-const-iterator">const_iterator</a> first,
    <a href="../small-vector.html#symbol-small-vector-const-iterator">const_iterator</a> last);</code></pre></div>

<p>Erases [first, last). Returns the iterator following the last erased element.</p>

#### `front` {#function-front}

<div class="function-declaration" id="function-front-1"><pre><code>[[<span class="na">nodiscard</span>]] T &amp;front() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-front-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> T &amp;front() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `insert` {#function-insert}

<div class="function-declaration" id="function-insert-1"><pre><code><a href="../small-vector.html#symbol-small-vector-iterator">iterator</a> insert(
    <a href="../small-vector.html#symbol-small-vector-const-iterator">const_iterator</a> position,
    <span class="k">const</span> T &amp;value);</code></pre></div>

<p>Inserts a copy of <code>value</code> before <code>position</code>. Returns an iterator to the new element.</p>

<div class="function-declaration" id="function-insert-2"><pre><code><a href="../small-vector.html#symbol-small-vector-iterator">iterator</a> insert(
    <a href="../small-vector.html#symbol-small-vector-const-iterator">const_iterator</a> position,
    T &amp;&amp;value);</code></pre></div>

<div class="function-declaration" id="function-insert-3"><pre><code><span class="k">template</span> &lt;std::input_iterator Iterator&gt; <a href="../small-vector.html#symbol-small-vector-iterator">iterator</a> insert(
    <a href="../small-vector.html#symbol-small-vector-const-iterator">const_iterator</a> position,
    Iterator first,
    Iterator last);</code></pre></div>

<p>Inserts a copy of the range before <code>position</code> (which may be <code><a href="#function-end-1">end</a>()</code>). The range must not alias this vector. Returns an iterator to the first inserted element. Single-pass (input) iterators are collected into a temporary vector first.</p>

<div class="function-declaration" id="function-insert-4"><pre><code><a href="../small-vector.html#symbol-small-vector-iterator">iterator</a> insert(
    <a href="../small-vector.html#symbol-small-vector-const-iterator">const_iterator</a> position,
    std::initializer_list&lt;T&gt; values);</code></pre></div>

#### `is_inline` {#function-is-inline}

<div class="function-declaration" id="function-is-inline-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_inline() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the elements live inside the object (no heap allocation).</p>

#### `max_size` {#function-max-size}

<div class="function-declaration" id="function-max-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="../../types.html#symbol-usize">usize</a> max_size() <span class="k">noexcept</span>;</code></pre></div>

#### `pop_back` {#function-pop-back}

<div class="function-declaration" id="function-pop-back-1"><pre><code><span class="k">void</span> pop_back() <span class="k">noexcept</span>;</code></pre></div>

<p>Destroys the last element. Asserts when empty.</p>

#### `push_back` {#function-push-back}

<div class="function-declaration" id="function-push-back-1"><pre><code><span class="k">void</span> push_back(
    <span class="k">const</span> T &amp;value);</code></pre></div>

<p>Appends a copy of <code>value</code>. <code>value</code> may refer to an element of this vector.</p>

<div class="function-declaration" id="function-push-back-2"><pre><code><span class="k">void</span> push_back(
    T &amp;&amp;value);</code></pre></div>

<p>Appends by move.</p>

#### `rbegin` {#function-rbegin}

<div class="function-declaration" id="function-rbegin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../small-vector.html#symbol-small-vector-reverse-iterator">reverse_iterator</a> rbegin() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-rbegin-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../small-vector.html#symbol-small-vector-const-reverse-iterator">const_reverse_iterator</a> rbegin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `rend` {#function-rend}

<div class="function-declaration" id="function-rend-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../small-vector.html#symbol-small-vector-reverse-iterator">reverse_iterator</a> rend() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-rend-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../small-vector.html#symbol-small-vector-const-reverse-iterator">const_reverse_iterator</a> rend() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `reserve` {#function-reserve}

<div class="function-declaration" id="function-reserve-1"><pre><code><span class="k">void</span> reserve(
    <a href="../../types.html#symbol-usize">usize</a> count);</code></pre></div>

<p>Makes room for at least <code>count</code> elements without constructing any.</p>

#### `resize` {#function-resize}

<div class="function-declaration" id="function-resize-1"><pre><code><span class="k">void</span> resize(
    <a href="../../types.html#symbol-usize">usize</a> count);</code></pre></div>

<p>Grows with value-initialised elements or shrinks by destroying the tail.</p>

<div class="function-declaration" id="function-resize-2"><pre><code><span class="k">void</span> resize(
    <a href="../../types.html#symbol-usize">usize</a> count,
    <span class="k">const</span> T &amp;value);</code></pre></div>

<p>Grows with copies of <code>value</code> or shrinks by destroying the tail. <code>value</code> may refer to an element of this vector.</p>

#### `shrink_to_fit` {#function-shrink-to-fit}

<div class="function-declaration" id="function-shrink-to-fit-1"><pre><code><span class="k">void</span> shrink_to_fit();</code></pre></div>

<p>Moves the elements back inline when they fit, or trims heap capacity to <code><a href="#function-size-1">size</a>()</code>.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `swap` {#function-swap}

<div class="function-declaration" id="function-swap-1"><pre><code><span class="k">void</span> swap(
    <a href="small-vector.html">small_vector</a> &amp;other) <span class="k">noexcept</span>;</code></pre></div>

<p>Exchanges contents with another vector of the same type.</p>

### Private

#### `allocate_storage` {#function-allocate-storage}

<div class="function-declaration" id="function-allocate-storage-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> T *allocate_storage(
    <a href="../../types.html#symbol-usize">usize</a> count);</code></pre></div>

#### `append_range` {#function-append-range}

<div class="function-declaration" id="function-append-range-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Iterator&gt; <span class="k">void</span> append_range(
    Iterator first,
    Iterator last);</code></pre></div>

#### `assign_range` {#function-assign-range}

<div class="function-declaration" id="function-assign-range-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Iterator&gt; <span class="k">void</span> assign_range(
    Iterator first,
    Iterator last);</code></pre></div>

#### `deallocate_storage` {#function-deallocate-storage}

<div class="function-declaration" id="function-deallocate-storage-1"><pre><code><span class="k">static</span> <span class="k">void</span> deallocate_storage(
    T *storage,
    <a href="../../types.html#symbol-usize">usize</a> count) <span class="k">noexcept</span>;</code></pre></div>

#### `destroy_range` {#function-destroy-range}

<div class="function-declaration" id="function-destroy-range-1"><pre><code><span class="k">static</span> <span class="k">void</span> destroy_range(
    T *first,
    <a href="../../types.html#symbol-usize">usize</a> count) <span class="k">noexcept</span>;</code></pre></div>

#### `emplace_with_growth` {#function-emplace-with-growth}

<div class="function-declaration" id="function-emplace-with-growth-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... Args&gt; T *emplace_with_growth(
    <a href="../../types.html#symbol-usize">usize</a> index,
    Args &amp;&amp;...args);</code></pre></div>

<p>Reallocates leaving an uninitialised hole at <code>index</code> and constructs the new element there first, so arguments aliasing old elements are still intact when read.</p>

#### `grown_capacity` {#function-grown-capacity}

<div class="function-declaration" id="function-grown-capacity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> grown_capacity(
    <a href="../../types.html#symbol-usize">usize</a> required) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `index_of` {#function-index-of}

<div class="function-declaration" id="function-index-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> index_of(
    <a href="../small-vector.html#symbol-small-vector-const-iterator">const_iterator</a> position) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `inline_data` {#function-inline-data}

<div class="function-declaration" id="function-inline-data-1"><pre><code>[[<span class="na">nodiscard</span>]] T *inline_data() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-inline-data-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> T *inline_data() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `open_gap` {#function-open-gap}

<div class="function-declaration" id="function-open-gap-1"><pre><code><span class="k">void</span> open_gap(
    <a href="../../types.html#symbol-usize">usize</a> index,
    <a href="../../types.html#symbol-usize">usize</a> count);</code></pre></div>

<p>Shifts [index, size) right by <code>count</code> leaving uninitialised storage in the gap. <code>mSize</code> is not updated; the caller fills the gap and adds <code>count</code>.</p>

#### `reallocate` {#function-reallocate}

<div class="function-declaration" id="function-reallocate-1"><pre><code><span class="k">void</span> reallocate(
    <a href="../../types.html#symbol-usize">usize</a> newCapacity);</code></pre></div>

#### `release_heap` {#function-release-heap}

<div class="function-declaration" id="function-release-heap-1"><pre><code><span class="k">void</span> release_heap() <span class="k">noexcept</span>;</code></pre></div>

#### `relocate_range` {#function-relocate-range}

<div class="function-declaration" id="function-relocate-range-1"><pre><code><span class="k">static</span> <span class="k">void</span> relocate_range(
    T *source,
    T *destination,
    <a href="../../types.html#symbol-usize">usize</a> count) <span class="k">noexcept</span>;</code></pre></div>

<p>Moves <code>count</code> elements from <code>source</code> to uninitialised <code>destination</code> and ends the lifetime of the sources. The ranges must not overlap.</p>

#### `take_from` {#function-take-from}

<div class="function-declaration" id="function-take-from-1"><pre><code><span class="k">void</span> take_from(
    <a href="small-vector.html">small_vector</a> &amp;other) <span class="k">noexcept</span>;</code></pre></div>

<p>Steals the heap buffer of <code>other</code> or relocates its inline elements. <code>other</code> ends up empty and inline. This vector must be empty and inline.</p>

## Operator Overloads

### Public

#### `operator<=>` {#operator-3c--3d--3e}

<div class="function-declaration" id="operator-3c--3d--3e-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">auto</span> <span class="k">operator</span>&lt;=&gt;(
    <span class="k">const</span> <a href="small-vector.html">small_vector</a> &amp;other) <span class="k">const</span> <span class="k">requires</span> <a href="../detail.html#symbol-detail-synth-three-way-comparable">detail::synth_three_way_comparable</a>&lt;T&gt;;</code></pre></div>

<p>Lexicographical ordering like <code>std::vector</code>&#x27;s: uses <code>T</code>&#x27;s <code>operator&lt;=&gt;</code> when it has one and falls back to operator&lt; (weak ordering) otherwise. Absent for element types that are not even less-than comparable.</p>

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="small-vector.html">small_vector</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="small-vector.html">small_vector</a> &amp;other);</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="small-vector.html">small_vector</a> &amp;<span class="k">operator</span>=(
    <a href="small-vector.html">small_vector</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-3"><pre><code><a href="small-vector.html">small_vector</a> &amp;<span class="k">operator</span>=(
    std::initializer_list&lt;T&gt; values);</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="small-vector.html">small_vector</a> &amp;other) <span class="k">const</span>;</code></pre></div>

#### `operator[]` {#operator-5b--5d}

<div class="function-declaration" id="operator-5b--5d-1"><pre><code>[[<span class="na">nodiscard</span>]] T &amp;<span class="k">operator</span>[](
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="operator-5b--5d-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> T &amp;<span class="k">operator</span>[](
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
