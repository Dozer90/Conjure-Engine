# Hosting, the native API and hot reload

📘 **Public API wiki** · 🛠 Handbook: [Scripting module](../../handbook/modules/scripting.md)

How the .NET runtime is started (or faked), what the managed directory must contain, what the two ABI tables
are and how their hash protects you, how a game assembly is reloaded while playing, and how the engine builds
it. Headers: `conjure/scripting/script_host.h`, `conjure/scripting/dotnet_script_host.h`,
`conjure/scripting/null_script_host.h`, `conjure/scripting/native_api.h`, `conjure/scripting/native_api_hash.h`,
`conjure/scripting/hot_reload.h` and `conjure/scripting/managed_build.h`. Everything is in `namespace
conjure::scripting`; the C ABI records are global `conjure_*` structs.

## At a glance

| You want to... | Use |
|---|---|
| Make a host | `create_script_host(script_backend::DOTNET)` or `NULL_HOST` |
| Know whether a .NET runtime exists without starting one | `is_runtime_available()` |
| Check a managed directory before initialising | `validate_managed_directory(dir)`, `detect_host_mode(dir)` |
| Fake the managed side in a test | `null_script_host`: `add_fake_class`, `set_create_hook`, `set_phase_hook`, `recorder()` |
| Reload the game assembly when it changes | `hot_reload_controller(runtime, assemblyPath)`: `request()`, `tick(now)`; or `runtime.reload_game_assembly(path)` directly |
| Build the C# project from the engine | `managed_build_runner`: `build(request, onComplete)` or `run_blocking(request, dotnet)` |
| Read the native API's identity | `CONJURE_NATIVE_API_HASH`, `native_entry_descriptors()`, `find_native_entry(name)` |

## Quick example

```cpp
#include "conjure/scripting/hot_reload.h"
#include "conjure/scripting/managed_build.h"

using namespace conjure;

void rebuild_and_reload(scripting::managed_build_runner &builder, scripting::hot_reload_controller &reload,
  const std::filesystem::path &project) {
    builder.build(project, "Debug", [&reload](const scripting::build_report &report) {
        if (!report.succeeded) {
            for (const scripting::build_diagnostic &diagnostic : report.diagnostics) {
                CONJURE_LOG_ERROR(LOG_SCRIPTING, "{}({}): {} {}", diagnostic.file, diagnostic.line,
                  diagnostic.code, diagnostic.message);
            }
            return;
        }
        reload.request();                      // coalesced; the next frame's tick performs it
    });
}
```

The engine drains `core::jobs::post_to_main_thread` before every frame, which is where `onComplete` runs, and the
`scripting.frame_begin` system ticks the controller.

## Hosts

`script_host` is the interface: `initialize(config, nativeApi)` starts or attaches the runtime and performs the
ABI handshake, `shutdown()` stops managed execution for this host, `managed_api()` is the table the managed side
filled, `status()` is `UNINITIALIZED`, `READY` or `DISABLED` (initialise failed; everything then is a no-op).
`script_host_config` names the `backend`, the `managedDirectory`, an optional `dotnetRootOverride` and
`waitForDebugger`.

`dotnet_script_host` runs the real CLR once per process: it locates `hostfxr` through `nethost`, initialises it
for `Conjure.Bootstrap.runtimeconfig.json` (`host_mode::FRAMEWORK_DEPENDENT`) or the self-contained command
line (`SELF_CONTAINED`), loads `Conjure.Bootstrap` and calls its `Initialize` export, which fills the managed
table. Failures are `RUNTIME_NOT_FOUND`, `HOSTFXR_INCOMPATIBLE`, `RUNTIME_CONFIG_INVALID`, `MANAGED_SDK_MISSING`,
`HOST_ALREADY_RUNNING` (a runtime of another mode already lives in the process) and `ABI_MISMATCH`.
`is_runtime_started()` says whether a CLR is resident.

`null_script_host` is the C++ fake every scripting, scene, engine and editor test uses: slots come from a free
list, `apply_fields` stores the blob per slot and `collect_fields` echoes it, `run_phase` and
`dispatch_collisions` are logged, and `load_game_assembly` registers the fake classes you added
(`add_fake_class(info)`, or a `<assembly>.classes.json` fixture parsed with `parse_classes_fixture`). Hooks
stand in for script code: `set_create_hook`, `set_destroy_hook`, `set_phase_hook`, `set_collision_hook` run inside
the managed call and may call back into the runtime like a script would. `recorder()` lists every call, create,
destroy, phase and collision it saw; `live_slot_count()`, `slot_fields(slot)`, `slot_phase_count(slot, phase)`
inspect one slot.

> [!IMPORTANT]
> The managed directory (`script_host_config::managedDirectory`, by default `managed/` next to the executable)
> must hold `Conjure.Bootstrap.dll`, its `runtimeconfig.json` and `Conjure.Managed.dll`; the build lays them out
> there (`conjure_managed_sdk`). `validate_managed_directory` names the first missing file.

## The native API

