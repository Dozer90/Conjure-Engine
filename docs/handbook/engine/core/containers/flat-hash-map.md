---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# flat_hash_map.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/containers/flat_hash_map.h` |
| API ↗ | [Containers](../../../../api/core/containers.md) |

<p>Open-addressing hash map storing <code>std::pair&lt;const K, V&gt;</code> in place. Iteration order is the slot order (preserved by copies) and changes on rehash. Not thread-safe; concurrent const access is fine. Rehashing happens only when an insertion would push <code>size()</code> above <code>capacity()</code> * <code>MAX_LOAD_FACTOR</code>, so after <code>reserve(n)</code> the next n insertions move nothing. Pointers and references to elements are otherwise invalidated only by erasing an element of the same probe run (backward shift). Re-fetch <code>end()</code> after inserting: an insertion can move the slot the iteration order starts at. Arguments passed to an inserting call must not refer to elements of the same map.</p>

## Structs

| File | Description |
|---|---|
| [`detail::identity_key_of`](flat-hash-map/detail-identity-key-of.md) |  |
| [`detail::pair_key_of`](flat-hash-map/detail-pair-key-of.md) |  |

## Classes

| File | Description |
|---|---|
| [`detail::hash_table`](flat-hash-map/detail-hash-table.md) | The table behind <code><a href="flat-hash-map/flat-hash-map.html">flat_hash_map</a></code> and <code><a href="flat-hash-map/flat-hash-set.html">flat_hash_set</a></code>. Value is the stored element, KeyOf extracts its key. Not thread-safe; concurrent const access is fine. |
| [`detail::hash_table::basic_iterator`](flat-hash-map/detail-hash-table-basic-iterator.md) | Forward iterator over occupied slots. Walks the table cyclically from the slot after the anchor and stops at the anchor, so it visits every element exactly once even when a probe run wraps around the end of the table. |
| [`flat_hash_map`](flat-hash-map/flat-hash-map.md) | Open-addressing hash map storing <code>std::pair&lt;const K, V&gt;</code> in place. Iteration order is the slot order (preserved by copies) and changes on rehash. Not thread-safe; concurrent const access is fine. Rehashing happens only when an insertion would push <code>size()</code> above <code>capacity()</code> * <code>MAX_LOAD_FACTOR</code>, so after <code>reserve(n)</code> the next n insertions move nothing. Pointers and references to elements are otherwise invalidated only by erasing an element of the same probe run (backward shift). Re-fetch <code>end()</code> after inserting: an insertion can move the slot the iteration order starts at. Arguments passed to an inserting call must not refer to elements of the same map. |
| [`flat_hash_set`](flat-hash-map/flat-hash-set.md) | Open-addressing hash set with the same table (and the same rehash and invalidation rules) as <code><a href="flat-hash-map/flat-hash-map.html">flat_hash_map</a></code>. Elements are immutable through iterators. Not thread-safe; concurrent const access is fine. |

## Type Aliases

### `detail::hash_table::basic_iterator::difference_type` {#symbol-detail-hash-table-basic-iterator-difference-type}

```cpp
using difference_type = isize;
```

### `detail::hash_table::basic_iterator::iterator_category` {#symbol-detail-hash-table-basic-iterator-iterator-category}

```cpp
using iterator_category = std::forward_iterator_tag;
```

### `detail::hash_table::basic_iterator::pointer` {#symbol-detail-hash-table-basic-iterator-pointer}

```cpp
using pointer = std::conditional_t<IsConst, const Value *, Value *>;
```

### `detail::hash_table::basic_iterator::reference` {#symbol-detail-hash-table-basic-iterator-reference}

```cpp
using reference = std::conditional_t<IsConst, const Value &, Value &>;
```

### `detail::hash_table::basic_iterator::value_type` {#symbol-detail-hash-table-basic-iterator-value-type}

```cpp
using value_type = Value;
```

### `detail::hash_table::const_iterator` {#symbol-detail-hash-table-const-iterator}

```cpp
using const_iterator = basic_iterator<true>;
```

### `detail::hash_table::iterator` {#symbol-detail-hash-table-iterator}

```cpp
using iterator = basic_iterator<false>;
```

### `detail::hash_table::key_type` {#symbol-detail-hash-table-key-type}

```cpp
using key_type = typename KeyOf::key_type;
```

