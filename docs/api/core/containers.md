# Containers

📘 **Public API wiki** · 🛠 Handbook: [Core module](../../handbook/modules/core.md)

Cache-friendly containers that complement the standard library. Each one lives in its own header under
`conjure/core/containers/`; include just the one you need (the umbrella `conjure/core/containers/containers.h`
pulls in all of them and is meant for tests and tools). Everything is in `namespace conjure::core` and follows the
standard container vocabulary (`size()`, `begin()`, `insert()`, `erase()`), so what you know from `std::vector`
and `std::unordered_map` carries over.

## At a glance

| You want to... | Use | Header |
|---|---|---|
| Hand out handles that detect stale use (particles, GPU resources, requests) | `slot_map<T>` with `slot_key` | `conjure/core/containers/slot_map.h` |
| Print or parse a `slot_key` | `to_string(key)`, `parse_slot_key(text)`, `std::format("{}", key)` | `conjure/core/containers/slot_key_format.h` |
| A short list that usually fits in a few elements (children, tags) | `small_vector<T, N>` | `conjure/core/containers/small_vector.h` |
| A growing array whose elements must never move | `stable_vector<T>` | `conjure/core/containers/stable_vector.h` |
| A general key -> value map, fast lookups | `flat_hash_map<K, V>` / `flat_hash_set<K>` | `conjure/core/containers/flat_hash_map.h` |
| A small map, or one that iterates in key order | `flat_map<K, V>` | `conjure/core/containers/flat_map.h` |

`conjure/core/containers/detail.h` holds shared internals; never include it directly.

## Quick example

```cpp
#include "conjure/core/containers/slot_map.h"

using namespace conjure;
using namespace conjure::core;

struct particle {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 life = 1.0f;
};

class particle_system {
    private:
        slot_map<particle> mParticles = {};

    public:
        [[nodiscard]] slot_key spawn(f32 x, f32 y) {
            return mParticles.insert(particle{x, y, 1.0f});
        }

        void kill(slot_key key) {
            mParticles.erase(key);              // false for a stale key, nothing happens
        }

        void update(f32 deltaSeconds) {
            for (auto &[key, item] : mParticles) {
                item.life -= deltaSeconds;
            }
        }

        [[nodiscard]] bool is_alive(slot_key key) const {
            return mParticles.contains(key);    // stale keys are rejected, even after slot reuse
        }
};
```

## Handles that detect stale use: `slot_map`

A `slot_map<T>` stores elements densely and addresses them through a `slot_key` (a slot index plus a generation).
When an element is erased its slot's generation changes, so every copy of the old key stops matching: `find()`
returns `nullptr`, `contains()` returns false and `erase()` returns false. That is what makes handles safe to keep
in other systems, in scripts and in the editor.

| You want to... | Use |
|---|---|
| Add an element | `emplace(args...)` or `insert(value)`; both return the key |
| Get the element behind a key | `find(key)` returns `T *` or `nullptr` |
| Ask whether a key is still live | `contains(key)` |
| Remove | `erase(key)` returns `bool`; `erase(iterator)` returns the next iterator |
| Visit every live element | `for (auto &[key, value] : map)`, or `entries()` for the dense array |
| Remove everything | `clear()` (every outstanding key becomes stale) |
| Pre-size | `reserve(count)` |

`slot_key` itself is 8 bytes and trivially copyable: `index`, `generation`, `is_valid()` (false only for the null
key), `to_u64()` / `from_u64()` for opaque storage, `slot_key::null()`, defaulted comparisons and `std::hash`.
Include `conjure/core/containers/slot_key_format.h` to print it as `"index:generation"` or parse it back.

> [!TIP]
> Store a `slot_key` wherever you would otherwise store a pointer to an element: it survives reallocation and
> tells you when the element is gone. Look the element up fresh each frame rather than caching the `T *`.

> [!WARNING]
> Pointers from `find()`, and references and iterators obtained while iterating, are invalidated by `insert`,
> `emplace`, `erase`, `clear` and `reserve`. Erasing swaps the last element into the hole, so iteration order is
> dense storage order and changes on erase. `erase(iterator)` returns the iterator to reuse when erasing while
> iterating. Not thread-safe for mutation; concurrent read-only access is fine.

> [!NOTE]
> A custom key type works too (`slot_map<T, my_key>`) when it has unsigned `index` and `generation` fields of at
> most 32 bits each; narrower fields make smaller keys and retire slots sooner. `slot_count()`,
> `free_slot_count()` and `retired_slot_count()` report the slot table's state.

## Short lists: `small_vector`

