---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# small_vector.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/containers/small_vector.h` |
| API ↗ | [Containers](../../../../api/core/containers.md) |

<p><code><a href="small-vector/small-vector.html">small_vector</a></code>: a <code>std::vector</code> with inline storage for the first <code>N</code> elements.</p>



## Classes

| File | Description |
|---|---|
| [`small_vector`](small-vector/small-vector.md) |  |

## Type Aliases

### `small_vector::const_iterator` {#symbol-small-vector-const-iterator}

```cpp
using const_iterator = const T *;
```

### `small_vector::const_pointer` {#symbol-small-vector-const-pointer}

```cpp
using const_pointer = const T *;
```

### `small_vector::const_reference` {#symbol-small-vector-const-reference}

```cpp
using const_reference = const T &;
```

### `small_vector::const_reverse_iterator` {#symbol-small-vector-const-reverse-iterator}

```cpp
using const_reverse_iterator = std::reverse_iterator<const_iterator>;
```

### `small_vector::difference_type` {#symbol-small-vector-difference-type}

```cpp
using difference_type = isize;
```

### `small_vector::iterator` {#symbol-small-vector-iterator}

```cpp
using iterator = T *;
```

### `small_vector::pointer` {#symbol-small-vector-pointer}

```cpp
using pointer = T *;
```

### `small_vector::reference` {#symbol-small-vector-reference}

```cpp
using reference = T &;
```

### `small_vector::reverse_iterator` {#symbol-small-vector-reverse-iterator}

```cpp
using reverse_iterator = std::reverse_iterator<iterator>;
```

### `small_vector::size_type` {#symbol-small-vector-size-type}

```cpp
using size_type = usize;
```

### `small_vector::value_type` {#symbol-small-vector-value-type}

```cpp
using value_type = T;
```

## Unions

### `small_vector::inline_storage` {#symbol-small-vector-inline-storage}

```cpp
union inline_storage {
    T elements[N];

    inline_storage() noexcept {}
    ~inline_storage() {}

    inline_storage(const inline_storage &) = delete;
    inline_storage &operator=(const inline_storage &) = delete;
};
```

Raw inline storage. <code>A</code> union keeps the natural alignment of <code>T</code> without an alignas specifier and never constructs the elements itself.

## Functions

### Public

#### `swap` {#function-swap}

<div class="function-declaration" id="function-swap-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> T, <a href="../types.html#symbol-usize">usize</a> N&gt; <span class="k">void</span> swap(
    <a href="small-vector/small-vector.html">small_vector</a>&lt;T, N&gt; &amp;left,
    <a href="small-vector/small-vector.html">small_vector</a>&lt;T, N&gt; &amp;right) <span class="k">noexcept</span>;</code></pre></div>

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
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="small_vector"></a>
<a id="small_vectorallocate_storage"></a>
<a id="small_vectorappend_range"></a>
<a id="small_vectorassign"></a>
<a id="small_vectorassign_range"></a>
<a id="small_vectorat"></a>
<a id="small_vectorback"></a>
<a id="small_vectorbegin"></a>
<a id="small_vectorcapacity"></a>
<a id="small_vectorcbegin"></a>
<a id="small_vectorcend"></a>
<a id="small_vectorclear"></a>
<a id="small_vectorconst_iterator"></a>
<a id="small_vectorconst_pointer"></a>
<a id="small_vectorconst_reference"></a>
<a id="small_vectorconst_reverse_iterator"></a>
<a id="small_vectordata"></a>
<a id="small_vectordeallocate_storage"></a>
<a id="small_vectordestroy_range"></a>
<a id="small_vectordifference_type"></a>
<a id="small_vectoremplace"></a>
<a id="small_vectoremplace_back"></a>
<a id="small_vectoremplace_with_growth"></a>
<a id="small_vectorempty"></a>
<a id="small_vectorend"></a>
<a id="small_vectorerase"></a>
<a id="small_vectorfront"></a>
<a id="small_vectorgrown_capacity"></a>
<a id="small_vectorindex_of"></a>
<a id="small_vectorinline_capacity"></a>
<a id="small_vectorinline_data"></a>
<a id="small_vectorinline_storage"></a>
<a id="small_vectorinline_storageelements"></a>
<a id="small_vectorinline_storageinline_storage"></a>
<a id="small_vectorinline_storageinline_storage_1"></a>
<a id="small_vectorinline_storageoperator"></a>
<a id="small_vectorinsert"></a>
<a id="small_vectoris_inline"></a>
<a id="small_vectoris_trivially_relocatable"></a>
<a id="small_vectoriterator"></a>
<a id="small_vectormax_size"></a>
<a id="small_vectormcapacity"></a>
<a id="small_vectormdata"></a>
<a id="small_vectorminline"></a>
<a id="small_vectormsize"></a>
<a id="small_vectoropen_gap"></a>
<a id="small_vectoroperator"></a>
<a id="small_vectoroperator-stdspan"></a>
<a id="small_vectoroperator-stdspan_1"></a>
<a id="small_vectoroperator-stdspanconst-t"></a>
<a id="small_vectoroperator-stdspant"></a>
<a id="small_vectoroperator_1"></a>
<a id="small_vectoroperator_2"></a>
<a id="small_vectoroperator_3"></a>
<a id="small_vectorpointer"></a>
<a id="small_vectorpop_back"></a>
<a id="small_vectorpush_back"></a>
<a id="small_vectorrbegin"></a>
<a id="small_vectorreallocate"></a>
<a id="small_vectorreference"></a>
<a id="small_vectorrelease_heap"></a>
<a id="small_vectorrelocate_range"></a>
<a id="small_vectorrend"></a>
<a id="small_vectorreserve"></a>
<a id="small_vectorresize"></a>
<a id="small_vectorreverse_iterator"></a>
<a id="small_vectorshrink_to_fit"></a>
<a id="small_vectorsize"></a>
<a id="small_vectorsize_type"></a>
<a id="small_vectorsmall_vector"></a>
<a id="small_vectorsmall_vector_1"></a>
<a id="small_vectorswap"></a>
<a id="small_vectortake_from"></a>
<a id="small_vectorvalue_type"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="swap"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