`conjure_native_api` is a table of function pointers the engine fills and the managed SDK calls; `conjure_managed_api`
is the table the managed side fills (`load_game_assembly`, `create_instances`, `destroy_instances`, `run_phase`,
`dispatch_collisions`, `apply_fields`, `collect_fields`, `collect_garbage`, `shutdown`). Both carry a size, a
version and a layout hash that the handshake compares on both sides, so a stale `Conjure.Managed.dll` fails with
`ABI_MISMATCH` instead of calling the wrong function. The entry list is the X-macro table `native_api.inl`
(20 core entries the scripting module implements, then the bindings' groups); `native_api_hash.h` is generated
from it (`CONJURE_NATIVE_API_HASH`, `CONJURE_NATIVE_API_ENTRY_COUNT`, `CONJURE_NATIVE_API_CORE_ENTRY_COUNT`) and
`tools/generate_native_api.py` regenerates it together with the C# `NativeApi.g.cs` and the manifest under
`docs/generated/`. `native_entry_descriptors()`, `find_native_entry(fullName)`, `validate_native_api(api)` and
`validate_managed_api(api)` let tools check a table.

The records that cross the boundary are plain C structs with snake_case fields: `conjure_vec3`,
`conjure_quat_xyzw`, `conjure_transform`, `conjure_uuid`, the `conjure_frame_block` (time and the input snapshot
the engine writes before each phase; 64-byte aligned), `conjure_create_request`, `conjure_collision_record`,
`conjure_script_class_desc` and `conjure_script_field_desc` (what a loading assembly registers), the DOD
`conjure_chunk_view` / `conjure_query_scope`, and the query, asset and module records of the bindings.

## Hot reload

`hot_reload_controller(runtime, assemblyPath)` coalesces `request()`s inside a debounce window
(`set_debounce_seconds`, 0.2 s by default), reloads on the next `tick(nowSeconds)` (`request(true)` bypasses the
window) and can poll the assembly file's write time itself (`set_watch_interval_seconds`). `state()` is `IDLE`,
`PENDING` or `RELOADING`; `reload_count()`, `request_count()` and `last_result()` report what happened.

The reload itself (`script_runtime::reload_game_assembly`) loads the new assembly into a fresh collectible
context, collects every live instance's fields, destroys the instances, unloads the old context, swaps the class
registries and recreates the instances with their fields applied (`OnCreate` runs again, `OnStart` does not).
A failed load leaves the running game untouched; a context that survives unloading is logged as
`ASSEMBLY_LEAKED` and counted in `stats().zombieAssemblies`. A class that disappeared leaves its entries with
`CLASS_MISSING` and their JSON intact, so adding the class back revives them.

## Building managed code

`managed_build_runner(jobs)` runs `dotnet build <project> -c <configuration>` on a job (or inline without a job
system) and posts the `build_report` to the main-thread queue: `succeeded`, `timedOut`, `exitCode`, `diagnostics`
(`build_diagnostic {file, line, column, isError, code, message}` parsed from MSBuild's output), `output`,
`elapsedSeconds` and `launch` (the failure when `dotnet` could not be started). `build_request` adds an output
directory, `-p:Name=Value` properties and a timeout; `is_building()`, `wait()` and `set_dotnet_executable` round
it out, and `run_blocking(request, dotnet)` is the synchronous form tools use.

## API summary

| Signature (short) | Purpose |
|---|---|
| `unique_ptr<script_host> create_script_host(script_backend)` | a host |
| `bool is_runtime_available()`, `host_capabilities query_host_capabilities()` | probes |
| `result<host_mode> detect_host_mode(const path &)`, `result<void> validate_managed_directory(const path &)` | directory checks |
| `result<void> script_host::initialize(const script_host_config &, conjure_native_api &)`, `void shutdown()`, `const conjure_managed_api &managed_api() const`, `script_host_status status() const`, `script_backend backend() const` | the interface |
| `host_mode dotnet_script_host::mode() const`, `const path &managed_directory() const`, `static bool is_runtime_started()` | the CLR host |
| `void null_script_host::add_fake_class(script_class_info)`, `set_create_hook(...)`, `set_destroy_hook(...)`, `set_phase_hook(...)`, `set_collision_hook(...)`, `const null_host_recorder &recorder() const`, `static null_script_host *active()` | the fake |
| `hot_reload_controller(script_runtime &, path)`, `void request(bool immediate = false)`, `void tick(f64 nowSeconds)`, `reload_state state() const` | reloads |
| `managed_build_runner(core::jobs::job_system * = nullptr)`, `void build(const path &project, string_view configuration, function<void(const build_report &)>)`, `void build(const build_request &, ...)`, `static build_report run_blocking(const build_request &, string_view dotnet)` | builds |
| `span<const native_entry_descriptor> native_entry_descriptors()`, `u64 compute_native_api_hash()`, `i32 find_native_entry(string_view)`, `result<void> validate_native_api(const conjure_native_api &)`, `validate_managed_api(const conjure_managed_api &)` | `native_api.h` |

## See also

- 📘 API: [Scripts, components and the runtime](scripts-and-components.md), [Jobs and threading](../core/jobs.md), [Processes, libraries and images](../core/system.md)
- 🛠 Handbook: [Scripting module](../../handbook/modules/scripting.md)
