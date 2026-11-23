---
kind: class
---

[Engine](../../../../engine.md) → [Core](../../../core.md) → [stable_vector.h](../stable-vector.md)

# stable_vector
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/containers/stable_vector.h` |
| Source File Path | N/A |
| API ↗ | [Containers](../../../../../api/core/containers.md) |

## Syntax

```cpp
template <typename T, usize ChunkSize = 64> class stable_vector
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">stable_vector() = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">stable_vector(
    <span class="k">const</span> <a href="stable-vector.html">stable_vector</a> &amp;other);</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">stable_vector(
    <a href="stable-vector.html">stable_vector</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~stable_vector();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">CHUNK_SIZE</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature">ChunkSize</code></td><td>Elements per chunk.</td></tr>
<tr><td>private</td><td><code class="signature">mChunks</code></td><td><code class="signature">std::vector&lt;T *&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSize</code></td><td><code class="signature"><a href="../../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

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

<div class="function-declaration" id="function-begin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../stable-vector.html#symbol-stable-vector-iterator">iterator</a> begin() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-begin-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../stable-vector.html#symbol-stable-vector-const-iterator">const_iterator</a> begin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `capacity` {#function-capacity}

<div class="function-declaration" id="function-capacity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> capacity() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Elements that fit in the allocated chunks.</p>

#### `cbegin` {#function-cbegin}

<div class="function-declaration" id="function-cbegin-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../stable-vector.html#symbol-stable-vector-const-iterator">const_iterator</a> cbegin() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `cend` {#function-cend}

<div class="function-declaration" id="function-cend-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../stable-vector.html#symbol-stable-vector-const-iterator">const_iterator</a> cend() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `chunk_count` {#function-chunk-count}

<div class="function-declaration" id="function-chunk-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> chunk_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Destroys every element; chunks are kept so <code><a href="#function-capacity-1">capacity</a>()</code> is unchanged.</p>

#### `emplace_back` {#function-emplace-back}

<div class="function-declaration" id="function-emplace-back-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span>... Args&gt; T &amp;emplace_back(
    Args &amp;&amp;...args);</code></pre></div>

<p>Constructs an element at the end and returns a reference that stays valid until the element is destroyed (<code>pop_back</code>, clear, destruction). Never moves other elements.</p>

#### `empty` {#function-empty}

<div class="function-declaration" id="function-empty-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> empty() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `end` {#function-end}

<div class="function-declaration" id="function-end-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../stable-vector.html#symbol-stable-vector-iterator">iterator</a> end() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-end-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../stable-vector.html#symbol-stable-vector-const-iterator">const_iterator</a> end() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `front` {#function-front}

<div class="function-declaration" id="function-front-1"><pre><code>[[<span class="na">nodiscard</span>]] T &amp;front() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-front-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> T &amp;front() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `pop_back` {#function-pop-back}

<div class="function-declaration" id="function-pop-back-1"><pre><code><span class="k">void</span> pop_back() <span class="k">noexcept</span>;</code></pre></div>

<p>Destroys the last element. Asserts when empty. Chunks are kept.</p>

#### `push_back` {#function-push-back}

<div class="function-declaration" id="function-push-back-1"><pre><code>T &amp;push_back(
    <span class="k">const</span> T &amp;value);</code></pre></div>

<div class="function-declaration" id="function-push-back-2"><pre><code>T &amp;push_back(
    T &amp;&amp;value);</code></pre></div>

#### `reserve` {#function-reserve}

<div class="function-declaration" id="function-reserve-1"><pre><code><span class="k">void</span> reserve(
    <a href="../../types.html#symbol-usize">usize</a> count);</code></pre></div>

<p>Allocates chunks so that <code>count</code> elements fit without further allocation.</p>

#### `shrink_to_fit` {#function-shrink-to-fit}

<div class="function-declaration" id="function-shrink-to-fit-1"><pre><code><span class="k">void</span> shrink_to_fit() <span class="k">noexcept</span>;</code></pre></div>

<p>Frees chunks that hold no element.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `allocate_chunk` {#function-allocate-chunk}

<div class="function-declaration" id="function-allocate-chunk-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> T *allocate_chunk();</code></pre></div>

#### `deallocate_chunk` {#function-deallocate-chunk}

<div class="function-declaration" id="function-deallocate-chunk-1"><pre><code><span class="k">static</span> <span class="k">void</span> deallocate_chunk(
    T *chunk) <span class="k">noexcept</span>;</code></pre></div>

#### `release_chunks` {#function-release-chunks}

<div class="function-declaration" id="function-release-chunks-1"><pre><code><span class="k">void</span> release_chunks() <span class="k">noexcept</span>;</code></pre></div>

#### `slot_at` {#function-slot-at}

<div class="function-declaration" id="function-slot-at-1"><pre><code>[[<span class="na">nodiscard</span>]] T *slot_at(
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="stable-vector.html">stable_vector</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="stable-vector.html">stable_vector</a> &amp;other);</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="stable-vector.html">stable_vector</a> &amp;<span class="k">operator</span>=(
    <a href="stable-vector.html">stable_vector</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

#### `operator[]` {#operator-5b--5d}

<div class="function-declaration" id="operator-5b--5d-1"><pre><code>[[<span class="na">nodiscard</span>]] T &amp;<span class="k">operator</span>[](
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="operator-5b--5d-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> T &amp;<span class="k">operator</span>[](
    <a href="../../types.html#symbol-usize">usize</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
