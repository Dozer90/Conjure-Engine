# The native table

📘 **Public API wiki** · 🛠 Handbook: [Bindings module](../../handbook/modules/bindings.md)

The native half of the bindings: how the 106 bindings-owned entries of the scripting table are filled, counted
and marshalled. Headers: `conjure/bindings/bindings.h` (umbrella), `conjure/bindings/bindings_subsystem.h`,
`conjure/bindings/native_api_entries.h`, `conjure/bindings/call_counters.h`, `conjure/bindings/marshal.h` and
`conjure/bindings/bindings_error.h` with its `conjure/bindings/bindings_error.inl` status list. A game never
includes these; executables, plugins and tests do.

## At a glance

| You want to... | Use |
|---|---|
| Give an application a complete table before the game assembly loads | `app.add_subsystem(std::make_unique<scripting_bindings_subsystem>())` before `initialise()` |
| Fill a table by hand (a tool, a test without an application) | `binding_context` + `fill_native_table(api, context)`, then `wrap_with_counters(api)` |
| Know whether a group is live | `group_available("physics", context)`, `group_compiled("audio")` |
| Count calls per entry (tests, the feature-test scene) | `call_counters()`, `call_count(id)`, `reset_call_counters()`, `native_entry_descriptors()` |
| Read the last failure a script hit | `last_error_code()`, `last_error_message()`, `clear_last_error()` |
| Convert between ABI records and engine types | `marshal.h`: `to_entity` / `from_entity`, `to_uuid` / `from_uuid`, `to_quat` / `from_quat`, `to_transform` / `from_transform`, `read_utf8`, `write_utf8` |

## The subsystem

`scripting_bindings_subsystem` is an `engine_subsystem` the runtime and the editor add before `initialise()`.
Its `startup` (step 16) builds a `binding_context` from the `engine_context` (world, components, scenes, physics,
audio, assets, input, window, checks, stats, debug draw) and fills every bindings-owned slot of the runtime's
table; `post_startup` wraps the table in counting thunks (outside Distribution); `begin_frame` re-wraps after a
runtime that was re-initialised and applies the deferred scene fix-ups; `shutdown` releases the assets scripts
asked for (`script_residency`). Without the subsystem the handshake sees null slots and the runtime refuses to
start, so `conjure_runtime` adds it unconditionally when the module is built.

```cpp
#include "conjure/bindings/bindings_subsystem.h"
#include "conjure/engine/application.h"

engine::application_desc desc = {};
desc.config = engine::engine_config::headless_defaults();
desc.config.scripting.enabled = true;
desc.config.scripting.useNullHost = true;     // the C++ fake of the managed side: no .NET needed
engine::application app(std::move(desc));
app.add_subsystem(std::make_unique<bindings::scripting_bindings_subsystem>());
CONJURE_TRY(app.initialise());                // every slot is non-null at the handshake
```

## Filling a table by hand

`binding_context` is a struct of non-owning pointers (`world`, `components`, `scenes`, `physics`, `audio`,
`assets`, `assetDatabase`, `input`, `window`, `renderer`, `debugDraw`, `checks`, `modules`, `scripting`, `stats`,
`time`); `binding_context::from(engine_context)` fills it from an application. `fill_native_table(api,
context)` writes every bindings-owned slot of a `conjure_native_api`: the real entry when the group's services
are present and compiled, `stubs::stub_thunk` otherwise, so the table is always complete and a call into a
missing group answers `NOT_SUPPORTED` naming the entry. `wrap_with_counters(api)` is idempotent and installs the
`query_end` interposer the deferred scene fix-ups need; `is_scope_hook_installed(api)` tells whether it did.

> [!IMPORTANT]
> Fill the table before `script_runtime::initialize`: the handshake checks that every slot is non-null and the
> managed side caches the pointers. Re-filling later only takes effect after a runtime re-initialisation.

## The status protocol