### `detail::hash_table::value_type` {#symbol-detail-hash-table-value-type}

```cpp
using value_type = Value;
```

### `detail::identity_key_of::key_type` {#symbol-detail-identity-key-of-key-type}

```cpp
using key_type = K;
```

### `detail::pair_key_of::key_type` {#symbol-detail-pair-key-of-key-type}

```cpp
using key_type = K;
```

### `flat_hash_map::const_iterator` {#symbol-flat-hash-map-const-iterator}

```cpp
using const_iterator = typename table_type::const_iterator;
```

### `flat_hash_map::const_reference` {#symbol-flat-hash-map-const-reference}

```cpp
using const_reference = const value_type &;
```

### `flat_hash_map::difference_type` {#symbol-flat-hash-map-difference-type}

```cpp
using difference_type = isize;
```

### `flat_hash_map::hasher` {#symbol-flat-hash-map-hasher}

```cpp
using hasher = Hash;
```

### `flat_hash_map::iterator` {#symbol-flat-hash-map-iterator}

```cpp
using iterator = typename table_type::iterator;
```

### `flat_hash_map::key_equal` {#symbol-flat-hash-map-key-equal}

```cpp
using key_equal = Eq;
```

### `flat_hash_map::key_type` {#symbol-flat-hash-map-key-type}

```cpp
using key_type = K;
```

### `flat_hash_map::mapped_type` {#symbol-flat-hash-map-mapped-type}

```cpp
using mapped_type = V;
```

### `flat_hash_map::reference` {#symbol-flat-hash-map-reference}

```cpp
using reference = value_type &;
```

### `flat_hash_map::size_type` {#symbol-flat-hash-map-size-type}

```cpp
using size_type = usize;
```

### `flat_hash_map::table_type` {#symbol-flat-hash-map-table-type}

```cpp
using table_type = detail::hash_table<value_type, detail::pair_key_of<K, V>, Hash, Eq>;
```

### `flat_hash_map::value_type` {#symbol-flat-hash-map-value-type}

```cpp
using value_type = std::pair<const K, V>;
```

### `flat_hash_set::const_iterator` {#symbol-flat-hash-set-const-iterator}

```cpp
using const_iterator = typename table_type::const_iterator;
```

### `flat_hash_set::const_reference` {#symbol-flat-hash-set-const-reference}

```cpp
using const_reference = const K &;
```

### `flat_hash_set::difference_type` {#symbol-flat-hash-set-difference-type}

```cpp
using difference_type = isize;
```

### `flat_hash_set::hasher` {#symbol-flat-hash-set-hasher}

```cpp
using hasher = Hash;
```

### `flat_hash_set::iterator` {#symbol-flat-hash-set-iterator}

```cpp
using iterator = typename table_type::const_iterator;
```

### `flat_hash_set::key_equal` {#symbol-flat-hash-set-key-equal}

```cpp
using key_equal = Eq;
```

### `flat_hash_set::key_type` {#symbol-flat-hash-set-key-type}

```cpp
using key_type = K;
```

### `flat_hash_set::reference` {#symbol-flat-hash-set-reference}

```cpp
using reference = const K &;
```

### `flat_hash_set::size_type` {#symbol-flat-hash-set-size-type}

```cpp
using size_type = usize;
```

### `flat_hash_set::table_type` {#symbol-flat-hash-set-table-type}

```cpp
using table_type = detail::hash_table<K, detail::identity_key_of<K>, Hash, Eq>;
```

### `flat_hash_set::value_type` {#symbol-flat-hash-set-value-type}

```cpp
using value_type = K;
```

## Functions

### Public

#### `swap` {#function-swap}

