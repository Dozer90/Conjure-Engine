# Creating a module

🛠 **Engineering handbook** · 📘 API: [Writing a module](../../api/modules/writing-a-module.md)

The build and test side of writing a plugin or game module: the directory, the manifest, the CMake target, the
entry point and the two ways to test it (a static test host without `dlopen`, and the real shared library). The
API itself (`module_registrar`, `module_context`, services, events) is on the 📘 API page; this chapter assumes it
and adds what an engineer in the engine tree or an SDK user needs to get a module built and loaded.

## Contents
- [1. Directory layout](#1-directory-layout)
- [2. The manifest](#2-the-manifest)
- [3. The CMake target](#3-the-cmake-target)
- [4. The entry point](#4-the-entry-point)
- [5. Registering components, systems and importers](#5-registering-components-systems-and-importers)
- [6. Building](#6-building)
- [7. Testing with a test host](#7-testing-with-a-test-host)
- [Related](#related)

## 1. Directory layout

```
modules/com.studio.gameplay/            module root (the manifest's directory)
  module.json                           manifest, section 2
  CMakeLists.txt                        conjure_add_game_module(...), section 3
  source/gameplay_module.cpp            the entry (CONJURE_DECLARE_MODULE), components, systems
  managed/Gameplay.dll                  C# assemblies listed under "managed" (optional)
  assets/                               content listed under "content" (optional)
  bin/windows-x64-development/gameplay.dll        build output, never committed
  bin/linux-x64-development/libgameplay.so
```

The game project is one module whose `module.json` sits beside `project.json`; a project may ship no native
binary at all (pure C# or content). The binary directory is derived, never declared: `bin/<platform>-<arch>-
<config>/` with `platform` in `windows | linux | macos`, `arch` in `x64 | arm64`, `config` in
`debug | development | distribution`, so one module directory can hold builds for several targets. The test
fixtures under `tests/modules/fixtures/<name>/` are the smallest real examples of this layout.

## 2. The manifest

```json
{
  "schema": 1,
  "name": "com.studio.gameplay",
  "displayName": "Gameplay",
  "version": "1.2.0",
  "kind": "plugin",
  "engine": "^0.1.0",
  "platforms": ["windows", "linux", "macos"],
  "requires": ["renderer"],
  "binary": "gameplay",
  "dependencies": [{ "name": "conjure.physics", "version": "*" }],
  "managed": ["managed/Gameplay.dll"],
  "content": ["assets"],
  "editorOnly": false
}
```

| Key | Rule |
|---|---|
| `schema`, `name`, `version`, `engine` | required; `name` matches `[a-z0-9_.]{1,64}` (`MAX_MODULE_NAME_LENGTH`), `engine` is a source-compatibility range (`"1.2.3"`, `"^1.2.3"`, `"~1.2.3"`, `">=1.0.0 <2.0.0"`, `"*"`) |
| `kind` | `plugin` (default), `game` (one per project, loaded last) or `engine` (reserved for built-ins) |
| `platforms` | default all three; a manifest excluding the host is registered `SKIPPED`, never an error |
| `requires` | `renderer`, `physics`, `audio`, `scripting`, `editor`; resolution fails with `FEATURE_UNAVAILABLE` on a host built without one |
| `binary` | base name only; absent for a manifest-only module |
| `dependencies` | `{ "name", "version", "optional" }`; no self-dependency, no duplicates; the single `game` module may depend on built-ins (`conjure.physics`) and plugins alike |
| `managed`, `content` | paths relative to the module root; `managed` entries are appended to the registration set's `managedAssemblies` before the entry runs |
| `editorOnly` | excluded from `export_layout()` |

Unknown keys log a warning. `parse_module_manifest()` / `load_module_manifest()` (`conjure/modules/module_manifest.h`)
report `MANIFEST_INVALID` with the file and the JSON pointer of the offending member; `write_module_manifest()`
round-trips. `tests/modules/test_module_manifest.cpp` is the reference for every rule above.

## 3. The CMake target

```cmake
conjure_add_game_module(gameplay_module
    MANIFEST  "${CMAKE_CURRENT_SOURCE_DIR}/module.json"
    SOURCES   source/gameplay_module.cpp source/spinner_system.cpp
    DEPS      some_private_library            # optional
    DEFINES   GAMEPLAY_VERBOSE=1              # optional
)
```

`conjure_add_game_module(<target> MANIFEST <module.json> SOURCES ... [NAME <name>] [OUTPUT_ROOT <dir>] [DEPS ...]
[DEFINES ...] [EDITOR_UI])` (`cmake/ConjureModule.cmake`) creates a `SHARED` library that:

| Property | Value |
|---|---|
| Links | `conjure::shared` privately (the whole engine, one copy), `conjure::platform_defines`, `conjure::warnings`, `conjure::sanitizers`, `DEPS` |
| Defines | `CONJURE_MODULE_NAME="<name>"` (the manifest's `"name"` unless `NAME` says otherwise), which `CONJURE_DECLARE_MODULE` embeds in `conjure_module_describe()` |
| Output name | the manifest's `"binary"` (falls back to the target name) |
| Output directory | `<root>/bin/<platform>-<arch>-$<LOWER_CASE:$<CONFIG>>/`, with `<root>` the manifest's directory unless `OUTPUT_ROOT` is given; `.pdb` files land beside the binary |
| Visibility | `CXX_VISIBILITY_PRESET hidden`, `VISIBILITY_INLINES_HIDDEN`; only the two entry symbols (and anything you mark `CONJURE_MODULE_EXPORT`) are visible |
| Run path | `$ORIGIN` / `@loader_path`, so the module finds `conjure_shared` next to the host |
| `EDITOR_UI` | adds `conjure::thirdparty::imgui_interface` for panels drawn with the editor's ImGui context |

The target depends on `conjure_abi_identity`, so a module always compiles against the current
`conjure/modules/abi_identity.h`. `ConjureConfig.cmake` exports the same function for installed projects
(`find_package(conjure CONFIG)`), recording the engine's options so the `CONJURE_WITH_*` macros match.

> [!IMPORTANT]
> A module must be built in the same configuration as the host that loads it: the editor in Development loads
> `bin/<platform>-<arch>-development/`, and a Debug module presented to it is refused with `ABI_MISMATCH` on
> `buildConfig` before any of its code runs. The editor builds the game module itself, so this is only a concern
> for hand-built plugins.

## 4. The entry point

One source file declares the entry once, at namespace scope:

```cpp
#include "conjure/modules/modules.h"

using namespace conjure::modules;

namespace {
    module_entry_status gameplay_entry(module_context &context) {
        module_registrar &registrar = context.registrar();
        registrar.register_managed_assembly("managed/Gameplay.dll");
        // register_extension(...), set_lifecycle(...), fail(...) as the module needs
        return module_entry_status::OK;
    }
}

CONJURE_DECLARE_MODULE(gameplay_entry)
```

`CONJURE_DECLARE_MODULE` (`conjure/modules/module_entry.h`) expands to the two exported C functions:
`conjure_module_describe()` returns a `static constexpr conjure_module_info` built by `make_module_info(
CONJURE_MODULE_NAME)`, and `conjure_module_entry(module_context *)` forwards to your function and returns its status
as `std::uint32_t`. `describe()` is the only module code that runs before `compare_abi()` passed, and the name it
returns must equal the manifest's (`NAME_MISMATCH` otherwise). The entry runs on the main thread between frames,
with `context.registrar()` bound for its duration only. Additional exported C functions (counters, test probes) use
`CONJURE_MODULE_EXPORT` as the fixtures do.

## 5. Registering components, systems and importers

Every extension is a trivially copyable descriptor defined by the consuming area and passed to
`registrar.register_extension(desc)`; the consumer reads it back with `extensions_of<Desc>()` on attach.

| Extension | Descriptor | Notes |
|---|---|---|
| Component type | `ecs::component_type_desc` via `ecs::describe_component<T>()` (`conjure/ecs/component_type_desc.h`) | `T` is named with `CONJURE_COMPONENT(T, "name")`; the registry deep-copies the `reflect::type_info` at attach, so reflect the struct (`CONJURE_REFLECT_STRUCT`) for the inspector, scene files and C# mirrors |
| System | `ecs::system_extension_desc` | `name`, `phase`, `flags`, `reads` / `writes` / `after` / `before` as `const core::string_id *` plus counts, and a `run(system_context &)` function pointer; the ECS consumer rejects a system without a declared access set in `validate()` |
| Asset importer, asset type | `assets::importer_extension_desc` (`"assets.importer"`), `assets::asset_type_extension_desc` (`"assets.type"`) | designed in `07_modules.md` 9 with the assets module; factories are called by the assets consumer on attach |
| Editor panel, command | `editor::panel_desc` (`"editor.panel"`), `editor::command_desc` (`"editor.command"`) | designed; stored and never read on a runtime host |
| Native C# binding | `scripting::native_binding_desc` (`"scripting.binding"`) | designed; fills an existing null slot of the native table by name (R36) |
| C# assembly | `registrar.register_managed_assembly("managed/X.dll")` or the manifest's `"managed"` | appended to the single game `AssemblyLoadContext` |

The arrays a `system_extension_desc` points at must outlive the module (namespace-scope `constexpr` arrays in the
module do). Strings in descriptors may point at module literals; consumers copy what they keep. The ECS descriptors
exist today; the others arrive with their modules and the module system needs no change for them.

## 6. Building

Inside the engine tree a module is an ordinary target: `python tools/build.py --build-dir build/dev --target
gameplay_module`. The fixtures show the pattern for a module whose binary should not land in the source tree:
`tests/modules/CMakeLists.txt` calls `conjure_add_game_module(conjure_fixture_<name> MANIFEST ... OUTPUT_ROOT
"${PROJECT_BINARY_DIR}/test_modules/<name>" ...)` and copies the manifest beside the output with
`configure_file(... COPYONLY)`, so the build tree holds a complete module directory. Against an installed SDK,
`find_package(conjure CONFIG)` provides the same function. The editor's `module.build` protocol command
runs the project's CMake build for the editor's configuration and then requests a reload.

> [!WARNING]
> Every public engine header feeds `CONJURE_ABI_HEADER_HASH`. After pulling engine changes, rebuild the module
> before loading it; a stale module is refused with `ABI_MISMATCH` naming `headerHash`, which is the intended
> protection against loading a binary compiled against another layout.

## 7. Testing with a test host

Compile the entry into the test executable as a static module and load it through a `module_manager` with shared
libraries disabled; the load path is the same as for a binary minus `dlopen` and the ABI check.

```cpp
#include "conjure/testing/doctest.h"

#include "conjure/modules/module_context.h"
#include "conjure/modules/module_entry.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/event_bus.h"

using namespace conjure;
using namespace conjure::modules;

namespace {
    module_entry_status gameplay_entry(module_context &context) {
        context.registrar().register_managed_assembly("managed/Gameplay.dll");
        return module_entry_status::OK;
    }

    CONJURE_DECLARE_STATIC_MODULE(
      gameplay_under_test,
      R"({"schema": 1, "name": "com.studio.gameplay", "version": "1.2.0", "engine": "*"})",
      &gameplay_entry
    )
}

TEST_CASE("gameplay module - loads in a static test host") {
    core::event_bus events = {};
    module_manager_desc desc = {};
    desc.host = module_host_kind::TEST;
    desc.allowSharedLibraries = false;
    module_manager manager(desc, events);

    const module_id id = manager.add_static_module(conjure_static_module_gameplay_under_test()).value();
    CONJURE_REQUIRE_OK(manager.load(id));
    CHECK(manager.registry().state(id) == module_state::LOADED);
    CHECK(manager.registry().registrations(id).managedAssemblies.size() == 1);
}
```

Add a `module_consumer` (a recording one like `tests/modules/fake_consumer.h`, or the real `ecs_module_consumer`
from `engine/src/engine/module_consumers/`) with `manager.add_consumer()` to check what the entry registered
reaches the subsystem, and
`manager.provide_service()` to hand the module the services it asks for. To test the real binary, build it with
`conjure_add_game_module(... OUTPUT_ROOT <build tree>)`, point `discover()` or `add_manifest()` at the output and
link the test executable with `conjure_add_test_module(... LINK_ENGINE_SHARED)`, as `tests/modules/` does; without
that option the test executable carries its own engine copy and the plugin another, which the ABI check does not
detect but the duplicated globals (two loggers, two `string_id` tables) break at run time.

## Related
- design records: [`../../design/07_modules.md`](../../design/07_modules.md) 2.1 (manifest), 4.1 (macros), 4.8
  (module author view), 5 (build system), [`../../design/01_ecs.md`](../../design/01_ecs.md) 4.7 (extension
  descriptors), [`../../design/09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md) R18, R36, R37
- `cmake/ConjureModule.cmake`, `tests/modules/CMakeLists.txt`, `tests/modules/fixtures/`
- handbook: [Module system](index.md), [Importing and distributing modules](importing-and-distributing.md),
  [Modules module](../modules/modules.md), [Testing](../testing.md)
- 📘 API: [Writing a module](../../api/modules/writing-a-module.md), [Loading and managing modules](../../api/modules/loading-and-managing.md)
