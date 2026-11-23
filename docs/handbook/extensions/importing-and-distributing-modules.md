# Importing and Distributing Modules
<!-- doc-template:chapter -->
[Handbook](../index.md) → [Extensions](index.md)

<!-- doc-content:purpose:start -->
How modules get into a project and out of it again: where the editor and runtime look for them, what a module
directory must contain, how the exported game is laid out, what "compatible" means for a module binary and how C#
assemblies travel with a module. The manager API that performs all of this (`discover`, `load_all`,
`export_layout`) is on the 📘 API page; this chapter is about the conventions and the engine wiring around it.
The wiring lives in the `engine` module (`modules_subsystem`) and in the editor `module.*` commands.
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
<a id="1-declaring-modules-in-a-project"></a>

## Declaring modules in a project

A project imports a module by placing its directory where the host searches:

| Location | Contents | Who adds it |
|---|---|---|
| `<engine>/plugins/<name>/` | plugins shipped with the engine installation | `discover()` when `engine_plugins` is true |
| `<project>/modules/<name>/` | plugins the project imports (copied or checked in) | `discover()` for each `search_paths` entry |
| `<project>/module.json` | the project's own game module, beside `project.json` | `add_manifest()` |

`project.json` selects the search paths and may name modules that must be present:

```json
"modules": { "search_paths": ["modules"], "engine_plugins": true, "required": ["com.studio.gameplay"] }
```

A module's own dependencies are declared in its manifest, not in the project: `"dependencies": [{ "name":
"com.studio.gameplay_common", "version": ">=1.0.0 <2.0.0", "optional": false }]`. Resolution finds the dependency
by name among every registered module (plugins, the game module, the built-in `conjure.*` static modules), checks
its version against the range and orders the loads so dependencies come first; a missing non-optional dependency is
`DEPENDENCY_MISSING` naming it, and a module that only *can* use another declares it `"optional": true` and asks
`registry.find()` at run time. Platform-specific modules list `"platforms"`; on another platform they are `SKIPPED`
and count as missing for dependents.

> [!NOTE]
> The engine reads the `project.json` block and searches these directories in
> `engine/src/engine/subsystems/modules_subsystem.cpp`. `tests/modules/`
> exercises the manager itself (`discover(directory)`, `add_manifest(path)`, `add_static_module(desc)`,
> resolution and every failure code) over a directory laid out exactly like `<project>/modules/`.

<a id="2-binary-layout"></a>

## Binary layout

```
<module root>/
  module.json
  bin/<platform>-<arch>-<config>/<decorated binary>
  managed/<assemblies>
  <content directories>
```

| Part | Values |
|---|---|
| `platform` | `windows`, `linux`, `macos` (`module_platform`, `host_platform()`) |
| `arch` | `x64`, `arm64` (`module_architecture`, `host_architecture()`) |
| `config` | `debug`, `development`, `distribution` (`build_config`; the host's own by default, `module_manager_desc::binaryConfig` overrides) |
| decorated binary | `"binary": "game"` becomes `game.dll`, `libgame.so`, `libgame.dylib` (`decorate_binary_name()`) |

`module_binary_path(manifest, root, triplet)` and `module_binary_directory(root, triplet)`
(`conjure/modules/module_manifest.h`) derive the path; `conjure_add_game_module()` writes the binary exactly there.
Because the configuration is part of the path, a module directory can carry Debug, Development and Distribution
builds side by side and the host picks its own. A missing binary is `BINARY_NOT_FOUND` with the full path the host
looked at, so a wrong configuration is visible in the message.

In the editor (`shadowCopyBinaries = true`, `shadowCopyDirectory = <project>/.conjure/module_cache`) the binary is
copied to `<cache>/<name>/<n>/` and the copy is opened, so rebuilding `bin/.../game.dll` while the editor runs
succeeds on Windows too; the runtime loads in place. The cache is cleaned when a manager starts.

<a id="3-loading-at-start-up"></a>

## Loading at start-up

The engine (`modules_subsystem`, step 15) constructs the services and consumers first,
then the manager, in this order:

1. `provide_service()` for `engine::engine_context`, `engine::application`, `ecs::world`, `assets::asset_database`,
   `scene::scene_manager`.
2. `add_consumer()` for ecs, assets, scripting and, in the editor executable, the editor consumer; consumers attach
   in this order and detach in reverse.
3. `add_static_module()` for every built-in library (`conjure.ecs`, `conjure.renderer`,...), manifest-only.
4. `discover(<engine>/plugins)` when `engine_plugins`, `discover()` per `search_paths` entry,
   `add_manifest(<project>/module.json)`.
5. `load_all()`: resolve everything, load in order, continue past failures, report the first error. The game module
   is forced last.
6. Every frame start: `apply_pending()` with `isInsideFrame` wired, performing the editor's `module.load / unload /
   reload` requests.

The runtime executable runs steps 1 to 5 over the exported tree with the same code; there is no bypass path and one
test suite covers both hosts. Content directories listed in a manifest are mounted by the assets consumer as
`modules://<name>/` on attach and unmounted on detach.

<a id="4-export-layout"></a>

## Export layout

`module_manager::export_layout(outputDirectory, build_config::DISTRIBUTION)` is step 4 of the engine's export
pipeline, after the project was built in Distribution. For every `LOADED` module
that is not an engine module, not `editorOnly` and not static it writes:

