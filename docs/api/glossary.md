# Glossary

📘 **Public API wiki**

Every term the wiki uses without explaining it in place, one line each, with the page that explains it. Terms are
alphabetical; engine type names are in code spans.

| Term | Meaning | Where |
|---|---|---|
| Alignment | The address multiple a block of memory must start at; a power of two. Allocators take it as a parameter, `align_up()` rounds sizes to it | [Memory](core/memory.md) |
| Arena | A bump allocator: allocations are cheap and never freed one by one; `reset()` recycles everything at once | [Memory](core/memory.md) |
| Assertion | A check for a programmer error that can never fail in a correct program (`CONJURE_ASSERT`); compiled out of Distribution builds | [Logging and assertions](core/logging-and-assertions.md) |
| Assert handler | The function called when an assertion fails; returns `BREAK`, `CONTINUE` or `ABORT` | [Logging and assertions](core/logging-and-assertions.md) |
| Backward-shift deletion | How `flat_hash_map` erases without tombstones: later elements of the same probe run move one slot back | [Containers](core/containers.md) |
| Build configuration | One of Debug, Development or Distribution; decides whether asserts, trace logging and developer tools exist | [Data types](core/data-types.md) |
| Category (error) | Which system an `error` came from, named like `"core"` or `"inventory"`; bound to an enumeration with `CONJURE_DECLARE_ERROR_CATEGORY` | [Errors and results](core/errors-and-results.md) |
| Category (log) | A named logging channel with its own minimum level, such as `LOG_CORE` | [Logging and assertions](core/logging-and-assertions.md) |
| Cumulative counters | The allocation counters only ever grow; live allocations are `allocationCount - freeCount` | [Memory](core/memory.md) |
| Data-oriented design (DOD) | Laying data out for how it is processed (dense arrays, bit masks, arenas) rather than around objects; the engine's internals work this way and every wiki page shows the simple facade first and the data-oriented escape hatch second | [Containers](core/containers.md), [Memory](core/memory.md) |
| Dense storage | Elements packed contiguously with no holes, as in `slot_map`, so iteration is cache friendly | [Containers](core/containers.md) |
| Distribution build | The shipping configuration: no asserts, no editor, no developer tooling; `CONJURE_DISTRIBUTION` is 1 | [Data types](core/data-types.md) |
| `error` | A recoverable failure: category, code and message; carried by `result<T>` instead of being thrown | [Errors and results](core/errors-and-results.md) |
| `error_code` | The generic error enumeration any system may use (`NOT_FOUND`, `INVALID_ARGUMENT`, `IO_FAILURE`, ...) | [Errors and results](core/errors-and-results.md) |
| Flag enumeration | An `enum class` whose values are single bits, combined with `\|` after `CONJURE_ENUM_FLAGS(type);` and tested with `has_flag()` | [Data types](core/data-types.md) |
| FNV-1a | The hash function behind `string_id`, error categories and asset identity (`hash::fnv1a64`, `hash::fnv1a32`) | [Strings and identifiers](core/strings-and-identifiers.md) |
| Frame allocator | Per-frame scratch memory usable from every thread; everything it hands out dies at the next `reset()` | [Memory](core/memory.md) |
| Generation | The counter stored in a slot and in every `slot_key` issued for it; bumped on erase so stale keys stop matching | [Containers](core/containers.md) |
| Handle | A small value (a `slot_key`, a `uuid`, a `string_id`) that refers to something without being a pointer to it | [Containers](core/containers.md), [Data types](core/data-types.md) |
| High-water mark | The largest value a statistic has reached, such as `frame_allocator::high_water_bytes()`; used to size budgets | [Memory](core/memory.md) |
| Interning | Recording the text of a `string_id` in the process-wide table so the hash can be shown as a name again | [Strings and identifiers](core/strings-and-identifiers.md) |
| Level (log) | `TRACE`, `DEBUG`, `INFO`, `WARN`, `ERR`, `FATAL` or `OFF`; a message is delivered when it is at or above both the category and global level | [Logging and assertions](core/logging-and-assertions.md) |
| Load factor | `size() / capacity()` of a hash table; `flat_hash_map` rehashes when an insertion would push it above 0.875 | [Containers](core/containers.md) |
| Module | A unit of the engine (core, ecs, renderer, ...) or of a game, with its own headers, log category, error category and tests | [Public API wiki](index.md) |
| Null id / null key | The all-zero `string_id`, `uuid` or `slot_key`, meaning "none"; test with `is_null()` or `is_valid()` | [Strings and identifiers](core/strings-and-identifiers.md), [Containers](core/containers.md) |
| Pool allocator | Fixed-size blocks carved from slabs, freed individually; for many objects of one type | [Memory](core/memory.md) |
| Probe run | In an open-addressing hash table, the consecutive occupied slots a lookup walks from a key's home bucket | [Containers](core/containers.md) |
| Radix sort | A stable, allocation-free sort for unsigned integer keys with optional payload indices (`radix_sort_u64`) | [Strings and identifiers](core/strings-and-identifiers.md) |
| `result<T>` | Either a value or an `error`; the engine's replacement for exceptions. Check it before reading the value | [Errors and results](core/errors-and-results.md) |
| Scratch memory | Memory needed only briefly (one frame, one load) and released all at once; arenas and the frame allocator provide it | [Memory](core/memory.md) |
| Sink | A destination for log messages: console, file or a callback of your own | [Logging and assertions](core/logging-and-assertions.md) |
| Slab | One heap block of a `pool_allocator` holding `blocksPerSlab` blocks and their free bitmap | [Memory](core/memory.md) |
| `slot_key` / `slot_map` | A generational handle and the dense container it addresses; stale keys are detected, not dereferenced | [Containers](core/containers.md) |
| Stable storage | Storage whose element addresses never change while the element exists, as in `stable_vector` | [Containers](core/containers.md) |
| `status` | `result<void>`: success or an `error`, no value | [Errors and results](core/errors-and-results.md) |
| `string_id` | A name reduced to its 64-bit FNV-1a hash; compared by hash, printed by name once interned | [Strings and identifiers](core/strings-and-identifiers.md) |
| Tracked allocation | A heap block obtained through `tracked_allocate`, counted in the process-wide counters and released with `tracked_free` | [Memory](core/memory.md) |
| Transparent comparator | A comparator such as `std::less<>` that lets a `flat_map<std::string, V>` be searched with a `std::string_view` | [Containers](core/containers.md) |
| Trivially destructible | A type whose destructor does nothing, which is what arenas and the frame allocator require of the objects they hold | [Memory](core/memory.md) |
| `uuid` | A 128-bit identifier that stays stable across sessions and machines; used for assets and prefabs | [Data types](core/data-types.md) |
| Verify | `CONJURE_VERIFY`: an assertion whose expression still runs in Distribution builds; for checked calls with side effects | [Logging and assertions](core/logging-and-assertions.md) |
| Wave | A stage of the engine's development plan; wave 1 is core, later waves add ECS, platform, rendering, physics, scripting and the editor | [Public API wiki](index.md) |
