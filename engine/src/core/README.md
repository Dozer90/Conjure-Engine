# core

Foundation module. Every other module links `conjure::core`, which is an umbrella (INTERFACE target)
over the core areas below; each area is its own static library `conjure::core_<area>` with its own
test executable (`conjure_core_<area>_tests`) so areas can be built and tested independently. The
binding design is `docs/design/10_core.md`; where an area document spells a core name differently,
`10_core.md` and the headers win (`docs/design/09_cross_area_resolutions.md` section 3).

Public headers live in `engine/include/conjure/core/`: flat for the small areas (`foundation`,
`strings`, `time`, `system`) and under `conjure/core/<area>/` for `math`, `containers`, `memory`,
`jobs`, `json`, `vfs` and `reflect`, each of which also has an umbrella header. Every area README
documents its purpose, types, threading model and how to test it.

| Area | Directory | Public headers | Depends on |
|---|---|---|---|
| foundation | `foundation/` | `platform_detect.h`, `types.h`, `api.h`, generated `build_info.h`, `hash.h`, `error.h`, `result.h`, `assert.h`, `log.h`, `uuid.h`, `enum_flags.h`, `non_copyable.h` | - |
| math | `math/` | `math.h` (umbrella, also `conjure/core/math.h`), `glm_types.h`, `math_utils.h`, `transform.h`, `aabb.h`, `plane.h`, `frustum.h`, `ray.h`, `rect.h` | glm |
| containers | `containers/` | `containers.h` (umbrella), `slot_map.h`, `slot_key_format.h`, `small_vector.h`, `stable_vector.h`, `flat_hash_map.h`, `flat_map.h`, `detail.h` | - |
| strings | `strings/` | `string_id.h`, `string_utils.h`, `bit_vector.h`, `radix_sort.h` | - |
| memory | `memory/` | `memory.h` (umbrella), `allocation_counters.h`, `tracked_allocation.h`, `arena.h`, `pool_allocator.h`, `frame_allocator.h` | - |
| time | `time/` | `clock.h` (`clock`, `steady_clock`, `manual_clock`, `stopwatch`), `random.h`, `event.h`, `delegate.h`, `event_bus.h` | - |
| jobs | `jobs/` | `jobs.h` (umbrella), `job_system.h`, `job_handle.h`, `parallel_for.h`, `main_thread.h`, `dedicated_thread.h` | - |
| json | `json/` | `json.h` (umbrella with `parse` / `to_string`), `value.h`, `document.h`, `writer.h` | yyjson (private) |
| vfs | `vfs/` | `vfs.h` (umbrella), `file_system.h`, `native_file_system.h`, `memory_file_system.h`, `mount_table.h`, `path_utils.h` | - |
| system | `system/` | `process.h`, `dynamic_library.h`, `image.h` | stb (private) |
| reflect | `reflect/` | `reflect.h` (umbrella + `CONJURE_REFLECT_*` macros), `type_id.h`, `field_info.h`, `type_info.h`, `type_registry.h`, `json_conversion.h` | json, strings, math, containers, time |

Every area depends on `foundation`; the last column lists the additional public dependencies
(`10_core.md` C2). Third-party headers never appear in public headers; glm is the single exception,
included only through `conjure/core/math/glm_types.h`.

## Conventions shared by every area

- No exceptions: fallible calls return `result<T>` / `status` with a `core::error` (`error.h`). Core areas
  use the generic `error_code` category; modules bind their own enum with `CONJURE_DECLARE_ERROR_CATEGORY`.
- Programmer errors assert (`CONJURE_ASSERT`, compiled out in Distribution; `CONJURE_VERIFY` keeps evaluating
  its expression). `CONJURE_LOG_TRACE/DEBUG` compile out in Distribution as well.
- Logging goes through a `log_category` per module (`CONJURE_DECLARE_LOG_CATEGORY` / `CONJURE_DEFINE_LOG_CATEGORY(name,
  "display")`); sinks are `std::shared_ptr<log_sink>` added with `log::add_sink`.
- `new`/`delete` and `malloc` are confined to `memory` (`tracked_allocate` / `tracked_free` bump the process-wide
  `memory::counters()`); `std::thread` is confined to `jobs` (`job_system`, `dedicated_thread`).
- Platform code lives in per-platform translation units (`*_windows.cpp`, `*_posix.cpp`) selected by CMake, never
  behind `#ifdef` in shared code; `platform_detect.h` is the only header that reads compiler macros.
- With `using namespace conjure::core;` at global scope the unqualified name `clock` is ambiguous with the C
  library's `::clock()`: write `core::clock`.

## Threading model

- Logging is thread-safe; sinks run outside the logger lock and must synchronise themselves.
- `uuid::generate()` is thread-safe (per-thread generator); `random::default_engine()` is `thread_local`.
- The assert handler is an atomic function pointer.
- `memory::counters()`, `tracked_allocate` / `tracked_free` and `frame_allocator::allocate` are thread-safe;
  `arena` and `pool_allocator` are not.
- `jobs::job_system` and the main-thread queue are thread-safe; `jobs::is_main_thread()` names the thread
  designated by `set_main_thread()` (the test main does this for every test executable).
- `string_id` interning is mutex-guarded; `event_bus::enqueue` is thread-safe while `publish` / `dispatch_queued`
  are main-thread calls; `event<>` and `delegate` are not synchronised.
- `vfs::memory_file_system` and `mount_table` are mutex-guarded; `native_file_system` is stateless apart from its
  root. `reflect::registry()` guards registration with a mutex and keeps lookups lock-free.
- Per-area details are in each area's README.

## Testing

`tests/core/<area>/test_<header>.cpp` mirrors every public header; `conjure_core_<area>_tests` is one CTest entry per
`TEST_CASE`. Everything is headless and deterministic (`manual_clock`, seeded `random`, 0-worker job systems,
`memory_file_system` or a `temp_directory`). Every public header also compiles on its own through
`conjure_header_check` (part of the default build), and `tools/check_style.py --tests-mirror-headers` runs as the
CTest entry `style.check`.

```
python tools/build.py --build-dir build/core --test -R core_
```
