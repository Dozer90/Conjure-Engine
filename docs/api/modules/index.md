# Modules and plugins

📘 **Public API wiki** · 🛠 Handbook: [Modules module](../../handbook/modules/modules.md)

A Conjure **module** is a package the engine loads at start-up: a `module.json` manifest, optionally a native
shared library (`.dll`, `.so`, `.dylib`), optionally C# assemblies and content directories. Your game project is
one module; everything you reuse between projects (a gameplay library, an importer for a custom file format, an
editor tool) is a plugin module. Module authors include one header, `conjure/modules/modules.h`; hosts and tools
that load modules use the headers described on [Loading and managing modules](loading-and-managing.md).

| Page | Covers |
|---|---|
| [Writing a module](writing-a-module.md) | the entry point, registering components, systems and other extensions, the manifest, versions, host services, events |
| [Loading and managing modules](loading-and-managing.md) | `module_manager`, discovery, dependency order, load / unload / reload, the registry, consumers, the ABI check, failure codes |

## What a module can add

A module does not call into the engine to install things. Its entry function hands the engine **descriptors**
(plain structs) and the engine's subsystems apply them once every check has passed:

| You want to add... | Descriptor | Applied by |
|---|---|---|
| A component type | `ecs::component_type_desc` from `ecs::describe_component<T>()` | the ECS |
| A system | `ecs::system_extension_desc` | the ECS scheduler |
| An asset importer or asset type | the assets descriptors | the asset pipeline |
| An editor panel or command | the editor descriptors | the editor (editor host only) |
| C# code | an assembly path (`"managed"` in the manifest, or `register_managed_assembly()`) | the scripting runtime |
| Content (textures, scenes, prefabs) | a directory listed under `"content"` | mounted as `modules://<name>/` |

Components a module adds live in the same archetype chunks as engine components and its systems run in the same
parallel schedule; nothing on a hot path knows which module something came from.

> [!NOTE]
> The ECS descriptors exist today. The assets, scripting and editor descriptors, and the subsystems that apply
> them, arrive with their own modules in later waves; the module system itself does not change when they do.

## The manifest in one look

Every module directory holds a `module.json`:

```json
{
  "schema": 1,
  "name": "com.studio.space_shooter",
  "version": "0.3.1",
  "kind": "game",
  "engine": "^0.1.0",
  "requires": ["renderer", "physics"],
  "binary": "space_shooter",
  "dependencies": [{ "name": "com.studio.gameplay_common", "version": ">=1.0.0 <2.0.0" }],
  "managed": ["managed/SpaceShooter.dll"],
  "content": ["assets"]
}
```

`schema`, `name`, `version` and `engine` are required; everything else has a default. The binary is named by its
base name only: the engine looks for it under `bin/<platform>-<arch>-<config>/` next to the manifest, so one module
directory can carry builds for several platforms and configurations. A module without `"binary"` is a pure C# or
content module. [Writing a module](writing-a-module.md#the-manifest) lists every field.

## Lifecycle

```
discover  ->  resolve  ->  load  ->  (running)  ->  unload or reload
module.json   dependency    ABI check, entry,       detach in reverse, binary closed
found         order         validate, attach        generation of the id advances
```

1. **Discover.** The host registers manifests (the editor scans `<engine>/plugins/*`, `<project>/modules/*` and
   the project itself). A manifest whose `platforms` exclude the host is skipped, never an error.
2. **Resolve.** Dependencies are ordered deterministically (the same order on every machine), engine version and
   required features are checked, the single `game` module goes last.
3. **Load.** For a native module the engine opens the binary and compares an ABI signature (compiler, standard
   library, configuration, engine version, a hash of the public headers) before any module code runs. The entry
   runs, every subsystem validates what was registered, and only then is anything applied. A module that fails at
   any step leaves the engine exactly as it was.
4. **Running.** Your components, systems and assets behave like engine ones. The module's `module_context` stays
   valid; lifecycle callbacks (`onActivated`, `onDeactivating`) bracket the loaded period.
5. **Unload or reload.** Unloading refuses while something still uses the module (or strips the instances, your
   choice) and while loaded modules depend on it. The editor reloads a rebuilt module in place: state is
   snapshotted, the binary is swapped, state is restored.

> [!WARNING]
> Loading, unloading and reloading happen on the main thread between frames. Code that runs during a frame
> (systems, editor panels) must *request* a change with `request_load()` / `request_unload()` /
> `request_reload()`; the engine applies the queue at the next frame start.

> [!TIP]
> Test a module without building a shared library: `CONJURE_DECLARE_STATIC_MODULE` compiles the entry into the
> test executable and `module_manager::add_static_module()` loads it through the very same path. The handbook's
> [Creating a module](../../handbook/module-system/creating-a-module.md) shows the test host.

## See also

- 📘 API: [Writing a module](writing-a-module.md), [Loading and managing modules](loading-and-managing.md)
- 📘 API: [Errors and results](../core/errors-and-results.md) (every failure is a `result<T>` carrying a
  `module_error`), [Time, events and random](../core/time-and-events.md) (the event bus module events travel on)
- 📘 API: [Glossary](../glossary.md)
- 🛠 Handbook: [Module system](../../handbook/module-system/index.md) (creating, importing and distributing
  modules), [Modules module](../../handbook/modules/modules.md) (internals)