`small_vector<T, N>` is a `std::vector` that keeps its first `N` elements inside the object; the heap is touched
only when the list grows past `N`. It has the familiar API (`push_back`, `emplace_back`, `insert`, `erase`,
`resize`, `reserve`, `operator[]`, iterators, `==` and `<=>`) plus `is_inline()`, `INLINE_CAPACITY` and implicit
conversion to `std::span<T>`.

```cpp
#include "conjure/core/containers/small_vector.h"

#include <span>

using namespace conjure;
using namespace conjure::core;

struct node {
    small_vector<u32, 4> children = {};     // no heap allocation until the fifth child
};

u32 first_child_or(const node &parent, u32 fallback) {
    const std::span<const u32> children = parent.children;
    return children.empty() ? fallback : children.front();
}
```

> [!TIP]
> Pick `N` from the common case, not the maximum: a `small_vector<u32, 4>` costs 16 bytes of inline storage and
> still grows without limit. `shrink_to_fit()` moves the elements back inline when they fit again.

> [!WARNING]
> The range overloads of `insert()` must not alias the vector itself. Single-element `push_back`, `emplace_back`,
> `insert` and `emplace` are safe with arguments that refer to existing elements.

## Elements that never move: `stable_vector`

`stable_vector<T, ChunkSize = 64>` grows by adding fixed-size chunks instead of reallocating, so a reference
returned by `emplace_back()` stays valid until that element is destroyed by `pop_back()`, `clear()` or the vector's
destruction. Use it for registries where other code keeps pointers (asset slots, system descriptors). It supports
`emplace_back`, `push_back`, `pop_back`, `clear`, `reserve`, `shrink_to_fit`, `operator[]`, `at`, `front`, `back`
and random-access iterators; there is deliberately no insertion or erasure in the middle.

```cpp
#include "conjure/core/containers/stable_vector.h"

#include <string>

using namespace conjure;
using namespace conjure::core;

struct asset_slot {
    std::string path = {};
    u32 referenceCount = 0;
};

asset_slot &register_asset(stable_vector<asset_slot> &slots, std::string path) {
    asset_slot &slot = slots.emplace_back();   // &slot is valid for as long as the slot exists
    slot.path = std::move(path);
    return slot;
}
```

> [!WARNING]
> Iterators of two different `stable_vector`s may be compared for equality (always false) but never ordered or
> subtracted; that asserts. Chunks are kept on `clear()` and `pop_back()`; call `shrink_to_fit()` to release them.

## General-purpose maps: `flat_hash_map` and `flat_hash_set`

`flat_hash_map<K, V>` is an open-addressing hash map that stores `std::pair<const K, V>` in place; it is the
default choice for anything keyed by `string_id`, `uuid`, an integer or a string. `flat_hash_set<K>` is the same
table without values.

| You want to... | Use |
|---|---|
| Look up | `find(key)` (iterator or `end()`), `contains(key)`, `count(key)` (0 or 1) |
| Insert only if absent, building the value in place | `try_emplace(key, args...)` returns `{iterator, inserted}` |
| Insert or overwrite | `insert_or_assign(key, value)` |
| Get or create a default | `map[key]` |
| Read a key that must exist | `at(key)` |
| Remove | `erase(key)` returns `bool`; `erase(iterator)` returns the next iterator |
| Avoid rehashing during a burst of insertions | `reserve(count)` first |

```cpp
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/string_id.h"

using namespace conjure;
using namespace conjure::core;

class component_registry {
    private:
        flat_hash_map<string_id, u32> mIndexByName = {};

    public:
        u32 register_component(string_id name) {
            const u32 next = static_cast<u32>(mIndexByName.size());
            return mIndexByName.try_emplace(name, next).first->second;   // existing index when already registered
        }

        [[nodiscard]] bool has(string_id name) const {
            return mIndexByName.contains(name);
        }
};
```

> [!TIP]
> Prefer `try_emplace(key, args...)` over `map[key] = value`: the value is constructed only when the key is new,
> and the returned pair tells you whether it was. `operator[]` is for "get or create a default".

> [!WARNING]
> Rehashing happens only when an insertion would push the load above 0.875 of the capacity, so after
> `reserve(n)` the next `n` insertions move nothing. Erasing shifts the following elements of the same probe run
> back by one slot, which invalidates pointers to those elements only. Re-fetch `end()` after inserting: an
> insertion can move the slot where iteration starts. Iteration order is slot order; it changes on rehash and is
> never sorted. Arguments to an inserting call must not refer to elements of the same map.

## Sorted maps: `flat_map`

