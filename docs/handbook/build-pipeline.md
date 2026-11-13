# Build pipeline

🛠 **Engineering handbook**

This chapter explains the CMake build from the root `CMakeLists.txt` down: how the three configurations
are created, what the `conjure_add_*` helpers do to a target, how the umbrella shared library
`conjure_shared` and game modules are built, how shaders are compiled and indexed, how warnings and sanitizers
are applied, how third-party libraries are pinned and kept out of our warning set, where the generated headers
come from, and which build steps the design record still has in the plan.

## Contents

- [Configure flow](#1-configure-flow)
- [Configurations](#2-configurations)
- [Options and consistency rules](#3-options-and-consistency-rules)
- [Module helpers](#4-module-helpers)
- [Warnings as errors](#5-warnings-as-errors)
- [Sanitizers](#6-sanitizers)
- [Testing helpers](#7-testing-helpers)
- [Third-party dependencies](#8-third-party-dependencies)
- [Generated build info](#9-generated-build-info)
- [Planned steps](#10-planned-steps)

## 1. Configure flow

The root `CMakeLists.txt` (CMake 3.28+) runs in this order:

1. `include(cmake/ConjureConfigs.cmake)` **before** `project()`, so `Debug;Development;Distribution` exist
   for every target including fetched ones.
2. `project(Conjure VERSION 0.1.0 LANGUAGES C CXX)`, then `conjure_derive_config_flags()` copies the
   `RelWithDebInfo` / `Release` compiler and linker flags into the custom configurations.
3. In-source builds are rejected; `cmake/` is added to `CMAKE_MODULE_PATH`.
4. Options (section 3) and their consistency checks.
5. Global settings: C++20 without extensions, C11, position-independent code, `CMAKE_EXPORT_COMPILE_COMMANDS`,
   `USE_FOLDERS`, outputs in `${PROJECT_BINARY_DIR}/bin` (executables and shared libraries) and `lib`
   (static libraries). MSVC additionally gets `/MP /utf-8 /Zc:__cplusplus /Zc:preprocessor /EHsc /bigobj`.
6. `include(ConjureWarnings)`, `include(ConjureModule)`, `include(ConjureSharedLibrary)`, `include(ConjureTesting)`,
   `include(ConjureSanitizers)`.
7. `add_subdirectory(third_party)`, then `engine`, then `conjure_finalize_engine()` (section 4.4: every engine
   library is declared by now), then `managed` (when `CONJURE_BUILD_MANAGED AND CONJURE_WITH_SCRIPTING`),
   `editor`, `runtime`, `tests` (after `enable_testing()`), `samples`, each behind its option.
8. `conjure_print_configuration_summary()` prints the resulting matrix.

`engine/CMakeLists.txt` walks `CONJURE_ENGINE_MODULES` in dependency order and adds every module directory
that has a `CMakeLists.txt`; `engine/src/core/CMakeLists.txt` does the same for `CONJURE_CORE_AREAS`.
Missing directories print a status line and are skipped, so partial trees configure. When
`CONJURE_WITH_RENDERER` is on it also includes `ConjureShaders` and adds `engine/shaders/` (section 4.6).

## 2. Configurations

`cmake/ConjureConfigs.cmake`:

- Multi-config generators get `CMAKE_CONFIGURATION_TYPES "Debug;Development"`, or `"Debug;Development;Distribution"`
  when the cache holds `CONJURE_BUILD_EDITOR=OFF`: the editor is never part of a Distribution build and a
  multi-config tree builds every configuration from one set of targets, so the configuration exists there only
  without the editor. Single-config generators default `CMAKE_BUILD_TYPE` to `Development` and restrict its
  choices to the three.
- `conjure_derive_config_flags()` sets `CMAKE_<LANG>_FLAGS_DEVELOPMENT` from `RELWITHDEBINFO` and
  `CMAKE_<LANG>_FLAGS_DISTRIBUTION` from `RELEASE` (same for `EXE/SHARED/MODULE/STATIC` linker flags), and
  maps imported targets that only know the standard configurations with
  `CMAKE_MAP_IMPORTED_CONFIG_DEVELOPMENT "RelWithDebInfo;Release;"` and `..._DISTRIBUTION "Release;RelWithDebInfo;"`.
- Before `project()` it sets `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT` to `$<$<CONFIG:Debug,Development>:ProgramDatabase>`
  unless one is given (CMP0141 takes `/Zi` out of the flag defaults, and CMake's own default names only Debug and
  RelWithDebInfo), so MSVC Development objects carry debug info and Distribution produces no PDB.
- `conjure::platform_defines` (`cmake/ConjureWarnings.cmake`) adds `CONJURE_DEBUG=1`, `CONJURE_DEVELOPMENT=1`
  or `CONJURE_DISTRIBUTION=1` through `$<CONFIG:...>` generator expressions, plus `CONJURE_WITH_*` and
  `CONJURE_RHI_*` for the enabled subsystems. On Windows it also defines `NOMINMAX`, `WIN32_LEAN_AND_MEAN`,
  `UNICODE`, `_UNICODE`, `_CRT_SECURE_NO_WARNINGS`.
- A single-config `Distribution` build forces `CONJURE_BUILD_EDITOR` off; a multi-config tree with the editor on
  has no `Distribution` configuration at all (above), so the `*-dist` presets are the way to build what ships.
  `conjure_add_executable(... WIN32_GUI)` hides the console window only in Distribution
  (`WIN32_EXECUTABLE $<CONFIG:Distribution>` and `/ENTRY:mainCRTStartup` on MSVC) so developers keep stdout.
- `CONJURE_WITH_SCRIPTING` needs the .NET host pack (`cmake/FindNetHost.cmake`). When the root file does not
  find it, scripting and `CONJURE_BUILD_MANAGED` are turned off for that tree with a CMake warning instead of
  failing the configure; `-DCONJURE_WITH_SCRIPTING=OFF` makes the choice explicit.

## 3. Options and consistency rules

The option table is in [Getting started](getting-started.md#7-build-options). The root file enforces:

| Rule | Error |
|---|---|
| `CONJURE_BUILD_EDITOR` requires `CONJURE_PLATFORM_GLFW` | the editor renders through Dear ImGui's GLFW backend |
| `CONJURE_WITH_RENDERER` requires `CONJURE_RHI_D3D12` or `CONJURE_RHI_VULKAN` | a renderer with no backend is a configure error |
| `CONJURE_RHI_D3D12` is `OFF` (cache `INTERNAL`) off Windows | D3D12 is Windows only |
| `CONJURE_ENABLE_TSAN` excludes `CONJURE_ENABLE_ASAN` and MSVC | `cmake/ConjureSanitizers.cmake` |
| `CONJURE_WITH_SCRIPTING` needs the .NET host pack | `cmake/FindNetHost.cmake` fails with the install hint or `-DCONJURE_WITH_SCRIPTING=OFF` |

## 4. Module helpers

`cmake/ConjureModule.cmake` declares the three target kinds. Libraries link `conjure::platform_defines`
publicly (so the `CONJURE_*` macros reach consumers) and `conjure::warnings` / `conjure::sanitizers`
privately; executables (`conjure_add_executable`, and `conjure_add_test_executable` in
`cmake/ConjureTesting.cmake`) link all three privately.

### 4.1 `conjure_add_module`

```cmake
conjure_add_module(<name>
    SOURCES         <files...>          # required; private sources and headers under engine/src/<name>
    PUBLIC_HEADERS  <files...>          # absolute paths under engine/include/conjure/<name>
    PUBLIC_DEPS     <targets...>        # dependencies that leak into the public headers
    PRIVATE_DEPS    <targets...>        # dependencies used only in .cpp files (third-party goes here)
    PUBLIC_DEFINES  <defs...>
    PRIVATE_DEFINES <defs...>
)
```

Creates the static library `conjure_<name>` with alias `conjure::<name>`; public include directories
`engine/include` and `${PROJECT_BINARY_DIR}/generated/include`, private `engine/src`; the private define
`CONJURE_MODULE_NAME="<name>"`; `FOLDER "Engine"`; the target property `CONJURE_PUBLIC_HEADERS` (read by
`conjure_header_check()`); registration in the global property `CONJURE_STATIC_LIBRARIES` through
`conjure_register_static_library()` (whole-archived into `conjure_shared` by `conjure_finalize_engine()`,
section 4.4); and a `source_group(TREE engine)` for IDEs. Unknown arguments and a missing `SOURCES` are fatal.

### 4.2 `conjure_add_core_area`

Same arguments and behaviour as `conjure_add_module`, but the target is `conjure_core_<area>` with alias
`conjure::core_<area>`, `FOLDER "Engine/core"` and `CONJURE_MODULE_NAME="core"`. Areas list the other areas
they use in `PUBLIC_DEPS` (`conjure::core_foundation` at least); nothing is implied. The real declaration of
the math area:

```cmake
conjure_add_core_area(math
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/math.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/math/aabb.h
        # ... one line per public header
    SOURCES
        aabb.cpp
        frustum.cpp
        look_rotation.cpp
        look_rotation.h
        projection.cpp
        ray.cpp
        transform.cpp
    PUBLIC_DEPS
        conjure::core_foundation
        conjure::thirdparty::glm
)
```

`engine/src/core/CMakeLists.txt` then links every area into the `INTERFACE` umbrella `conjure::core`, which
is what other modules depend on. `tests/core/CMakeLists.txt` adds one test directory and one
`conjure_header_check(conjure_core_<area>)` per area.

### 4.3 `conjure_add_executable`

```cmake
conjure_add_executable(<name> [WIN32_GUI] SOURCES <files...> DEPS <targets...> DEFINES <defs...> FOLDER <folder>)
```

An `add_executable` with the shared interface targets, `FOLDER` defaulting to `Executables`, and the
Distribution-only GUI subsystem described in section 2. `tests/core/system/CMakeLists.txt` uses it for the
`conjure_process_child` helper.

> [!WARNING]
> `PUBLIC_DEPS` becomes part of every consumer's include path and link line. A third-party target listed
> there (other than `conjure::thirdparty::glm`) would let its headers reach public headers, which
> `tools/check_style.py` rejects (`third-party-include`: a public header includes Conjure and C++ standard
> library headers only, glm through `conjure/core/math/glm_types.h`). Keep yyjson, stb, Jolt, NVRHI, GLFW,
> miniaudio, ImGui and nethost in `PRIVATE_DEPS`.

### 4.4 The umbrella shared library and game modules

`cmake/ConjureSharedLibrary.cmake` provides `conjure_finalize_engine()`, called once by the root file right
after `add_subdirectory(engine)` (idempotent). It creates two targets:

| Target | What it is |
|---|---|
| `conjure_abi_identity` | custom target writing `${PROJECT_BINARY_DIR}/generated/include/conjure/modules/abi_identity.h`: `CONJURE_ABI_HEADER_HASH` (a hash over every public header of every engine module, recomputed at build time), `CONJURE_ABI_COMPILER_MAJOR` and the runtime facts `CONJURE_ABI_RUNTIME_DYNAMIC_CRT` / `_ASAN` / `_UBSAN`; `conjure_modules` and `conjure_shared` depend on it |
| `conjure_shared` (alias `conjure::shared`) | `SHARED` library whole-archiving every library in `CONJURE_STATIC_LIBRARIES` (every engine module of waves 1-4; `editor_core` and `editor_ui` outside Distribution), so one copy of every engine symbol and global exists in the process; consumers get `CONJURE_SHARED_IMPORTS`, the engine include directories, every engine library's interface definitions, `conjure::platform_defines` and glm |

Every static library is compiled with `CONJURE_SHARED_EXPORTS` and hidden visibility (`conjure_apply_export_settings()`
in `cmake/ConjureModule.cmake`), so `conjure_shared` exports exactly the declarations that carry `CONJURE_API`
(`conjure/core/api.h`) on every platform and nothing is generated at build time. `tools/check_exports.py` keeps the
annotations complete and reads the export count back from the built library; both run as the CTest entries
`exports.check_annotations` and `exports.count_shared_library` (limit 52,428 of the 65,535 a Windows DLL may hold),
with `exports.check_tool` as the checker's self-test. [Modules module](modules/modules.md) 7 has the export story in
detail; `tools/check_distribution_symbols.py` (CTest `runtime.distribution_symbols`, label `process`) proves that the
Distribution `conjure_runtime` and `conjure_shared` carry no editor or ImGui symbol.

`conjure_add_game_module()` (`cmake/ConjureModule.cmake`) builds a plugin or game module:

```cmake
conjure_add_game_module(<target>
    MANIFEST    <module.json>           # required; the "name" (and optional "binary") are read from it
    SOURCES     <files...>              # required
    [NAME <module name>] [OUTPUT_ROOT <dir>] [DEPS <targets...>] [DEFINES <defs...>] [EDITOR_UI]
)
```

It creates a `SHARED` library linking `conjure::shared` privately (never the static libraries), with hidden
visibility, `CONJURE_MODULE_NAME="<name>"`, `FOLDER "GameModules"`, `$ORIGIN` / `@loader_path` rpaths, and
every output under `<OUTPUT_ROOT or manifest directory>/bin/<platform>-<arch>-<config>` (the binary triplet of
`07_modules.md`). `tests/modules/CMakeLists.txt` uses it for the fixture plugins; the handbook chapter
[Creating a module](module-system/creating-a-module.md) 3 shows the call from a module author's side.

### 4.5 `LINK_ENGINE_SHARED`

`conjure_add_test_module()` and `conjure_add_test_executable()` accept `LINK_ENGINE_SHARED`, which links
`conjure::shared` and `conjure::test_support_shared` instead of the static libraries so that a test host and
the plugins it loads share one engine copy. `tests/modules/` is the suite that needs it; the consequences for
`memory_guard` are in [Testing](testing.md#2-registering-tests-with-cmake).

### 4.6 Shaders

`cmake/ConjureShaders.cmake` provides `conjure_add_shaders(TARGET <name> OUTPUT_DIR <dir> SHADERS <entries...>
[INCLUDE_DIRS ...] [DEPENDENCIES ...])`, where each entry is `source;entry;stage;defines`. It compiles every
permutation to DXIL with `dxc` (`CONJURE_RHI_D3D12`) and to SPIR-V with glslang or `dxc -spirv`
(`CONJURE_RHI_VULKAN`, chosen by `CONJURE_SPIRV_COMPILER`), and `cmake/ConjureShaderIndex.cmake` writes
`<OUTPUT_DIR>/index.json` at build time for `rhi::shader_library`. `engine/shaders/CMakeLists.txt` declares the
engine's own list as the target `conjure_shaders` into `${PROJECT_BINARY_DIR}/shaders`; the list holds the RHI
fullscreen-triangle pair and every renderer pass shader (`engine/src/renderer/assets/shader_asset.cpp` lists the
same permutations), and `conjure_renderer` and the test executables that render (rhi, renderer, engine, editor,
integration, benchmarks) depend on it. Compiler lookup, depfiles and the index
format are in [RHI module](modules/rhi.md#5-shader-pipeline); the user-facing side is 📘 API:
[Shaders](../api/rhi/shaders.md).

## 5. Warnings as errors

`cmake/ConjureWarnings.cmake` defines `conjure::warnings`, applied privately to every Conjure target and
never to third-party code:

| Compiler | Flags |
|---|---|
| MSVC | `/W4 /permissive- /external:W0`, the opt-in `/w14xxx` set (`4242 4254 4263 4265 4287 4296 4311 4545 4546 4547 4549 4555 4619 4640 4826 4905 4906 4928`), `/we4289`, `/wd4201` (nameless struct/union used by the math types), `/wd4251 /wd4275` (dll-interface warnings for standard-library members of exported classes; `conjure_shared` and its consumers are built with one toolchain, `07_modules.md` 5); `/WX` when `CONJURE_WARNINGS_AS_ERRORS` |
| GCC and Clang | `-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough`; GCC adds `-Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op`; `-Werror` when `CONJURE_WARNINGS_AS_ERRORS` |

Third-party headers stay silent because every `FetchContent_Declare` carries `SYSTEM` (CMake marks their
include directories as system directories; on MSVC that is `/external:I`, silenced by `/external:W0`) and
the wrapper targets use `target_include_directories(... SYSTEM PUBLIC ...)`. Test programs alone receive
`-Wno-c2y-extensions` on Clang 21+ (`CONJURE_TEST_WARNING_OPTIONS` in `cmake/ConjureTesting.cmake`) because
doctest's `TEST_CASE` expands `__COUNTER__`; engine targets never see it.

> [!IMPORTANT]
> Do not disable a warning to make a build pass (`AGENTS.md` rule 6). Fix the code; if a third-party header
> is at fault, route it through a `SYSTEM` include as above. The three `/wd` entries above are design decisions
> recorded with their reasons, not build fixes.

## 6. Sanitizers

`cmake/ConjureSanitizers.cmake` defines `conjure::sanitizers`, linked privately by every target:

| Option | MSVC | GCC / Clang | Preset |
|---|---|---|---|
| `CONJURE_ENABLE_ASAN` | `/fsanitize=address` | `-fsanitize=address -fno-omit-frame-pointer` (compile and link) | `windows-msvc-asan`, `linux-clang-sanitize` |
| `CONJURE_ENABLE_UBSAN` | ignored | `-fsanitize=undefined -fno-omit-frame-pointer` | `linux-clang-sanitize` |
| `CONJURE_ENABLE_TSAN` | fatal error | `-fsanitize=thread -fno-omit-frame-pointer`; exclusive with ASan | `linux-clang-tsan` (nightly) |

Runtime options for the CI sanitizer leg are in [Continuous integration](ci.md#4-sanitizer-options).

## 7. Testing helpers

`cmake/ConjureTesting.cmake` provides `conjure_add_test_module()`, `conjure_add_test_executable()` and
`conjure_header_check()`, plus the option `CONJURE_TEST_GOLDEN_IMAGES` (OFF). The [Testing](testing.md)
chapter documents them in full; the build-relevant facts are:

- A test module compiles its `SOURCES` together with `tests/support/src/test_main.cpp` and
  `tests/support/src/tracked_global_new.cpp` (the global `operator new`/`delete` replacement, test
  executables only, never engine code), links `conjure::<module>` when that target exists, and registers one
  CTest entry per `TEST_CASE` through doctest's `doctest_discover_tests()`.
- `conjure_header_check(<target>)` writes one `.cpp` per entry of `CONJURE_PUBLIC_HEADERS` containing just
  `#include "<path relative to engine/include>"`, compiles them as the object library `<target>_header_check`
  with the target's usage requirements and the project warning set, and hangs that off the aggregate target
  `conjure_header_check`, which is part of `ALL`. A header that does not compile alone therefore breaks the
  default build.
- `tests/CMakeLists.txt` registers `tools/check_style.py --tests-mirror-headers` plus its self-test as the
  CTest entries `style.check` and `style.check_tool` (label `style`), `tools/check_docs.py` plus its self-test
  as `docs.check` and `docs.check_tool` (label `docs`), and the self-tests of `tools/build.py`,
  `tools/prune_builds.py`, `tools/download_test_assets.py` and `tools/generate_native_api.py` as
  `build.driver_tool`, `build.prune_tool`, `test_assets.downloader_tool` and `native_api.generator_tool` (label
  `tools`), each with a 120 s timeout, when a Python 3 interpreter is found. The self-tests live in `tools/tests/`
  and never touch the network.
- Independently of Python, it registers `build.abi_identity_non_ascii_path` (label `build`, 60 s timeout):
  `cmake/ConjureSharedLibrary.cmake` in script mode over a header list that names a path with a non-ASCII
  directory, which fails if the list is read back truncated.
- Also independently of Python, it registers `build.managed_project_references` (label `build`, 60 s timeout):
  the `CHECK_CLEAN` script mode of `cmake/ConjureManaged.cmake` over a probe `App.csproj` whose `<ProjectReference>`
  names `..\Lib\Lib.csproj` with backslashes, next to a `Lib/` that holds an `obj/`; it passes only when the
  check reports `Lib/obj`, so it fails when the reference parser returns the path unresolved or resolves it
  against anything but the project's directory.

## 8. Third-party dependencies

Every dependency is declared exactly once in `third_party/CMakeLists.txt` with `FetchContent`, pinned to a
tag or commit, declared `SYSTEM` and (where upstream CMake is used) `EXCLUDE_FROM_ALL`, and exposed as a
`conjure::thirdparty::<name>` target. Engine code never names an upstream target, so a dependency can be
swapped or vendored in one place. `BUILD_SHARED_LIBS` is forced `OFF` for the duration and restored;
`CMAKE_POLICY_VERSION_MINIMUM 3.5` keeps old upstream `cmake_minimum_required` lines configurable under
CMake 4; `FETCHCONTENT_UPDATES_DISCONNECTED ON` stops re-fetching on every configure.

| Target | Upstream | Pin | Built when | Notes |
|---|---|---|---|---|
| `conjure::thirdparty::stb` | nothings/stb | commit `d14d46f6` | always | `src/stb_impl.c`; private to `core_system` (and linked by `conjure_core_system_tests` to decode fixtures) |
| `conjure::thirdparty::cgltf` | jkuhlmann/cgltf | `v1.14` | always | `src/cgltf_impl.c` |
| `conjure::thirdparty::miniaudio` | mackron/miniaudio | `0.11.22` | `CONJURE_WITH_AUDIO` | `src/miniaudio_impl.c`; links `dl pthread m` on Linux |
| `conjure::thirdparty::glm` | g-truc/glm | `1.0.1` | always | interface target with `GLM_FORCE_RADIANS`, `GLM_FORCE_DEPTH_ZERO_TO_ONE`, `GLM_ENABLE_EXPERIMENTAL`, `GLM_FORCE_SILENT_WARNINGS`, `GLM_FORCE_EXPLICIT_CTOR`; the only third-party header allowed in a public header, via `conjure/core/math/glm_types.h` |
| `conjure::thirdparty::yyjson` | ibireme/yyjson | `0.10.0` | always | private to `core_json` |
| `conjure::thirdparty::doctest` | doctest/doctest | `v2.4.11` | `CONJURE_BUILD_TESTS OR CONJURE_BUILD_BENCHMARKS` | also includes `scripts/cmake/doctest.cmake` and caches `_DOCTEST_DISCOVER_TESTS_SCRIPT` for the `tests/` directories |
| `conjure::thirdparty::glfw` | glfw/glfw | `3.4` | `CONJURE_PLATFORM_GLFW` | static; Wayland and X11 both on Linux |
| `conjure::thirdparty::nvrhi` | NVIDIA-RTX/NVRHI | commit `bf0bbd9b` | `CONJURE_WITH_RENDERER` | D3D12 (`d3d12 dxgi dxguid`) and/or Vulkan per `CONJURE_RHI_*`; validation layer on |
| glslang (`CONJURE_GLSLANG_EXECUTABLE`) | KhronosGroup/glslang | `15.1.0` | `CONJURE_WITH_RENDERER AND CONJURE_RHI_VULKAN` | HLSL front-end for SPIR-V; the default `CONJURE_SPIRV_COMPILER` of `conjure_add_shaders()` (section 4.6, [RHI module](modules/rhi.md#5-shader-pipeline)) |
| `conjure::thirdparty::jolt` | jrouwe/JoltPhysics | `v5.3.0` | `CONJURE_WITH_PHYSICS` | `SOURCE_SUBDIR Build`; object stream and debug renderer on, profiler off |
| `conjure::thirdparty::imgui` | ocornut/imgui + CedricGuillemet/ImGuizmo | `v1.91.9b-docking`, commit `105e4fde` | `CONJURE_BUILD_EDITOR` | one static library from the listed `.cpp` files, GLFW backend, `IMGUI_DEFINE_MATH_OPERATORS`, obsolete functions disabled |
| `conjure::thirdparty::nethost` | .NET SDK host pack | found, not fetched | `CONJURE_WITH_SCRIPTING` | `cmake/FindNetHost.cmake` picks the newest pack of the lowest major >= 8 and defines `NETHOST_USE_AS_STATIC` |

Adding a dependency (`AGENTS.md` rule 9): declare it here with a pinned `GIT_TAG`, `SYSTEM` and
`EXCLUDE_FROM_ALL`, turn off its tests, docs and install, create the `conjure::thirdparty::<name>` alias,
give it `FOLDER "ThirdParty"`, list it in `PRIVATE_DEPS` of the consuming module, and extend
`THIRD_PARTY_INCLUDE_ROOTS` in `tools/check_style.py` so the include-order rule requires angle brackets for the
new headers (and `THIRD_PARTY_FLAT_HEADER_PREFIXES` for headers included without a directory, so they are
grouped as third-party rather than standard library). The public-header rule needs no entry: it accepts only
Conjure and C++ standard library headers and rejects every other include on its own.

> [!NOTE]
> The design records (`docs/design/00_brief.md`, `02_renderer.md`, `11_platform.md`, `14_physics.md`) name the
> file `cmake/Dependencies.cmake` (and `00_brief.md` the warning file `CompilerWarnings.cmake`); the
> implementation put the declarations in `third_party/CMakeLists.txt` and the warnings in
> `cmake/ConjureWarnings.cmake`. The rules are identical, and `.github/workflows/ci.yml` keys its dependency cache
> on the hash of `third_party/CMakeLists.txt`.

## 9. Generated build info

`engine/src/core/CMakeLists.txt` runs `configure_file(... @ONLY)` on
`engine/src/core/foundation/build_info.h.in`, writing `${PROJECT_BINARY_DIR}/generated/include/conjure/core/build_info.h`.
The directory is a public include directory of every module and of the `conjure::core` umbrella, so the
header is included as `conjure/core/build_info.h`. It defines the macros `CONJURE_VERSION_MAJOR`,
`CONJURE_VERSION_MINOR`, `CONJURE_VERSION_PATCH`, `CONJURE_VERSION_STRING`, `CONJURE_ENGINE_NAME` and the
constants in `namespace conjure::core::build_info`:

```cpp
#include "conjure/core/build_info.h"

#include <format>
#include <string>

std::string engine_banner() {
    using namespace conjure::core;
    // VERSION_NUMBER orders versions: major * 1,000,000 + minor * 1,000 + patch.
    return std::format("{} {} ({})", build_info::ENGINE_NAME, build_info::VERSION_STRING, build_info::VERSION_NUMBER);
}
```

The version comes from `project(Conjure VERSION 0.1.0)` in the root `CMakeLists.txt`; change it there or in
the template, never in the generated file.

The second generated header is `conjure/modules/abi_identity.h`, written into the same
`generated/include` tree by the `conjure_abi_identity` target (section 4.4). Unlike `build_info.h` it is
produced at build time, not configure time, because its header hash must follow every public header edit.

## 10. Planned steps

The design record describes build steps that later waves add. The shader pipeline and the umbrella shared
library landed with wave 2 (sections 4.4 to 4.6); the rows below are still to come and the links are the
specification to implement against.

> [!NOTE]
> Planned, not implemented. Where this page and a design note disagree once the step lands, the note and
> the code win; update this section in the same pull request.

| Step | Helper | Design record |
|---|---|---|
| Golden-image tests (`CONJURE_TEST_GOLDEN_IMAGES`, nightly software rasterisers); `golden_image::render_offscreen` waits for a headless view target in the engine | `conjure_add_golden_tests()` | [08_testing.md section 3.8](../design/08_testing.md#38-cmake-integration-cmakeconjuretestingcmake) |

Landed since the first version of this page: the managed build (`conjure_add_managed_project()`,
`managed/CMakeLists.txt`, wave 3), the export pipeline (`engine::export_pipeline`, a library call the editor's
`--export` and `project.export` use, wave 4), the benchmark executable (`tests/benchmarks`, one CTest entry per
scenario under the label `benchmark`, wave 4) and the `CONJURE_API`-only export switch (section 4.4, wave 4).
`conjure_add_test_module()`'s `ENGINE` option links `conjure::test_support_engine` (or
`conjure::test_support_engine_shared` with `LINK_ENGINE_SHARED`), the engine-level fixtures of
[Testing](testing.md#4-the-support-library).

## Related

- design record: `docs/design/00_brief.md` (repository layout and fixed decisions)
- design record: `docs/design/07_modules.md` section 5 (shared library, `CONJURE_API`, game modules)
- design record: `docs/design/08_testing.md` sections 3.8 and 7.3 (test CMake integration, CI)
- design record: `docs/design/02_renderer.md` 3.12 (shader build), `docs/design/03_scripting.md` 3.10, `docs/design/06_engine_loop.md` 3.6 (planned steps)
- `docs/ARCHITECTURE.md` section 9 (build and configuration matrix)
- [Getting started](getting-started.md), [Repository layout](repository-layout.md), [Testing](testing.md),
  [Modules module](modules/modules.md) 7 (umbrella library, exports), [RHI module](modules/rhi.md) 5 (shader pipeline),
  [Creating a module](module-system/creating-a-module.md)
