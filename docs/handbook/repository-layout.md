# Repository layout

🛠 **Engineering handbook**

This chapter is the map of the repository: what every top-level directory holds, how an engine module
directory is laid out (and how `core` differs), where tests and their helpers live, and which files are
generated or downloaded and therefore never committed.

## Contents

- [Top level](#1-top-level)
- [Engine module anatomy](#2-engine-module-anatomy)
- [Where tests live](#3-where-tests-live)
- [Tools](#4-tools)
- [Documentation](#5-documentation)
- [Generated and ignored files](#6-generated-and-ignored-files)

## 1. Top level

| Path | Purpose |
|---|---|
| `AGENTS.md`, `CLAUDE.md` | the entry point for every contributor; `CLAUDE.md` only points at `AGENTS.md` |
| `README.md` | short project description, build requirements, pointers into `docs/` |
| `CMakeLists.txt` | root build: options, configurations, global compiler settings, `add_subdirectory` order |
| `CMakePresets.json` | configure, build, test and workflow presets per platform and configuration |
| `cmake/` | build helpers: `ConjureConfigs.cmake`, `ConjureModule.cmake` (modules, core areas, executables, `conjure_add_game_module`), `ConjureSharedLibrary.cmake` (the umbrella `conjure_shared`, `abi_identity.h`), `ConjureShaders.cmake` and `ConjureShaderIndex.cmake` (shader compilation and `index.json`), `ConjureTesting.cmake`, `ConjureWarnings.cmake`, `ConjureSanitizers.cmake`, `FindNetHost.cmake` |
| `third_party/` | `CMakeLists.txt` with every pinned dependency and `src/` with the single-file implementation units (`stb_impl.c`, `cgltf_impl.c`, `miniaudio_impl.c`) |
| `engine/` | the engine: `include/conjure/<module>/` public headers, `src/<module>/` implementation, `shaders/` the engine's HLSL sources and their `CMakeLists.txt` (`conjure_add_shaders`), `CMakeLists.txt` with the module list |
| `editor/` | the `conjure_editor` executable: `src/main.cpp` over `conjure::shared` ([Editor module](modules/editor.md)); `resources/` the Windows icon resource and its generated `.ico` |
| `runtime/` | the `conjure_runtime` executable: `src/main.cpp` over `conjure::shared` ([Runtime](modules/runtime.md)) |
| `managed/` | the C# SDK (`Conjure.Managed`), the test scripts and the project templates built with `dotnet` |
| `samples/` | sample projects, first `feature_test` (wave 5; skipped until `feature_test/CMakeLists.txt` exists) |
| `tests/` | the test support library, one test directory per module, the style check registration |
| `tools/` | Python scripts: build driver, style, export, documentation, Distribution-symbol and CI-skip checks, asset download, the native API and icon generators; `tests/` holds the self-tests of the documentation checker, the icon generator and the other tools ([Tools](#4-tools)) |
| `docs/` | requirements, architecture, conventions, style, design record, both documentation sets |
| `.claude/skills/` | agent skills: `build-and-test`, `add-engine-module`, `add-component`, `code-review-checklist`, `write-docs`, `editor-ai-control` (driving the editor over its AI protocol, generated from `editor.schema`; `AGENTS.md` 1 names every `SKILL.md`) |
| `.github/workflows/` | `ci.yml`, the pull-request matrix, and `nightly.yml`, the nightly matrix (slow cases, TSan, Distribution export, golden images, benchmarks; see [Continuous integration](ci.md)) |
| `mkdocs.yml` | the documentation site (navigation, theme, plugins) |
| `.clang-format`, `.editorconfig`, `.gitattributes`, `.gitignore` | editor and git configuration: LF everywhere, binary extensions marked, build outputs ignored |

Engine modules in dependency order, as listed in `engine/CMakeLists.txt` (`CONJURE_ENGINE_MODULES`):
`core`, `ecs`, `platform`, `rhi`, `assets`, `audio`, `modules`, `scene`, `renderer`, `physics`,
`scripting`, `engine`, `bindings`, `editor` (the last declares `conjure_editor_core` and `conjure_editor_ui`
and returns early unless `CONJURE_BUILD_EDITOR`). All of them are in the tree since wave 4; a module directory
that does not exist is still skipped with a status message. `editor/` and `runtime/` hold the two executables'
`main.cpp`, `tests/integration/` the cross-module scenarios, `tests/benchmarks/` the benchmark harness and
scenarios, `tests/support/engine/` the engine-level test fixtures. Modules that talk to
hardware keep each backend's files together: `rhi` under `src/rhi/backends/{d3d12,vulkan,null}/`, `platform`
under `src/platform/glfw/` next to a flat `null_backend.cpp`, `audio` as flat `miniaudio_backend.{h,cpp}` and
`null_backend.cpp`.

## 2. Engine module anatomy

A module `<name>` is the static library `conjure_<name>` (alias `conjure::<name>`) declared with
`conjure_add_module()` in `engine/src/<name>/CMakeLists.txt`:

```
engine/include/conjure/<name>/<type>.h      public headers, one class each, documented with <summary> blocks
engine/src/<name>/CMakeLists.txt            conjure_add_module(<name> PUBLIC_HEADERS ... SOURCES ... PUBLIC_DEPS ... PRIVATE_DEPS ...)
engine/src/<name>/log_category.h            CONJURE_DECLARE_LOG_CATEGORY(LOG_<NAME>);
engine/src/<name>/log_category.cpp          CONJURE_DEFINE_LOG_CATEGORY(LOG_<NAME>, "<name>");
engine/src/<name>/<type>.cpp                implementation; private headers stay here too
engine/src/<name>/<backend>/                a real backend's files (rhi: backends/<api>/, platform: glfw/); the null backend sits beside them
engine/src/<name>/README.md                 purpose, main types, threading model, dependencies, how to test
tests/<name>/CMakeLists.txt                 conjure_add_test_module(<name> SOURCES ...) and conjure_header_check(conjure_<name>)
tests/<name>/test_<type>.cpp                one test file per public header
```

Rules that follow from the layout (`docs/CONVENTIONS.md` 2 and 3, `AGENTS.md` 2):

- Public headers include other Conjure headers with quotes (`#include "conjure/core/log.h"`) and never a
  third-party header; glm is the single exception, reachable only through `conjure/core/math/glm_types.h`.
  Third-party code is a `PRIVATE_DEPS` entry and is included from `src/` only.
- Platform code lives in per-platform translation units chosen by CMake (`process_windows.cpp`,
  `process_posix.cpp`), never behind `#ifdef _WIN32` in shared code; `conjure/core/platform_detect.h` is the
  only header that reads compiler macros.
- Every source file is listed explicitly; there is no globbing anywhere in the build.

### 2.1 How core differs

`core` is an umbrella: `engine/src/core/CMakeLists.txt` creates the `INTERFACE` target `conjure_core`
(alias `conjure::core`) and adds one subdirectory per area from `CONJURE_CORE_AREAS`, each a static library
`conjure_core_<area>` (alias `conjure::core_<area>`) declared with `conjure_add_core_area()`. Every area
depends on `foundation`; the remaining dependencies are listed in `engine/src/core/README.md`.

| Area | `engine/src/core/<area>/` | Public headers | Test target |
|---|---|---|---|
| `foundation` | `assert.cpp`, `error.cpp`, `log.cpp`, `uuid.cpp`, `local_time_{windows,posix}.cpp`, private `local_time.h`, `console_output.h`, `build_info.h.in` | flat: `conjure/core/platform_detect.h`, `types.h`, `api.h`, `hash.h`, `error.h`, `result.h`, `assert.h`, `log.h`, `uuid.h`, `enum_flags.h`, `non_copyable.h`; generated `build_info.h` | `conjure_core_foundation_tests` |
| `math` | `aabb.cpp`, `frustum.cpp`, `ray.cpp`, `transform.cpp`, `projection.cpp`, private `look_rotation.{h,cpp}` | `conjure/core/math.h` plus `conjure/core/math/*.h` | `conjure_core_math_tests` |
| `containers` | `containers.cpp` | `conjure/core/containers/*.h` (header-only templates) | `conjure_core_containers_tests` |
| `strings` | `string_id.cpp`, `string_utils.cpp`, `bit_vector.cpp`, `radix_sort.cpp` | flat: `conjure/core/string_id.h`, `string_utils.h`, `bit_vector.h`, `radix_sort.h` | `conjure_core_strings_tests` |
| `memory` | `arena.cpp`, `pool_allocator.cpp`, `frame_allocator.cpp`, `allocation_counters.cpp`, `tracked_allocation.cpp` | `conjure/core/memory/*.h` | `conjure_core_memory_tests` |
| `time` | `clock.cpp`, `random.cpp`, `event.cpp`, `event_bus.cpp` | flat: `conjure/core/clock.h`, `random.h`, `event.h`, `delegate.h`, `event_bus.h` | `conjure_core_time_tests` |
| `jobs` | `job_system.cpp`, `job_handle.cpp`, `main_thread.cpp`, `dedicated_thread.cpp`, private `thread_name.{h,cpp}` | `conjure/core/jobs/*.h` | `conjure_core_jobs_tests` |
| `json` | `value.cpp`, `document.cpp`, `writer.cpp`, `json.cpp`, private `json_internal.h` | `conjure/core/json/*.h` | `conjure_core_json_tests` |
| `vfs` | `file_system.cpp`, `native_file_system.cpp`, `memory_file_system.cpp`, `mount_table.cpp`, `path_utils.cpp`, `native_platform_{windows,posix}.cpp`, private `native_platform.{h,cpp}`, `vfs_internal.{h,cpp}` | `conjure/core/vfs/*.h` | `conjure_core_vfs_tests` |
| `system` | `process.cpp`, `dynamic_library.cpp`, `image.cpp`, `*_windows.cpp` / `*_posix.cpp`, `win32_utils.{h,cpp}`, private `system_internal.{h,cpp}` | flat: `conjure/core/process.h`, `dynamic_library.h`, `image.h` | `conjure_core_system_tests` |
| `reflect` | `type_info.cpp`, `type_registry.cpp`, `field_info.cpp`, `json_conversion.cpp`, `reflect.cpp`, private `pointer_index.{h,cpp}` | `conjure/core/reflect/*.h` | `conjure_core_reflect_tests` |

Small areas keep their headers flat under `conjure/core/`; large ones use `conjure/core/<area>/` with an
umbrella header (`math.h`, `containers.h`, `memory.h`, `jobs.h`, `json.h`, `vfs.h`, `reflect.h`). Each area
has its own `README.md` with the same four sections as a module README.

## 3. Where tests live

```
tests/CMakeLists.txt                   adds support/, then one directory per module (CONJURE_TEST_MODULES), integration/ and
                                       benchmarks/ when present; registers the Python CTest entries: style.check and
                                       docs.check, the tools' self-tests and brand.icons_up_to_date (section 4)
tests/support/                         conjure_test_support (and conjure_test_support_shared, the same sources against
                                       conjure::shared): include/conjure/testing/*.h, src/*.cpp, src/test_main.cpp,
                                       src/tracked_global_new.cpp; tests/ holds its self-tests (conjure_support_tests)
tests/<module>/                        conjure_<module>_tests: test_<header>.cpp per public header, plus a
                                       <module>_test_support.{h,cpp} where the suite needs a fixture or a precondition probe
tests/core/<area>/                     conjure_core_<area>_tests, mirroring engine/src/core/<area>
tests/modules/fixtures/<name>/         one fixture plugin per directory (module.json and a .cpp), built with
                                       conjure_add_game_module() into build/<preset>/test_modules/<name>/
```

Conventions worth knowing when looking for something:

- Test helpers shared by one executable sit next to the tests as plain headers and are listed in the same
  `conjure_add_test_module()` call: `tests/core/containers/tracked_value.h`, `tests/core/math/math_test.h`,
  `tests/core/jobs/jobs_test_support.{h,cpp}`, `tests/core/vfs/vfs_test_helpers.h`,
  `tests/core/reflect/reflect_test_types.h`, `tests/support/tests/support_child_runner.h`.
- Helper programs the tests spawn are separate targets in the same directory: `tests/core/system/` builds
  `conjure_process_child`, `conjure_test_shared_library` and `conjure_test_dependent_library` (the two
  libraries go to `bin/core_system_libraries/` on purpose); `tests/support/tests/` builds
  `conjure_support_child`. Their paths reach the tests as compile definitions.
- `tools/style_exceptions.json` lists the public headers that have no test file (macro-only headers) and
  the test files allowed to use otherwise forbidden constructs, each with its reason.
- `tests/assets/` is both the `assets` module unit-test directory and the in-repo fixture root of
  `docs/design/08_testing.md` 2.1: `projects/`, `scenes/` and `prefabs/` hold the read-only fixtures
  `conjure::testing::repo_asset()` resolves through `CONJURE_TEST_REPO_ASSET_ROOT` (`tests/assets/README.md`
  lists each).
- `tests/.downloaded/` is created by `tools/download_test_assets.py` and ignored (section 6). Planned by
  `docs/design/08_testing.md` 2.1 and not present yet: `tests/downloaded/`, `tests/golden/`, `tests/sanitizers/`.

## 4. Tools

| Script | Role |
|---|---|
| `tools/build.py` | cross-platform build driver: default preset per host, MSVC environment import, `--build-dir`, `--target`, `--test -R/-L`, `--clean` |
| `tools/check_style.py` | mechanical style rules over `engine/`, `editor/`, `runtime/`, `tests/`, `samples/`; `--tests-mirror-headers`; CTest entry `style.check` |
| `tools/style_exceptions.json` | the two exception tables read by `check_style.py` |
| `tools/check_exports.py` | reports public classes, free functions and `extern` variables without `CONJURE_API` (`docs/design/07_modules.md` 5 and 9); every module by default (or the modules named); CTest `exports.check_annotations`, `exports.check_tool`, `exports.count_shared_library` |
| `tools/check_docs.py` | documentation structure and coverage checks (`docs/design/16_documentation.md` D9); CTest entry `docs.check` |
| `tools/tests/test_check_docs.py` | the checker self-test (generates its fake docs tree in a temporary directory); CTest entry `docs.check_tool` |
| `tools/check_distribution_symbols.py` | proves the Distribution `conjure_runtime` and `conjure_shared` carry no editor or Dear ImGui symbol; CTest `runtime.distribution_symbols` (labels `runtime;process`; registered in a Distribution tree, in a multi-config tree (where it skips with 77 unless ctest runs `-C Distribution`) or with `CONJURE_DISTRIBUTION_BUILD_DIR`) and its self-test `runtime.check_distribution_symbols_tool`; `--self-test` also runs in `style-and-headers` |
| `tools/ctest_check_skips.py` | fails a CI run when a skipped test carries no allowed label or name tag, reading the ctest JUnit output; used in `ci.yml` and `nightly.yml`, with `--self-test` in `style-and-headers` |
| `tools/generate_native_api.py` | generates `native_api_hash.h`, `NativeApi.g.cs` and `docs/generated/native_api_manifest.json` from `native_api.inl`, and `BindingStatus.g.cs` from `bindings_error.inl`; CTest `conjure_scripting_api_generated_up_to_date` runs `--check` |
| `tools/generate_brand_icons.py`, `tools/tests/test_generate_brand_icons.py` | the editor icon ([Logo and application icon](brand.md)); CTest `brand.icons_up_to_date` and `brand.icons_tool` |
| `tools/tests/test_build.py`, `test_check_style.py`, `test_download_test_assets.py`, `test_generate_native_api.py` | the self-tests of `build.py`, `check_style.py`, `download_test_assets.py` and `generate_native_api.py`; CTest `build.driver_tool`, `style.check_tool`, `test_assets.downloader_tool`, `native_api.generator_tool` |
| `tools/docs.py` | `install`, `serve`, `build`, `check` for the documentation site |
| `tools/docs_requirements.txt` | pinned MkDocs toolchain |
| `tools/download_test_assets.py`, `tools/test_assets.json`, `tools/test_assets.lock` | downloads the public fixtures listed in the manifest into `tests/.downloaded/`, verifies every file against the SHA-256 pinned in the committed lock (a file the lock does not know is pinned on first download and the lock is committed with the change; `--verify` and `--locked` fail on an unpinned asset), writes the `.assets_ready` stamp |

## 5. Documentation

| Path | Content |
|---|---|
| `docs/DESIGN_DOC.md` | the customer's requirements |
| `docs/ARCHITECTURE.md` | binding top-level map: modules, dependency graph, frame, waves |
| `docs/CONVENTIONS.md`, `docs/CODE_STYLE.cpp` | engineering conventions and the authoritative code style (a C++ file, excluded from the site) |
| `docs/design/00_brief.md` ... `17_demo_games.md` | the design record, one note per area; linked from the handbook, never from the wiki |
| `docs/index.md` | landing page of the site: pick your document set |
| `docs/api/` | 📘 Public API wiki, one directory per module |
| `docs/handbook/` | 🛠 Engineering handbook (this set); `modules/` one page per engine module, `module-system/` the module system chapter |
| `docs/assets/` | images, SVG diagrams, `extra.css`; `brand/` holds the logo masters and the per-size icon drawings ([Logo and application icon](brand.md)) |

## 6. Generated and ignored files

Nothing in this table is committed; `.gitignore` covers all of it.

| Path | Produced by | Notes |
|---|---|---|
| `build/`, `out/`, `install/`, `export/` | CMake presets, `tools/build.py --build-dir`, future export | every build tree, including private ones |
| `build/<preset>/generated/include/conjure/core/build_info.h` | `configure_file()` in `engine/src/core/CMakeLists.txt` | edit `build_info.h.in` or the `project()` version, never the output |
| `build/<preset>/generated/include/conjure/modules/abi_identity.h` | the `conjure_abi_identity` target (`cmake/ConjureSharedLibrary.cmake`) | header hash, compiler major and runtime facts; rewritten at build time when a public header changes |
| `build/<preset>/shaders/` (`*.dxil`, `*.spv`, `index.json`) | `conjure_add_shaders()` from `engine/shaders/CMakeLists.txt` | read by `rhi::shader_library`; depfiles make a changed `.hlsli` rebuild its permutations |
| `build/<preset>/test_modules/<name>/` | `tests/modules/CMakeLists.txt` | the fixture plugins' `module.json` copies and `bin/<platform>-<arch>-<config>/` binaries |
| `build/<preset>/_deps/` | FetchContent | cached in CI keyed by the hash of `third_party/CMakeLists.txt` |
| `build/<preset>/tests/.../header_check/<target>/*.cpp` | `conjure_header_check()` | one stub per public header, regenerated only when the header list changes |
| `build/<preset>/tests/.../<target>_labels.cmake`, `<target>_tests-*.cmake` | `conjure_add_test_module()`, doctest discovery | the CTest entries and their labels |
| `build/<preset>/test_output/<executable>/`, `test_tmp/<executable>/` | test executables | `temp_directory` lives under `test_tmp`; per executable so parallel CTest processes never share a directory |
| `CMakeUserPresets.json` | you | personal presets, ignored |
| `tests/.downloaded/` and `tests/.assets_ready` | `tools/download_test_assets.py` | ignored by `.gitignore`; the hash lock `tools/test_assets.lock` is committed |
| `**/bin/`, `**/obj/`, `*.nupkg` | `dotnet` | managed build outputs |
| `*.dxil`, `*.spv`, `*.cso` | `dxc`, glslang | compiled shaders, wherever they land |
| `.venv-docs/`, `site/` | `tools/docs.py install` / `build` | documentation toolchain and site output |
| `__pycache__/`, `*.pyc` | Python | |
| `.vs/`, `.vscode/`, `.idea/`, `*.user`, `*.log`, `*.dmp`, `/logs/` | IDEs, the engine | editor state, logs, crash dumps |

Generator outputs that the build needs on every machine are committed instead and checked by CTest: the editor's
icon (`editor/resources/conjure_editor.ico`, `engine/src/editor/ui/editor_icon_data.cpp`) comes from
`tools/generate_brand_icons.py`, and `brand.icons_up_to_date` fails while either differs from the drawings.

`.gitattributes` normalises every text file to LF and marks `.png .ico .jpg .hdr .glb .bin .wav .ogg .mp3 .ttf
.dxil .spv` as binary; `tools/check_style.py` rejects a CRLF source file, so configure your editor rather than git.

## Related

- design record: `docs/design/00_brief.md` (repository layout, waves)
- design record: `docs/design/10_core.md` (core areas and their headers)
- design record: `docs/design/08_testing.md` section 2.1 (directories owned by the test strategy)
- `docs/ARCHITECTURE.md` section 2 (module map and dependency graph)
- `engine/src/<module>/README.md` for every module and the core area READMEs
- [Build pipeline](build-pipeline.md), [Testing](testing.md), the module pages under [`modules/`](modules/core.md)