<div class="function-declaration" id="function-swap-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> K, <span class="k">typename</span> V, <span class="k">typename</span> Hash, <span class="k">typename</span> Eq&gt; <span class="k">void</span> swap(
    <a href="flat-hash-map/flat-hash-map.html">flat_hash_map</a>&lt;K, V, Hash, Eq&gt; &amp;left,
    <a href="flat-hash-map/flat-hash-map.html">flat_hash_map</a>&lt;K, V, Hash, Eq&gt; &amp;right) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-swap-2"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> K, <span class="k">typename</span> Hash, <span class="k">typename</span> Eq&gt; <span class="k">void</span> swap(
    <a href="flat-hash-map/flat-hash-set.html">flat_hash_set</a>&lt;K, Hash, Eq&gt; &amp;left,
    <a href="flat-hash-map/flat-hash-set.html">flat_hash_set</a>&lt;K, Hash, Eq&gt; &amp;right) <span class="k">noexcept</span>;</code></pre></div>

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
<a id="detailhash_table"></a>
<a id="detailhash_tableallocate_table"></a>
<a id="detailhash_tablebasic_iterator"></a>
<a id="detailhash_tablebasic_iteratorbasic_iterator"></a>
<a id="detailhash_tablebasic_iteratorbasic_iterator_1"></a>
<a id="detailhash_tablebasic_iteratordifference_type"></a>
<a id="detailhash_tablebasic_iteratorhash_table"></a>
<a id="detailhash_tablebasic_iteratoriterator_category"></a>
<a id="detailhash_tablebasic_iteratormanchor"></a>
<a id="detailhash_tablebasic_iteratormindex"></a>
<a id="detailhash_tablebasic_iteratormmask"></a>
<a id="detailhash_tablebasic_iteratormmeta"></a>
<a id="detailhash_tablebasic_iteratormvalues"></a>
<a id="detailhash_tablebasic_iteratoroperator"></a>
<a id="detailhash_tablebasic_iteratoroperator-"></a>
<a id="detailhash_tablebasic_iteratoroperator_1"></a>
<a id="detailhash_tablebasic_iteratoroperator_2"></a>
<a id="detailhash_tablebasic_iteratorpointer"></a>
<a id="detailhash_tablebasic_iteratorreference"></a>
<a id="detailhash_tablebasic_iteratorskip_empty"></a>
<a id="detailhash_tablebasic_iteratorvalue_type"></a>
<a id="detailhash_tablebegin"></a>
<a id="detailhash_tablecapacity"></a>
<a id="detailhash_tablecapacity_for"></a>
<a id="detailhash_tableclear"></a>
<a id="detailhash_tableclone_from"></a>
<a id="detailhash_tableconst_iterator"></a>
<a id="detailhash_tablecontains"></a>
<a id="detailhash_tabledeallocate_table"></a>
<a id="detailhash_tabledestroy_elements"></a>
<a id="detailhash_tableempty"></a>
<a id="detailhash_tableend"></a>
<a id="detailhash_tableerase"></a>
<a id="detailhash_tableerase_index"></a>
<a id="detailhash_tablefind"></a>
<a id="detailhash_tablefind_index"></a>
<a id="detailhash_tableforget_table"></a>
<a id="detailhash_tablegrow_threshold"></a>
<a id="detailhash_tablehash_function"></a>
<a id="detailhash_tablehash_mask"></a>
<a id="detailhash_tablehash_of"></a>
<a id="detailhash_tablehash_table"></a>
<a id="detailhash_tablehash_table_1"></a>
<a id="detailhash_tablehome_of"></a>
<a id="detailhash_tablehome_of_slot"></a>
<a id="detailhash_tableinsert"></a>
<a id="detailhash_tableinsert_new"></a>
<a id="detailhash_tableinsert_with"></a>
<a id="detailhash_tableis_occupied"></a>
<a id="detailhash_tableiterator"></a>
<a id="detailhash_tableiterator_at"></a>
<a id="detailhash_tablekey_eq"></a>
<a id="detailhash_tablekey_type"></a>
<a id="detailhash_tableload_factor"></a>
<a id="detailhash_tablemanchor"></a>
<a id="detailhash_tablemax_capacity"></a>
<a id="detailhash_tablemax_load_factor"></a>
<a id="detailhash_tablemcapacity"></a>
<a id="detailhash_tablemequal"></a>
<a id="detailhash_tablemgrowat"></a>
<a id="detailhash_tablemhash"></a>
<a id="detailhash_tablemin_capacity"></a>
<a id="detailhash_tablemmask"></a>
<a id="detailhash_tablemmeta"></a>
<a id="detailhash_tablemsize"></a>
<a id="detailhash_tablemvalues"></a>
<a id="detailhash_tablenext_empty_after"></a>
<a id="detailhash_tableno_slot"></a>
<a id="detailhash_tableoccupied_bit"></a>
<a id="detailhash_tableoperator"></a>
<a id="detailhash_tablerehash"></a>
<a id="detailhash_tablerelocate"></a>
<a id="detailhash_tablereserve"></a>
<a id="detailhash_tablesize"></a>
<a id="detailhash_tableswap"></a>
<a id="detailhash_tabletag_of"></a>
<a id="detailhash_tablevalue_type"></a>
<a id="detailidentity_key_of"></a>
<a id="detailidentity_key_ofkey"></a>
<a id="detailidentity_key_ofkey_type"></a>
<a id="detailpair_key_of"></a>
<a id="detailpair_key_ofkey"></a>
<a id="detailpair_key_ofkey_type"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="flat_hash_map"></a>
<a id="flat_hash_mapat"></a>
<a id="flat_hash_mapbegin"></a>
<a id="flat_hash_mapcapacity"></a>
<a id="flat_hash_mapcbegin"></a>
<a id="flat_hash_mapcend"></a>
<a id="flat_hash_mapclear"></a>
<a id="flat_hash_mapconst_iterator"></a>
<a id="flat_hash_mapconst_reference"></a>
<a id="flat_hash_mapcontains"></a>
<a id="flat_hash_mapcount"></a>
<a id="flat_hash_mapdifference_type"></a>
<a id="flat_hash_mapemplace"></a>
<a id="flat_hash_mapempty"></a>
<a id="flat_hash_mapend"></a>
<a id="flat_hash_maperase"></a>
<a id="flat_hash_mapfind"></a>
<a id="flat_hash_mapflat_hash_map"></a>
<a id="flat_hash_mapflat_hash_map_1"></a>
<a id="flat_hash_maphash_function"></a>
<a id="flat_hash_maphasher"></a>
<a id="flat_hash_mapinsert"></a>
<a id="flat_hash_mapinsert_or_assign"></a>
<a id="flat_hash_mapiterator"></a>
<a id="flat_hash_mapkey_eq"></a>
<a id="flat_hash_mapkey_equal"></a>
<a id="flat_hash_mapkey_type"></a>
<a id="flat_hash_mapload_factor"></a>
<a id="flat_hash_mapmapped_type"></a>
<a id="flat_hash_mapmax_capacity"></a>
<a id="flat_hash_mapmax_load_factor"></a>
<a id="flat_hash_mapmtable"></a>
<a id="flat_hash_mapoperator"></a>
<a id="flat_hash_mapoperator_1"></a>
<a id="flat_hash_mapoperator_2"></a>
<a id="flat_hash_mapreference"></a>
<a id="flat_hash_mapreserve"></a>
<a id="flat_hash_mapsize"></a>
<a id="flat_hash_mapsize_type"></a>
<a id="flat_hash_mapswap"></a>
<a id="flat_hash_maptable_type"></a>
<a id="flat_hash_maptry_emplace"></a>
<a id="flat_hash_mapvalue_type"></a>
<a id="flat_hash_set"></a>
<a id="flat_hash_setbegin"></a>
<a id="flat_hash_setcapacity"></a>
<a id="flat_hash_setcbegin"></a>
<a id="flat_hash_setcend"></a>
<a id="flat_hash_setclear"></a>
<a id="flat_hash_setconst_iterator"></a>
<a id="flat_hash_setconst_reference"></a>
<a id="flat_hash_setcontains"></a>
<a id="flat_hash_setcount"></a>
<a id="flat_hash_setdifference_type"></a>
<a id="flat_hash_setemplace"></a>
<a id="flat_hash_setempty"></a>
<a id="flat_hash_setend"></a>
<a id="flat_hash_seterase"></a>
<a id="flat_hash_setfind"></a>
<a id="flat_hash_setflat_hash_set"></a>
<a id="flat_hash_setflat_hash_set_1"></a>
<a id="flat_hash_sethasher"></a>
<a id="flat_hash_setinsert"></a>
<a id="flat_hash_setiterator"></a>
<a id="flat_hash_setkey_equal"></a>
<a id="flat_hash_setkey_type"></a>
<a id="flat_hash_setload_factor"></a>
<a id="flat_hash_setmax_capacity"></a>
<a id="flat_hash_setmax_load_factor"></a>
<a id="flat_hash_setmtable"></a>
<a id="flat_hash_setoperator"></a>
<a id="flat_hash_setoperator_1"></a>
<a id="flat_hash_setreference"></a>
<a id="flat_hash_setreserve"></a>
<a id="flat_hash_setsize"></a>
<a id="flat_hash_setsize_type"></a>
<a id="flat_hash_setswap"></a>
<a id="flat_hash_settable_type"></a>
<a id="flat_hash_setvalue_type"></a>
<a id="hash_table"></a>
<a id="hash_tableallocate_table"></a>
<a id="hash_tablebasic_iterator"></a>
<a id="hash_tablebasic_iteratorbasic_iterator"></a>
<a id="hash_tablebasic_iteratorbasic_iterator_1"></a>
<a id="hash_tablebasic_iteratordifference_type"></a>
<a id="hash_tablebasic_iteratorhash_table"></a>
<a id="hash_tablebasic_iteratoriterator_category"></a>
<a id="hash_tablebasic_iteratormanchor"></a>
<a id="hash_tablebasic_iteratormindex"></a>
<a id="hash_tablebasic_iteratormmask"></a>
<a id="hash_tablebasic_iteratormmeta"></a>
<a id="hash_tablebasic_iteratormvalues"></a>
<a id="hash_tablebasic_iteratoroperator"></a>
<a id="hash_tablebasic_iteratoroperator-"></a>
<a id="hash_tablebasic_iteratoroperator_1"></a>
<a id="hash_tablebasic_iteratoroperator_2"></a>
<a id="hash_tablebasic_iteratorpointer"></a>
<a id="hash_tablebasic_iteratorreference"></a>
<a id="hash_tablebasic_iteratorskip_empty"></a>
<a id="hash_tablebasic_iteratorvalue_type"></a>
<a id="hash_tablebegin"></a>
<a id="hash_tablecapacity"></a>
<a id="hash_tablecapacity_for"></a>
<a id="hash_tableclear"></a>
<a id="hash_tableclone_from"></a>
<a id="hash_tableconst_iterator"></a>
<a id="hash_tablecontains"></a>
<a id="hash_tabledeallocate_table"></a>
<a id="hash_tabledestroy_elements"></a>
<a id="hash_tableempty"></a>
<a id="hash_tableend"></a>
<a id="hash_tableerase"></a>
<a id="hash_tableerase_index"></a>
<a id="hash_tablefind"></a>
<a id="hash_tablefind_index"></a>
<a id="hash_tableforget_table"></a>
<a id="hash_tablegrow_threshold"></a>
<a id="hash_tablehash_function"></a>
<a id="hash_tablehash_mask"></a>
<a id="hash_tablehash_of"></a>
<a id="hash_tablehash_table"></a>
<a id="hash_tablehash_table_1"></a>
<a id="hash_tablehome_of"></a>
<a id="hash_tablehome_of_slot"></a>
<a id="hash_tableinsert"></a>
<a id="hash_tableinsert_new"></a>
<a id="hash_tableinsert_with"></a>
<a id="hash_tableis_occupied"></a>
<a id="hash_tableiterator"></a>
<a id="hash_tableiterator_at"></a>
<a id="hash_tablekey_eq"></a>
<a id="hash_tablekey_type"></a>
<a id="hash_tableload_factor"></a>
<a id="hash_tablemanchor"></a>
<a id="hash_tablemax_capacity"></a>
<a id="hash_tablemax_load_factor"></a>
<a id="hash_tablemcapacity"></a>
<a id="hash_tablemequal"></a>
<a id="hash_tablemgrowat"></a>
<a id="hash_tablemhash"></a>
<a id="hash_tablemin_capacity"></a>
<a id="hash_tablemmask"></a>
<a id="hash_tablemmeta"></a>
<a id="hash_tablemsize"></a>
<a id="hash_tablemvalues"></a>
<a id="hash_tablenext_empty_after"></a>
<a id="hash_tableno_slot"></a>
<a id="hash_tableoccupied_bit"></a>
<a id="hash_tableoperator"></a>
<a id="hash_tablerehash"></a>
<a id="hash_tablerelocate"></a>
<a id="hash_tablereserve"></a>
<a id="hash_tablesize"></a>
<a id="hash_tableswap"></a>
<a id="hash_tabletag_of"></a>
<a id="hash_tablevalue_type"></a>
<a id="identity_key_of"></a>
<a id="identity_key_ofkey"></a>
<a id="identity_key_ofkey_type"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="pair_key_of"></a>
<a id="pair_key_ofkey"></a>
<a id="pair_key_ofkey_type"></a>
<a id="structs-and-types"></a>
<a id="swap"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
