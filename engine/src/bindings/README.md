# bindings

The managed-facing surface of the engine: the 106 bindings-owned entries of the native scripting table
(transform, entity hierarchy, scene, prefab, physics, audio, input, asset, diagnostics and modules groups of
`native_api.inl`), the counting thunks that wrap every slot outside Distribution, the marshalling rules
between the C ABI records and the engine's types, the status and last-error protocol, and the engine
subsystem that fills the table before `script_runtime::initialize` runs the handshake. The C# facades and
the `[NativeComponent]` mirrors in `managed/Conjure.Managed` are the other half of the module. Static
library `conjure::bindings`, namespace `conjure::bindings`, public dependencies `conjure::core`,
`conjure::ecs`, `conjure::assets`, `conjure::scene`, `conjure::scripting` and `conjure::engine`; `physics`,
`audio` and `renderer` compile in under their `CONJURE_WITH_*` options and the stubs cover a compiled-out
group. Design: `docs/design/15_bindings.md`; ARCHITECTURE 4.6 and 6;
`docs/design/09_cross_area_resolutions.md` R5, R26, R48-R50, R55-R57, R61, R64.

## Public headers (`conjure/bindings/`)

| Header | Main types |
|---|---|
| `bindings.h` | umbrella include |
| `log_category.h` | `LOG_BINDINGS` |
| `bindings_error.inl` | `CONJURE_BINDINGS_FOR_EACH_STATUS(X)`: the status list, read by `tools/generate_native_api.py --status` |
| `bindings_error.h` | `bindings_error` (16 codes), `BINDINGS_ERROR_COUNT`, `LAST_ERROR_CAPACITY`, `bindings_error_name`, `to_status` / `from_status`, `fail`, `map_error`, `last_error_code` / `last_error_message` / `clear_last_error` |
| `marshal.h` | the layout checks of the ABI records, `to_entity` / `from_entity`, `to_uuid` / `from_uuid`, the `vec2/3/4`, `quat`, `mat4` and `transform` conversions, `read_utf8`, `write_utf8`, `write_entities`, `rgba_to_color`, `structural_guard` |
| `call_counters.h` | `native_function_id` (one enumerator per `.inl` line), `NATIVE_FUNCTION_COUNT`, `native_entry_descriptor`, `native_entry_descriptors()`, `function_name`, `find_function`, `is_bindings_owned`, `call_counters()`, `call_count`, `reset_call_counters`, `counting_thunk<Id, Ret(Args...)>` |
| `native_api_entries.h` | `CONJURE_BINDINGS_FOR_EACH_ENTRY`, `CONJURE_BINDINGS_OWNS_GROUP`, `CONJURE_BINDINGS_GROUP_COMPILED`, `entries::*` (one function per owned entry), `stubs::stub_thunk`, `binding_context`, `group_available` / `group_compiled`, `fill_native_table`, `wrap_with_counters`, `is_scope_hook_installed`, `active_context`, `is_stub_installed` / `is_implementation_installed` / `is_counting_installed`, `detail::fail_entry` |
| `bindings_subsystem.h` | `script_residency`, `scripting_bindings_subsystem` |

Private files: `entry_support.h` (the main-thread guard, entity and component resolution, the deferred
`scene_fixup` queue), `native_api_fill.cpp` (filling, wrapping, the active context, the stubs, the
per-frame failure log and the fixups), `entries_*.cpp` (one file per group), `marshal.cpp`,
`bindings_error.cpp`, `call_counters.cpp`, `bindings_subsystem.cpp`, `log_category.cpp`.

