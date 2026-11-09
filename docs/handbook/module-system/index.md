# Module system

🛠 **Engineering handbook**

A Conjure module is a unit of engine functionality with its own public headers, library, tests and README. Two
kinds exist. **Engine modules** are the static libraries `conjure_<name>` that the umbrella shared library
`conjure_shared` whole-archives. **Plugin and game modules** are shared libraries (or manifest-only
directories) that `conjure_modules` discovers through `module.json`, proves ABI-compatible, loads in dependency
order and hands to consumers as plain data; they are built with `conjure_add_game_module()`. This chapter
describes both, following the binding design [`../../design/07_modules.md`](../../design/07_modules.md) and the
headers under `conjure/modules/`; where this page and a header differ, the header wins.

## Contents
- [1. Engine modules](#1-engine-modules)
- [2. The plugin system](#2-the-plugin-system)
- [3. Create a module](#3-create-a-module)
- [4. Import a module](#4-import-a-module)
- [5. Distribute a module](#5-distribute-a-module)
- [Related](#related)

## 1. Engine modules

| Piece | Where | Rule |
|---|---|---|
| Public headers | `engine/include/conjure/<module>/` | compile on their own (`conjure_header_check`), `/// <summary>` on public methods, no third-party includes |
| Sources | `engine/src/<module>/` | one `CMakeLists.txt` listing every file explicitly through `conjure_add_module(<name> SOURCES ... PUBLIC_HEADERS ... PUBLIC_DEPS ... PRIVATE_DEPS ...)` |
| Library | `conjure_<module>` (alias `conjure::<module>`), static | created by `conjure_add_module()` in `cmake/ConjureModule.cmake`; gets `conjure::platform_defines`, `conjure::warnings`, `conjure::sanitizers` and `CONJURE_MODULE_NAME="<name>"`; registered in the global `CONJURE_STATIC_LIBRARIES` list |
| Log category | `<module>/log_category.h` | `CONJURE_DECLARE_LOG_CATEGORY` / `CONJURE_DEFINE_LOG_CATEGORY(name, "display")` |
| Errors | `<module>/<module>_error.h` | `enum class <module>_error : u16` bound with `CONJURE_DECLARE_ERROR_CATEGORY` |
| Tests | `tests/<module>/test_<header>.cpp` | `conjure_add_test_module(<module> SOURCES ... [DEPS ...] [LABELS ...] [TIMEOUT s])`, one CTest entry per case |
| README | `engine/src/<module>/README.md` | purpose, main types, threading model, how to test |
| Handbook page | `docs/handbook/modules/<module>.md` | required by `tools/check_docs.py` for every `engine/src/<module>/` that has a `CMakeLists.txt` |

`engine/CMakeLists.txt` adds the modules in dependency order (`core, ecs, platform, rhi, assets, audio,
modules, scene, renderer, physics, scripting, engine`) and skips a directory without a `CMakeLists.txt`
(the configure log says `engine: module '<name>' not present yet, skipping`), so the tree builds while waves
are brought online. `core` is itself an umbrella `INTERFACE` target over the area libraries
`conjure::core_<area>` (see [Core](../modules/core.md) 1). The skill `.claude/skills/add-engine-module/SKILL.md`
scaffolds a new module with this layout.

### 1.1 The umbrella shared library

`conjure_finalize_engine()` in `cmake/ConjureSharedLibrary.cmake` runs once after every engine module is
declared (`engine/src/modules/CMakeLists.txt` defers it to the end of the `engine/` directory) and creates two
targets:

| Target | What it is |
|---|---|
| `conjure_abi_identity` | writes the generated header `conjure/modules/abi_identity.h`: `CONJURE_ABI_HEADER_HASH` (a hash over every public header of every engine module, recomputed whenever one changes), `CONJURE_ABI_COMPILER_MAJOR`, `CONJURE_ABI_RUNTIME_FLAGS` (bit 0 dynamic CRT, bit 1 ASan, bit 2 UBSan) |
| `conjure_shared` (alias `conjure::shared`) | a `SHARED` library that whole-archives every library registered in the global property `CONJURE_STATIC_LIBRARIES` by `conjure_register_static_library()`, so one copy of every engine symbol and global (one logger, one `string_id` table) exists in the process. It compiles with `CONJURE_SHARED_EXPORTS`; consumers get `CONJURE_SHARED_IMPORTS`, the engine include directories, every library's interface compile definitions, `conjure::platform_defines` and glm |

`CONJURE_API` (`conjure/core/api.h`) is dllexport under `CONJURE_SHARED_EXPORTS`, dllimport under
`CONJURE_SHARED_IMPORTS` and empty for static consumers. On MSVC the export table is a module definition file
generated at build time from every object of the whole-archived libraries, so everything is exported whether
or not it carries `CONJURE_API`; the annotation adds the dllimport side so consumers call through the import
thunk and exported data symbols resolve. Executables, game modules and `conjure_modules_tests` link
`conjure::shared` and nothing else from the engine.

> [!NOTE]
> The design text names the defines `CONJURE_ENGINE_EXPORTS` / `CONJURE_ENGINE_SHARED` and a
> `cmake/ConjureAbiIdentity.cmake`; the header and `cmake/ConjureSharedLibrary.cmake` are the source of truth.

## 2. The plugin system

> [!NOTE]
> Implemented by `conjure_modules` (`namespace conjure::modules`, depends on `core` only;
> `engine/src/modules/CMakeLists.txt`, headers under `conjure/modules/`, umbrella `conjure/modules/modules.h`).
> Its test suite under `tests/modules/` (fixture plugins built with `conjure_add_game_module()`) is being
> completed with wave 2, and the consumers (`ecs`, `assets`, `scripting`, `editor`) arrive with their own
> modules. The record is `07_modules.md` with `09_cross_area_resolutions.md` R14, R17-R20, R36, R37.

The module system does five things: find modules through manifests, prove a binary is safe to load before any
of its code runs, load/unload/reload in dependency order on the main thread between frames, collect what a
module registers as plain data, and hand that data to **consumers** that apply it atomically. Nothing in it
runs per frame except one relaxed atomic load in `module_manager::apply_pending()`.

### 2.1 Manifest

Every module has a `module.json` beside its content (`conjure/modules/module_manifest.h`, `07_modules.md` 2.1):

```json
{
  "schema": 1,
  "name": "com.studio.space_shooter",
  "version": "0.3.1",
  "kind": "game",
  "engine": "^0.1.0",
  "platforms": ["windows", "linux", "macos"],
  "requires": ["renderer", "physics", "scripting"],
  "binary": "space_shooter",
  "dependencies": [{ "name": "conjure.physics", "version": "*", "optional": false }],
  "managed": ["managed/SpaceShooter.dll"],
  "content": ["assets"],
  "editorOnly": false
}
```

Required keys are `schema`, `name` (`[a-z0-9_.]{1,64}`, `MAX_MODULE_NAME_LENGTH`), `version` and `engine` (a
source-compatibility range: `"1.2.3"`, `"^1.2.3"`, `"~1.2.3"`, `">=1.0.0 <2.0.0"`, `"*"`); everything else has a
default and unknown keys log a warning. `kind` is `engine`, `plugin` or `game`; `platforms` defaults to all
three, and a manifest that excludes the host is registered as `SKIPPED`, never an error. `binary` is a base
name whose location is derived, never declared: `module_binary_path()` gives
`<root>/bin/<platform>-<arch>-<config>/` plus the platform decoration (`space_shooter.dll`,
`libspace_shooter.so`, `libspace_shooter.dylib`, `decorate_binary_name`). A module with no `binary` is
manifest-only (pure C# or content). `parse_module_manifest` / `load_module_manifest` return
`MANIFEST_INVALID` with the file and JSON pointer of the offending member; `write_module_manifest` round-trips
and drops `editorOnly` for export.

### 2.2 ABI boundary

A shared module exports exactly two C-linkage symbols, `conjure_module_describe` and `conjure_module_entry`
(`conjure/modules/module_abi.h`). `describe()` returns a pure-C `conjure_module_info` whose
`conjure_module_abi_signature` carries `magic`, struct size, `CONJURE_MODULE_ABI_VERSION`, compiler id and
major, standard library, build configuration, architecture, runtime flags, feature flags, the exact engine
version and `CONJURE_ABI_HEADER_HASH` (section 1.1). `compare_abi()` checks `structSize` first (a smaller struct
is rejected before any other field is read), then every field in declaration order, before `entry()` runs; a
mismatch is `ABI_MISMATCH` naming the field with expected and actual values and the fix is "rebuild the
module". Binary compatibility is exact, source compatibility is the manifest range; once identity is proven,
ordinary C++ engine headers cross the boundary.

### 2.3 Registration is data

`conjure_module_entry(module_context *)` receives a `module_context` (`conjure/modules/module_context.h`: id,
host kind, engine version, manifest, root directory, `try_service<T>()` / `require_service<T>()`, and
`retain()` for event subscriptions so module callbacks are released before the binary is unmapped) whose
`registrar()` (`conjure/modules/module_registrar.h`) appends trivially copyable **extension records** under
`DescType::EXTENSION_POINT` (a `core::string_id`) into one aligned byte buffer, the
`module_registration_set` (`conjure/modules/module_registration_set.h`), plus managed assembly paths
(`register_managed_assembly`), the lifecycle callbacks (`set_lifecycle(module_lifecycle{onActivated,
onDeactivating})`) and `fail(reason)`, which rejects the load after the entry returns. The descriptor types
belong to the consuming area: `ecs::component_type_desc` and `ecs::system_extension_desc`
(`conjure/ecs/component_type_desc.h`) exist; the assets, editor and scripting descriptors arrive with their
modules.

Consumers (`conjure/modules/module_consumer.h`) are added to the manager before anything loads. After the
entry returns every consumer runs `validate()`; only when all pass does the manager `attach()` in add order, so
a failed load leaves the engine untouched. `can_detach()` is a side-effect-free veto (`IN_USE`,
`UNLOAD_BLOCKED`), `detach()` fills a `reload_context` snapshot during a reload and `restore()` reads it back.
Ownership is per *type*, never per instance: consumers record `(what they attached -> module_id)` and no hot
path carries a tag.

```cpp
// Module author view: conjure/modules/module_entry.h, module_context.h, module_registrar.h
#include "conjure/modules/modules.h"

#include "conjure/ecs/component_type_desc.h"

#include "conjure/core/types.h"

using namespace conjure;
using namespace conjure::modules;

namespace game {
    struct spinner {
        f32 radiansPerSecond = 1.0f;
    };
}
CONJURE_COMPONENT(game::spinner, "game.spinner");

namespace {
    void on_activated(module_context &) {}
    void on_deactivating(module_context &) {}

    module_entry_status game_entry(module_context &context) {
        module_registrar &registrar = context.registrar();          // valid only while the entry runs
        registrar.register_extension(ecs::describe_component<game::spinner>());
        registrar.register_managed_assembly("managed/Game.dll");
        registrar.set_lifecycle(module_lifecycle{&on_activated, &on_deactivating});
        return module_entry_status::OK;
    }
}

CONJURE_DECLARE_MODULE(game_entry)
```

`context.host()` is `module_host_kind::EDITOR`, `RUNTIME` or `TEST` for registrations that only make sense in
one host. `tests/modules/fixtures/module_alpha/module_alpha.cpp` is the real-code version of this example.

### 2.4 Lifecycle

`module_manager(module_manager_desc, core::event_bus &)` (`conjure/modules/module_manager.h`) owns:

| Step | API | Behaviour |
|---|---|---|
| Discovery | `discover(directory)`, `add_manifest(path)`, `add_static_module(desc)` | every immediate subdirectory with a `module.json`, in name order; per-module errors are collected into one error and the valid ones are still added |
| Resolution | `module_resolver` (`conjure/modules/module_resolver.h`) | engine range, required features, dependency versions, then Kahn's algorithm with ties broken by name, so load order and therefore component ids are reproducible on every machine; the single `game` module is forced last |
| Loading | `load(id)`, `load_all()` | `load` takes the transitive closure of dependencies and stops at the first failure (earlier successes stay loaded); `load_all` continues past failures and returns the first error |
| Unloading | `unload(id[, policy])`, `unload_all()` | `unload_policy::REFUSE_IF_IN_USE` (the `defaultUnloadPolicy`) fails with `IN_USE` while instances exist; `REMOVE_INSTANCES` strips them; `HAS_DEPENDENTS` while LOADED dependents exist |
| Reloading | `reload(id, scope)` | cascade over LOADED dependents: detach with snapshots in reverse load order, close and reopen the binaries (`reload_scope::FULL`), attach in order and restore; `MANAGED_ONLY` is the editor's C# hot reload and involves only consumers with `participates_in_managed_reload()` |
| Deferred | `request_load` / `request_unload` / `request_reload`, `apply_pending()`, `pending_count()` | the editor enqueues during a frame and the engine applies at the next frame start (R14); stale ids are dropped with a warning |
| Export | `export_layout(directory, build_config)` | section 5 |

Every mutator asserts the main thread and the engine's structural sync point (`module_manager_desc::isInsideFrame`
hook; null means "always outside a frame"). Module states are `DISCOVERED`, `SKIPPED`, `RESOLVED`, `LOADED`,
`FAILED` (retry allowed), `UNLOADED` and `FAULTED` (`conjure/modules/module_types.h`); `module_id` is a 16-bit
index plus 16-bit generation, so a stale id is `STALE_ID` instead of addressing the slot's next occupant. On
Windows the editor sets `shadowCopyBinaries` so a rebuild can overwrite the original. The built-in engine
libraries are **manifest-only static modules** (`conjure.ecs`, `conjure.renderer`, ...) added with
`CONJURE_DECLARE_STATIC_MODULE(identifier, manifestJson, nullptr)` so game modules can depend on them; they
register their types through the engine's subsystem adapters, never through a module entry (R18). Events
(`conjure/modules/module_events.h`): `module_loaded_event`, `module_unloading_event`,
`module_unloaded_event`, `module_load_failed_event`, published on the bus the manager was given.

## 3. Create a module

1. Write `module.json` (2.1) in the module root; a project's game module sits beside `project.json`.
2. Declare the binary with `conjure_add_game_module(<target> MANIFEST <module.json> SOURCES ... [NAME <manifest
   name>] [OUTPUT_ROOT <directory>] [DEPS ...] [DEFINES ...] [EDITOR_UI])` (`cmake/ConjureModule.cmake`): a
   `SHARED` library linking `conjure::shared` privately, hidden visibility, `CONJURE_MODULE_NAME="<name>"` (which
   `CONJURE_DECLARE_MODULE` embeds in `describe()`), output name from the manifest's `binary` member, placed
   where `module_binary_path()` derives it: `<root>/bin/<platform>-<arch>-<config>/`. `EDITOR_UI` adds the
   ImGui interface target for panels.
3. Implement the entry (2.3). Describe components with `CONJURE_COMPONENT` and, for the inspector, scene files
   and C# mirrors, `CONJURE_REFLECT_STRUCT` (`conjure/core/reflect/reflect.h`); systems declare their read and
   write masks in `system_extension_desc`.
4. Ship managed code by listing assemblies under `managed` or calling `register_managed_assembly`; they are
   appended to the single game `AssemblyLoadContext` (R36, scripting module).
5. Test without `dlopen`: `CONJURE_DECLARE_STATIC_MODULE(identifier, manifestJson, entry)` defines
   `conjure_static_module_<identifier>()`, which `module_manager::add_static_module()` accepts; the fixtures in
   `tests/modules/` show both the static and the shared path.

An *engine* module (a new `conjure_<name>` static library) follows section 1 and the `add-engine-module`
skill instead; it never loads at runtime.

> [!IMPORTANT]
> A descriptor passed to `register_extension` must be trivially copyable and not over-aligned (both
> `static_assert`ed); strings inside it may point at module literals because consumers copy what they keep.

## 4. Import a module

What exists today is the manager side: `discover()` over a directory, `add_manifest()`, resolution and the
failure codes. The engine and editor wiring below is wave-4 design (`07_modules.md`, R14, R20, R24):

- Discovery directories: `<engine>/plugins/*`, `<project>/modules/*` and `<project>` itself; `project.json`
  selects them with `"modules": { "search_paths": ["modules"], "engine_plugins": true, "required": ["..."] }`
  (R20).
- The editor's `module.list / load / unload / reload / discover / build` protocol commands only enqueue
  requests; `editor_module_consumer` applies module panels and commands at the next `apply_pending()` (R14).
- Content directories are mounted as `modules://<name>/` in the VFS (R24).

Failure codes are `module_error` values in the `modules` category (`conjure/modules/module_error.h`), returned
as `core::result` and published as `module_load_failed_event`: manifest (`MANIFEST_NOT_FOUND`,
`MANIFEST_INVALID`, `INVALID_NAME`, `DUPLICATE_NAME`, `PLATFORM_UNSUPPORTED`), resolution
(`ENGINE_VERSION_MISMATCH`, `FEATURE_UNAVAILABLE`, `DEPENDENCY_MISSING`, `DEPENDENCY_VERSION`,
`DEPENDENCY_CYCLE`, `DEPENDENCY_FAILED`, `MULTIPLE_GAME_MODULES`), binary (`SHARED_LIBRARIES_DISABLED`,
`BINARY_NOT_FOUND`, `BINARY_COPY_FAILED`, `BINARY_LOAD_FAILED`, `ENTRY_SYMBOL_MISSING`, `ABI_MISMATCH`,
`NAME_MISMATCH`), entry (`ENTRY_FAILED`, `REGISTRATION_CONFLICT`, `SERVICE_MISSING`), unload (`HAS_DEPENDENTS`,
`IN_USE`, `UNLOAD_BLOCKED`) and state (`NOT_LOADED`, `ALREADY_LOADED`, `INVALID_ID`, `STALE_ID`, `FAULTED`,
`EXPORT_FAILED`). A module whose unload goes wrong half-way is `FAULTED`: its binary stays resident for the
process lifetime and it accepts no further operation.

## 5. Distribute a module

- `module_manager::export_layout(directory, build_config::DISTRIBUTION)` writes, for every LOADED module that
  is not an engine module, not editor-only and not static, `<directory>/modules/<name>/{module.json, bin/<triplet>/,
  managed/}` (the manifest without `editorOnly`); `BINARY_NOT_FOUND` names the missing configuration and
  `EXPORT_FAILED` any copy error. Content directories are cooked by the asset pipeline, not copied raw
  (`07_modules.md` 3.7); the call is step 4 of `engine::export_pipeline::run` (`docs/ARCHITECTURE.md` 8, wave 4).
- The runtime runs the same discovery, resolution and ABI checks as the editor; there is no bypass path.
- A module built against stale public headers is refused with `ABI_MISMATCH` naming `headerHash`; the installed
  SDK ships the hash frozen.

> [!IMPORTANT]
> Any change to `conjure_module_abi_signature`, `module_context`, `module_registrar` or
> `module_registration_set` bumps `CONJURE_MODULE_ABI_VERSION` (`conjure/modules/module_abi.h`), and the
> layout-hash tests of the C# mirrors are the gate for every reflected struct a module ships (R48, R50, R55).

## Related
- design records: [`../../design/07_modules.md`](../../design/07_modules.md) (binding),
  [`../../design/09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md) R14, R17-R20, R24,
  R36, R37, [`../../design/06_engine_loop.md`](../../design/06_engine_loop.md) 3.4 (startup order, step 15 adds
  subsystems from modules)
- `cmake/ConjureModule.cmake` (`conjure_add_module`, `conjure_add_core_area`, `conjure_register_static_library`,
  `conjure_add_game_module`), `cmake/ConjureSharedLibrary.cmake` (`conjure_finalize_engine`),
  `conjure/core/api.h`, `conjure/core/dynamic_library.h`
- handbook: [Architecture](../architecture.md) 2-4, [Core module](../modules/core.md)
