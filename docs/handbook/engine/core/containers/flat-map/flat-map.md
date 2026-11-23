---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [flat_map.h](../flat-map.md)

# flat_map
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/containers/flat_map.h` |
| Source File Path | N/A |
| API ↗ | [Containers](../../../../../api/core/containers.md) |

## Syntax

```cpp
template <typename K, typename V, typename Compare = std::less<K>> class flat_map
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">flat_map() = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> flat_map(
    Compare compare);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flat_map(
    std::initializer_list&lt;<a href="../flat-map.html#symbol-flat-map-value-type">value_type</a>&gt; values);</code></td><td>Builds the map from pairs; for duplicate keys the first one wins.</td></tr>
<tr><td>public</td><td><code class="signature">flat_map(
    <span class="k">const</span> <a href="flat-map.html">flat_map</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flat_map(
    <a href="flat-map.html">flat_map</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~flat_map() = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mCompare</code></td><td><code class="signature">Compare</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mEntries</code></td><td><code class="signature"><a href="../flat-map.html#symbol-flat-map-storage-type">storage_type</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `at` {#function-at}

<div class="function-declaration" id="function-at-1"><pre><code>[[<span class="na">nodiscard</span>]] V &amp;at(
    <span class="k">const</span> K &amp;key);</code></pre></div>

<p>The value for <code>key</code>. The key must be present: a missing key is a fatal programmer error (asserts, then terminates), never an exception.</p>

<div class="function-declaration" id="function-at-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> V &amp;at(
    <span class="k">const</span> K &amp;key) <span class="k">const</span>;</code></pre></div>

<div class="function-declaration" id="function-at-3"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> KeyLike&gt; <span class="k">requires</span> <a href="../detail.html#symbol-detail-transparent-compare">detail::transparent_compare</a>&lt;Compare&gt; [[<span class="na">nodiscard</span>]] V &amp;at(
    <span class="k">const</span> KeyLike &amp;key);</code></pre></div>

<div class="function-declaration" id="function-at-4"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> KeyLike&gt; <span class="k">requires</span> <a href="../detail.html#symbol-detail-transparent-compare">detail::transparent_compare</a>&lt;Compare&gt; [[<span class="na">nodiscard</span>]] <span class="k">const</span> V &amp;at(
    <span class="k">const</span> KeyLike &amp;key) <span class="k">const</span>;</code></pre></div>

#### `begin` {#function-begin}

<div class="function-declaration" id="function-begin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-iterator">iterator</a> begin() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-begin-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-const-iterator">const_iterator</a> begin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `capacity` {#function-capacity}

<div class="function-declaration" id="function-capacity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> capacity() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `cbegin` {#function-cbegin}

<div class="function-declaration" id="function-cbegin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-const-iterator">const_iterator</a> cbegin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `cend` {#function-cend}

<div class="function-declaration" id="function-cend-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-const-iterator">const_iterator</a> cend() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> K &amp;key) <span class="k">const</span>;</code></pre></div>

<div class="function-declaration" id="function-contains-2"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> KeyLike&gt; <span class="k">requires</span> <a href="../detail.html#symbol-detail-transparent-compare">detail::transparent_compare</a>&lt;Compare&gt; [[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> KeyLike &amp;key) <span class="k">const</span>;</code></pre></div>

#### `count` {#function-count}

<div class="function-declaration" id="function-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> count(
    <span class="k">const</span> K &amp;key) <span class="k">const</span>;</code></pre></div>

<p>0 or 1.</p>

<div class="function-declaration" id="function-count-2"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> KeyLike&gt; <span class="k">requires</span> <a href="../detail.html#symbol-detail-transparent-compare">detail::transparent_compare</a>&lt;Compare&gt; [[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> count(
    <span class="k">const</span> KeyLike &amp;key) <span class="k">const</span>;</code></pre></div>

#### `emplace` {#function-emplace}

<div class="function-declaration" id="function-emplace-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... Args&gt; std::pair&lt;iterator, <span class="k">bool</span>&gt; emplace(
    Args &amp;&amp;...args);</code></pre></div>

<p>Constructs a pair from the arguments and inserts it when its key is absent.</p>

#### `empty` {#function-empty}

<div class="function-declaration" id="function-empty-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> empty() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `end` {#function-end}

<div class="function-declaration" id="function-end-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-iterator">iterator</a> end() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-end-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-const-iterator">const_iterator</a> end() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `entries` {#function-entries}

<div class="function-declaration" id="function-entries-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../flat-map.html#symbol-flat-map-value-type">value_type</a>&gt; entries() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The sorted pairs as a contiguous read-only view.</p>

#### `erase` {#function-erase}

<div class="function-declaration" id="function-erase-1"><pre><code><span class="k">bool</span> erase(
    <span class="k">const</span> K &amp;key);</code></pre></div>

<p>Removes the element with <code>key</code>. Returns <code>false</code> when absent.</p>

<div class="function-declaration" id="function-erase-2"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> KeyLike&gt; <span class="k">requires</span> <a href="../detail.html#symbol-detail-transparent-compare">detail::transparent_compare</a>&lt;Compare&gt; &amp;&amp; (!std::is_convertible_v&lt;<span class="k">const</span> KeyLike &amp;, <a href="../flat-map.html#symbol-flat-map-const-iterator">const_iterator</a>&gt;) &amp;&amp; (!std::is_convertible_v&lt;<span class="k">const</span> KeyLike &amp;, <a href="../flat-map.html#symbol-flat-map-iterator">iterator</a>&gt;) <span class="k">bool</span> erase(
    <span class="k">const</span> KeyLike &amp;key);</code></pre></div>

<p>Removes the element whose key compares equal to <code>key</code> (transparent comparators only). Returns <code>false</code> when absent. Iterators are excluded so that they reach <code><a href="#function-erase-1">erase</a>(position)</code>.</p>

<div class="function-declaration" id="function-erase-3"><pre><code><a href="../flat-map.html#symbol-flat-map-iterator">iterator</a> erase(
    <a href="../flat-map.html#symbol-flat-map-const-iterator">const_iterator</a> position);</code></pre></div>

<p>Removes the element at <code>position</code>. Returns the iterator following it.</p>

<div class="function-declaration" id="function-erase-4"><pre><code><a href="../flat-map.html#symbol-flat-map-iterator">iterator</a> erase(
    <a href="../flat-map.html#symbol-flat-map-const-iterator">const_iterator</a> first,
    <a href="../flat-map.html#symbol-flat-map-const-iterator">const_iterator</a> last);</code></pre></div>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-iterator">iterator</a> find(
    <span class="k">const</span> K &amp;key);</code></pre></div>

<p>The element with key <code>key</code>, or <code><a href="#function-end-1">end</a>()</code>.</p>

<div class="function-declaration" id="function-find-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-const-iterator">const_iterator</a> find(
    <span class="k">const</span> K &amp;key) <span class="k">const</span>;</code></pre></div>

<div class="function-declaration" id="function-find-3"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> KeyLike&gt; <span class="k">requires</span> <a href="../detail.html#symbol-detail-transparent-compare">detail::transparent_compare</a>&lt;Compare&gt; [[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-iterator">iterator</a> find(
    <span class="k">const</span> KeyLike &amp;key);</code></pre></div>

<p>Heterogeneous lookup, available when Compare declares <code>is_transparent</code> (e.g. finding by <code>std::string_view</code> in a <code>std::string</code> keyed map).</p>

<div class="function-declaration" id="function-find-4"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> KeyLike&gt; <span class="k">requires</span> <a href="../detail.html#symbol-detail-transparent-compare">detail::transparent_compare</a>&lt;Compare&gt; [[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-const-iterator">const_iterator</a> find(
    <span class="k">const</span> KeyLike &amp;key) <span class="k">const</span>;</code></pre></div>

#### `insert` {#function-insert}

<div class="function-declaration" id="function-insert-1"><pre><code>std::pair&lt;iterator, <span class="k">bool</span>&gt; insert(
    <span class="k">const</span> <a href="../flat-map.html#symbol-flat-map-value-type">value_type</a> &amp;value);</code></pre></div>

<p>Inserts when the key is absent. Returns the element and whether it was inserted.</p>

<div class="function-declaration" id="function-insert-2"><pre><code>std::pair&lt;iterator, <span class="k">bool</span>&gt; insert(
    <a href="../flat-map.html#symbol-flat-map-value-type">value_type</a> &amp;&amp;value);</code></pre></div>

<div class="function-declaration" id="function-insert-3"><pre><code><span class="k">template</span> &lt;std::input_iterator Iterator&gt; <span class="k">void</span> insert(
    Iterator first,
    Iterator last);</code></pre></div>

<p>Inserts every element of the range that is not already present.</p>

#### `insert_or_assign` {#function-insert-or-assign}

<div class="function-declaration" id="function-insert-or-assign-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> M&gt; std::pair&lt;iterator, <span class="k">bool</span>&gt; insert_or_assign(
    <span class="k">const</span> K &amp;key,
    M &amp;&amp;value);</code></pre></div>

<p>Inserts or overwrites. Returns the element and <code>true</code> when it was newly inserted.</p>

<div class="function-declaration" id="function-insert-or-assign-2"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> M&gt; std::pair&lt;iterator, <span class="k">bool</span>&gt; insert_or_assign(
    K &amp;&amp;key,
    M &amp;&amp;value);</code></pre></div>

#### `key_comp` {#function-key-comp}

<div class="function-declaration" id="function-key-comp-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-key-compare">key_compare</a> key_comp() <span class="k">const</span>;</code></pre></div>

#### `lower_bound` {#function-lower-bound}

<div class="function-declaration" id="function-lower-bound-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-iterator">iterator</a> lower_bound(
    <span class="k">const</span> K &amp;key);</code></pre></div>

<p>First element whose key is not less than <code>key</code>.</p>

<div class="function-declaration" id="function-lower-bound-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-const-iterator">const_iterator</a> lower_bound(
    <span class="k">const</span> K &amp;key) <span class="k">const</span>;</code></pre></div>

<div class="function-declaration" id="function-lower-bound-3"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> KeyLike&gt; <span class="k">requires</span> <a href="../detail.html#symbol-detail-transparent-compare">detail::transparent_compare</a>&lt;Compare&gt; [[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-iterator">iterator</a> lower_bound(
    <span class="k">const</span> KeyLike &amp;key);</code></pre></div>

<div class="function-declaration" id="function-lower-bound-4"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> KeyLike&gt; <span class="k">requires</span> <a href="../detail.html#symbol-detail-transparent-compare">detail::transparent_compare</a>&lt;Compare&gt; [[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-const-iterator">const_iterator</a> lower_bound(
    <span class="k">const</span> KeyLike &amp;key) <span class="k">const</span>;</code></pre></div>

#### `rbegin` {#function-rbegin}

<div class="function-declaration" id="function-rbegin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-reverse-iterator">reverse_iterator</a> rbegin() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-rbegin-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-const-reverse-iterator">const_reverse_iterator</a> rbegin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `rend` {#function-rend}

<div class="function-declaration" id="function-rend-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-reverse-iterator">reverse_iterator</a> rend() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-rend-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-const-reverse-iterator">const_reverse_iterator</a> rend() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `reserve` {#function-reserve}

<div class="function-declaration" id="function-reserve-1"><pre><code><span class="k">void</span> reserve(
    <a href="../../types.html#symbol-usize">usize</a> count);</code></pre></div>

#### `shrink_to_fit` {#function-shrink-to-fit}

<div class="function-declaration" id="function-shrink-to-fit-1"><pre><code><span class="k">void</span> shrink_to_fit();</code></pre></div>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `swap` {#function-swap}

<div class="function-declaration" id="function-swap-1"><pre><code><span class="k">void</span> swap(
    <a href="flat-map.html">flat_map</a> &amp;other) <span class="k">noexcept</span>;</code></pre></div>

#### `try_emplace` {#function-try-emplace}

<div class="function-declaration" id="function-try-emplace-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... Args&gt; std::pair&lt;iterator, <span class="k">bool</span>&gt; try_emplace(
    <span class="k">const</span> K &amp;key,
    Args &amp;&amp;...args);</code></pre></div>

<p>Inserts {key, <code>V(args...)</code>} when <code>key</code> is absent; the value is only constructed in that case. Returns the element and whether it was inserted.</p>

<div class="function-declaration" id="function-try-emplace-2"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... Args&gt; std::pair&lt;iterator, <span class="k">bool</span>&gt; try_emplace(
    K &amp;&amp;key,
    Args &amp;&amp;...args);</code></pre></div>

#### `upper_bound` {#function-upper-bound}

<div class="function-declaration" id="function-upper-bound-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-iterator">iterator</a> upper_bound(
    <span class="k">const</span> K &amp;key);</code></pre></div>

<p>First element whose key is greater than <code>key</code>.</p>

<div class="function-declaration" id="function-upper-bound-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-const-iterator">const_iterator</a> upper_bound(
    <span class="k">const</span> K &amp;key) <span class="k">const</span>;</code></pre></div>

<div class="function-declaration" id="function-upper-bound-3"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> KeyLike&gt; <span class="k">requires</span> <a href="../detail.html#symbol-detail-transparent-compare">detail::transparent_compare</a>&lt;Compare&gt; [[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-iterator">iterator</a> upper_bound(
    <span class="k">const</span> KeyLike &amp;key);</code></pre></div>

<div class="function-declaration" id="function-upper-bound-4"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> KeyLike&gt; <span class="k">requires</span> <a href="../detail.html#symbol-detail-transparent-compare">detail::transparent_compare</a>&lt;Compare&gt; [[<span class="na">nodiscard</span>]] <a href="../flat-map.html#symbol-flat-map-const-iterator">const_iterator</a> upper_bound(
    <span class="k">const</span> KeyLike &amp;key) <span class="k">const</span>;</code></pre></div>

### Private

#### `find_in` {#function-find-in}

<div class="function-declaration" id="function-find-in-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Storage, <span class="k">typename</span> KeyLike&gt; [[<span class="na">nodiscard</span>]] <span class="k">auto</span> find_in(
    Storage &amp;storage,
    <span class="k">const</span> KeyLike &amp;key) <span class="k">const</span>;</code></pre></div>

#### `lower_bound_in` {#function-lower-bound-in}

<div class="function-declaration" id="function-lower-bound-in-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Storage, <span class="k">typename</span> KeyLike&gt; [[<span class="na">nodiscard</span>]] <span class="k">auto</span> lower_bound_in(
    Storage &amp;storage,
    <span class="k">const</span> KeyLike &amp;key) <span class="k">const</span>;</code></pre></div>

#### `upper_bound_in` {#function-upper-bound-in}

<div class="function-declaration" id="function-upper-bound-in-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Storage, <span class="k">typename</span> KeyLike&gt; [[<span class="na">nodiscard</span>]] <span class="k">auto</span> upper_bound_in(
    Storage &amp;storage,
    <span class="k">const</span> KeyLike &amp;key) <span class="k">const</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="flat-map.html">flat_map</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="flat-map.html">flat_map</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="flat-map.html">flat_map</a> &amp;<span class="k">operator</span>=(
    <a href="flat-map.html">flat_map</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="flat-map.html">flat_map</a> &amp;other) <span class="k">const</span>;</code></pre></div>

#### `operator[]` {#operator-5b--5d}

<div class="function-declaration" id="operator-5b--5d-1"><pre><code>[[<span class="na">nodiscard</span>]] V &amp;<span class="k">operator</span>[](
    <span class="k">const</span> K &amp;key);</code></pre></div>

<p>The value for <code>key</code>, default-constructed and inserted when absent.</p>

<div class="function-declaration" id="operator-5b--5d-2"><pre><code>[[<span class="na">nodiscard</span>]] V &amp;<span class="k">operator</span>[](
    K &amp;&amp;key);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
