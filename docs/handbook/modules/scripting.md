# Scripting module

🛠 **Engineering handbook** · 📘 API: [Scripting](../../api/scripting/index.md), [Scripts, components and the runtime](../../api/scripting/scripts-and-components.md), [Hosting, the native API and hot reload](../../api/scripting/hosting-and-hot-reload.md)

`conjure::scripting` (target `conjure_scripting`, alias `conjure::scripting`) is the wave-3 module that hosts C#
on the .NET 8 runtime: one CLR per process through `hostfxr`, the versioned native and managed API tables,
`script_component` entries owning managed instances by slot, the six exclusive script systems, hot reload through
a collectible load context with field preservation, `[EcsComponent]` mirrors registered into the component
registry, DOD `Span<T>` query scopes and `dotnet build` driven from the engine. This chapter is for people
changing the module or the managed SDK: the two tables, the runtime's re-entrancy rules, the component
registration, the managed build and tests. The binding design is [`03_scripting.md`](../../design/03_scripting.md);
the cross-area decisions are R4-R6, R26-R29, R36, R39, R54, R61 and R64 of
[`09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md); `engine/src/scripting/README.md`
carries the finer notes.

## Contents

- [1. Shape of the module](#1-shape-of-the-module)
- [2. The two tables and the handshake](#2-the-two-tables-and-the-handshake)
- [3. The runtime](#3-the-runtime)
- [4. The component registration](#4-the-component-registration)
- [5. Hot reload](#5-hot-reload)
- [6. The managed side](#6-the-managed-side)
- [7. nethost on MSVC](#7-nethost-on-msvc)
- [8. Threading and destruction order](#8-threading-and-destruction-order)
- [9. Tests](#9-tests)
- [10. Open items](#10-open-items)
- [Related](#related)

## 1. Shape of the module

`engine/src/scripting/CMakeLists.txt`: `conjure_add_module(scripting ... PUBLIC_DEPS conjure::core conjure::ecs
conjure::assets PRIVATE_DEPS <nethost>)`, guarded by `CONJURE_WITH_SCRIPTING` and the presence of `conjure::ecs`
and `conjure::assets`; never `scene` (R29). `managed/CMakeLists.txt` builds the C# projects with
`conjure_add_managed_project` (`cmake/ConjureManaged.cmake`) behind stamp files and makes `conjure_scripting`
depend on the SDK.

| Header | Main types | 📘 Wiki page |
|---|---|---|
| `script_runtime.h`, `script_component.h`, `script_class_registry.h`, `script_fields.h`, `script_system.h` | `script_runtime`, `script_component` / `script_store`, `script_class_registry`, the field blob, `register_script_systems` | [Scripts, components and the runtime](../../api/scripting/scripts-and-components.md) |
| `script_host.h`, `dotnet_script_host.h`, `null_script_host.h`, `native_api.h`, `native_api_hash.h`, `hot_reload.h`, `managed_build.h` | the hosts, the ABI records and tables, the generated hash, `hot_reload_controller`, `managed_build_runner` | [Hosting, the native API and hot reload](../../api/scripting/hosting-and-hot-reload.md) |
| `scripting.h`, `scripting_error.h`, `log_category.h`, `native_api.inl` | umbrella, `script_error` and `status_of`, `LOG_SCRIPTING`, the X-macro table (an input of `native_api.h`, not a header that compiles alone) | not user-facing |

Private: `hostfxr_api.h` (the only file that includes `nethost.h`, `hostfxr.h` and `coreclr_delegates.h`),
`hostfxr_runtime.{h,cpp}` (the process-wide CLR), `native_api_core.cpp` (the 20 core entries: log, class
registration, faults, entities, components, queries), and one `.cpp` per public header.

## 2. The two tables and the handshake

`conjure_native_api` is filled by the engine (the core entries by `fill_core_native_entries` inside
`script_runtime::initialize`, the ten bindings groups by the wave-4 `scripting_bindings_subsystem` before it);
`conjure_managed_api` is filled by `Conjure.Bootstrap.Initialize`. Both carry size, version and layout hash;
`validate_native_api` / `validate_managed_api` and the managed side compare them, so any drift is
`ABI_MISMATCH` at start-up. The entry list is the X-macro table `native_api.inl` (`CONJURE_NATIVE_FN(group,
name, ret, (args))`, 20 core entries then the `// --- bindings (R26) ---` marker); `tools/generate_native_api.py`
derives `native_api_hash.h`, `managed/Conjure.Managed/Generated/NativeApi.g.cs` and
`docs/generated/native_api_manifest.json`, and the CTest entry `conjure_scripting_api_generated_up_to_date`
fails when they are stale. The managed layouts (`LayoutHash.cs`) are the layouts `sizeof(T)` and `Span<T>` use:
fields at their natural alignment in declaration order, checked against `Unsafe.SizeOf<T>`; `Marshal.OffsetOf`
is never consulted because it widens `bool`.

## 3. The runtime

One `script_runtime` per process (`script_runtime::active()` is what the component hooks and the core entries
reach). It owns the host, the class registry (staged then committed tables), the store, the 64-byte aligned
frame block, the pending create / destroy queues, the deferred command buffer (R5, R54) and the query scopes.
`attach_world` wires the ADDED / REMOVED observers of `script_component` and adopts the sets the world's
entities already carry; `set_mode(PLAY)` creates every instance in one `create_instances` call.

Every managed call may run script code that calls back in: `OnCreate` may destroy its own entity, `OnDestroy`
may spawn scripted entities (the store reallocates), either may open a query scope. The runtime therefore
never holds a `script_set *` or `script_entry *` across a managed call, destroys the instance of a request whose
entry vanished during `create_instances` in the next flush iteration, and force-closes scopes left open by a
managed call. `OnCreate` / `OnDestroy` run inside the phase whose flush created them; the flushes of
`frame_begin`, `frame_end` and `set_mode` run no phase, so a query scope there is `NOT_IN_PHASE`. Collision
callbacks run as `FIXED_UPDATE`. `set_mode(EDIT)` / `destroy_all` end a play session: `STARTED` and `FAULTED` are
cleared so the next `PLAY` runs `OnStart` again (the started flag is for hot reload only, D8).

## 4. The component registration

`register_script_component` registers `script_component` through `component_registry::register_described` with
`component_type_desc {ops = script_component_ops(), serializer = script_component_serializer(), reflection =
nullptr}`: the copy hook deep-copies the set, destruct frees it, `writeJson` / `readJson` carry
`{"scripts":[{"class","hash","fields"}]}` and `remapEntities` rewrites `ENTITY` fields through the class schema.
The ECS consults the hooks for every archive, scene file, prefab, snapshot and `component_to_json` path, which
is how scripts cross the scene module without a dependency (R29; `tests/integration/test_scripting_scene.cpp`).
All hooks resolve the runtime through `script_runtime::active()` and do nothing when no runtime is alive.

## 5. Hot reload

`reload_game_assembly`: load new -> collect fields -> destroy -> unload -> swap registries -> recreate
(`OnCreate` again, `OnStart` not). A failed load leaves the running game untouched; a leaked load context is a
WARN (`ASSEMBLY_LEAKED` is logged and counted, not returned). `hot_reload_controller` debounces requests and
optionally polls the assembly's write time; `scripting.frame_begin` ticks it. Plugin assemblies share the game
load context (R36), so a leaky plugin prevents unloading the game assembly.

## 6. The managed side

`managed/Conjure.Managed` is the SDK (`ScriptBehaviour`, `Entity`, `Transform`, `Input`, `Time`, `Log`,
`Physics`, `Audio`, `Prefab`, `Scene`, `Dod.ComponentQuery<T>`, `[EcsComponent]`, the interop layer and the
guard), `Conjure.Bootstrap` the assembly `hostfxr` loads (it owns the runtime config), `Conjure.TestScripts` the
scripts the `[dotnet]` tests drive (built as variants V1 / V2 / V3 for the reload tests) and
`Conjure.Templates/Game` the project a new game starts from. Every project lists its compile items explicitly and
the `conjure_managed_sources_match.*` CTest entries check that the csproj and the CMake lists agree.
`ComponentType<T>.Id` is cached per handshake generation so a new runtime with other component ids re-resolves;
`ScriptRuntime.Shutdown` forgets the native table.

## 7. nethost on MSVC

The static `libnethost.lib` of the host pack is built against the static release CRT, which the engine's `/MD`
and `/MDd` objects cannot link (LNK2038). On MSVC `engine/src/scripting/CMakeLists.txt` links the `nethost.lib`
import library instead and copies `nethost.dll` next to the executables after each build of `conjure_scripting`;
other platforms link `libnethost.a` statically through `conjure::thirdparty::nethost` (`cmake/FindNetHost.cmake`).

## 8. Threading and destruction order

Everything is main-thread only: the CLR is started and every managed call is made from the thread
`core::jobs::is_main_thread()` names, and every native entry but `log_write` asserts on other threads and fails
softly. `managed_build_runner` is the one exception: the build process runs on a job and its report is posted
with `core::jobs::post_to_main_thread`. A `script_runtime` must be detached (or destroyed) before the `world` it
is attached to: the component hooks resolve the runtime through `active()` while the world frees its entities.
Declare the runtime after the world and `shutdown()` it before the world dies.

## 9. Tests

`tests/scripting/` builds `conjure_scripting_tests` (one `test_<header>.cpp` per public header plus the topic
files of `03_scripting.md` 10) and `conjure_scripting_slow_tests` (the self-contained publish and the
test-scripts build, labels `scripting;dotnet;slow`, excluded by the default presets). The null-host cases run
everywhere; the `[dotnet]` cases start the real runtime from `bin/managed` and skip through
`precondition::DOTNET_RUNTIME` when no .NET 8+ runtime is installed. `scripting_test_support.h` provides
`scripting_fixture` (null host) and `dotnet_fixture` (the CLR with `Conjure.TestScripts` loaded).

```
python tools/build.py --build-dir build/<dir> --target conjure_scripting_tests --test -R "scripting|managed"
```

## 10. Open items

- `03_scripting.md` section 3 describes `libnethost` as linked statically everywhere; on MSVC the import library
  is linked instead (section 7).
- `tests/benchmarks` (`10k_scripts_update_phase` < 2 ms) waits for the benchmarks directory.
- The C# wiki pages (the API as seen from a script) arrive with the wave-4 bindings.

## Related

- design record: [`03_scripting.md`](../../design/03_scripting.md), [`15_bindings.md`](../../design/15_bindings.md), [`09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md)
- 📘 API: [Scripting](../../api/scripting/index.md)
- 🛠 Handbook: [ECS module](ecs.md), [Build pipeline](../build-pipeline.md)
