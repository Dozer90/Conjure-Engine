# Bindings module

🛠 **Engineering handbook** · 📘 API: [Scripting from C#: the bindings](../../api/bindings/index.md), [The C# API](../../api/bindings/csharp-api.md), [The native table](../../api/bindings/native-table.md)

`conjure::bindings` (target `conjure_bindings`, alias `conjure::bindings`) is the wave-4 module that fills the
106 bindings-owned entries of the native scripting table (the groups `transform`, the hierarchy half of
`entity`, `scene`, `prefab`, `physics`, `audio`, `input`, `asset`, `diagnostics`, `modules` under the
`// --- bindings (R26) ---` marker of `native_api.inl`), wraps every slot in a counting thunk outside
Distribution, routes structural requests made inside a DOD query scope into the runtime's command buffer (R5)
and ships the C# facades and `[NativeComponent]` mirrors of `managed/Conjure.Managed`. This chapter is for
people adding an entry, a facade or a mirror; the binding design is
[`15_bindings.md`](../../design/15_bindings.md), the cross-area decisions R5, R26, R48-R51, R54-R58, R61, R62
and R64 of [`09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md), and
`engine/src/bindings/README.md` carries the finer notes.

## Contents

- [1. Shape of the module](#1-shape-of-the-module)
- [2. Filling the table](#2-filling-the-table)
- [3. The status protocol](#3-the-status-protocol)
- [4. Structural routing](#4-structural-routing)
- [5. Call counters](#5-call-counters)
- [6. The managed side](#6-the-managed-side)
- [7. Threading model](#7-threading-model)
- [8. Extension points](#8-extension-points)
- [9. Tests](#9-tests)
- [10. Open items](#10-open-items)
- [Related](#related)

## 1. Shape of the module

`engine/src/bindings/CMakeLists.txt`: `conjure_add_module(bindings ... PUBLIC_DEPS conjure::core conjure::ecs
conjure::assets conjure::scene conjure::scripting conjure::engine PRIVATE_DEPS conjure::modules conjure::platform
[physics] [audio] [renderer])`, guarded by `CONJURE_WITH_SCRIPTING`; `entries_physics.cpp` and `entries_audio.cpp`
compile only with their modules, the stubs cover a compiled-out group.

| Header | Main types | 📘 Wiki page |
|---|---|---|
| `bindings.h` | umbrella | [Scripting from C#](../../api/bindings/index.md) |
| `bindings_subsystem.h` | `scripting_bindings_subsystem`, `script_residency` | [The native table](../../api/bindings/native-table.md) |
| `native_api_entries.h` | `binding_context`, `fill_native_table`, `wrap_with_counters`, `group_available`, `group_compiled`, `entries::*`, `stubs::stub_thunk` | same |
| `call_counters.h` | `native_function_id`, `native_entry_descriptor`, `call_counters`, `counting_thunk` | same |
| `marshal.h` | the ABI record conversions, `structural_guard` | same |
| `bindings_error.h`, `bindings_error.inl` | `bindings_error`, the status protocol, the last-error buffer | same |
| `log_category.h` | `LOG_BINDINGS` | not user-facing |

Private: `entry_support.h` (the main-thread guard, entity and component resolution, the deferred `scene_fixup`
queue), `native_api_fill.cpp` (filling, wrapping, the active context, the stubs, the per-frame failure log),
`entries_*.cpp` (one file per group), `marshal.cpp`, `bindings_error.cpp`, `call_counters.cpp`,
`bindings_subsystem.cpp`.

## 2. Filling the table

`scripting_bindings_subsystem::startup` (startup step 16, after the engine's own adapters and before the
scripting subsystem's `post_startup` runs `initialize` + `load_game_assembly`, R27) builds a `binding_context`
with `binding_context::from(engine_context)` and calls `fill_native_table(api, context)`: every bindings-owned
slot receives its `entries::*` function when the group's service is present and compiled, otherwise
`stubs::stub_thunk<Id, Ret(Args...)>::invoke`, which answers `NOT_SUPPORTED` with the entry name in the
last-error buffer. `post_startup` calls `wrap_with_counters` after the runtime refilled the core slots;
`begin_frame` re-wraps when `is_scope_hook_installed(api)` turned false (a runtime re-initialised after
`shutdown()`) and applies pending fix-ups. `wrap_with_counters` is idempotent in every configuration: a slot
already holding its thunk is skipped and the `query_end` interposer is peeled off and re-installed. The runtime
(`runtime/src/main.cpp`) and the editor (`editor_application::start_engine`) add the subsystem unconditionally.

## 3. The status protocol

Every entry returns `int32_t`: `>= 0` success (0, a count, a bool or a needed length), `< 0` is
`-bindings_error`; `uint64_t` entries return 0 on failure. `fail()` formats `"<entry>: <detail>"` into the
512-byte main-thread last-error buffer without allocating; `diagnostics_last_error` hands it to managed code,
where `NativeCheck.ThrowIfFailed` maps codes to exceptions. Predicates never throw on dead entities (B11).
Hierarchy refusals (`SELF_PARENT`, `HIERARCHY_CYCLE`, `HIERARCHY_TOO_DEEP`, `INVALID_SIBLING_INDEX`) map to
`INVALID_ARGUMENT`; getters have no side effects (`entity_get_guid` reports `COMPONENT_MISSING` instead of
generating). Array entries follow the count protocol (capacity 0 = count query); the physics queries fill a
scratch the physics world writes into (exact up to 1024 results). Lookups by name hash the script's string
without interning; only `entity_set_name` and `entity_set_tag` intern. The read-only asset entries never bind a
slot (`find_slot` + `type_of`); only `asset_ref_bind` and the loads create slots, and only after every check
passed (the type check before `bind_slot`, the `script_residency` check before `asset_manager::load`), since slots
are never freed. `asset_state_of` answers `UNLOADED` for an unknown uuid where its siblings answer `NOT_FOUND`.
An unknown layer index is `NOT_FOUND` in `physics_layer_name` and `physics_layers_collide` alike (2.2: layers are
looked up); the ray and shape casts refuse a non-finite or negative `max_distance` with `INVALID_ARGUMENT`. The
velocity setters write the `rigid_body` mirror only once `physics_world` queued the command, because the push
diffs the mirror and would otherwise apply a refused request. `physics_character_move` follows the same rule for
`character_controller::velocity`, which the step feeds to Jolt whenever no move is queued.

## 4. Structural routing

While a managed query scope holds the world locked, `marshal::structural_guard` routes every ECS-expressible edit
into `script_runtime::deferred_commands()` (`set_raw` / `add_raw`, `create_entity`, prefab roots through
`instantiate`) and the scene-level part of the same operation (adopting new entities into the hierarchy,
reparenting, enabling) into the bindings-owned `scene_fixup` queue, applied right after the command buffer
flushes: by the `query_end` interposer, by the main-thread guard of the next entry and by `begin_frame`. What
cannot be deferred (`entity_duplicate`) fails with `STRUCTURE_LOCKED`.

Fix-ups apply in call order, and the getters answer the pre-flush state until then. A deferred request is
therefore never compared against the live state: `entity_set_enabled` queues every request inside a scope (a
disable then an enable ends enabled; `scene::set_enabled` ignores an unchanged state when it runs), and only a
request that is wrong whatever the tree looks like at the flush is refused at the call (a deferred self-parent is
`INVALID_ARGUMENT`; cycle and depth checks wait for the apply, since earlier fix-ups may change the tree). A fix-up
the scene refuses when it is applied is logged at WARN, because the call already returned success; one whose
entity died first is skipped at DEBUG (`15_bindings.md` 6). `apply_pending_fixups` swaps the queue with a scratch
queue instead of moving it into a local, so a steady stream of scopes stops allocating once both have grown;
re-entrancy is unchanged (a nested apply is refused, observers queue into the emptied queue).

## 5. Call counters

Outside Distribution every slot (core entries included) is `counting_thunk<Id, Ret(Args...)>::invoke`,
incrementing `call_counters()[Id]` and forwarding. `native_function_id` has one enumerator per `.inl` line,
`native_entry_descriptors()` the table in order, `is_bindings_owned(id)` the ownership split, `reset_call_counters`
clears between cases. `Diagnostics.CallCount(NativeFunction.X)` reads the same span from C#; the feature-test scene
asserts every counter is positive except the entries of `tests/binding_exclusions.json`. In Distribution only the
`query_end` interposer the fix-ups need is installed.

## 6. The managed side

`managed/Conjure.Managed`: `Interop/NativeCheck.cs` (`ThrowIfFailed`, `ReadLastError`, `BindingStatusMap`),
`Interop/ConjureNativeException.cs`, `Interop/Utf8Marshal.cs`, `Interop/AssetRefBinder.cs`, the generated
`Interop/BindingStatus.g.cs` (from `bindings_error.inl` through `tools/generate_native_api.py --status`), the facades
`Transform`, `Entity` (hierarchy half), `Rigidbody`, `CharacterController`, `Physics`, `LayerMask`, `RaycastHit`,
`AudioSource`, `Audio`, `Prefab`, `Scene`, `Input` (bindings half), `AssetRef<T>`, `AssetId`, `AssetState`, the
`IAssetType` tags, `Diagnostics`, `Modules.Registry`, and the mirrors under `Api/Components/{Scene,Physics,Audio}`
with the `NativeBool`, `StringId`, `EntityHandle` primitives. Mirrors restate the owning note's field list
(`12_audio.md` 2.6, `13_scene.md` 2.1, `14_physics.md` 2.3) and change in the same PR as the native struct; the
layout hash (`type_info::layout_hash()`, R61) is the gate. `Conjure.TestScripts/BindingProbes.cs` holds the
probes the `[dotnet]` tests drive, among them `BindingRecordSizeProbe` (every ABI record's `Unsafe.SizeOf`).
`ecs::entity` is 4-aligned, so `Parent` is 12 bytes and `CharacterControllerData` 68.

> [!WARNING]
> `Transform`, `Rigidbody` and the other facades are `readonly struct`s returned by value: `Transform.Position = p`
> on `ScriptBehaviour.Transform` is CS1612. The template `Player.cs` copies to a local first; a facade with
> reference semantics is a design question for a later pass.

## 7. Threading model

Main thread only: every entry asserts `core::jobs::is_main_thread()` and fails softly with `WRONG_THREAD`
otherwise (logged at ERROR, without touching the last-error buffer). Managed code runs inside the six `EXCLUSIVE`
scripting systems (R27), so a correct host never hits it. The thread is the only check: no entry looks at
`script_runtime::current_phase()`, because the `OnCreate` / `OnDestroy` that the flushes of `frame_begin`,
`frame_end` and `set_mode` run have no phase and must reach every entry; `NOT_IN_PHASE` stays in
`bindings_error.inl` as a reserved code (removing it would renumber the C# statuses) and is never returned
(`15_bindings.md` 3.2, sweep F166). The counters are plain `u32`. The table is owned by
`script_runtime`; the bindings only write its slots. `binding_context` pointers are non-owning views valid from
`initialise()` to `shutdown()`; `script_residency` owns the asset handles scripts request through `asset_load`
(`asset_release` drops one, `on_before_unload` drops all).

## 8. Extension points

A new entry is a line in `native_api.inl` under the bindings marker (one enumerator, one descriptor and one
C# declaration follow from the generator), an `entries::` function in the group's `entries_*.cpp`, a `fill`
case and a stub, a facade member in `Conjure.Managed`, a probe in `BindingProbes.cs` and a call site in the
feature-test scene (`08_testing.md` 7.4 item 6). A plugin cannot add entries: `native_binding_desc` fills an
existing null slot by name (R36).

## 9. Tests

`tests/bindings/` (`conjure_bindings_tests`, labels `bindings`, `dotnet` for the CLR cases): one file per public
header plus `test_native_api_entries.cpp` (the table is complete in every context, the manifest matches),
`test_call_counters.cpp`, `test_span_scope_routing.cpp`, `test_*_bindings.cpp` per group and
`test_bindings_managed.cpp`, whose `[dotnet]` cases drive `BindingProbes.cs` through the real runtime inside a
headless `engine::application` (skipped through `precondition::DOTNET_RUNTIME` without the SDK; CI installs it).
`bindings_test_support.h` provides `bare_fixture` (world, scene manager, asset manager over a memory source, the
null window, check registry, a null-host runtime and optional physics and audio behind one filled table) and
`headless_config()` / `make_app()`.

```
python tools/build.py --build-dir build/mod_bindings --test -R bindings
```

## 10. Open items

From `09_cross_area_resolutions.md` 4 item 15: `renderer::debug_draw` is not exposed through
`engine_context`, so `Diagnostics.Draw.*` is a silent success in applications; a command-buffer prefab adoption
path with expected-repair accounting would remove the bindings-owned fix-up queue; `script_component` has no
reflection description, so the `ScriptComponentHandle` mirror cannot resolve. `entity_duplicate` inside a query
scope answers `STRUCTURE_LOCKED` because `entity_archive::capture` refuses a locked world; a deferred form
(reserve the copy at once, copy the subtree onto it when the scope ends) is planned with the prefab guid fix
(`15_bindings.md` 11, sweep F165).

## Related

- design record: [`15_bindings.md`](../../design/15_bindings.md), [`03_scripting.md`](../../design/03_scripting.md), [`09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md) (R5, R26, R48-R51, R54-R58, R61, R64)
- README: `engine/src/bindings/README.md`
- 📘 API: [Scripting from C#: the bindings](../../api/bindings/index.md)
- 🛠 Handbook: [Scripting module](scripting.md), [Engine module](engine.md), [Testing](../testing.md)
