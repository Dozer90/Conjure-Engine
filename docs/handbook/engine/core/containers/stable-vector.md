---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# stable_vector.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/containers/stable_vector.h` |
| API ↗ | [Containers](../../../../api/core/containers.md) |

<p><code><a href="stable-vector/stable-vector.html">stable_vector</a></code>: a growable sequence whose elements never move.</p>



## Classes

| File | Description |
|---|---|
| [`stable_vector`](stable-vector/stable-vector.md) |  |
| [`stable_vector::basic_iterator`](stable-vector/stable-vector-basic-iterator.md) | Random-access iterator by index. Stays valid across <code>emplace_back</code>; invalidated only when the element it points at is destroyed. Ordering (&lt;, &lt;=&gt;, -) is only defined between iterators of the same container and asserts otherwise; equality is defined for any pair and is <code>false</code> across containers. |

## Type Aliases

### `stable_vector::basic_iterator::difference_type` {#symbol-stable-vector-basic-iterator-difference-type}

```cpp
using difference_type = isize;
```

### `stable_vector::basic_iterator::iterator_category` {#symbol-stable-vector-basic-iterator-iterator-category}

```cpp
using iterator_category = std::random_access_iterator_tag;
```

### `stable_vector::basic_iterator::owner_pointer` {#symbol-stable-vector-basic-iterator-owner-pointer}

```cpp
using owner_pointer = std::conditional_t<IsConst, const stable_vector *, stable_vector *>;
```

### `stable_vector::basic_iterator::pointer` {#symbol-stable-vector-basic-iterator-pointer}

```cpp
using pointer = std::conditional_t<IsConst, const T *, T *>;
```

### `stable_vector::basic_iterator::reference` {#symbol-stable-vector-basic-iterator-reference}

```cpp
using reference = std::conditional_t<IsConst, const T &, T &>;
```

### `stable_vector::basic_iterator::value_type` {#symbol-stable-vector-basic-iterator-value-type}

```cpp
using value_type = T;
```

### `stable_vector::const_iterator` {#symbol-stable-vector-const-iterator}

```cpp
using const_iterator = basic_iterator<true>;
```

### `stable_vector::const_pointer` {#symbol-stable-vector-const-pointer}

```cpp
using const_pointer = const T *;
```

### `stable_vector::const_reference` {#symbol-stable-vector-const-reference}

```cpp
using const_reference = const T &;
```

### `stable_vector::difference_type` {#symbol-stable-vector-difference-type}

```cpp
using difference_type = isize;
```

### `stable_vector::iterator` {#symbol-stable-vector-iterator}

```cpp
using iterator = basic_iterator<false>;
```

### `stable_vector::pointer` {#symbol-stable-vector-pointer}

```cpp
using pointer = T *;
```

### `stable_vector::reference` {#symbol-stable-vector-reference}

```cpp
using reference = T &;
```

### `stable_vector::size_type` {#symbol-stable-vector-size-type}

```cpp
using size_type = usize;
```

### `stable_vector::value_type` {#symbol-stable-vector-value-type}

```cpp
using value_type = T;
```

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="stable_vector"></a>
<a id="stable_vectorallocate_chunk"></a>
<a id="stable_vectorat"></a>
<a id="stable_vectorback"></a>
<a id="stable_vectorbasic_iterator"></a>
<a id="stable_vectorbasic_iteratorbasic_iterator"></a>
<a id="stable_vectorbasic_iteratordifference_type"></a>
<a id="stable_vectorbasic_iteratorindex"></a>
<a id="stable_vectorbasic_iteratoriterator_category"></a>
<a id="stable_vectorbasic_iteratormindex"></a>
<a id="stable_vectorbasic_iteratormowner"></a>
<a id="stable_vectorbasic_iteratoroperator"></a>
<a id="stable_vectorbasic_iteratoroperator-"></a>
<a id="stable_vectorbasic_iteratoroperator-_1"></a>
<a id="stable_vectorbasic_iteratoroperator-_2"></a>
<a id="stable_vectorbasic_iteratoroperator-_3"></a>
<a id="stable_vectorbasic_iteratoroperator_1"></a>
<a id="stable_vectorbasic_iteratoroperator_2"></a>
<a id="stable_vectorbasic_iteratoroperator_3"></a>
<a id="stable_vectorbasic_iteratoroperator_4"></a>
<a id="stable_vectorbasic_iteratoroperator_5"></a>
<a id="stable_vectorbasic_iteratoroperator_6"></a>
<a id="stable_vectorbasic_iteratorowner"></a>
<a id="stable_vectorbasic_iteratorowner_pointer"></a>
<a id="stable_vectorbasic_iteratorpointer"></a>
<a id="stable_vectorbasic_iteratorreference"></a>
<a id="stable_vectorbasic_iteratorvalue_type"></a>
<a id="stable_vectorbegin"></a>
<a id="stable_vectorcapacity"></a>
<a id="stable_vectorcbegin"></a>
<a id="stable_vectorcend"></a>
<a id="stable_vectorchunk_count"></a>
<a id="stable_vectorchunk_size"></a>
<a id="stable_vectorclear"></a>
<a id="stable_vectorconst_iterator"></a>
<a id="stable_vectorconst_pointer"></a>
<a id="stable_vectorconst_reference"></a>
<a id="stable_vectordeallocate_chunk"></a>
<a id="stable_vectordifference_type"></a>
<a id="stable_vectoremplace_back"></a>
<a id="stable_vectorempty"></a>
<a id="stable_vectorend"></a>
<a id="stable_vectorfront"></a>
<a id="stable_vectoriterator"></a>
<a id="stable_vectormchunks"></a>
<a id="stable_vectormsize"></a>
<a id="stable_vectoroperator"></a>
<a id="stable_vectoroperator_1"></a>
<a id="stable_vectorpointer"></a>
<a id="stable_vectorpop_back"></a>
<a id="stable_vectorpush_back"></a>
<a id="stable_vectorreference"></a>
<a id="stable_vectorrelease_chunks"></a>
<a id="stable_vectorreserve"></a>
<a id="stable_vectorshrink_to_fit"></a>
<a id="stable_vectorsize"></a>
<a id="stable_vectorsize_type"></a>
<a id="stable_vectorslot_at"></a>
<a id="stable_vectorstable_vector"></a>
<a id="stable_vectorstable_vector_1"></a>
<a id="stable_vectorvalue_type"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