The managed side (`managed/Conjure.Managed`): `Interop/NativeCheck.cs` (`ThrowIfFailed`, `ReadLastError`,
`BindingStatusMap`), `Interop/ConjureNativeException.cs`, `Interop/Utf8Marshal.cs`,
`Interop/AssetRefBinder.cs`, the generated `Interop/BindingStatus.g.cs`, the facades `Transform`,
`Rigidbody`, `CharacterController`, `Physics`, `LayerMask`, `RaycastHit`, `AudioSource`, `Audio`, `Prefab`,
`Scene`, `Input` (the `Input.Bindings.cs` half), `Entity` (the `Entity.Hierarchy.cs` half),
`AssetRef<T>`, `AssetId`, `AssetState`, the `IAssetType` tags, `Diagnostics` and `Modules.Registry`, and the
mirrors under `Api/Components/{Scene,Physics,Audio}` with the `NativeBool`, `StringId` and `EntityHandle`
primitives. `Conjure.TestScripts/BindingProbes.cs` holds the scripts the `[dotnet]` tests drive, among them
`BindingRecordSizeProbe`, which reports `Unsafe.SizeOf` of every 2.4 record and of `AssetRef<T>` so the native
`static_assert`s and the C# layouts are compared on every run (the `Conjure.Managed.Tests` cases of design 10
live there, as this repository has no managed test project). `AssetRefBinder<T>` passes each `AssetRef<T>`
field's `AssetTypeId<T>` to `asset_ref_bind`, so a field holding an id of another asset type stays unbound
(logged once per field) instead of landing bound.

## Using it

The subsystem is added to the application after the engine's own subsystems and before the scripting
subsystem initialises the runtime (`15_bindings.md` 8): `runtime/main.cpp` adds it with
`application.add_subsystem(...)` before `initialise()`, `editor/main.cpp` is meant to do the same (an open
request to the editor, see below) and `engine::application` does **not** add it itself. A reader who omits
`add_subsystem` gets a table of null bindings slots at the handshake. A test or tool wires it by hand:

```cpp
engine::application_desc desc = {};
desc.config = engine::engine_config::headless_defaults();
desc.config.scripting.enabled = true;
engine::application app(std::move(desc));
app.add_subsystem(std::make_unique<bindings::scripting_bindings_subsystem>());
CONJURE_TRY(app.initialise());          // startup fills the table; the handshake sees every slot non-null
```

Without an application, a table is filled from a `binding_context` directly:

```cpp
bindings::binding_context context = {};
context.world = &world;
context.components = &registry;
context.scenes = &scenes;              // physics, audio, assets, input, window, checks, stats: as available
conjure_native_api api = scripting::make_native_api();
CONJURE_TRY(bindings::fill_native_table(api, context));
bindings::wrap_with_counters(api);     // counting thunks outside Distribution; a no-op twice
```

`wrap_with_counters` is idempotent in every configuration: a slot already holding its thunk is skipped and
the `query_end` hook it installs is peeled off and re-installed over whatever it covered, so wrapping an
initialised table twice (a second startup, a fixture, a tool) never chains the hook behind itself.
`is_scope_hook_installed(api)` tells whether the hook is in place; the subsystem's `begin_frame` checks it
and re-wraps after `script_runtime::initialize` refilled the core slots (a runtime re-initialised after
`shutdown()`).

Open requests: `editor/main.cpp` does not add the subsystem yet (the editor executable gets null bindings
slots at the handshake until it does).

Every entry follows the status convention of `15_bindings.md` B4: an `int32_t` result `>= 0` is a success
(0, a count, a bool or a needed length) and `< 0` is `-bindings_error`; `uint64_t` entries return 0 on
failure. Every failure goes through `fail()`, which formats `"<entry>: <detail>"` into the main thread's
512-byte last-error buffer without allocating; `diagnostics_last_error` hands it to managed code, where
`NativeCheck.ThrowIfFailed` maps the code to an exception (`EntityNotAliveException`,
`AssetNotLoadedException`, `NotSupportedException`, `InvalidOperationException`, `ArgumentException`,
`KeyNotFoundException`, else `ConjureNativeException`). Predicates (`IsAlive`, `IsPlaying`, `IsSleeping`,
`GetChildren`, `FindBy*`) never throw on dead entities (B11). A hierarchy request refused for its arguments
(`SELF_PARENT`, `HIERARCHY_CYCLE`, `HIERARCHY_TOO_DEEP`, `INVALID_SIBLING_INDEX`) maps to `INVALID_ARGUMENT`.
Getters have no side effects: `entity_get_guid` reports `COMPONENT_MISSING` for an entity without a
`scene.guid` (C# `Entity.Guid` reads `AssetId.Null`) instead of generating one; guids come from
`scene::create_entity`, the serializer and `scene::ensure_guid`.

Array entries follow the count protocol of 2.1 (`capacity == 0` is a count query, the full count comes back).
The physics queries (`raycast_all`, `overlap_sphere`, `overlap_box`) fill a scratch the physics world writes
into: a count query is exact up to 1024 results and a larger capacity grows the scratch to that capacity, so
a caller's buffer is always filled completely. Lookups by name (`entity_find_by_tag`, `modules_find`,
`physics_layer_index`) hash the script's string without interning it; only `entity_set_name` and
`entity_set_tag` intern, where tools must see the text.

