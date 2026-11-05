# core

Foundation module. Every other module links `conjure::core`, which is an umbrella over the core
areas below; each area is its own static library `conjure::core_<area>` with its own tests
(`conjure_core_<area>_tests`) so areas can be built and tested independently. The design is in
`docs/design/10_core.md`.

| Area | Directory | Contents |
|---|---|---|
| foundation | `foundation/` | `platform_detect.h`, `types.h`, `api.h`, generated `build_info.h`, `hash.h`, `error.h`/`result.h`, `assert.h`, `log.h`, `uuid.h`, `enum_flags.h`, `non_copyable.h` |
| math | `math/` | glm aliases, `transform`, `aabb`, `frustum`, `ray`, `rect` |
| containers | `containers/` | `slot_map`, `small_vector`, `stable_vector`, `flat_hash_map`, `flat_map` |
| strings | `strings/` | `string_id`, string utilities, `bit_vector`, `radix_sort` |
| memory | `memory/` | `arena`, `pool_allocator`, `frame_allocator`, `allocation_counters`, tracked allocation |
| time | `time/` | `clock`, `manual_clock`, `random`, `event`, `delegate`, `event_bus` |
| jobs | `jobs/` | `job_system`, `parallel_for`, main-thread queue, `dedicated_thread` |
| json | `json/` | yyjson wrapper: `value`, `document`, `writer`, `parse`, canonical `to_string` |
| vfs | `vfs/` | `file_system` interface, native and memory file systems, mounts |
| system | `system/` | `process`, `dynamic_library`, `image` |
| reflect | `reflect/` | `type_id`, `type_info`, `field_info`, `type_registry`, JSON conversion |

## Threading model

- Logging is thread-safe; sinks run outside the logger lock and must synchronise themselves.
- `uuid::generate()` is thread-safe (per-thread generator).
- The assert handler is an atomic function pointer.
- Per-area guarantees are documented in each area's README.

## Testing

`tests/core/<area>/` - `conjure_core_<area>_tests`. Everything is headless.
