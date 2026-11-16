# Core areas: foundation to memory

🛠 **Engineering handbook** · 📘 API: [Data types](../../../api/core/data-types.md), [Errors and results](../../../api/core/errors-and-results.md), [Logging and assertions](../../../api/core/logging-and-assertions.md), [Math](../../../api/core/math.md), [Containers](../../../api/core/containers.md), [Strings and identifiers](../../../api/core/strings-and-identifiers.md), [Memory](../../../api/core/memory.md)

The first five areas of `conjure::core`: what each is for, its public headers, the algorithms worth knowing
before changing them, what is and is not thread-safe, where the area can be extended and how it is tested.
The shape of the module, the shared conventions and the build and test commands are on the parent page
[Core module](../core.md); the remaining areas are on [Core areas: time to reflect](time-to-reflect.md).
`C<n>` references point at the conventions table of [`10_core.md`](../../../design/10_core.md).

## Contents
- [1. foundation](#1-foundation)
- [2. math](#2-math)
- [3. containers](#3-containers)
- [4. strings](#4-strings)
- [5. memory](#5-memory)
- [Related](#related)

## 1. foundation

Purpose: the part of core every other area and module builds on.

### 1.1 Headers

| Header | Contents |
|---|---|
| `conjure/core/platform_detect.h` | `CONJURE_PLATFORM_*`, `CONJURE_COMPILER_*`, `CONJURE_ARCH_*`, `CONJURE_STDLIB_*`; exactly one of `CONJURE_DEBUG` / `CONJURE_DEVELOPMENT` / `CONJURE_DISTRIBUTION` is 1 (Development is the fallback); `CONJURE_ASSERTS_ENABLED`, `CONJURE_DEV_TOOLS_ENABLED`; helpers `CONJURE_FORCE_INLINE`, `CONJURE_DEBUG_BREAK`, `CONJURE_UNREACHABLE_HINT`, `CONJURE_SYMBOL_EXPORT/IMPORT`, `CONJURE_UNIQUE_NAME`. The only header allowed to read `_WIN32` and friends |
| `conjure/core/types.h` | `u8`..`u64`, `i8`..`i64`, `f32`, `f64`, `usize`, `isize`, `byte`, `KIB`/`MIB`/`GIB`, `CACHE_LINE_SIZE = 64`, directly in `namespace conjure`; 64-bit targets only |
| `conjure/core/api.h` | `CONJURE_API` (export under `CONJURE_SHARED_EXPORTS`, import under `CONJURE_SHARED_IMPORTS`, empty for static consumers) and `CONJURE_PLUGIN_EXPORT` |
| `conjure/core/build_info.h` | generated from `engine/src/core/foundation/build_info.h.in`: `CONJURE_VERSION_MAJOR/MINOR/PATCH/STRING`, `CONJURE_ENGINE_NAME`, `build_info::VERSION_*` |
| `conjure/core/hash.h` | `hash::fnv1a64` / `fnv1a32` over `std::string_view` and `std::span<const std::byte>`, `*_step`, `combine`; `constexpr`; the constants are ABI (string ids, error categories, C# `NameHash`) |
| `conjure/core/error.h` | `error_code` (`NONE`, `UNKNOWN`, `INVALID_ARGUMENT` ... `CANCELLED`, `COUNT`), `class error` (`category()`, `code()`, `code_name()`, `generic_code()`, `message()`, `is(code)`, `is_category<Enum>()`, `to_string()`), `make_error(code[, detail])` / `make_error(code, fmt, args...)`, `CONJURE_DECLARE_ERROR_CATEGORY(enum, "name", name_fn)`, `std::formatter` |
| `conjure/core/result.h` | `result<T>` (value or `error`; `value()`, `get_error()`, `value_or()`, `operator->`, `operator*`), `result<void>` with `success()`, alias `status`, `CONJURE_TRY`, `CONJURE_TRY_ASSIGN` |
| `conjure/core/assert.h` | `CONJURE_ASSERT`, `CONJURE_VERIFY`, `CONJURE_UNREACHABLE`, `CONJURE_FATAL_ASSERT`, `assert_info`, `assert_action {BREAK, CONTINUE, ABORT}`, `set_assert_handler` / `current_assert_handler` / `default_assert_handler` |
| `conjure/core/log.h` | `log_level` (`ERR`, not `ERROR`) with `to_string` / `parse_log_level` (a `std::optional`: `nullopt` for an unknown name), `log_category`, `log_message`, `log_sink` with `console_log_sink` / `file_log_sink` / `callback_log_sink`, `log::add_sink` / `remove_sink` / `clear_sinks` / `set_console_enabled` / `set_global_level` / `categories` / `find_category` / `set_category_level` / `flush`, `format_log_line`, `LOG_CORE`, the `CONJURE_DECLARE/DEFINE_LOG_CATEGORY` and `CONJURE_LOG_*` macros |
| `conjure/core/uuid.h` | `uuid` (two `u64` halves): `generate()` (RFC 4122 v4), `parse`, `to_string`, `is_null`, `hash`, `<=>`, `std::hash`, `std::formatter` |
| `conjure/core/enum_flags.h` | `CONJURE_ENUM_FLAGS(type);` plus `has_flag`, `has_any_flag`, `with_flag`, `without_flag`, `to_underlying` |

### 1.2 Algorithms

- An error category id is the FNV-1a 32 of its name; `error::generic_code()` is `error_code::NONE` for module
  errors, so compare with `is(<enum>::VALUE)`.
- `make_error(code, "fixed text")` with one string argument resolves to the `std::string_view` overload (the
  `std::format_string` overload needs at least one argument).
- Log messages are formatted only when the category and global levels allow them and are delivered to sinks
  *outside* the logger lock, so a sink may log or add/remove sinks.
- `parse_log_level` matches the seven names of `to_string` case-insensitively and returns `std::nullopt` for
  anything else. It never answers `OFF` for a name it does not know (code sweep F209): a caller tests
  `has_value()` and, where `OFF` is no sensible answer (a filter's minimum level), rejects `OFF` itself.
- `wrong_result_alternative` asserts, breaks and aborts: a mis-accessed `result` is never continued.
- There is no copy-control base class (decision C13 of `docs/design/10_core.md`, code sweep F212). A type that
  must not be copied or moved deletes the operations itself and defaults the ones it keeps; `docs/CODE_STYLE.cpp`
  has the example. Do not add such a base: it hides in a base-class list what the class body can say directly.

### 1.3 Threading

Logging is thread-safe and sinks must synchronise themselves, because `log::write` calls them concurrently
without holding its own lock: `file_log_sink` locks internally; `callback_log_sink` does not, so the callable
you pass must be thread-safe; `console_log_sink` holds no lock either and relies on one `fprintf` per message,
so lines stay whole but their order across threads is not defined. Category and global levels are relaxed
atomics; the assert handler is an atomic function pointer; `uuid::generate()` uses a per-thread
`std::mt19937_64` seeded from `std::random_device`.

### 1.4 Extension points

A module error enum bound with `CONJURE_DECLARE_ERROR_CATEGORY`; a module log category; a custom `log_sink`;
a test assert handler (`conjure::testing::expect_assert`).

### 1.5 Tests

`tests/core/foundation/`: `test_assert`, `test_enum_flags`, `test_error`, `test_hash` (pins the FNV-1a vectors
the C# `NameHash` must reproduce, wave 4), `test_log`, `test_result`, `test_uuid`. The macro-only headers are
listed in `tools/style_exceptions.json`.

## 2. math

Purpose: glm behind one header plus the geometry value types every module shares (`namespace conjure::math`).

### 2.1 Headers

| Header | Contents |
|---|---|
| `conjure/core/math.h` | one-line include of the umbrella |
| `conjure/core/math/math.h` | umbrella plus `perspective`, `infinite_perspective`, `orthographic`, `look_at` (view matrix), `project`, `unproject` |
| `conjure/core/math/glm_types.h` | the glm includes; aliases `vec2/3/4`, `ivec2/3/4`, `uvec2/3/4`, `quat`, `mat3/4`; `nearly_equal` for each, `same_rotation`, `safe_normalize`, `std::formatter`s |
| `conjure/core/math/math_utils.h` | `PI`, `EPSILON`, `radians`, `degrees`, `wrap_angle`, `nearly_equal`, `clamp`, `lerp`, `smoothstep`, `is_power_of_two`, `align_up/down`, `next/previous_power_of_two`, `log2_floor`, `divide_round_up` (no glm) |
| `conjure/core/math/transform.h` | `transform` (TRS, 40 bytes, trivially copyable, mirrored by C#): `to_matrix`, `from_matrix`, `operator*`, `inverse`, `forward/right/up`, `transform_point/direction`, `inverse_transform_*`, `look_at`, `lerp` |
| `conjure/core/math/aabb.h`, `conjure/core/math/plane.h`, `conjure/core/math/frustum.h`, `conjure/core/math/ray.h`, `conjure/core/math/rect.h` | `aabb` (inverted by default, 8-corner `transformed`), `plane`, `frustum` (Gribb-Hartmann extraction, conservative p-vertex test), `ray` (slab, sphere, plane, Moller-Trumbore), `rect` / `urect` |

### 2.2 Algorithms and conventions

Right-handed, Y up, -Z forward (glTF); column-major matrices; depth 0..1 (`GLM_FORCE_DEPTH_ZERO_TO_ONE`);
radians everywhere; explicit glm constructors (`vec3(0.0f)`). `quat` is constructed `(w, x, y, z)` and stored
`x, y, z, w` (documented in `conjure/core/math/glm_types.h`, which `static_assert`s only the unpadded sizes;
`test_glm_types.cpp` pins the layout, R58). `transform` composition and `inverse()` are exact for uniform
scale only (a TRS cannot hold shear).

### 2.3 Threading

Everything is a pure function or value type: safe from any thread, no state.

### 2.4 Tests

`tests/core/math/`, one file per header plus `math_test.h`; frustum extraction verified against
`glm::perspective`, transforms against glm matrix products.

## 3. containers

Purpose: cache-friendly containers, header-only templates in `namespace conjure::core`.

### 3.1 Headers

| Header | Type | Use for |
|---|---|---|
| `conjure/core/containers/slot_map.h` | `slot_key`, `slot_map<T, Key>`, `slot_map_entry` | handles that detect stale use; dense `{key, value}` storage, swap-remove, O(1) allocation-free `find` |
| `conjure/core/containers/slot_key_format.h` | `to_string`, `parse_slot_key`, `std::formatter<slot_key>` | logging and serialising keys (`"12:3"`) |
| `conjure/core/containers/small_vector.h` | `small_vector<T, N>` | short lists that usually fit inline; `std::span` conversion; memcpy relocation for trivially copyable `T` |
| `conjure/core/containers/stable_vector.h` | `stable_vector<T, ChunkSize>` | growing arrays whose element addresses never move (asset slots) |
| `conjure/core/containers/flat_hash_map.h` | `flat_hash_map<K, V>`, `flat_hash_set<K>` | general hashing: linear probing, backward-shift deletion, power-of-two capacity, max load 0.875 |
| `conjure/core/containers/flat_map.h` | `flat_map<K, V, Compare>` | small maps and deterministic iteration order (sorted vector, transparent comparators) |
| `conjure/core/containers/detail.h`, `conjure/core/containers/containers.h` | shared internals; umbrella | never include `detail.h` directly |

### 3.2 Algorithms

- `slot_map` keys are `{u32 index, u32 generation}`; generation 0 is never issued, a wrap retires the slot,
  `clear()` bumps every generation. A custom key only needs unsigned `index`/`generation` fields of at most
  32 bits (`slot_key_type` concept).
- `flat_hash_map` stores 31 bits of hash per slot, rehashes only when `size()` would pass `capacity() * 0.875`
  (so after `reserve(n)` the next `n` insertions move nothing, whatever the hash distribution), and `erase`
  shifts later elements of the same probe run back one slot, invalidating pointers to those elements only;
  iteration starts after an empty anchor slot so `erase(iterator)` returns the next unvisited element.
- Checked access never throws: `at()` is fatal on a miss in every configuration; `operator[]` and position
  arguments assert and early-out in Debug/Development.
- Every container allocates through `std::allocator` from one or two private functions, so container memory
  is not seen by `memory::counters()` outside test executables (`docs/design/10_core.md` 2.5); those
  functions are the only places to touch if containers are ever routed through `tracked_allocate`.

### 3.3 Threading

No container is thread-safe for mutation; concurrent read-only access is fine (tested with 8 readers).

### 3.4 Tests

`tests/core/containers/` with property tests against the standard containers (`std::mt19937_64(0x5EED)`),
`tracked_value.h` for leak and relocation checks, and 2^17 / 2^21 bucket tables filled to their grow
threshold.

## 4. strings

Purpose: hashed identifiers, ASCII string helpers, a packed bit set and a stable radix sort
(`conjure::core_strings`).

### 4.1 Headers

| Header | Contents |
|---|---|
| `conjure/core/string_id.h` | `string_id` (64-bit FNV-1a, `explicit` constructor, `intern`, `from_hash`, `view`/`c_str`/`to_string`, `interned_count`), `"name"_sid` in `inline namespace literals`, `std::hash`, `std::formatter` |
| `conjure/core/string_utils.h` | `namespace str`: ASCII case and classification, `trim*`, `starts_with`/`ends_with` (+ `i` variants), `iequals`, `contains`, `split`/`split_lines`/`join`/`replace_all`, `parse_integer<T>`/`parse_int`/`parse_uint`/`parse_double`/`parse_float`/`parse_bool`, `format_bytes`, UTF-8 <-> UTF-16/UTF-32/wide, `is_valid_utf8`, `is_valid_identifier` |
| `conjure/core/bit_vector.h` | `bit_vector`: 64-bit words, `set`/`reset`/`test`, `any`/`none`/`all`, `count`, `find_first_set/clear`, bitwise ops, `set_bits()` range |
| `conjure/core/radix_sort.h` | `radix_sort_u64` / `radix_sort_u32`, keys with optional `u32` payloads, caller-provided scratch |

### 4.2 Algorithms

- The `string_id` constructor interns the name only when it is *not* constant-evaluated, and the language
  decides that: namespace-scope and `static const` ids are constant-initialised and never interned (use
  `string_id::intern`), runtime values always intern, `"x"_sid` is `consteval` and never locks.
- A collision (second spelling under an existing hash) asserts, logs `ERR` on `LOG_CORE` and keeps the first
  spelling; the table lock is released before reporting, so handlers may format ids.
- The radix sort skips any 8-bit pass in which every key shares the digit, so sorted and all-equal inputs cost
  one histogram sweep.

### 4.3 Threading

The name table is a process-wide map guarded by a `std::shared_mutex` (lookups and re-registration of a known
name shared, a new name exclusive) that is never destroyed (views stay valid through static destructors);
comparison never touches it; `bit_vector` is a plain value type. Entries are never erased, so `string_id` is for
bounded vocabularies: unique per-spawn names grow memory for the life of the process.

### 4.4 Tests

`tests/core/strings/`, including interning from 8 threads, UTF round trips and 100k-element sorts against
`std::stable_sort`.

## 5. memory

Purpose: counted heap allocation and the engine's allocators (`namespace conjure::core::memory`).

### 5.1 Headers

| Header | Contents |
|---|---|
| `conjure/core/memory/allocation_counters.h` | `allocation_counters` (three cumulative atomics), `memory_snapshot` (`live_count()`), `counters()`, `snapshot()`, `record_allocation`, `record_free` |
| `conjure/core/memory/tracked_allocation.h` | `tracked_allocate(bytes, alignment)`, `tracked_free(pointer, bytes)`, `tracked_reallocate`, `DEFAULT_ALIGNMENT`, `MAX_ALLOCATION_BYTES` (`PTRDIFF_MAX`), `is_power_of_two`, `align_up`, `is_aligned` |
| `conjure/core/memory/arena.h` | `arena(blockBytes = 64 KiB)`: bump allocator over a chain of blocks, `allocate`, `construct<T>` (trivially destructible), `reset()` keeps the first regular-sized block and frees every other one, `release()` frees all |
| `conjure/core/memory/pool_allocator.h` | `pool_allocator(blockSize, alignment, blocksPerSlab, maxSlabs = 0)`: fixed-size blocks in slabs, `allocate()` returns `nullptr` on exhaustion, `deallocate`, `construct`/`destroy`, `shrink_to_fit`, `owns` |
| `conjure/core/memory/frame_allocator.h` | `frame_allocator(arenaBytes, threadBlockBytes)`: per-frame, per-thread scratch with heap overflow; `allocate`, `allocate_span<T>`, `construct<T>`, `reset`, `high_water_bytes`, `overflow_count`, `epoch` |
| `conjure/core/memory/memory.h` | umbrella |

### 5.2 Algorithms

- Counting is cumulative (nothing is decremented); `tracked_reallocate` counts one free plus one allocation.
  `tracked_allocation.cpp` is the only platform backend (`_aligned_malloc` on Windows, `posix_memalign`
  elsewhere); requests above `MAX_ALLOCATION_BYTES` are refused before the runtime sees them so size
  arithmetic above never wraps. Everything outside the standard library and the core containers allocates
  through it.
- `arena::reset()` keeps the first block whose capacity is the regular block size and frees every other block,
  so a dedicated oversized block never survives a reset wherever it sits in the chain. An arena whose first
  request of every cycle is oversized re-allocates that block every cycle; a larger block size fixes that.
- The pool keeps a sorted slab table and a free bitmap per slab, so `allocate`/`deallocate`/`owns` are
  O(log slabCount) and a double free is detected.
- The frame allocator is the **epoch-based** design: a thread claims a block with one compare-exchange that
  never moves the offset past the capacity, then bumps inside it through a `thread_local` slot
  `{owner, epoch, cursor, end, arenaExhausted}`; `reset()` frees overflow blocks, rewinds the arena and takes
  a new epoch from a process-wide counter, so a stale slot never matches.

> [!WARNING]
> `DEFAULT_ALIGNMENT` is `alignof(std::max_align_t)`, which is 8 on MSVC x64 while
> `__STDCPP_DEFAULT_NEW_ALIGNMENT__` is 16. A global `operator new` replacement (the test support's
> `tracked_global_new.cpp`) must pass the new-alignment explicitly or 16-byte aligned types end up under-aligned
> on MSVC only.

### 5.3 Threading

Counters and `tracked_*` are thread-safe; `arena` and `pool_allocator` are not (the ECS wraps its chunk pool
under the world lock); `frame_allocator::allocate` and the getters are lock-free from any thread, `reset()`
must not race `allocate()`.

### 5.4 Tests

`tests/core/memory/`; the frame allocator tests use `std::thread` (allowed by `10_core.md` 2.5) to prove
disjoint ranges and epoch invalidation; size-limit tests use `SIZE_MAX`-based requests rejected before any
system call.

## Related
- parent page: [Core module](../core.md); sibling: [Core areas: time to reflect](time-to-reflect.md)
- design records: [`../../../design/10_core.md`](../../../design/10_core.md) 2.1-2.5,
  [`../../../design/09_cross_area_resolutions.md`](../../../design/09_cross_area_resolutions.md) R30, R58, R63
- area READMEs: `engine/src/core/{foundation,math,containers,strings,memory}/README.md`
- handbook: [Coding standards](../../coding-standards.md) 5-7 (error model, asserts, logging),
  [Data-oriented design](../../data-oriented-design.md) 7-10 (frame memory, allocation-free steady state,
  `string_id`, `slot_map`)
