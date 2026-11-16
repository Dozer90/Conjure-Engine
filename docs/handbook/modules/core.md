# Core module

🛠 **Engineering handbook** · 📘 API: [Errors and results](../../api/core/errors-and-results.md) and the other [Core pages](../../api/index.md)

`conjure::core` is the foundation every other module links: the error model, logging and asserts, math,
containers, strings, memory, time, jobs, JSON, the virtual file system, OS services and reflection. This chapter
describes the shape of the module, the conventions every area shares, an index of the areas and how the module
is built and tested. The areas themselves (purpose, headers, algorithms, threading guarantees, extension points,
tests) are documented on two sub-pages:

- [Core areas: foundation to memory](core/foundation-to-memory.md): foundation, math, containers, strings, memory
- [Core areas: time to reflect](core/time-to-reflect.md): time, jobs, json, vfs, system, reflect

Public headers live under `engine/include/conjure/core/`, sources under `engine/src/core/<area>/`, tests under
`tests/core/<area>/`; every area has a README at `engine/src/core/<area>/README.md` with the finer behavioural
notes, and the binding design is [`../../design/10_core.md`](../../design/10_core.md).

## Contents
- [1. Shape of the module](#1-shape-of-the-module)
- [2. Conventions shared by every area](#2-conventions-shared-by-every-area)
- [3. Areas at a glance](#3-areas-at-a-glance)
- [4. Building and testing](#4-building-and-testing)
- [Related](#related)

## 1. Shape of the module

Core is one module made of eleven independently buildable **areas**. `engine/src/core/CMakeLists.txt` creates
the umbrella `conjure_core` (an `INTERFACE` target, alias `conjure::core`) that adds `engine/include` and the
generated include directory, then adds each area directory; every area declares a static library
`conjure_core_<area>` (alias `conjure::core_<area>`) with `conjure_add_core_area(<area> PUBLIC_HEADERS ...
SOURCES ... PUBLIC_DEPS ...)` from `cmake/ConjureModule.cmake` and a test executable `conjure_core_<area>_tests`.
Other modules link `conjure::core`; a core area links only the areas it needs (`10_core.md` C2):

| Area | Library | Headers under `conjure/core/` | Public deps beyond `foundation` |
|---|---|---|---|
| foundation | `conjure::core_foundation` | flat | - (`Threads::Threads`) |
| math | `conjure::core_math` | `math.h`, `math/` | glm |
| containers | `conjure::core_containers` | `containers/` | - |
| strings | `conjure::core_strings` | flat | - |
| memory | `conjure::core_memory` | `memory/` | - |
| time | `conjure::core_time` | flat | - |
| jobs | `conjure::core_jobs` | `jobs/` | - |
| json | `conjure::core_json` | `json/` | yyjson (private) |
| vfs | `conjure::core_vfs` | `vfs/` | - |
| system | `conjure::core_system` | flat | jobs (private), stb (private) |
| reflect | `conjure::core_reflect` | `reflect/` | json, strings, math, containers, time |

Each area library is registered with `conjure_register_static_library()`, so the whole of core is
whole-archived into the umbrella shared library `conjure_shared` by `conjure_finalize_engine()`
([Module system](../module-system/index.md) 2, [Modules module](modules.md) 7).

## 2. Conventions shared by every area

| Convention | Rule | Record |
|---|---|---|
| Errors | fallible calls return `result<T>` / `status` with a generic `core::error_code`; core declares no category of its own; programmer errors assert | C3 |
| Allocation | `new`/`delete`/`malloc` are confined to `memory` | C4 |
| Threads | `std::thread` / `std::jthread` are confined to `jobs`; every other thread is a `jobs::dedicated_thread`, the `system::process` pipe readers included (`system` links `jobs` for them) | C2, C4, C14 |
| Platform code | per-platform translation units (`*_windows.cpp`, `*_posix.cpp`) selected by CMake; only `conjure/core/platform_detect.h` reads compiler macros | CONVENTIONS 3 |
| Third-party headers | never in public headers; glm is the single exception, through `conjure/core/math/glm_types.h` | C6 |
| Determinism | everything is headless and deterministic given the same inputs | C7 |

Two names need qualifying after a `using namespace`: write `core::clock` (ambiguous with the C library's
`::clock()`) and `reflect::type_info` (ambiguous with `::type_info` from `<typeinfo>` on MSVC).

## 3. Areas at a glance

| Area | Purpose | Namespace | Details |
|---|---|---|---|
| foundation | platform detection, fundamental types, hashing, errors and results, asserts, logging, uuid, enum flags | `conjure`, `conjure::core` | [foundation](core/foundation-to-memory.md#1-foundation) |
| math | glm behind one header, geometry value types (`transform`, `aabb`, `plane`, `frustum`, `ray`, `rect`) | `conjure::math` | [math](core/foundation-to-memory.md#2-math) |
| containers | `slot_map`, `small_vector`, `stable_vector`, `flat_hash_map`, `flat_map` | `conjure::core` | [containers](core/foundation-to-memory.md#3-containers) |
| strings | `string_id`, ASCII and UTF helpers, `bit_vector`, radix sort | `conjure::core`, `conjure::core::str` | [strings](core/foundation-to-memory.md#4-strings) |
| memory | counted allocation, `arena`, `pool_allocator`, `frame_allocator` | `conjure::core::memory` | [memory](core/foundation-to-memory.md#5-memory) |
| time | clocks, randomness, `event`, `delegate`, `event_bus` | `conjure::core`, `conjure::core::random` | [time](core/time-to-reflect.md#1-time) |
| jobs | `job_system`, `parallel_for`, the main-thread queue, `dedicated_thread` | `conjure::core::jobs` | [jobs](core/time-to-reflect.md#2-jobs) |
| json | `value`, `document`, `writer`; canonical JSON | `conjure::core::json` | [json](core/time-to-reflect.md#3-json) |
| vfs | `file_system` backends, `mount_table`, path helpers | `conjure::core::vfs` | [vfs](core/time-to-reflect.md#4-vfs) |
| system | `process`, `dynamic_library`, `image` | `conjure::core` | [system](core/time-to-reflect.md#5-system) |
| reflect | `type_info`, `type_registry`, `CONJURE_REFLECT_*`, JSON conversion | `conjure::core::reflect` | [reflect](core/time-to-reflect.md#6-reflect) |

Each area section on the sub-pages has the same subheadings: *Headers* (a table of every public header),
*Algorithms*, *Threading*, *Extension points* where the area has any, and *Tests*.

## 4. Building and testing

```
python tools/build.py --build-dir build/core --test -R core_                               # every core area
python tools/build.py --build-dir build/core_jobs --target conjure_core_jobs_tests --test -R core_jobs
```

`tests/core/<area>/test_<header>.cpp` mirrors every public header (`tools/check_style.py --tests-mirror-headers`,
CTest entry `style.check`); `conjure_core_<area>_tests` is one CTest entry per `TEST_CASE`. Every public header
also compiles alone through `conjure_header_check` (part of the default build). Everything is headless and
deterministic: `manual_clock`, seeded `random`, 0-worker job systems, `memory_file_system` or a
`conjure::testing::temp_directory`. The wave-1 exit gates are listed in `docs/ARCHITECTURE.md` 11.

> [!TIP]
> Build and test one area at a time while working in it (`--target conjure_core_<area>_tests --test -R
> core_<area>`); the whole module is only needed before the review checklist.

## Related
- area pages: [Core areas: foundation to memory](core/foundation-to-memory.md),
  [Core areas: time to reflect](core/time-to-reflect.md)
- design records: [`../../design/10_core.md`](../../design/10_core.md) (the binding contract),
  [`../../design/09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md) R30, R58, R61, R63,
  [`../../design/08_testing.md`](../../design/08_testing.md) 2-3 (test support, `conjure_add_test_module`)
- area READMEs: `engine/src/core/README.md` and `engine/src/core/<area>/README.md`
- handbook: [Architecture](../architecture.md), [Coding standards](../coding-standards.md),
  [Data-oriented design](../data-oriented-design.md), [Module system](../module-system/index.md)
