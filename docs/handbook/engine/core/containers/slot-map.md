---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# slot_map.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/containers/slot_map.h` |
| API ↗ | [Containers](../../../../api/core/containers.md) |

<p>Dense storage addressed by generational keys. Insertion returns a key that stays valid until that element is erased; afterwards <code>find()</code> returns <code>nullptr</code> for it, even when the slot has been reused. Not thread-safe; concurrent const access is fine. <code>T</code> must be move-constructible and move-assignable (swap-remove). Pointers returned by <code>find()</code> and references and iterators obtained during iteration are invalidated by insert, emplace, erase, clear and reserve. <code>A</code> moved-from map is empty (no slots) and reusable.</p>

## Structs

| File | Description |
|---|---|
| [`slot_key`](slot-map/slot-key.md) | Default key of a <code><a href="slot-map/slot-map.html">slot_map</a></code>: a slot index and the generation of that slot when the key was issued. The all-zero key is <code>null</code>; generation 0 is never issued. |
| [`slot_map::slot_entry`](slot-map/slot-map-slot-entry.md) | One slot of the indirection table. <code>A</code> slot is live (<code>denseIndex</code> valid), free (on the free list, generation = the one issued next) or retired (generation wrapped). |
| [`std::hash<conjure::core::slot_key>`](slot-map/std-hash-conjure-core-slot-key.md) |  |
| [`std::tuple_element<0, conjure::core::slot_map_entry<Key, T>>`](slot-map/std-tuple-element-0-conjure-core-slot-map-entry-key-t.md) |  |
| [`std::tuple_element<1, conjure::core::slot_map_entry<Key, T>>`](slot-map/std-tuple-element-1-conjure-core-slot-map-entry-key-t.md) |  |
| [`std::tuple_size<conjure::core::slot_map_entry<Key, T>>`](slot-map/std-tuple-size-conjure-core-slot-map-entry-key-t.md) |  |

## Classes

