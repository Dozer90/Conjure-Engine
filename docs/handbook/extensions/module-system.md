# Module System
<!-- doc-template:chapter -->
[Handbook](../index.md) → [Extensions](index.md)

<!-- doc-content:purpose:start -->
"Module" names two things in Conjure. An **engine module** is a static library `conjure_<name>` with its own public
headers, tests and README, built into the engine (`core`, `ecs`, `renderer`,...). A **plugin or game module** is a
package the engine loads at run time: a `module.json` manifest, optionally a native shared library, C# assemblies
and content, discovered, checked, loaded and unloaded by `conjure_modules`. This section is about the second kind
and about the one place the two meet: the umbrella shared library `conjure_shared` that hosts and plugins link.
It follows the and the headers under
`conjure/modules/`; where this text and a header differ, the header wins.
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
<a id="1-engine-modules"></a>

## Engine modules

| Piece | Where | Rule |
|---|---|---|
| Public headers | `engine/include/conjure/<module>/` | compile on their own (`conjure_header_check`), `/// <summary>` on public methods, no third-party includes |
| Sources | `engine/src/<module>/` | one `CMakeLists.txt` listing every file explicitly through `conjure_add_module(<name> SOURCES ... PUBLIC_HEADERS ... PUBLIC_DEPS ... PRIVATE_DEPS ...)` |
| Library | `conjure_<module>` (alias `conjure::<module>`), static | created by `conjure_add_module()` in `cmake/ConjureModule.cmake`; registered in the global `CONJURE_STATIC_LIBRARIES` list, which `conjure_shared` whole-archives |
| Log and error categories | `<module>/log_category.h`, `<module>/<module>_error.h` | `CONJURE_DEFINE_LOG_CATEGORY`, `CONJURE_DECLARE_ERROR_CATEGORY` |
| Tests | `tests/<module>/test_<header>.cpp` | `conjure_add_test_module(<module> SOURCES ... [DEPS ...] [LINK_ENGINE_SHARED])` |
| Documentation | `engine/src/<module>/README.md`, `docs/handbook/<scope>/<module>.md`, `docs/api/<module>/` | required by `tools/check_docs.py` |

`engine/CMakeLists.txt` adds the modules in dependency order (`core, ecs, platform, rhi, assets, audio, modules,
scene, renderer, physics, scripting, engine, bindings, editor`) and stops the configure with a `FATAL_ERROR` when a
listed directory has no `CMakeLists.txt`. [Repository Tour](../getting-started/repository-tour.md) 2 and
[Building and Configuration](../development/building-and-configuration.md) 4
describe the anatomy and the CMake helpers. An engine module never loads at run time.

<a id="2-how-a-plugin-fits-the-engine"></a>

## How a plugin fits the engine

```
   conjure_editor.exe / conjure_runtime.exe        game.dll  (conjure_add_game_module)
                 |                                      |
                 +------------ link ---------- conjure_shared.dll  (one copy of every engine global)
                                                        ^
                                 whole-archives  conjure_core ... conjure_modules ... conjure_engine
```

- **One engine copy.** `conjure_finalize_engine()` (`cmake/ConjureSharedLibrary.cmake`) whole-archives every engine
  static library into `conjure_shared`; executables and plugins link it and nothing else from the engine. One
  logger, one `string_id` table, one component registry, whatever the plugin count. `CONJURE_API`
  (`conjure/core/api.h`) is the export/import annotation; every public class with out-of-line members and every
  out-of-line free function carries it ([Modules module](../engine/modules.md) 7).
- **A C front door, then C++.** A plugin exports exactly two C symbols. `conjure_module_describe()` returns an
  ABI signature (compiler, standard library, configuration, architecture, runtime flags, features, exact engine
  version, a hash of every public header) that must equal the host's field for field before
  `conjure_module_entry()` runs; after that ordinary C++ engine headers cross the boundary. Binary compatibility
  is exact; source compatibility is the manifest's `"engine"` range.
- **Registration is data.** The entry fills a `module_registration_set` through `module_registrar`: trivially
  copyable descriptors under a `string_id` extension point (`"ecs.component"`, `"ecs.system"`, `"editor.panel"`,
...), assembly paths, lifecycle callbacks, failure reasons. Descriptor types belong to the consuming area
  (`ecs::component_type_desc` in `conjure/ecs/component_type_desc.h`), so `conjure_modules` depends on `core` only.
- **Consumers apply, atomically.** Each subsystem implements `module_consumer`. After the entry every consumer
  runs `validate()`; only when all pass does the manager `attach()` in order. Ownership is per type: consumers
  remember `(what they attached -> module_id)` and strip it on `detach()`; no hot path carries a module tag.
- **Main thread, between frames.** `load`, `unload`, `reload` and `apply_pending` assert the main thread and the
  engine's structural sync point. In-frame code enqueues with `request_*`; the engine applies at frame start. The
  `module_registry` and `service_table` are immutable in between and readable from jobs.
- **Built-ins are modules too.** The `engine` module registers one manifest-only static module per enabled
  engine library (`conjure.ecs`, `conjure.renderer`,...) so a game module can depend on `conjure.physics` like on
  any other module; the game project itself is the single `kind: "game"` module, loaded last.
- **Same path in editor and runtime.** Discovery, resolution, the ABI check and the export layout are one code
  path and one test suite; the runtime loads `modules/<name>/` exactly as the editor loaded the project tree.

<a id="3-chapters"></a>

## Chapters

| Chapter | Covers |
|---|---|
| [Creating a module](creating-a-module.md) | `conjure_add_game_module()`, directory layout, the manifest, the entry point, registering components, systems and importers, building, testing with a static test host |
| [Importing and distributing modules](importing-and-distributing-modules.md) | how a project declares and loads modules, `bin/<platform>-<arch>-<config>`, the export layout, versioning and compatibility, C# assemblies in modules |
| [Modules module](../engine/modules.md) | internals of `conjure_modules`: load algorithm, shadow copies, reload snapshots, consumers, the umbrella library, `check_exports.py`, tests and fixtures |
| 📘 API: [Modules and plugins](../../api/modules/index.md), [Writing a module](../../api/modules/writing-a-module.md), [Loading and managing modules](../../api/modules/loading-and-managing.md) | the user-facing pages: every public header by include path, short signatures, examples |

> [!NOTE]
> The consumers (ecs, assets and scripting in `engine/src/engine/module_consumers/`, the editor
> `editor_module_consumer`) and the engine wiring are implemented. The wiring is the `project.json` `"modules"`
> block (`engine/src/engine/project.cpp`), discovery in `engine/src/engine/subsystems/modules_subsystem.cpp`, and
> the `module.list`/`discover`/`load`/`unload`/`reload`/`build` commands in
> `engine/src/editor/core/commands/commands_modules.cpp`. The editor Modules panel and the
> `scripting::native_binding_desc` record are not built yet.

## Related

- `cmake/ConjureModule.cmake` (`conjure_add_module`, `conjure_register_static_library`, `conjure_add_game_module`),
  `cmake/ConjureSharedLibrary.cmake` (`conjure_finalize_engine`), `conjure/core/api.h`, `conjure/core/dynamic_library.h`
- handbook: -4, [Building and Configuration](../development/building-and-configuration.md), [Core module](../engine/core.md)

<a id="contents"></a>
<!-- doc-content:body:end -->
