# Scripting
<!-- doc-template:wiki-index -->
[API](../index.md)

<!-- doc-content:purpose:start -->
C# scripting on the .NET 8 runtime: one CLR per process, a versioned native API table the managed SDK calls
through function pointers, script components that own managed instances by slot, six script systems in the
frame, hot reload with field preservation, `[EcsComponent]` mirrors registered into the ECS, and `dotnet build`
driven from the engine. Everything on these pages is in `namespace conjure::scripting`; the C# side
(`Conjure.Managed`: `ScriptBehaviour`, `Entity`, `Transform`, `Input`, `Time`, `Log`, `Physics`, `Audio`,
`Prefab`, `Scene`, `Dod.ComponentQuery<T>`) is documented in the pages with the bindings that fill the rest
of the table.
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
## The mental model in five sentences

1. A `script_host` owns the managed side: the real CLR (`dotnet_script_host`) or a C++ fake
   (`null_script_host`) that every other module's tests use. `create_script_host(backend)` makes one.
2. A `script_runtime` owns the host, the class registry, the script store, the frame block and the pending
   queues. It is attached to an `ecs::world` and a `component_registry`; there is one per process.
3. An entity's scripts live in a 4-byte `script_component`, a handle into the runtime's store. Each entry
   names a class (by hash), keeps its fields as JSON and, in `PLAY` mode, a managed slot. The component carries its
   own copy, destroy and serialiser hooks, so scene files, prefabs, duplicates and snapshots carry scripts
   without the scene module knowing scripting exists.
4. `set_mode(PLAY)` creates every instance (`OnCreate`, then `OnStart` before the first phase); `run_phase` makes
   one native-to-managed call per phase; `set_mode(EDIT)` destroys them (`OnDestroy`). The six systems
   `register_script_systems` adds do this inside the engine frame.
5. Fields cross the boundary as a compact blob (`script_fields.h`); `collect_entry_fields` and `set_field` are
   how the inspector reads and writes a live script.

## Where to go next

| Page | Read it when you want to... |
|---|---|
| [Scripts, components and the runtime](scripts-and-components.md) | attach scripts to entities, read and write fields, run the script phases, understand the store, the class registry, collision dispatch and the DOD query scopes |
| [Hosting, the native API and hot reload](hosting-and-hot-reload.md) | start the CLR or the null host, lay out the managed directory, understand the two ABI tables and their hash, reload a game assembly and build it with `dotnet` |

## Headers

| Header | What it holds |
|---|---|
| `conjure/scripting/script_runtime.h` | `script_runtime`, `script_runtime_config`, `script_runtime_stats`, `script_mode`, `collision_event` |
| `conjure/scripting/script_component.h` | `script_component`, `script_entry`, `script_entry_id`, `script_set`, `script_store`, `register_script_component()` |
| `conjure/scripting/script_class_registry.h` | `script_class_info`, `script_field_info`, `script_class_registry`, `script_name_hash()` |
| `conjure/scripting/script_fields.h` | `field_blob_writer`, `field_blob_reader`, `json_to_blob()`, `blob_to_json()` |
| `conjure/scripting/script_system.h` | `register_script_systems()` and the six system names |
| `conjure/scripting/script_host.h` | `script_host`, `script_backend`, `script_host_config`, `create_script_host()`, `is_runtime_available()` |
| `conjure/scripting/dotnet_script_host.h` | `dotnet_script_host` (the CLR) |
| `conjure/scripting/null_script_host.h` | `null_script_host`, `null_host_recorder` (the fake for tests) |
| `conjure/scripting/native_api.h` | the C ABI records and the two tables, `script_phase`, `script_callback`, `script_field_kind`, `collision_kind` |
| `conjure/scripting/native_api_hash.h` | the generated table hash and entry counts |
| `conjure/scripting/hot_reload.h` | `hot_reload_controller`, `reload_state` |
| `conjure/scripting/managed_build.h` | `managed_build_runner`, `build_request`, `build_report`, `build_diagnostic` |

## Quick tour

```cpp
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/script_system.h"

using namespace conjure;

core::result<void> start_scripting(ecs::component_registry &registry, ecs::world &world, ecs::scheduler &scheduler,
  const std::filesystem::path &managedDirectory, const std::filesystem::path &gameAssembly) {
    CONJURE_TRY(scripting::register_script_component(registry));

    static scripting::script_runtime runtime(scripting::create_script_host(scripting::script_backend::DOTNET));
    runtime.attach_registry(&registry);
    runtime.attach_world(&world);
    scripting::script_runtime_config config = {};
    config.host.managedDirectory = managedDirectory;          // Conjure.Bootstrap.* and Conjure.Managed.dll
    CONJURE_TRY(runtime.initialize(config));
    CONJURE_TRY(runtime.load_game_assembly(gameAssembly));
    CONJURE_TRY(scripting::register_script_systems(scheduler, runtime));

    runtime.set_mode(scripting::script_mode::PLAY);
    return core::result<void>::success();                     // every entity's scripts are now live
}
```

The engine does this in its scripting subsystem, with the bindings filling the rest of the native table between
`attach_world` and `initialize`. The runtime is destroyed before the world it is attached to.

> [!IMPORTANT]
> Everything is main-thread only: the CLR is started and every managed call is made from the thread
> `core::jobs::is_main_thread()` names. The one exception is `managed_build_runner`, whose build runs on a job
> and posts its report to the main-thread queue.

## Local terms

| Term | Meaning |
|---|---|
| Class hash | `fnv1a64` of the full C# type name (`script_name_hash("Game.Player")`); what a `script_entry` stores |
| Slot | the managed side's index of a live instance; `INVALID_SLOT` while in `EDIT` mode or without a class |
| Field blob | the compact binary form of a script's fields that crosses the boundary (`script_fields.h`) |
| Frame block | the shared memory (`conjure_frame_block`) the engine writes before each phase and `Time` / `Input` read in C# |
| Phase | `FIXED_UPDATE`, `UPDATE` or `LATE_UPDATE`: one managed call each per frame |
| Query scope | a locked, chunk-by-chunk view of components a `ComponentQuery<T>` opens from C#; structural requests inside it are deferred |

## See also

- 📘 API: [Entities, components and systems](../ecs/index.md), [Scene files, prefabs and snapshots](../scene/scene-files-and-prefabs.md), [Processes, libraries and images](../core/system.md)
- 🛠 Handbook: [Scripting module](../../handbook/engine/scripting.md)

## Supporting headers

| Include path | Purpose |
|---|---|
| `conjure/scripting/log_category.h` | Declares the module log category used to filter its diagnostics through core logging. |
| `conjure/scripting/scripting.h` | Convenience include for the public module APIs; individual headers can be included separately. |
| `conjure/scripting/scripting_error.h` | Declares `script_error`, the module failure codes carried by `core::error` and `core::result<T>`. |
<!-- doc-content:body:end -->