The read-only asset entries (`asset_ref_get`, `asset_type_of`, `asset_state_of`, the prefab and audio clip
resolution) never bind: they look the slot up with `asset_manager::find_slot` and ask the sources through
`asset_manager::type_of`, so a uuid a script merely polls costs no permanent slot (05: slots are never
freed); an unknown uuid is `NOT_FOUND`. Only `asset_ref_bind` and the loads create slots.

A group whose service is missing from the context (physics disabled, no window, no module registry) or
compiled out gets `stubs::stub_thunk` in every slot: `NOT_SUPPORTED` with the entry name in the last-error
buffer, so the table is never incomplete and the managed `NotSupportedException` names the entry. The
diagnostics group is always real: checks, the last-error read and the counters need no service, and
drawing without a `renderer::debug_draw` is a silent success.

## Structural routing (R5)

While a managed query scope holds the world locked, `marshal::structural_guard` routes every ECS-expressible
edit into `script_runtime::deferred_commands()` (component writes through `set_raw` / `add_raw`, new
entities through `create_entity`, prefab roots through `instantiate`), and the scene-level part of the same
operation (adopting the new entities into the hierarchy, reparenting, enabling) goes into a bindings-owned
`scene_fixup` queue. The queue is applied right after the command buffer flushes: by the `query_end`
interposer `wrap_with_counters` installs, by the main-thread guard of the next entry and by `begin_frame`.
What cannot be deferred (`entity_duplicate`, which must capture a subtree from an unlocked world) fails
with `STRUCTURE_LOCKED`.

## Threading and ownership

Main thread only: every entry asserts `core::jobs::is_main_thread()` and fails softly with `WRONG_THREAD`
otherwise (logged at ERROR; the wrong-thread path never touches the last-error buffer or the per-frame log
bits, which only the main thread owns); the managed SDK throws before reaching native in the normal case.
The counters are plain `u32` for the same reason. The table is owned by `script_runtime`; the bindings only write its slots. The
`binding_context` pointers are non-owning views of the engine services, valid from
`application::initialise()` to `shutdown()`. `script_residency` owns the asset handles scripts request
through `asset_load`; `asset_release` drops one, `scene_manager::on_before_unload` drops all.

## Call counters

Outside Distribution every slot (core entries included, after the scripting subsystem's `post_startup`
re-wrap) is a `counting_thunk<Id, Ret(Args...)>::invoke` that increments `call_counters()[Id]` and forwards
to the real function or the stub. `Diagnostics.CallCount(NativeFunction.X)` reads the same span from C#;
`reset_call_counters()` clears it between test cases. In Distribution `wrap_with_counters` installs only
the `query_end` interposer the fixups need.

## How to test

```
python tools/build.py --build-dir build/<dir> --test -R bindings
```

`tests/bindings/` has one file per public header plus the topic files of `15_bindings.md` 10:
`test_native_api_entries.cpp` (the table is complete in every context, the manifest matches),
`test_call_counters.cpp`, `test_span_scope_routing.cpp`, `test_*_bindings.cpp` per group and
`test_bindings_managed.cpp`, whose `[dotnet]` cases drive `Conjure.TestScripts/BindingProbes.cs` through
the real runtime inside a headless `engine::application`. `bindings_test_support.h` provides
`bare_fixture` (a world with the scene, physics and audio components, an asset manager over a memory source,
a scene manager, the null window, the check registry, a null-host script runtime and, on request, a physics
world and a null-backend audio engine, all behind one filled table) and `headless_config()` / `make_app()`
for the application cases. Everything is headless and deterministic; the `[dotnet]` cases are skipped
through `precondition::DOTNET_RUNTIME` when no .NET runtime is installed.