`flat_map<K, V, Compare = std::less<K>>` keeps `std::pair<K, V>` entries in a sorted vector: lookups are a binary
search, insertion and erasure shift the tail. It wins for small maps and whenever iteration order must be
deterministic (serialisation, load order). The API matches `flat_hash_map` and adds `lower_bound`, `upper_bound`,
reverse iterators and `entries()` (a contiguous read-only `std::span`).

```cpp
#include "conjure/core/containers/flat_map.h"

#include <functional>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::core;

flat_map<std::string, int, std::less<>> settings = {{"width", 1280}, {"height", 720}};

int setting_or(std::string_view name, int fallback) {
    const auto found = settings.find(name);          // std::less<> allows lookup by string_view
    return found == settings.end() ? fallback : found->second;
}
```

> [!TIP]
> Give string-keyed maps the transparent comparator `std::less<>` as shown; then `find`, `contains`, `count`,
> `lower_bound`, `upper_bound`, `at` and `erase(key)` accept a `std::string_view` or a `const char *` without
> building a `std::string`.

> [!WARNING]
> Entries are plain `std::pair<K, V>`, so a key *can* be modified through an iterator; doing so breaks the sort
> order and every later lookup. Treat keys as read-only.

## Checked access and invalidation

All containers share two rules. First, nothing throws: `at()` on a missing key or an out-of-range index is a
fatal programmer error (it asserts, then terminates, in every build configuration). `small_vector` and
`stable_vector` assert on an out-of-range `operator[]` and on `front()`, `back()` or `pop_back()` of an empty
container, in Debug and Development builds only; the maps' `operator[]` inserts a default value instead. Second,
none of them is thread-safe for mutation; any number of threads may read concurrently as long as nobody writes.

| Container | What invalidates pointers and iterators |
|---|---|
| `slot_map` | `insert`, `emplace`, `erase`, `clear`, `reserve` |
| `small_vector` | any growth past capacity, `insert`/`erase` before the element, `shrink_to_fit` |
| `stable_vector` | only destroying that element (`pop_back`, `clear`, destruction) |
| `flat_hash_map` / `flat_hash_set` | a rehash (insertion past the load threshold), `erase` of an element in the same probe run, `clear` |
| `flat_map` | `insert`/`erase` before the element, any growth past capacity |

## API summary

| Signature (short) | Purpose |
|---|---|
| `slot_map<T, Key = slot_key>` | dense storage with generational keys |
| `Key emplace(Args &&...)` / `Key insert(T)` | add, returning the key |
| `T *find(Key)` / `bool contains(Key)` / `bool erase(Key)` / `iterator erase(const_iterator)` | key access |
| `std::span<entry> entries()` / `Key key_at(usize)` / `void clear()` / `void reserve(usize)` | bulk access |
| `struct slot_key { u32 index; u32 generation; }` with `is_valid()`, `to_u64()`, `from_u64()`, `null()` | the default key |
| `std::string to_string(slot_key)` / `std::optional<slot_key> parse_slot_key(std::string_view)` | text form |
| `small_vector<T, N>` | inline storage for `N` elements; `std::vector` API, `is_inline()`, `operator std::span<T>()` |
| `stable_vector<T, ChunkSize = 64>` | `T &emplace_back(Args &&...)`, `pop_back()`, `clear()`, `reserve()`, `shrink_to_fit()`, `chunk_count()` |
| `flat_hash_map<K, V, Hash = std::hash<K>, Eq = std::equal_to<K>>` | open-addressing map |
| `std::pair<iterator, bool> try_emplace(const K &, Args &&...)` / `insert_or_assign(const K &, M &&)` / `insert(value)` / `emplace(args...)` | insertion |
| `iterator find(const K &)` / `bool contains(const K &)` / `V &at(const K &)` / `V &operator[](const K &)` | lookup |
| `bool erase(const K &)` / `iterator erase(const_iterator)` / `void reserve(usize)` / `void clear()` / `f32 load_factor()` | maintenance |
| `flat_hash_set<K, Hash, Eq>` | same table, keys only |
| `flat_map<K, V, Compare = std::less<K>>` | sorted vector map; adds `lower_bound`, `upper_bound`, `entries()`, `rbegin()`/`rend()`, heterogeneous lookup with a transparent `Compare` |

## See also

- 📘 API: [Strings and identifiers](strings-and-identifiers.md): `string_id` as a map key, `bit_vector` for masks
- 📘 API: [Memory](memory.md) for allocators when a container's allocation pattern is the bottleneck
- 📘 API: [Glossary](../glossary.md)
