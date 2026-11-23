---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# bit_vector.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/bit_vector.h` |
| API ↗ | [Strings and identifiers](../../../api/core/strings-and-identifiers.md) |

<p><code>A</code> dynamically sized set of bits packed into 64-bit words. Used for component masks, free-slot tracking and dirty flags. Bits past <code>size()</code> are always zero, so whole-word operations (count, any, equality, bitwise ops) need no masking at the call site. <code>A</code> moved-from <code><a href="bit-vector/bit-vector.html">bit_vector</a></code> is empty (size 0, no words) and fully reusable. Not thread-safe.</p>



## Classes

| File | Description |
|---|---|
| [`bit_vector`](bit-vector/bit-vector.md) | <code>A</code> dynamically sized set of bits packed into 64-bit words. Used for component masks, free-slot tracking and dirty flags. Bits past <code>size()</code> are always zero, so whole-word operations (count, any, equality, bitwise ops) need no masking at the call site. <code>A</code> moved-from <code><a href="bit-vector/bit-vector.html">bit_vector</a></code> is empty (size 0, no words) and fully reusable. Not thread-safe. |
| [`bit_vector::set_bit_iterator`](bit-vector/bit-vector-set-bit-iterator.md) | Forward iterator over the indices of the set bits, ascending. Dereferencing yields the index by value, so it models <code>std::forward_iterator</code> (<code>C</code>++20) but only the legacy input iterator category, exactly like <code>std::ranges::iota_view</code>&#x27;s iterator. |
| [`bit_vector::set_bit_range`](bit-vector/bit-vector-set-bit-range.md) | Range over the set bits: for (<code><a href="types.html#symbol-usize">usize</a></code> index: <code>bits.set_bits()</code>) { ... }. Its iterators refer to the <code><a href="bit-vector/bit-vector.html">bit_vector</a></code>, not to the range object, so it is a borrowed range and <code>std::ranges</code> algorithms may take it by value: <code>std::ranges::find(bits.set_bits(), 7)</code>. |

## Type Aliases

### `bit_vector::set_bit_iterator::difference_type` {#symbol-bit-vector-set-bit-iterator-difference-type}

```cpp
using difference_type = isize;
```

### `bit_vector::set_bit_iterator::iterator_category` {#symbol-bit-vector-set-bit-iterator-iterator-category}

```cpp
using iterator_category = std::input_iterator_tag;
```

### `bit_vector::set_bit_iterator::iterator_concept` {#symbol-bit-vector-set-bit-iterator-iterator-concept}

```cpp
using iterator_concept = std::forward_iterator_tag;
```

### `bit_vector::set_bit_iterator::pointer` {#symbol-bit-vector-set-bit-iterator-pointer}

```cpp
using pointer = const usize *;
```

### `bit_vector::set_bit_iterator::reference` {#symbol-bit-vector-set-bit-iterator-reference}

```cpp
using reference = usize;
```

### `bit_vector::set_bit_iterator::value_type` {#symbol-bit-vector-set-bit-iterator-value-type}

```cpp
using value_type = usize;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-set_bit_range"></a><code>set_bit_range</code> | <code class="signature"><span class="k">template</span> &lt;<wbr>&gt; <span class="k">inline</span> <span class="k">constexpr</span> <span class="k">bool</span> std::<wbr>ranges::<wbr>enable_<wbr>borrowed_<wbr>range&lt;<wbr><a href="bit-vector/bit-vector.html">conjure::<wbr>core::<wbr>bit_<wbr>vector</a>::<wbr>&gt;</code> | <code class="signature">true</code> |  |

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="bit_vector"></a>
<a id="bit_vectorall"></a>
<a id="bit_vectorany"></a>
<a id="bit_vectorbit_vector"></a>
<a id="bit_vectorbit_vector_1"></a>
<a id="bit_vectorcapacity"></a>
<a id="bit_vectorclear"></a>
<a id="bit_vectorclear_trailing_bits"></a>
<a id="bit_vectorcount"></a>
<a id="bit_vectorempty"></a>
<a id="bit_vectorfind_first_clear"></a>
<a id="bit_vectorfind_first_set"></a>
<a id="bit_vectorfind_last_set"></a>
<a id="bit_vectorflip"></a>
<a id="bit_vectorflip_all"></a>
<a id="bit_vectorintersects"></a>
<a id="bit_vectoris_subset_of"></a>
<a id="bit_vectormsize"></a>
<a id="bit_vectormwords"></a>
<a id="bit_vectornone"></a>
<a id="bit_vectornpos"></a>
<a id="bit_vectoroperator"></a>
<a id="bit_vectoroperator_1"></a>
<a id="bit_vectoroperator_2"></a>
<a id="bit_vectoroperator_3"></a>
<a id="bit_vectoroperator_4"></a>
<a id="bit_vectoroperator_5"></a>
<a id="bit_vectorpush_back"></a>
<a id="bit_vectorremove"></a>
<a id="bit_vectorreserve"></a>
<a id="bit_vectorreset"></a>
<a id="bit_vectorreset_all"></a>
<a id="bit_vectorresize"></a>
<a id="bit_vectorset"></a>
<a id="bit_vectorset_all"></a>
<a id="bit_vectorset_bit_iterator"></a>
<a id="bit_vectorset_bit_iteratordifference_type"></a>
<a id="bit_vectorset_bit_iteratoriterator_category"></a>
<a id="bit_vectorset_bit_iteratoriterator_concept"></a>
<a id="bit_vectorset_bit_iteratormindex"></a>
<a id="bit_vectorset_bit_iteratormowner"></a>
<a id="bit_vectorset_bit_iteratoroperator"></a>
<a id="bit_vectorset_bit_iteratoroperator_1"></a>
<a id="bit_vectorset_bit_iteratoroperator_2"></a>
<a id="bit_vectorset_bit_iteratorpointer"></a>
<a id="bit_vectorset_bit_iteratorreference"></a>
<a id="bit_vectorset_bit_iteratorset_bit_iterator"></a>
<a id="bit_vectorset_bit_iteratorvalue_type"></a>
<a id="bit_vectorset_bit_range"></a>
<a id="bit_vectorset_bit_rangebegin"></a>
<a id="bit_vectorset_bit_rangeend"></a>
<a id="bit_vectorset_bit_rangemowner"></a>
<a id="bit_vectorset_bit_rangeset_bit_range"></a>
<a id="bit_vectorset_bits"></a>
<a id="bit_vectorsize"></a>
<a id="bit_vectortest"></a>
<a id="bit_vectorword_bits"></a>
<a id="bit_vectorword_count"></a>
<a id="bit_vectorwords"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="operator"></a>
<a id="operator_1"></a>
<a id="operator_2"></a>
<a id="operator_3"></a>
<a id="other-declarations"></a>
<a id="set_bit_range"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
