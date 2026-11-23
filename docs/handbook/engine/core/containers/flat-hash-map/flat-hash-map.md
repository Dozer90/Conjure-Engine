---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [flat_hash_map.h](../flat-hash-map.md)

# flat_hash_map
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/containers/flat_hash_map.h` |
| Source File Path | N/A |
| API ↗ | [Containers](../../../../../api/core/containers.md) |

## Syntax

```cpp
template <typename K, typename V, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>> class flat_hash_map
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">flat_hash_map() = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> flat_hash_map(
    <a href="../../types.html#symbol-usize">usize</a> initialCapacity);</code></td><td>Pre-sizes the table for <code>initialCapacity</code> elements; 0 allocates nothing.</td></tr>
<tr><td>public</td><td><code class="signature">flat_hash_map(
    std::initializer_list&lt;<a href="../flat-hash-map.html#symbol-flat-hash-map-value-type">value_type</a>&gt; values);</code></td><td>Builds the map from pairs; for duplicate keys the first one wins.</td></tr>
<tr><td>public</td><td><code class="signature">flat_hash_map(
    <span class="k">const</span> <a href="flat-hash-map.html">flat_hash_map</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">flat_hash_map(
    <a href="flat-hash-map.html">flat_hash_map</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~flat_hash_map() = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">MAX_CAPACITY</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature">table_<wbr>type::<wbr>MAX_<wbr>CAPACITY</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">MAX_<wbr>LOAD_<wbr>FACTOR</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../types.html#symbol-f32">f32</a></code></td><td><code class="signature">table_<wbr>type::<wbr>MAX_<wbr>LOAD_<wbr>FACTOR</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTable</code></td><td><code class="signature"><a href="../flat-hash-map.html#symbol-flat-hash-map-table-type">table_type</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `at` {#function-at}

<div class="function-declaration" id="function-at-1"><pre><code>[[<span class="na">nodiscard</span>]] V &amp;at(
    <span class="k">const</span> K &amp;key) <span class="k">noexcept</span>;</code></pre></div>

<p>The value for <code>key</code>. The key must be present: a missing key is a fatal programmer error (asserts, then terminates), never an exception.</p>

<div class="function-declaration" id="function-at-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> V &amp;at(
    <span class="k">const</span> K &amp;key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `begin` {#function-begin}

<div class="function-declaration" id="function-begin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-flat-hash-map-iterator">iterator</a> begin() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-begin-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-flat-hash-map-const-iterator">const_iterator</a> begin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `capacity` {#function-capacity}

<div class="function-declaration" id="function-capacity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> capacity() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Buckets (a power of two, 0 before the first insertion). The table rehashes only when <code><a href="#function-size-1">size</a>()</code> would exceed <code><a href="#function-capacity-1">capacity</a>()</code> * <code>MAX_LOAD_FACTOR</code>.</p>

#### `cbegin` {#function-cbegin}

<div class="function-declaration" id="function-cbegin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-flat-hash-map-const-iterator">const_iterator</a> cbegin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `cend` {#function-cend}

<div class="function-declaration" id="function-cend-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-flat-hash-map-const-iterator">const_iterator</a> cend() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Destroys every element; keeps the table so <code><a href="#function-capacity-1">capacity</a>()</code> is unchanged.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> K &amp;key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `count` {#function-count}

<div class="function-declaration" id="function-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> count(
    <span class="k">const</span> K &amp;key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>0 or 1.</p>

#### `emplace` {#function-emplace}

<div class="function-declaration" id="function-emplace-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... Args&gt; std::pair&lt;iterator, <span class="k">bool</span>&gt; emplace(
    Args &amp;&amp;...args);</code></pre></div>

<p>Constructs a pair from the arguments and inserts it when its key is absent.</p>

#### `empty` {#function-empty}

<div class="function-declaration" id="function-empty-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> empty() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `end` {#function-end}

<div class="function-declaration" id="function-end-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-flat-hash-map-iterator">iterator</a> end() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-end-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-flat-hash-map-const-iterator">const_iterator</a> end() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `erase` {#function-erase}

<div class="function-declaration" id="function-erase-1"><pre><code><span class="k">bool</span> erase(
    <span class="k">const</span> K &amp;key) <span class="k">noexcept</span>;</code></pre></div>

<p>Removes the element with <code>key</code>. Returns <code>false</code> when absent. Never rehashes.</p>

<div class="function-declaration" id="function-erase-2"><pre><code><a href="../flat-hash-map.html#symbol-flat-hash-map-iterator">iterator</a> erase(
    <a href="../flat-hash-map.html#symbol-flat-hash-map-const-iterator">const_iterator</a> position) <span class="k">noexcept</span>;</code></pre></div>

<p>Removes the element at <code>position</code>. Returns the iterator following it.</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-flat-hash-map-iterator">iterator</a> find(
    <span class="k">const</span> K &amp;key) <span class="k">noexcept</span>;</code></pre></div>

<p>The element with key <code>key</code>, or <code><a href="#function-end-1">end</a>()</code>.</p>

<div class="function-declaration" id="function-find-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../flat-hash-map.html#symbol-flat-hash-map-const-iterator">const_iterator</a> find(
    <span class="k">const</span> K &amp;key) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `hash_function` {#function-hash-function}

<div class="function-declaration" id="function-hash-function-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> Hash &amp;hash_function() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `insert` {#function-insert}

<div class="function-declaration" id="function-insert-1"><pre><code>std::pair&lt;iterator, <span class="k">bool</span>&gt; insert(
    <span class="k">const</span> <a href="../flat-hash-map.html#symbol-flat-hash-map-value-type">value_type</a> &amp;value);</code></pre></div>

<p>Inserts when the key is absent. Returns the element and whether it was inserted.</p>

<div class="function-declaration" id="function-insert-2"><pre><code>std::pair&lt;iterator, <span class="k">bool</span>&gt; insert(
    <a href="../flat-hash-map.html#symbol-flat-hash-map-value-type">value_type</a> &amp;&amp;value);</code></pre></div>

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

#### `key_eq` {#function-key-eq}

<div class="function-declaration" id="function-key-eq-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> Eq &amp;key_eq() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `load_factor` {#function-load-factor}

<div class="function-declaration" id="function-load-factor-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-f32">f32</a> load_factor() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `reserve` {#function-reserve}

<div class="function-declaration" id="function-reserve-1"><pre><code><span class="k">void</span> reserve(
    <a href="../../types.html#symbol-usize">usize</a> count);</code></pre></div>

<p>Makes room for <code>count</code> elements without a rehash during the next insertions. <code><a href="#function-reserve-1">reserve</a>(0)</code> does nothing, so an empty map stays unallocated.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `swap` {#function-swap}

<div class="function-declaration" id="function-swap-1"><pre><code><span class="k">void</span> swap(
    <a href="flat-hash-map.html">flat_hash_map</a> &amp;other) <span class="k">noexcept</span>;</code></pre></div>

#### `try_emplace` {#function-try-emplace}

<div class="function-declaration" id="function-try-emplace-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... Args&gt; std::pair&lt;iterator, <span class="k">bool</span>&gt; try_emplace(
    <span class="k">const</span> K &amp;key,
    Args &amp;&amp;...args);</code></pre></div>

<p>Inserts {key, <code>V(args...)</code>} when <code>key</code> is absent; the value is only constructed in that case. Returns the element and whether it was inserted.</p>

<div class="function-declaration" id="function-try-emplace-2"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... Args&gt; std::pair&lt;iterator, <span class="k">bool</span>&gt; try_emplace(
    K &amp;&amp;key,
    Args &amp;&amp;...args);</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="flat-hash-map.html">flat_hash_map</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="flat-hash-map.html">flat_hash_map</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="flat-hash-map.html">flat_hash_map</a> &amp;<span class="k">operator</span>=(
    <a href="flat-hash-map.html">flat_hash_map</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="flat-hash-map.html">flat_hash_map</a> &amp;other) <span class="k">const</span>;</code></pre></div>

<p>Same keys with equal values, regardless of slot order.</p>

#### `operator[]` {#operator-5b--5d}

<div class="function-declaration" id="operator-5b--5d-1"><pre><code>[[<span class="na">nodiscard</span>]] V &amp;<span class="k">operator</span>[](
    <span class="k">const</span> K &amp;key);</code></pre></div>

<p>The value for <code>key</code>, default-constructed and inserted when absent.</p>

<div class="function-declaration" id="operator-5b--5d-2"><pre><code>[[<span class="na">nodiscard</span>]] V &amp;<span class="k">operator</span>[](
    K &amp;&amp;key);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