Every entry returns `int32_t`: `>= 0` is success (0, a count, a bool or a needed buffer length) and `< 0` is
`-bindings_error`; entries that return `uint64_t` return 0 on failure. The codes (`bindings_error.inl`, read by
the C# generator): `NOT_SUPPORTED`, `ENTITY_NOT_ALIVE`, `COMPONENT_MISSING`, `INVALID_ARGUMENT`, `STRUCTURE_LOCKED`,
`WRONG_THREAD` and the rest of the 16 (`bindings_error_name()` spells each). `fail()`
formats `"<entry>: <detail>"` into a 512-byte main-thread buffer without allocating; `diagnostics_last_error`
hands it to C#, where `NativeCheck.ThrowIfFailed` maps codes to exceptions. Array entries follow the count
protocol: a capacity of 0 asks for the count, a larger capacity fills the caller's buffer completely.

> [!WARNING]
> Every entry asserts the main thread and fails softly with `WRONG_THREAD` elsewhere. Managed code runs inside
> the `EXCLUSIVE` scripting systems, so a correct host never hits it; a tool calling entries from a job does.

## Call counters

Outside Distribution each slot is a `counting_thunk<Id, Ret(Args...)>::invoke` that increments
`call_counters()[Id]` and forwards. `native_function_id` has one enumerator per line of the native table
(`NATIVE_FUNCTION_COUNT` of them), `native_entry_descriptors()` lists `{ id, group, name, signature, ownedByBindings }`,
`function_name(id)` and `find_function("transform_set_local")` translate, `is_bindings_owned(id)` separates the
ten groups from scripting's core entries. The feature-test scene asserts every counter is positive except the
entries listed with a reason in `tests/binding_exclusions.json`; `Diagnostics.CallCount` reads the same span
from C#.

```cpp
bindings::reset_call_counters();
CONJURE_TRY(app.step_frame());
const bindings::native_function_id id = bindings::find_function("transform_set_local");
CHECK(bindings::call_count(id) > 0);
```

## Marshalling

`marshal.h` holds the layout contract between the C ABI records of `conjure/scripting/native_api.h` and the
engine's types, checked with `static_assert`s: `to_entity(uint64_t)` / `from_entity(entity)` (index and
generation packed), `to_uuid` / `from_uuid`, the `vec2` / `vec3` / `vec4` / `quat` / `mat4` / `transform`
conversions (`quat_storage_is_xyzw()` pins the x, y, z, w storage the mirrors rely on), `read_utf8` /
`write_utf8` for strings with the count protocol, `write_entities` for entity arrays, `rgba_to_color` and
`structural_guard`, which decides whether a structural request is applied now or deferred into the runtime's
command buffer because a query scope holds the world locked.

## API summary

| Signature (short) | Purpose |
|---|---|
| `class scripting_bindings_subsystem final : engine_subsystem` | the one subsystem to add; `is_filled()` |
| `class script_residency { holds(uuid), clear(), size() }` | the asset handles scripts hold through `asset_load` |
| `struct binding_context { ...; static binding_context from(const engine_context &) }` | the services the entries reach |
| `result<void> fill_native_table(conjure_native_api &, const binding_context &)` | fill every bindings-owned slot |
| `void wrap_with_counters(conjure_native_api &)` / `bool is_scope_hook_installed(const conjure_native_api &)` | counting thunks and the `query_end` interposer |
| `bool group_available(string_view, const binding_context &)` / `bool group_compiled(string_view)` | per-group availability |
| `span<u32> call_counters()` / `u32 call_count(native_function_id)` / `void reset_call_counters()` | the counters |
| `span<const native_entry_descriptor> native_entry_descriptors()` / `function_name(id)` / `find_function(name)` | the entry list |
| `bindings_error last_error_code()` / `string_view last_error_message()` / `void clear_last_error()` | the status protocol |
| `bindings_error map_error(const core::error &)` / `string_view bindings_error_name(bindings_error)` | engine errors to statuses |

## See also

- 📘 API: [The C# API](csharp-api.md), [Hosting, the native API and hot reload](../scripting/hosting-and-hot-reload.md)
- 📘 API: [The frame, layers and time](../engine/frame-layers-and-time.md) (subsystems)
- 🛠 Handbook: [Bindings module](../../handbook/modules/bindings.md)
