---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# flat_map.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/containers/flat_map.h` |
| API ↗ | [Containers](../../../../api/core/containers.md) |

<p><code><a href="flat-map/flat-map.html">flat_map</a></code>: a sorted vector of key/value pairs with binary search.</p>



## Classes

| File | Description |
|---|---|
| [`flat_map`](flat-map/flat-map.md) |  |

## Type Aliases

### `flat_map::const_iterator` {#symbol-flat-map-const-iterator}

```cpp
using const_iterator = typename storage_type::const_iterator;
```

### `flat_map::const_reference` {#symbol-flat-map-const-reference}

```cpp
using const_reference = const value_type &;
```

### `flat_map::const_reverse_iterator` {#symbol-flat-map-const-reverse-iterator}

```cpp
using const_reverse_iterator = typename storage_type::const_reverse_iterator;
```

### `flat_map::difference_type` {#symbol-flat-map-difference-type}

```cpp
using difference_type = isize;
```

### `flat_map::iterator` {#symbol-flat-map-iterator}

```cpp
using iterator = typename storage_type::iterator;
```

### `flat_map::key_compare` {#symbol-flat-map-key-compare}

```cpp
using key_compare = Compare;
```

### `flat_map::key_type` {#symbol-flat-map-key-type}

```cpp
using key_type = K;
```

### `flat_map::mapped_type` {#symbol-flat-map-mapped-type}

```cpp
using mapped_type = V;
```

### `flat_map::reference` {#symbol-flat-map-reference}

```cpp
using reference = value_type &;
```

### `flat_map::reverse_iterator` {#symbol-flat-map-reverse-iterator}

```cpp
using reverse_iterator = typename storage_type::reverse_iterator;
```

### `flat_map::size_type` {#symbol-flat-map-size-type}

```cpp
using size_type = usize;
```

### `flat_map::storage_type` {#symbol-flat-map-storage-type}

```cpp
using storage_type = std::vector<value_type>;
```

### `flat_map::value_type` {#symbol-flat-map-value-type}

```cpp
using value_type = std::pair<K, V>;
```

## Functions

### Public

#### `swap` {#function-swap}

<div class="function-declaration" id="function-swap-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> K, <span class="k">typename</span> V, <span class="k">typename</span> Compare&gt; <span class="k">void</span> swap(
    <a href="flat-map/flat-map.html">flat_map</a>&lt;K, V, Compare&gt; &amp;left,
    <a href="flat-map/flat-map.html">flat_map</a>&lt;K, V, Compare&gt; &amp;right) <span class="k">noexcept</span>;</code></pre></div>

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
<a id="flat_map"></a>
<a id="flat_mapat"></a>
<a id="flat_mapconst_iterator"></a>
<a id="flat_mapconst_reference"></a>
<a id="flat_mapconst_reverse_iterator"></a>
<a id="flat_mapcontains"></a>
<a id="flat_mapcount"></a>
<a id="flat_mapdifference_type"></a>
<a id="flat_mapemplace"></a>
<a id="flat_maperase"></a>
<a id="flat_mapfind"></a>
<a id="flat_mapfind_in"></a>
<a id="flat_mapflat_map"></a>
<a id="flat_mapflat_map_1"></a>
<a id="flat_mapinsert"></a>
<a id="flat_mapinsert_or_assign"></a>
<a id="flat_mapiterator"></a>
<a id="flat_mapkey_compare"></a>
<a id="flat_mapkey_type"></a>
<a id="flat_maplower_bound"></a>
<a id="flat_maplower_bound_in"></a>
<a id="flat_mapmapped_type"></a>
<a id="flat_mapmcompare"></a>
<a id="flat_mapmentries"></a>
<a id="flat_mapoperator"></a>
<a id="flat_mapoperator_1"></a>
<a id="flat_mapoperator_2"></a>
<a id="flat_mapreference"></a>
<a id="flat_mapreverse_iterator"></a>
<a id="flat_mapsize_type"></a>
<a id="flat_mapstorage_type"></a>
<a id="flat_maptry_emplace"></a>
<a id="flat_mapupper_bound"></a>
<a id="flat_mapupper_bound_in"></a>
<a id="flat_mapvalue_type"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="swap"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