```
<out>/modules/<name>/
  module.json                                   re-serialised: editorOnly dropped, "managed" lists every
                                                assembly, including those the entry registered
  bin/<platform>-<arch>-distribution/           copied as a whole (binary and companions)
  managed/<assemblies>                          each listed assembly copied
```

A module whose Distribution binary is missing fails with `BINARY_NOT_FOUND` and the message "build the project in
Distribution first"; a copy error is `EXPORT_FAILED` naming the file. Content directories are not copied raw: the
asset pipeline cooks them into the game's asset bundles. The runtime then runs section 3 over `<out>/modules/`.
`tests/modules/test_module_export_layout.cpp` pins every rule here.

<a id="5-versioning-and-compatibility"></a>

## Versioning and compatibility

Two notions of compatibility exist and they are deliberately different:

| Compatibility | Checked by | Rule |
|---|---|---|
| Source: "this module's code was written against engine X" | resolution, manifest `"engine"` range | `semantic_version` of the host must lie in the range; `ENGINE_VERSION_MISMATCH` otherwise. Likewise `"dependencies[].version"` against each dependency's `"version"` (`DEPENDENCY_VERSION`) |
| Binary: "this binary can be mapped into this process" | `compare_abi()` before the entry runs | exact equality of `conjure_module_abi_signature`: struct size, `CONJURE_MODULE_ABI_VERSION`, compiler id and major, standard library, build configuration, architecture, runtime flags (dynamic CRT, ASan, UBSan), compiled-in feature flags, engine `major.minor.patch`, `CONJURE_ABI_HEADER_HASH` |

Consequences for module authors and SDK maintainers:

- A plugin binary is tied to one engine build. Shipping a plugin means shipping one `bin/<triplet>/` per engine
  version and configuration it supports, or shipping source and letting the project build it (the editor's
  `module.build`). A third-party binary marketplace with relaxed `major.minor` compatibility is explicitly out of
  scope for the first version.
- `CONJURE_MODULE_ABI_VERSION` (`conjure/modules/module_abi.h`) is bumped whenever `conjure_module_abi_signature`,
  `module_context`, `module_registrar` or `module_registration_set` changes layout. Any other public header change
  is caught by the header hash, which the installed SDK ships frozen in `abi_identity.h`.
- The manifest `"version"` follows semantic versioning as consumers of your module read it through ranges: bump
  `major` for a breaking change (dependents with `^1.x` stop resolving, on purpose), `minor` for additions,
  `patch` for fixes. Ranges are half-open and have no pre-release tags; `to_string(version_range)` writes the
  canonical `">=a.b.c <d.e.f"` form, so caret and tilde spellings are not preserved by `write_module_manifest()`.
- Module names are `[a-z0-9_.]{1,64}`; use a reverse-domain prefix so two studios' plugins never collide
  (`DUPLICATE_NAME` is a hard error at registration).

> [!IMPORTANT]
> Every reflected struct a module ships with a C# mirror is gated by the layout-hash tests of the bindings
>; a layout change without a mirror update fails those tests, not the module
> load.

<a id="6-c-assemblies-in-modules"></a>

## C# assemblies in modules

As implemented by `scripting_module_consumer` (`engine/src/engine/module_consumers/scripting_module_consumer.cpp`), except the native-binding records:

- A module lists its assemblies under `"managed"` (relative to the module root) or adds them from the entry with
  `module_registrar::register_managed_assembly()`; both end up in `module_registration_set::managedAssemblies`,
  deduplicated, manifest entries first. A module may be managed-only (no `"binary"`), as the fixture
  `module_managed_only` is.
- The scripting consumer appends them to the **single game `AssemblyLoadContext`** on attach (one collectible ALC
  for the game and every module, so `ScriptBehaviour` has one type identity), and on detach or a `MANAGED_ONLY`
  reload calls `script_runtime::reload_game_assembly`, which preserves script fields itself; the `reload_context`
  is unused by scripting and a leaked ALC is logged, never blocking.
- `reload_scope::MANAGED_ONLY` is the editor's "C# hot reload": the native binary stays open, only consumers with
  `participates_in_managed_reload()` (scripting) detach and restore, and the module's lifecycle callbacks bracket
  the operation like a full reload.
- Native bindings a module adds for its C# code travel as `scripting::native_binding_desc { entryName, function }`
  records filling an existing null slot of the fixed native table; a new slot needs an `.inl` line in the engine
  tree. This part is not implemented: `scripting::native_binding_desc` is not defined and no consumer reads it.
  A slot filled before the bindings run is overridden by `bindings::fill_native_table` with a WARN when its group
  is compiled in (B3).
- `export_layout()` copies every listed assembly into `<out>/modules/<name>/managed/` and writes the full list
  into the exported manifest's `"managed"` array, so the exported tree describes itself; a missing assembly at
  export time is `EXPORT_FAILED`.
- Scripts and the AI agent see modules read-only through `Conjure.Modules.Registry` (list, state, version, last
  error); loading and unloading is never driven from managed code.

The assembly paths and their export are implemented and tested today (`test_module_manager_shared`,
`test_module_export_layout`); the ALC side is `scripting_module_consumer` over
`script_runtime::reload_game_assembly`.

## Related

- handbook: [Module system](module-system.md), [Creating a module](creating-a-module.md), [Modules module](../engine/modules.md)
- 📘 API: [Loading and managing modules](../../api/modules/loading-and-managing.md), [Writing a module](../../api/modules/writing-a-module.md)

<a id="contents"></a>
<!-- doc-content:body:end -->
