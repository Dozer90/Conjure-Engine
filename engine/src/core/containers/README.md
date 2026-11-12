# core/containers

Cache-friendly containers the rest of the engine builds on. Public headers live in
`engine/include/conjure/core/containers/`; everything is a header-only template in
`namespace conjure::core`. `containers.cpp` holds the `slot_key` text helpers and the fatal-access
hook and gives the area its static library (`conjure::core_containers`). Design:
`docs/design/10_core.md` section 2.3.

| Header | Type | Use it for |
|---|---|---|
| `slot_map.h` | `slot_key`, `slot_map<T, Key>`, `slot_map_entry<Key, T>` | Handles that must detect stale use (GPU resources, scripts, editor requests). Dense `{key, value}` storage with swap-remove, O(1) allocation-free lookup, one generation per slot. |
| `slot_key_format.h` | `to_string`, `parse_slot_key`, `std::formatter<slot_key>` | Logging and serialising keys ("12:3"). Separate so `slot_map.h` stays light. |
| `small_vector.h` | `small_vector<T, N>` | Short lists that usually fit inline (children, component ids). `std::vector` API subset, `std::span` conversion, memcpy relocation for trivially copyable `T`. |
| `stable_vector.h` | `stable_vector<T, ChunkSize>` | Growing arrays whose element addresses must never move (asset slots). Chunked storage, random-access iterators. |
| `flat_hash_map.h` | `flat_hash_map<K, V>`, `flat_hash_set<K>` | General-purpose hashing: linear probing with backward-shift deletion, power-of-two capacity, max load 0.875, rehash on load only. |
| `flat_map.h` | `flat_map<K, V, Compare>` | Small maps and deterministic iteration order (sorted vector, binary search, transparent comparators). |
| `detail.h` | `detail::fatal_container_access`, `transparent_compare`, `mix_hash`, `synth_three_way` | Shared internals; never include it directly. |
| `containers.h` | umbrella | Tests and tools. |

## Behaviour worth knowing

- `slot_key` is 8 bytes (`u32 index`, `u32 generation`); all-zero is the null key and generation 0 is
  never issued. A custom key only needs assignable unsigned `index`/`generation` fields of at most
  32 bits each (declared in any order); narrower fields shrink the key and retire slots earlier
  (generation wrap retires a slot for good). `clear()` bumps every generation, so no key survives
  it. The dense storage holds `slot_map_entry` objects, so iterators are plain pointers, `*it` is a
  real reference into the map, `for (auto &[key, value] : map)` binds straight into storage and
  `entries()` exposes the whole dense array. Pointers, references and iterators are invalidated by
  insert, erase, clear and reserve. A moved-from `slot_map` is empty (no slots, no free list) and
  reusable.
- Checked access never throws. `at()` (index or key) is checked in every configuration and a miss
  is fatal: it asserts, then terminates, exactly like `result::value()` on an error. `operator[]`,
  `front`, `back`, `pop_back`, `emplace`/`insert` positions and `erase` ranges assert in
  Debug/Development; the position/range variants also early-out so a continuing assert handler
  cannot corrupt the container.
- `small_vector` `push_back`/`emplace_back`/`insert`/`emplace` arguments and the `value` of
  `resize(count, value)` and `assign(count, value)` may alias elements of the same vector (only the
  range overloads of `insert` and `assign` must not). A single-pass (input iterator) range is
  collected into a temporary vector and moved in directly. Moving a heap-backed vector steals the
  buffer; moving an inline one relocates elements. `operator<=>` follows `std::vector`: it uses
  `T`'s `operator<=>` when there is one and falls back to `operator<` (weak ordering) otherwise.
- `stable_vector` iterators may only be ordered (`<`, `<=>`, `-`) against iterators of the same
  vector (asserted); equality works across vectors and is false.
- `flat_hash_map` rehashes only when an insertion would push `size()` above
  `capacity() * 0.875`, never because of long probe runs: probe sequences wrap around the end of
  the table and every slot stores 31 bits of the key's hash, from which `erase` recomputes home
  buckets. So after `reserve(n)` the next `n` insertions move nothing and element pointers stay
  valid, whatever the hash distribution (a hash with only 256 distinct values still fills the
  table to 0.875). `erase` never rehashes but shifts later elements of the same probe run one slot
  back, invalidating pointers to those elements only (`docs/design/10_core.md` 2.3 and
  `09_cross_area_resolutions.md` 3 record this: backward-shift deletion cannot keep every pointer
  stable). Iteration order is the slot order starting after an empty anchor slot that
  `erase` never fills, so `erase(iterator)` returns the next unvisited element even when a run
  wraps around the table; an insertion can move the anchor, so re-fetch `end()` after inserting.
  Copies keep the iteration order. Hash values go through a 64-bit finaliser, so identity-style
  `std::hash` specialisations are fine. Arguments passed to an inserting call must not refer to
  elements of the same map. Capacity is limited to 2^31 buckets. Nothing is allocated before the
  first insertion: `reserve(0)`, a zero-sized constructor and an empty initializer list leave
  `capacity()` at 0.
- `flat_map` stores `std::pair<K, V>`; do not modify a key through an iterator. With a
  transparent comparator (`std::less<>`) `find`, `contains`, `count`, `lower_bound`,
  `upper_bound`, `at` and `erase(key)` all accept key-like types such as `std::string_view`; the
  heterogeneous `erase(key)` excludes iterators, so `erase(iterator)` still erases by position.

## Allocation

Every container allocates through `std::allocator` from one or two private functions
(`allocate_storage`/`deallocate_storage`, `allocate_chunk`/`deallocate_chunk`,
`allocate_table`/`deallocate_table`); `slot_map` and `flat_map` sit on `std::vector`. Those are the
only places to touch when the memory area's tracked allocation is wired in.

## Threading

None of the containers is thread-safe for mutation. Concurrent read-only access (`find`,
`contains`, `operator[] const`, iteration) from any number of threads is safe as long as no thread
mutates; the tests exercise 8 concurrent readers on `slot_map` and `flat_hash_map`.

## Testing

`tests/core/containers/` builds `conjure_core_containers_tests`: one `test_<header>.cpp` per public
header plus `tracked_value.h` (counting element type, leak check, trivially copyable and
over-aligned elements). Every file contains a property test against the matching standard
container with `std::mt19937_64(0x5EED)`. `test_flat_hash_map.cpp` also fills reserved tables of
2^17 and 2^21 buckets exactly to their grow threshold and checks that no element moved, stores
100,000 keys through a 256-value hash at the minimum capacity, and exercises probe runs that wrap
around the end of the table.

```
python tools/build.py --build-dir build/core_containers --target conjure_core_containers_tests --test -R core_containers
build/core_containers/bin/conjure_core_containers_tests
```