| File | Description |
|---|---|
| [`slot_map`](slot-map/slot-map.md) | Dense storage addressed by generational keys. Insertion returns a key that stays valid until that element is erased; afterwards <code>find()</code> returns <code>nullptr</code> for it, even when the slot has been reused. Not thread-safe; concurrent const access is fine. <code>T</code> must be move-constructible and move-assignable (swap-remove). Pointers returned by <code>find()</code> and references and iterators obtained during iteration are invalidated by insert, emplace, erase, clear and reserve. <code>A</code> moved-from map is empty (no slots) and reusable. |
| [`slot_map_entry`](slot-map/slot-map-entry.md) | What a <code><a href="slot-map/slot-map.html">slot_map</a></code> stores and what its iterators point at: the key and the element. Supports structured bindings (auto &amp;[key, value] binds straight into the map&#x27;s storage) and <code>key()</code>/<code>value()</code> accessors. The key is read-only; changing it would corrupt the map. |

## Type Aliases

### `slot_map::const_iterator` {#symbol-slot-map-const-iterator}

```cpp
using const_iterator = const entry *;
```

### `slot_map::entry` {#symbol-slot-map-entry}

```cpp
using entry = slot_map_entry<Key, T>;
```

### `slot_map::generation_type` {#symbol-slot-map-generation-type}

```cpp
using generation_type = std::remove_cvref_t<decltype(Key::generation)>;
```

### `slot_map::index_type` {#symbol-slot-map-index-type}

```cpp
using index_type = std::remove_cvref_t<decltype(Key::index)>;
```

### `slot_map::iterator` {#symbol-slot-map-iterator}

```cpp
using iterator = entry *;
```

### `slot_map::key_type` {#symbol-slot-map-key-type}

```cpp
using key_type = Key;
```

### `slot_map::size_type` {#symbol-slot-map-size-type}

```cpp
using size_type = usize;
```

### `slot_map::value_type` {#symbol-slot-map-value-type}

```cpp
using value_type = T;
```

### `std::tuple_element<0, conjure::core::slot_map_entry<Key, T>>::type` {#symbol-std-tuple-element-0-conjure-core-slot-map-entry-key-t-type}

```cpp
using type = const Key;
```

### `std::tuple_element<1, conjure::core::slot_map_entry<Key, T>>::type` {#symbol-std-tuple-element-1-conjure-core-slot-map-entry-key-t-type}

```cpp
using type = T;
```

## Concepts

### `slot_key_type` {#symbol-slot-key-type}

```cpp
template <typename Key> concept slot_key_type = std::is_trivially_copyable_v<Key> && std::is_default_constructible_v<Key> && std::unsigned_integral<std::remove_cvref_t<decltype(Key::index)>> && std::unsigned_integral<std::remove_cvref_t<decltype(Key::generation)>> && sizeof(Key::index) <= sizeof(u32) && sizeof(Key::generation) <= sizeof(u32) && requires( Key key, std::remove_cvref_t<decltype(Key::index)> index, std::remove_cvref_t<decltype(Key::generation)> generation ) { key.index = index; key.generation = generation; };
```

Requirements on a custom key type: trivially copyable, default constructible (the <code>null</code> key), with assignable unsigned <code>index</code> and <code>generation</code> fields of at most 32 bits each. Narrower fields are allowed: a <code><a href="../types.html#symbol-u16">u16</a></code> generation retires a slot after 65535 reuses.

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
<a id="slot_key"></a>
<a id="slot_key_type"></a>
<a id="slot_keyfrom_u64"></a>
<a id="slot_keygeneration"></a>
<a id="slot_keyindex"></a>
<a id="slot_keyis_valid"></a>
<a id="slot_keynull"></a>
<a id="slot_keyoperator"></a>
<a id="slot_keyoperator_1"></a>
<a id="slot_keyto_u64"></a>
<a id="slot_map"></a>
<a id="slot_map_entry"></a>
<a id="slot_map_entryget"></a>
<a id="slot_map_entrykey"></a>
<a id="slot_map_entrymkey"></a>
<a id="slot_map_entrymvalue"></a>
<a id="slot_map_entryslot_map_entry"></a>
<a id="slot_map_entryvalue"></a>
<a id="slot_mapbegin"></a>
<a id="slot_mapcapacity"></a>
<a id="slot_mapcbegin"></a>
<a id="slot_mapcend"></a>
<a id="slot_mapclear"></a>
<a id="slot_mapconst_iterator"></a>
<a id="slot_mapcontains"></a>
<a id="slot_mapemplace"></a>
<a id="slot_mapempty"></a>
<a id="slot_mapend"></a>
<a id="slot_mapentries"></a>
<a id="slot_mapentry"></a>
<a id="slot_maperase"></a>
<a id="slot_maperase_at"></a>
<a id="slot_mapfind"></a>
<a id="slot_mapfree_slot_count"></a>
<a id="slot_mapgeneration_type"></a>
<a id="slot_mapindex_type"></a>
<a id="slot_mapinsert"></a>
<a id="slot_mapinvalid_index"></a>
<a id="slot_mapiterator"></a>
<a id="slot_mapkey_at"></a>
<a id="slot_mapkey_type"></a>
<a id="slot_maplive_slot"></a>
<a id="slot_mapmake_key"></a>
<a id="slot_mapmax_slot_count"></a>
<a id="slot_mapmentries"></a>
<a id="slot_mapmfreecount"></a>
<a id="slot_mapmfreehead"></a>
<a id="slot_mapmretiredcount"></a>
<a id="slot_mapmslots"></a>
<a id="slot_mapoperator"></a>
<a id="slot_maprelease_slot"></a>
<a id="slot_mapreserve"></a>
<a id="slot_mapretired_slot_count"></a>
<a id="slot_mapsize"></a>
<a id="slot_mapsize_type"></a>
<a id="slot_mapslot_count"></a>
<a id="slot_mapslot_entry"></a>
<a id="slot_mapslot_entrydenseindex"></a>
<a id="slot_mapslot_entrygeneration"></a>
<a id="slot_mapslot_entrynextfree"></a>
<a id="slot_mapslot_map"></a>
<a id="slot_mapslot_map_1"></a>
<a id="slot_mapvalue_type"></a>
<a id="std"></a>
<a id="stdhashconjurecoreslot_key"></a>
<a id="stdhashconjurecoreslot_keyoperator"></a>
<a id="stdoperator"></a>
<a id="stdtuple_element0-conjurecoreslot_map_entrykey-t"></a>
<a id="stdtuple_element0-conjurecoreslot_map_entrykey-ttype"></a>
<a id="stdtuple_element1-conjurecoreslot_map_entrykey-t"></a>
<a id="stdtuple_element1-conjurecoreslot_map_entrykey-ttype"></a>
<a id="stdtuple_sizeconjurecoreslot_map_entrykey-t"></a>
<a id="stdtype"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
