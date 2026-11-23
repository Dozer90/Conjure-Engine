---
kind: class
---

[Engine](../../../engine.md) → [Core](../../core.md) → [bit_vector.h](../bit-vector.md)

# bit_vector
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::core` |
| Include File Path | `engine/include/conjure/core/bit_vector.h` |
| Source File Path | `engine/src/core/strings/bit_vector.cpp` |
| API ↗ | [Strings and identifiers](../../../../api/core/strings-and-identifiers.md) |

## Syntax

```cpp
class CONJURE_API bit_vector
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">bit_vector() = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> bit_vector(
    <a href="../types.html#symbol-usize">usize</a> size,
    <span class="k">bool</span> value = false);</code></td><td>Creates <code>size</code> bits, all set to <code>value</code>.</td></tr>
<tr><td>public</td><td><code class="signature">bit_vector(
    <span class="k">const</span> <a href="bit-vector.html">bit_vector</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">bit_vector(
    <a href="bit-vector.html">bit_vector</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td>Moving transfers the words without copying. The moved-from vector is empty (size 0, no words) and may be resized and used again; a self-move assignment changes nothing.</td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~bit_vector() = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mSize</code></td><td><code class="signature"><a href="../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWords</code></td><td><code class="signature">std::vector&lt;<a href="../types.html#symbol-u64">u64</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">NPOS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a></code></td><td><code class="signature">static_<wbr>cast&lt;<wbr><a href="../types.html#symbol-usize">usize</a>&gt;(-<span class="mi">1</span>)</code></td><td>Returned by the <code>find_</code>* functions when no matching bit exists.</td></tr>
<tr><td>public</td><td><code class="signature">WORD_BITS</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../types.html#symbol-usize">usize</a></code></td><td><code class="signature"><span class="mi">64</span></code></td><td>Bits per storage word.</td></tr>
</tbody>
</table>

## Functions

### Public

#### `all` {#function-all}

<div class="function-declaration" id="function-all-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> all() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when every bit is set (always <code>true</code> for an empty vector).</p>

#### `any` {#function-any}

<div class="function-declaration" id="function-any-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> any() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when at least one bit is set.</p>

#### `capacity` {#function-capacity}

<div class="function-declaration" id="function-capacity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-usize">usize</a> capacity() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of bits that fit without reallocating.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Removes every bit (size becomes 0). Storage is kept.</p>

#### `count` {#function-count}

<div class="function-declaration" id="function-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-usize">usize</a> count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of set bits.</p>

#### `empty` {#function-empty}

<div class="function-declaration" id="function-empty-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> empty() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when there are no bits at all.</p>

#### `find_first_clear` {#function-find-first-clear}

<div class="function-declaration" id="function-find-first-clear-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-usize">usize</a> find_first_clear(
    <a href="../types.html#symbol-usize">usize</a> from = <span class="mi">0</span>) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Index of the first clear bit at or after <code>from</code>, or <code>NPOS</code>.</p>

#### `find_first_set` {#function-find-first-set}

<div class="function-declaration" id="function-find-first-set-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-usize">usize</a> find_first_set(
    <a href="../types.html#symbol-usize">usize</a> from = <span class="mi">0</span>) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Index of the first set bit at or after <code>from</code>, or <code>NPOS</code>.</p>

#### `find_last_set` {#function-find-last-set}

<div class="function-declaration" id="function-find-last-set-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-usize">usize</a> find_last_set() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Index of the last set bit, or <code>NPOS</code>.</p>

#### `flip` {#function-flip}

<div class="function-declaration" id="function-flip-1"><pre><code><span class="k">void</span> flip(
    <a href="../types.html#symbol-usize">usize</a> index) <span class="k">noexcept</span>;</code></pre></div>

<p>Inverts the bit at <code>index</code>. Asserts on an out-of-range index and does nothing.</p>

#### `flip_all` {#function-flip-all}

<div class="function-declaration" id="function-flip-all-1"><pre><code><span class="k">void</span> flip_all() <span class="k">noexcept</span>;</code></pre></div>

<p>Inverts every bit.</p>

#### `intersects` {#function-intersects}

<div class="function-declaration" id="function-intersects-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> intersects(
    <span class="k">const</span> <a href="bit-vector.html">bit_vector</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the two vectors share at least one set bit.</p>

#### `is_subset_of` {#function-is-subset-of}

<div class="function-declaration" id="function-is-subset-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_subset_of(
    <span class="k">const</span> <a href="bit-vector.html">bit_vector</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when every set bit of this vector is also set in <code>other</code>.</p>

#### `none` {#function-none}

<div class="function-declaration" id="function-none-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> none() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when no bit is set (always <code>true</code> for an empty vector).</p>

#### `push_back` {#function-push-back}

<div class="function-declaration" id="function-push-back-1"><pre><code><span class="k">void</span> push_back(
    <span class="k">bool</span> value);</code></pre></div>

<p>Appends one bit.</p>

#### `remove` {#function-remove}

<div class="function-declaration" id="function-remove-1"><pre><code><a href="bit-vector.html">bit_vector</a> &amp;remove(
    <span class="k">const</span> <a href="bit-vector.html">bit_vector</a> &amp;other) <span class="k">noexcept</span>;</code></pre></div>

<p>Clears every bit that is set in <code>other</code> (this &amp; ~other).</p>

#### `reserve` {#function-reserve}

<div class="function-declaration" id="function-reserve-1"><pre><code><span class="k">void</span> reserve(
    <a href="../types.html#symbol-usize">usize</a> bits);</code></pre></div>

<p>Ensures room for <code>bits</code> bits without changing <code><a href="#function-size-1">size</a>()</code>.</p>

#### `reset` {#function-reset}

<div class="function-declaration" id="function-reset-1"><pre><code><span class="k">void</span> reset(
    <a href="../types.html#symbol-usize">usize</a> index) <span class="k">noexcept</span>;</code></pre></div>

<p>Clears the bit at <code>index</code>. Asserts on an out-of-range index and does nothing.</p>

#### `reset_all` {#function-reset-all}

<div class="function-declaration" id="function-reset-all-1"><pre><code><span class="k">void</span> reset_all() <span class="k">noexcept</span>;</code></pre></div>

<p>Clears every bit (size is unchanged).</p>

#### `resize` {#function-resize}

<div class="function-declaration" id="function-resize-1"><pre><code><span class="k">void</span> resize(
    <a href="../types.html#symbol-usize">usize</a> size,
    <span class="k">bool</span> value = false);</code></pre></div>

<p>Changes the number of bits. New bits take <code>value</code>; removed bits are discarded.</p>

#### `set` {#function-set}

<div class="function-declaration" id="function-set-1"><pre><code><span class="k">void</span> set(
    <a href="../types.html#symbol-usize">usize</a> index) <span class="k">noexcept</span>;</code></pre></div>

<p>Sets the bit at <code>index</code> to 1. Asserts on an out-of-range index and does nothing.</p>

<div class="function-declaration" id="function-set-2"><pre><code><span class="k">void</span> set(
    <a href="../types.html#symbol-usize">usize</a> index,
    <span class="k">bool</span> value) <span class="k">noexcept</span>;</code></pre></div>

<p>Sets the bit at <code>index</code> to <code>value</code>.</p>

#### `set_all` {#function-set-all}

<div class="function-declaration" id="function-set-all-1"><pre><code><span class="k">void</span> set_all() <span class="k">noexcept</span>;</code></pre></div>

<p>Sets every bit.</p>

#### `set_bits` {#function-set-bits}

<div class="function-declaration" id="function-set-bits-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="bit-vector-set-bit-range.html">set_bit_range</a> set_bits() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Range over the indices of the set bits in ascending order. The vector must not be modified while iterating.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of bits.</p>

#### `test` {#function-test}

<div class="function-declaration" id="function-test-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> test(
    <a href="../types.html#symbol-usize">usize</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The bit at <code>index</code>. Asserts on an out-of-range index and returns <code>false</code>.</p>

#### `word_count` {#function-word-count}

<div class="function-declaration" id="function-word-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../types.html#symbol-usize">usize</a> word_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of 64-bit words backing the bits.</p>

#### `words` {#function-words}

<div class="function-declaration" id="function-words-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../types.html#symbol-u64">u64</a>&gt; words() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The packed words, least significant bit first. Bits past <code><a href="#function-size-1">size</a>()</code> are zero.</p>

### Private

#### `clear_trailing_bits` {#function-clear-trailing-bits}

<div class="function-declaration" id="function-clear-trailing-bits-1"><pre><code><span class="k">void</span> clear_trailing_bits() <span class="k">noexcept</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator&=` {#operator-26--3d}

<div class="function-declaration" id="operator-26--3d-1"><pre><code><a href="bit-vector.html">bit_vector</a> &amp;<span class="k">operator</span>&amp;=(
    <span class="k">const</span> <a href="bit-vector.html">bit_vector</a> &amp;other) <span class="k">noexcept</span>;</code></pre></div>

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="bit-vector.html">bit_vector</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="bit-vector.html">bit_vector</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="bit-vector.html">bit_vector</a> &amp;<span class="k">operator</span>=(
    <a href="bit-vector.html">bit_vector</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="bit-vector.html">bit_vector</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator[]` {#operator-5b--5d}

<div class="function-declaration" id="operator-5b--5d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>[](
    <a href="../types.html#symbol-usize">usize</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operator^=` {#operator-5e--3d}

<div class="function-declaration" id="operator-5e--3d-1"><pre><code><a href="bit-vector.html">bit_vector</a> &amp;<span class="k">operator</span>^=(
    <span class="k">const</span> <a href="bit-vector.html">bit_vector</a> &amp;other) <span class="k">noexcept</span>;</code></pre></div>

#### `operator|=` {#operator-7c--3d}

<div class="function-declaration" id="operator-7c--3d-1"><pre><code><a href="bit-vector.html">bit_vector</a> &amp;<span class="k">operator</span>|=(
    <span class="k">const</span> <a href="bit-vector.html">bit_vector</a> &amp;other) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
