# Design C - C# Scripting Host (`conjure::scripting` + `Conjure.Managed`)

Status: **final** (judged design; binding for implementers). Module `conjure_scripting` (wave 3), namespace
`conjure::scripting`. The native *content* of the binding tables (entities, transform, physics, audio, prefab,
scene) is implemented by `conjure_bindings` (wave 4); this document defines the mechanism both share, the
managed SDK, and everything an implementer of either module must agree on.

Backbone: the performance-first proposal (4-byte `script_component`, shared frame block, X-macro ABI table,
binary field blob, C++ fake of the managed side for tests). Grafted from the robustness-first proposal: layout
hash in the handshake, explicit `quat_xyzw` ABI type, "UnmanagedCallersOnly only in Conjure.Managed", edit-mode
vs play-mode ownership of field values, `ref struct` query scope, enum/asset metadata for the inspector,
entity-reference remapping, wrong-thread detection on both sides, missing-class preservation, fault badges.

Third-party facts verified against the .NET hosting docs (`native-hosting.md`, `hostfxr.h`,
`coreclr_delegates.h`, `nethost.h`, unloadability guide) on 2026-11-06:

- `hostfxr_initialize_for_runtime_config` supports framework-dependent configs only ("self-contained
  components are not supported"). `hostfxr_initialize_for_dotnet_command_line` supports both, may be called
  once per process, and its handle supports only `hdt_load_assembly_and_get_function_pointer` and
  `hdt_get_function_pointer`.
- `load_assembly_and_get_function_pointer` **always loads the component into an isolated
  AssemblyLoadContext**. `hdt_load_assembly` (.NET 8+) with `load_context == NULL` loads into the **default**
  ALC and processes the assembly's `.deps.json`; `hdt_get_function_pointer` with `NULL` resolves in the
  default ALC. Only one runtime per process; it can never be unloaded; delegates live for the process.
- `hostfxr_set_error_writer` is **per thread**. `hostfxr_set_runtime_property_value` works only on the first
  context before the runtime loads. `Success_HostAlreadyInitialized` and `Success_DifferentRuntimeProperties`
  are successes. `get_hostfxr_path` returns `0x80008098` when the buffer is too small; `assembly_path` makes
  nethost search as if that assembly were an apphost (finds an app-local hostfxr).
- `[UnmanagedCallersOnly]` methods must be static, non-generic, blittable-only, never called from managed
  code. Collectible ALC unload is cooperative; completion is detected with a `WeakReference` + `GC.Collect`
  loop; statics, GC handles, running threads and fields on the ALC subclass keep it alive.

---

## 1. Overview and decisions

| # | Decision | Rationale |
|---|---|---|
| D1 | **One CLR per process, started once** by a process-wide `hostfxr_runtime` singleton (private). `dotnet_script_host` objects and `script_runtime` objects may be created and destroyed freely on top of it; the managed `Initialize` handshake is idempotent. | CoreCLR cannot be unloaded or restarted. Making that a type invariant lets one test executable run hundreds of cases that each build and tear down a runtime. |
| D2 | **Two hosting paths, one handshake.** Framework-dependent (editor, default export): `initialize_for_runtime_config` -> `hdt_load_assembly(Conjure.Managed.dll, default ALC)` -> `hdt_get_function_pointer`. Self-contained export: `initialize_for_dotnet_command_line({Conjure.Bootstrap.dll})` -> `hdt_get_function_pointer`. Never `load_assembly_and_get_function_pointer`. | The runtime-config path cannot host self-contained layouts, and `load_assembly_and_get_function_pointer` would put `Conjure.Managed` in an isolated ALC, breaking `ScriptBehaviour` type identity for the game ALC. `Conjure.Managed` must live in the default ALC. |
| D3 | **All traffic after the handshake goes through two C-ABI structs of function pointers**: `conjure_native_api` (native provides) and `conjure_managed_api` (managed provides). The native table is declared once in `native_api.inl` (X-macro); `tools/gen_native_api.py` generates `NativeApi.g.cs` and a 64-bit layout hash baked into both sides. Generated files are committed; a CTest check regenerates and diffs. | `delegate* unmanaged` calls are cheap and allocation-free. One source of truth plus size + version + hash validation turns drift into a clean `ABI_MISMATCH` at startup instead of a crash later. No Python needed to build. |
| D4 | **Per-frame hot data is shared memory, not calls**: a native-owned, 64-byte-aligned `conjure_frame_block` (time, frame index, full input snapshot) is written once per frame and read directly by `Time`/`Input` in C#. | `Input.IsKeyDown` in 10k scripts would otherwise cost 10k transitions per key per frame. |
| D5 | **One native->managed call per phase.** Native calls `run_phase(phase, dt)`; managed iterates dense per-class arrays of live instances. Creates, destroys and collision records are each one batched call per phase. | Transition count is O(phases), not O(entities). Per-class loops keep the virtual call site monomorphic and the instruction cache warm. |
| D6 | **`script_component` is 4 bytes and trivially copyable**: a handle into `script_store` (a `core::slot_map<script_set>`) owned by `script_runtime`. A set holds up to `MAX_SCRIPTS_PER_ENTITY = 8` entries (class hash, managed slot, serialised JSON fields, flags). | Archetype moves stay `memcpy`; chunk layout is not polluted by strings; multiple scripts per entity as in Unity. |
| D7 | **Native owns serialised state (JSON in `script_entry`); managed owns the live object.** In **edit mode** JSON is authoritative and no instances exist; in **play mode** the live instance is authoritative and JSON is a snapshot refreshed on reload and on request. A binary `field_blob` is the single wire format for `apply_fields`/`collect_fields`. | One representation for scene files, prefabs, inspector, undo and reload. The blob is allocation-free and avoids `System.Text.Json` metadata caches, which root game-assembly types and silently defeat ALC unloading. |
| D8 | **Hot reload = load new ALC first, then collect -> destroy -> unload -> recreate.** If the new assembly fails to load, nothing changes. `OnCreate` runs again, `OnStart` does not (started flag preserved), `OnDestroy` is not called. Unload is bounded (10 GC passes); a leak is logged and tolerated. | A reload must never kill a play session or hang the editor; survivors are exactly the serialised fields. |
| D9 | **`[UnmanagedCallersOnly]` entry points exist only in `Conjure.Managed`** (default ALC). Native never stores a pointer, `GCHandle` or function pointer that targets the game assembly; it refers to instances only by integer slot. | Confines every native-visible pointer to the non-collectible SDK so unloadability is enforceable by review. |
| D10 | **All managed execution is on the main thread** inside exclusive scheduler systems. Wrong-thread calls are rejected on both sides (`WRONG_THREAD` natively, `InvalidOperationException` in C#). No `Parallel.For` in the SDK. | User scripts are not thread-safe; the 10k budget is met by batching and the DOD span path, not by threads. |
| D11 | **Exceptions never cross the boundary.** Every `[UnmanagedCallersOnly]` entry is wrapped; the exception is logged through `native.log_write`; three *consecutive* faults of one callback on one instance disable that callback (fault badge in the inspector) until reload. | Engine code never throws or catches; a 60 Hz exception storm is useless to the user. |
| D12 | **DOD escape hatch**: `ref struct ComponentQuery` / `ChunkView` exposing `Span<T>` over native chunk columns inside a structural-locked scope; mirror structs verified by size + layout hash at first use. | `ref struct` makes "spans must not escape the scope" a compile-time rule; the hash turns layout drift into a logged error, not memory corruption. |
| D13 | **`null_script_host` is a C++ fake of the managed side** (implements `conjure_managed_api` in C++, records calls, echoes field blobs, registers fixture classes). | Runtime, store, codecs, reload sequencing and every other module's tests run without .NET; CLR tests carry the CTest label `dotnet`. |
| D14 | **Export** supports framework-dependent (default) and self-contained (`dotnet publish --self-contained` of `Conjure.Bootstrap`). The exported runtime detects the mode from `runtimeconfig.json` (`includedFrameworks` present => self-contained). | Players should not have to install .NET; both layouts converge on the same handshake. Validated by a CI test per platform. |
| D15 | ABI uses only blittable, fixed-width types: `uint64_t` entity ids, `int32_t` slots, UTF-8 `(const char *, int32_t)` pairs, `conjure_vec2/3/4`, `conjure_quat_xyzw` (explicit XYZW, converted from `math::quat` by name), `conjure_mat4` (column-major). C# mirrors are `System.Numerics` types. | No marshalling, no hidden allocations, no dependence on glm's quaternion storage order. |
| D16 | C# follows standard .NET conventions (PascalCase, `_camelCase` private fields). `docs/CODE_STYLE.cpp` governs C++ only. Native C-ABI/POD structs follow the struct example in the style guide (snake_case fields, no `m` prefix); classes use `mCamelCase`. | Game developers expect idiomatic C#; stated so reviews do not argue about it. |

---

## 2. Data model

### 2.1 C ABI tables (`native_api.h`, `native_api.inl`)

All ABI types are standard-layout and `static_assert`ed as such. Calling convention is the platform default
(x64 / arm64 only, so native `extern "C"` pointers and C# `delegate* unmanaged<...>` agree without
`CallConvs`). Every entry defaults to `nullptr`; managed installs a stub for null entries that throws
`NotSupportedException("entry physics_raycast not provided")`, caught by D11.

```cpp
// native_api.inl - exactly one entry per line; parsed by tools/gen_native_api.py.
//                 group      name                  return     arguments
CONJURE_NATIVE_FN(log,       write,                void,      (uint8_t level, const char *utf8, int32_t length))
CONJURE_NATIVE_FN(script,    register_class,       void,      (const conjure_script_class_desc *desc))
CONJURE_NATIVE_FN(script,    register_field,       void,      (uint64_t classHash, const conjure_script_field_desc *d))
CONJURE_NATIVE_FN(script,    mark_faulted,         void,      (int32_t slot, uint8_t callback))
CONJURE_NATIVE_FN(script,    add,                  int32_t,   (uint64_t entity, uint64_t classHash))
CONJURE_NATIVE_FN(script,    get_slots,            int32_t,   (uint64_t entity, int32_t *slots, int32_t capacity))
CONJURE_NATIVE_FN(entity,    create,               uint64_t,  (const char *utf8Name, int32_t length))
CONJURE_NATIVE_FN(entity,    destroy,              int32_t,   (uint64_t entity))
CONJURE_NATIVE_FN(entity,    is_alive,             int32_t,   (uint64_t entity))
CONJURE_NATIVE_FN(entity,    get_name,             int32_t,   (uint64_t entity, char *utf8, int32_t capacity))
CONJURE_NATIVE_FN(entity,    find_by_name,         uint64_t,  (const char *utf8, int32_t length))
CONJURE_NATIVE_FN(component, lookup_type,          uint32_t,  (uint64_t nameHash, uint32_t size, uint64_t layoutHash))
CONJURE_NATIVE_FN(component, register_managed,     uint32_t,  (const conjure_managed_component_desc *desc))
CONJURE_NATIVE_FN(component, has,                  int32_t,   (uint64_t entity, uint32_t typeId))
CONJURE_NATIVE_FN(component, add,                  int32_t,   (uint64_t entity, uint32_t typeId, const void *data))
CONJURE_NATIVE_FN(component, remove,               int32_t,   (uint64_t entity, uint32_t typeId))
CONJURE_NATIVE_FN(component, get,                  int32_t,   (uint64_t entity, uint32_t typeId, void *out))
CONJURE_NATIVE_FN(component, set,                  int32_t,   (uint64_t entity, uint32_t typeId, const void *data))
CONJURE_NATIVE_FN(transform, get_local,            int32_t,   (uint64_t entity, conjure_transform *out))
CONJURE_NATIVE_FN(transform, set_local,            int32_t,   (uint64_t entity, const conjure_transform *value))
CONJURE_NATIVE_FN(transform, get_world_matrix,     int32_t,   (uint64_t entity, conjure_mat4 *out))
CONJURE_NATIVE_FN(transform, set_parent,           int32_t,   (uint64_t entity, uint64_t parent))
CONJURE_NATIVE_FN(query,     begin,                int32_t,   (const uint32_t *ids, int32_t n, conjure_query_scope *o))
CONJURE_NATIVE_FN(query,     end,                  void,      (uint32_t scopeId))
// Ownership (R26, R64): log_*, script_*, component_*, query_* and the five entity_* entries above are
// implemented in this module (they need only ecs). Everything after the marker line
// `// --- bindings (R26) ---` (transform_*, the hierarchy/tag/layer/guid entity_* entries, scene_*, prefab_*,
// physics_*, audio_*, input_*, asset_*, diagnostics_*, modules_*; the full list is 15_bindings.md 2.3) is
// filled by conjure_bindings (wave 4) and lives in this same file so the struct is complete when
// conjure_scripting compiles (R54). Adding an entry = one line here + regenerate (tools/gen_native_api.py
// also emits docs/generated/native_api_manifest.json); appending only, never reordering within a version.
```

```cpp
struct conjure_native_api {
    uint32_t size = 0;                                  // sizeof(conjure_native_api), checked by managed
    uint32_t version = CONJURE_NATIVE_API_VERSION;      // bumped on any removal/reorder/signature change
    uint64_t layout_hash = CONJURE_NATIVE_API_HASH;     // FNV-1a of every .inl line, emitted by the generator
    const conjure_frame_block *frame = nullptr;         // shared per-frame block (D4)

#define CONJURE_NATIVE_FN(group, name, ret, args) ret (*group##_##name) args = nullptr;
#include "conjure/scripting/native_api.inl"
#undef CONJURE_NATIVE_FN
};

struct conjure_managed_api {
    uint32_t size = 0;
    uint32_t version = 0;
    uint64_t layout_hash = 0;
    int32_t (*load_game_assembly)(const char *utf8Path, int32_t length) = nullptr;  // 0 ok, else script_error
    int32_t (*unload_game_assembly)(void) = nullptr;                                // 0 collected, 1 leaked
    int32_t (*create_instances)(conjure_create_request *requests, int32_t count) = nullptr;
    int32_t (*destroy_instances)(const int32_t *slots, int32_t count, int32_t callOnDestroy) = nullptr;
    int32_t (*run_phase)(uint8_t phase, float deltaTime) = nullptr;
    int32_t (*dispatch_collisions)(const conjure_collision_record *records, int32_t count) = nullptr;
    int32_t (*apply_fields)(int32_t slot, const uint8_t *blob, int32_t size) = nullptr;
    int32_t (*collect_fields)(int32_t slot, uint8_t *buffer, int32_t capacity) = nullptr; // bytes needed
    int32_t (*collect_garbage)(void) = nullptr;
    int32_t (*shutdown)(void) = nullptr;
};
```

Records (all in `native_api.h`):

- `conjure_frame_block` (`alignas(64)`): `double time_seconds; double unscaled_time_seconds; float delta_time;
  float fixed_delta_time; float time_scale; uint64_t frame_index; uint32_t fixed_step_index; uint8_t
  current_phase; uint8_t in_play_mode; conjure_input_block input;`
- `conjure_input_block`: `uint64_t keys_down[8]; uint64_t keys_pressed[8]; uint64_t keys_released[8];
  float mouse_x, mouse_y, mouse_dx, mouse_dy, scroll_x, scroll_y; uint32_t mouse_buttons_down,
  mouse_buttons_pressed, mouse_buttons_released; conjure_gamepad_state gamepads[4];`
- `conjure_gamepad_state`: `uint32_t buttons_down, buttons_pressed, buttons_released; float axes[6]; uint8_t
  connected;`
- `conjure_vec2/3/4`, `conjure_quat_xyzw { float x, y, z, w; }`, `conjure_mat4 { float m[16]; }` (column
  major), `conjure_transform { conjure_vec3 position; conjure_quat_xyzw rotation; conjure_vec3 scale; }`.
- `conjure_create_request`: `uint64_t entity; uint64_t class_hash; const uint8_t *fields; int32_t fields_size;
  uint8_t flags; int32_t out_slot;` with `CREATE_FLAG_ALREADY_STARTED = 1`.
- `conjure_collision_record`: `int32_t slot; uint64_t other; conjure_vec3 point; conjure_vec3 normal; float
  impulse; uint8_t kind;` with `kind` in `COLLISION_ENTER, COLLISION_EXIT, TRIGGER_ENTER, TRIGGER_EXIT`.
- `conjure_script_class_desc`: `uint64_t hash; const char *full_name; int32_t name_length; uint16_t
  callback_mask; uint16_t field_count;`
- `conjure_script_field_desc`: `uint64_t hash; const char *name; int32_t name_length; uint8_t kind; uint8_t
  element_kind; const char *default_json; int32_t default_length; const char *meta_json; int32_t meta_length;`
  where `meta_json` is `{"tooltip":..,"min":..,"max":..,"enumNames":[..],"assetType":..,"hidden":bool}`.
- `conjure_managed_component_desc`: `uint64_t name_hash; const char *full_name; int32_t name_length; uint32_t
  size; uint32_t alignment; uint64_t layout_hash; const char *schema_json; int32_t schema_length;`
- `conjure_query_scope`: `uint32_t scope_id; int32_t chunk_count; const conjure_chunk_view *chunks;`
- `conjure_chunk_view`: `const uint64_t *entities; void *columns[CONJURE_QUERY_MAX_COMPONENTS]; int32_t count;`
  with `CONJURE_QUERY_MAX_COMPONENTS = 8`.
- `enum class script_field_kind : uint8_t { BOOL, INT32, INT64, FLOAT, DOUBLE, STRING, VEC2, VEC3, VEC4,
  QUAT, COLOR, ENTITY, ASSET, ENUM, LIST }` (`LIST` carries `element_kind`, which is never `LIST`).
- `enum class script_phase : uint8_t { FIXED_UPDATE, UPDATE, LATE_UPDATE, COUNT }` (argument of `run_phase`).
- `enum class script_callback : uint8_t { CREATE, START, FIXED_UPDATE, UPDATE, LATE_UPDATE, DESTROY,
  COLLISION_ENTER, COLLISION_EXIT, TRIGGER_ENTER, TRIGGER_EXIT, COUNT }` (bit index of `callback_mask`, index
  of fault counters).

Strings crossing native->managed by output buffer (`entity_get_name`) return the required length; the caller
retries with a bigger `stackalloc`/pooled buffer. Pointers inside records are valid only for the duration of
the call that carries them.

### 2.2 Native scripting state

```cpp
struct script_component {
    uint32_t set_handle = INVALID_SET_HANDLE;    // index+generation into script_store; 4 bytes, trivially copyable
};

struct script_entry {
    uint64_t class_hash = 0;                     // FNV-1a 64 of the full C# type name (core::hash::fnv1a64)
    int32_t managed_slot = INVALID_SLOT;         // -1 in edit mode, when the class is missing, or when disabled
    core::json::value fields = {};               // serialised state (scene format); snapshot while playing
    uint8_t flags = 0;                           // ENTRY_STARTED, ENTRY_FAULTED, ENTRY_CLASS_MISSING
};

struct script_set {
    ecs::entity owner = {};
    core::small_vector<script_entry, 2> entries = {};
};
```

`script_store` wraps `core::slot_map<script_set>` plus `std::vector<slot_ref> mSlotToEntry` (managed slot ->
`{set_handle, entry_index}`) so collision dispatch, inspector edits and `mark_faulted` resolve in O(1).
`script_entry_id { uint32_t set_handle; uint8_t entry_index; }` is the public handle to one script.

`register_script_component` registers the type with `ops.relocate = nullptr` (plain memcpy when a row moves
between archetypes, so D6 holds) but a custom `ops.copy` (duplicates the `script_set` into a new store entry with
the same JSON) and `ops.destruct` (frees the set), plus a custom `component_serializer` whose `writeJson` /
`readJson` / `remapEntities` hooks go through `script_runtime::active()`, the single runtime per process.
Archives, `entity.duplicate`, prefab instantiation and the editor's play-mode snapshot therefore deep-copy
scripts instead of aliasing store handles, and the scene module needs no dependency on scripting (R29).

`script_class_registry`: `std::vector<script_class_info>` indexed by dense class index plus
`std::unordered_map<uint64_t, uint32_t>` hash -> index. It is double buffered (`mActive`, `mStaging`) during
reload and swapped on success.

```cpp
struct script_field_info {
    uint64_t hash = 0;
    std::string name = {};
    script_field_kind kind = script_field_kind::INT32;
    script_field_kind element_kind = script_field_kind::INT32;
    core::json::value default_value = {};
    core::json::value meta = {};                 // tooltip, min, max, enumNames, assetType, hidden
};

struct script_class_info {
    uint64_t hash = 0;
    std::string full_name = {};
    uint16_t callback_mask = 0;
    std::vector<script_field_info> fields = {};  // sorted by hash for binary search
};
```

`field_blob` layout (little-endian, 4-byte aligned, version 1):

```
header : uint32 magic 'CSFB' | uint16 version | uint16 field_count
field  : uint64 name_hash | uint8 kind | uint8 element_kind | uint16 reserved | uint32 payload_size | payload
payload: BOOL=uint8, INT32/ENUM=int32, INT64=int64, FLOAT=float, DOUBLE=double, STRING=utf8 bytes,
         VEC2/3/4/COLOR=floats, QUAT=xyzw floats, ENTITY=uint64, ASSET=16-byte uuid,
         LIST=uint32 count + element payloads; every field padded to 4 bytes
```

`field_blob_writer` writes into a caller-provided `std::span<uint8_t>` and reports the required size (two-pass,
no allocation); `field_blob_reader` bounds-checks every read. `json_to_blob(classInfo, json, out)` and
`blob_to_json(classInfo, blob)` are pure functions. Missing JSON fields take `default_value`; JSON keys not in
the schema are preserved in `script_entry.fields` (so removed-then-restored fields survive) but not sent.

### 2.3 Managed runtime state (`managed/Conjure.Managed/Runtime/`)

- `InstanceTable`: `ScriptBehaviour?[] _instances` + `int[] _freeList` (LIFO reuse); slot = array index.
- `ScriptClassInfo` (owned by the game ALC catalog, dropped on unload): `Type`, `ulong Hash`,
  `Func<ScriptBehaviour> Factory` (compiled `Expression.New`), `FieldAccessor[] Fields` (sorted by hash),
  `ushort CallbackMask`, one `PhaseList` per overridden phase callback.
- `PhaseList`: dense `ScriptBehaviour[]` + `int Count`; each behaviour stores its index per phase in
  `int[3] PhaseIndices` so removal is swap-remove O(1). Classes iterate in full-name order (deterministic).
- `ScriptBehaviour` internals: `Entity Entity`, `int Slot`, `bool Started`, `byte[10] ConsecutiveFaults`,
  `ushort DisabledMask`.
- `GameAssemblyLoadContext : AssemblyLoadContext(isCollectible: true)`: `Load(name)` returns
  `typeof(ScriptBehaviour).Assembly` for `Conjure.Managed`, resolves the game's own dependencies through
  `AssemblyDependencyResolver(gameDllPath)` + `LoadFromStream` (bytes copied, files never locked), returns
  `null` for everything else (framework, default ALC). The main assembly is loaded with
  `LoadFromStream(dllBytes, pdbBytes)` so `dotnet build` can overwrite the output while the editor runs.
- `AssemblyLoader`: `GameAssemblyLoadContext? _current`, `List<WeakReference> _zombies` for diagnostics.
- `NativeBridge`: `static NativeApi* Api`, `static FrameBlock* Frame`, `static int MainThreadId`.

---

## 3. Algorithms and flow

### 3.1 Hosting sequence (`hostfxr_runtime`, `dotnet_script_host`)

Anchor files, produced by `managed/Conjure.Bootstrap` (`OutputType=Exe`, references `Conjure.Managed`, `Main`
returns 0 and is never run): `Conjure.Bootstrap.dll`, `Conjure.Bootstrap.runtimeconfig.json`,
`Conjure.Bootstrap.deps.json`, `Conjure.Managed.dll` (+ `.pdb`). One `dotnet publish` of the bootstrap yields
a correct framework-dependent or self-contained layout; `script_host_config.managed_directory` points at it.

1. **Locate hostfxr.** `get_hostfxr_parameters params{ sizeof(params), assembly_path = <dir>/Conjure.Bootstrap.dll,
   dotnet_root = config.dotnet_root_override or nullptr }`. Call `get_hostfxr_path(nullptr, &size, &params)`,
   expect `0x80008098`, allocate `size` `char_t`, call again. `libnethost` is linked statically through
   `conjure::thirdparty::nethost` (`NETHOST_USE_AS_STATIC`, already provided by `cmake/FindNetHost.cmake`).
   Failure -> `script_error::RUNTIME_NOT_FOUND` with the download URL in the message.
2. **Load hostfxr** with `core::dynamic_library` and resolve `hostfxr_initialize_for_runtime_config`,
   `hostfxr_initialize_for_dotnet_command_line`, `hostfxr_get_runtime_delegate`,
   `hostfxr_set_runtime_property_value`, `hostfxr_set_error_writer`, `hostfxr_close`. Missing symbol ->
   `HOSTFXR_INCOMPATIBLE`. `char_t` is `wchar_t` on Windows; `hostfxr_api.h` (private) owns conversions.
3. **Error writer.** `hostfxr_set_error_writer(&forward_to_log)` on the calling (main) thread; it emits
   `CONJURE_LOG_ERROR(scripting, ...)`. Registration is per thread, which is why all hosting is main-thread.
4. **Mode detection.** Parse `Conjure.Bootstrap.runtimeconfig.json` with `core::json`:
   `runtimeOptions.includedFrameworks` present => `host_mode::SELF_CONTAINED`, else `FRAMEWORK_DEPENDENT`.
5. **Initialise.** `hostfxr_initialize_parameters init{ sizeof(init), host_path = executable path, dotnet_root }`.
   - FRAMEWORK_DEPENDENT: `hostfxr_initialize_for_runtime_config(runtimeConfigPath, &init, &handle)`; accept
     `0`, `Success_HostAlreadyInitialized (1)`, `Success_DifferentRuntimeProperties (2)`.
   - SELF_CONTAINED: `hostfxr_initialize_for_dotnet_command_line(1, { bootstrapDllPath }, &init, &handle)`.
     `HostInvalidState` means a runtime already exists (only possible in tests) -> `HOST_ALREADY_RUNNING`.
6. **Properties** (first context only, before step 7): `APP_CONTEXT_BASE_DIRECTORY = managed_directory`,
   `CONJURE_ENGINE_VERSION`. GC knobs live in `runtimeconfig.template.json`: `System.GC.Server=false`,
   `System.GC.Concurrent=true`, `System.GC.RetainVM=true`, `System.Runtime.TieredPGO=true`,
   `rollForward=Major`.
7. **Delegates.** `hostfxr_get_runtime_delegate(handle, hdt_get_function_pointer, &getFn)`. In
   FRAMEWORK_DEPENDENT mode also `hdt_load_assembly` and call `loadAssembly(managedDllPath, nullptr, nullptr)`
   so `Conjure.Managed` and its `.deps.json` are loaded into the **default** ALC. In SELF_CONTAINED mode the
   bootstrap `.deps.json` already places it on the TPA. This step starts CoreCLR.
8. **Entry point.** `getFn("Conjure.Interop.NativeBridge, Conjure.Managed", "Initialize",
   UNMANAGEDCALLERSONLY_METHOD, nullptr, nullptr, &initFn)`.
9. `hostfxr_close(handle)`: delegates have process lifetime; the context is not needed again.
10. **Handshake.** `int32_t rc = initFn(&nativeApi, &managedApi)`. Managed checks `size`, `version`,
    `layout_hash` against `NativeApi.g.cs`, records the main thread id, installs stubs for null entries,
    fills `managedApi`, sets `GCSettings.LatencyMode = SustainedLowLatency`, hooks
    `AppDomain.CurrentDomain.UnhandledException` to log through native. Non-zero `rc` -> `ABI_MISMATCH`
    (message carries both hashes). Native then validates `managedApi` the same way.
11. `script_host::status() == READY`. Steps 1-9 run once per process (`hostfxr_runtime`); step 10 runs for
    every `dotnet_script_host::initialize`, and managed `Initialize` is idempotent (re-validates, refills).

Shutdown: `managed.shutdown()` destroys instances (`OnDestroy`), unloads the game ALC and clears tables. The
CLR stays resident until process exit by design.

### 3.2 Frame flow (engine loop, main thread, all exclusive systems)

```
(before)        engine scripting_subsystem::begin_frame writes frame_block (time, input snapshot from platform::input)
FRAME_BEGIN     scripting.frame_begin (EXCLUSIVE): hot_reload_controller::tick, flush_pending (pending OnCreate)
FIXED_UPDATE    scripting.collision_dispatch (EXCLUSIVE, before scripting.fixed_update): one dispatch_collisions
  x N steps     call over physics_world::contact_events() of the previous fixed step (R6)
                scripting.fixed_update (EXCLUSIVE): run_phase(FIXED_UPDATE) -> flush_pending()
PHYSICS_STEP    physics.push_transforms, physics.step           (no managed code)
FIXED_END       physics.pull_transforms, physics.contact_events (fills the span read in the next step)
UPDATE          scripting.update (EXCLUSIVE): run_phase(UPDATE) -> flush_pending()
LATE_UPDATE     scripting.late_update (EXCLUSIVE): run_phase(LATE_UPDATE) -> flush_pending()
FRAME_END       scripting.frame_end (EXCLUSIVE): flush_pending (deferred OnDestroy, pending entity destroys)
```

Phase names are `ecs::system_phase` values (R1); the six systems are registered by
`register_script_systems(ecs::scheduler &, script_runtime &)` from the engine's `scripting_subsystem` (R27).

Managed `RunPhase(phase, dt)`: if phase is FIXED_UPDATE or UPDATE, drain `_pendingStart` first (`OnStart`
once each, `Started = true`, native informed through `out_slot` flags on the next `flush_pending` snapshot:
`collect_started(slots)` is folded into `run_phase`'s return value as a count and native marks entries by
reading `started` from `conjure_create_request` order - simpler: native sets `ENTRY_STARTED` for every entry
created before this phase ran, because Start is attempted exactly once regardless of exceptions). Then
`for each class: for i < list.Count: list[i].InvokeUpdate(dt)`. Lists may shrink during iteration (swap
remove, loop re-checks `Count`); an instance removed mid-phase is skipped, never double-called. No
allocations, no dictionary lookups.

Script systems are exclusive: no other system runs concurrently, and the managed loop iterates its own
arrays, so structural changes inside a phase apply **immediately** to the world unless a query scope is
open (then they go to the command buffer and apply at `query_end`).

### 3.3 Creating scripts

`script_runtime::add_script(entity, classHash, initialFields)`: ensures `script_component` exists (immediate
add), appends a `script_entry`, and - only in play mode - pushes `pending_create{ setHandle, entryIndex }`.
Scene load in play mode and `enter_play()` push one pending create per entry.

`flush_pending()`: for each pending create whose class exists in the active registry, `json_to_blob` into a
`core::memory::frame_allocator` buffer, fill `conjure_create_request` (flag `ALREADY_STARTED` when
`ENTRY_STARTED`), then **one** `create_instances` call. Managed per request: `Factory()`, set
`Entity`/`Slot`, `ApplyFields`, insert into phase lists for overridden callbacks only, `OnCreate` under the
guard, queue `OnStart` unless `ALREADY_STARTED`. Native stores `out_slot` and the reverse map entry. Missing
class -> `ENTRY_CLASS_MISSING`, JSON kept, warning once per class.

`Prefab.Instantiate` / `Entity.Create` from C# return a live entity immediately (structural change applied
directly, D10/3.2); their scripts are created in the `flush_pending` of the current phase, so `OnCreate` runs
the same frame and `OnStart` before the next update phase.

### 3.4 Destroying scripts and entities

- `Entity.Destroy()` from C#: `entity_destroy` checks for a `script_component` with live slots. If present it
  records the entity in `mPendingEntityDestroys` and returns; otherwise the entity is destroyed immediately.
  `flush_pending()` then calls `destroy_instances(slots, callOnDestroy = 1)` (**entity still alive** during
  `OnDestroy`), frees the set, and destroys the entity.
- Destroys initiated natively (editor delete, native systems): a `world::observe(scriptComponentId, REMOVED, ...)`
  observer (fires before the data is destroyed, R5/R29) calls `script_runtime::on_component_removed(entity, component)`, which detaches the set and queues its
  slots; the next `flush_pending` runs `OnDestroy` with `Entity.IsAlive == false`. Documented: `OnDestroy`
  releases script-owned resources; it must not assume components exist.
- Scene unload / `exit_play()` / `shutdown()`: `destroy_all(callOnDestroy = true)` runs before the world is
  torn down, so bulk paths see alive entities. `remove_script(entity, id)` destroys one entry the same way.

### 3.5 Collision dispatch

Physics exposes `physics::physics_world::contact_events() -> std::span<const contact_event>` (`{ entity a, b;
vec3 point, normal; float impulse; kind }`, sorted by body-id pair, filled by `physics.contact_events` in
FIXED_END and cleared at the next PHYSICS_STEP; the ECS has no event queue, R6). `scripting.collision_dispatch`
(exclusive, first system of FIXED_UPDATE, so callbacks see the previous step's contacts) looks up both
participants' `script_set`s, emits one `conjure_collision_record` per entry whose class `callback_mask` has
the matching bit (normal flipped for `b`), builds the array in the frame allocator and sends **one**
`dispatch_collisions`. Managed indexes `_instances[record.slot]` directly and calls
`OnCollisionEnter(in Collision)` / `OnCollisionExit` / `OnTriggerEnter(Entity)` / `OnTriggerExit(Entity)`.

### 3.6 Fields: apply, collect, inspector

`ApplyFields` walks the blob with `FieldBlobReader`, binary-searches `ScriptClassInfo.Fields` by hash and
calls `FieldAccessor.Set` (cached `FieldInfo`, boxing acceptable at load time). Unknown hashes are counted
and logged once per class in Debug. `CollectFields` writes with `FieldBlobWriter`; if `capacity` is too
small it returns the needed size and native retries once with a grown frame buffer.

Inspector contract (editor, area D): schema from `script_class_registry`, values from `script_entry.fields`.
Edit mode: `set_field(id, fieldHash, json)` rewrites the JSON only. Play mode: `set_field` rewrites the JSON
*and* sends a single-field blob through `apply_fields`; `collect_entry_fields(id)` refreshes the JSON from
the live instance for display. Entity fields hold `uint64_t` ids; `remap_entity_fields(entity, remapFn)`
rewrites them during scene load / prefab instantiation using the class schema (LIST of ENTITY included) and
re-applies when live.

### 3.7 Field discovery (managed reflection, per assembly load)

For each non-abstract, non-generic `ScriptBehaviour` subclass with a public parameterless constructor:
fields in `MetadataToken` order, included when (`IsPublic` or `[SerializeField]`) and not `static`,
`readonly`, `const`, `[NonSerialized]`. Kind from the field type: `bool, int, long, float, double, string,
Vector2/3/4, Quaternion, Color, Entity, AssetRef<T>, enum, List<E>/E[]` with `E` a non-LIST kind. Anything
else is skipped with an INFO log. `[Range]`, `[Tooltip]`, `[HideInInspector]` and enum names populate
`meta_json`; the default value is read from a throwaway instance. Overridden callbacks:
`type.GetMethod(name, Instance|NonPublic|Public).DeclaringType != typeof(ScriptBehaviour)`. Each class and
field is reported to native through `script_register_class` / `script_register_field` into the staging
registry. The catalog is owned by the ALC and dropped at unload (D9).

### 3.8 Hot reload (`hot_reload_controller`, main thread, frame boundary)

Trigger: editor (file watcher on the game's `.cs` -> `managed_build_runner` -> on success `request()`), or
command `script.reload`, or the controller's own watch on the output `.dll`. Requests are debounced
200 ms and executed by `tick()` before the first script phase of the next frame:

1. `managed.load_game_assembly(path)` -> new collectible ALC B; catalog built; classes reported into the
   **staging** registry. Non-zero return -> log, discard staging, abort: the running game is untouched.
2. For every live entry: `collect_fields(slot)` -> `blob_to_json` with the **active** schema -> merged into
   `script_entry.fields` (unknown JSON keys preserved).
3. `destroy_instances(allSlots, callOnDestroy = 0)`; phase lists cleared; reverse map cleared.
4. `unload_game_assembly()`: drop catalog A and every reference, `WeakReference ref = new(alc)`,
   `alc.Unload()`, then `for (i < 10 && ref.IsAlive) { GC.Collect(); GC.WaitForPendingFinalizers(); }`.
   Returns 1 if still alive -> `CONJURE_LOG_WARN` naming the usual culprits (statics, timers, threads) and
   the zombie count is exposed to the editor. `Unloading` event lets game code stop its threads.
5. Swap registries. For every entry: `json_to_blob` with the **new** schema (missing fields -> defaults,
   removed fields dropped with a Debug log, class missing -> `ENTRY_CLASS_MISSING`, JSON preserved), then
   one `create_instances` with `ALREADY_STARTED` where `ENTRY_STARTED`. `OnCreate` runs; `OnStart` does not.
6. `on_reloaded` delegate fires (editor refreshes inspectors). Target for 10k instances: < 250 ms.

Guarantees: fields passing 3.7 survive; everything else (private non-serialised state, statics, event
subscriptions) is reset. Entities and native components are untouched. Reload in edit mode is steps 1, 4,
5 (registry only) since no instances exist.

### 3.9 DOD escape hatch (`Conjure.Dod`)

```csharp
using var query = ComponentQuery.Begin<LocalTransform, Velocity>();
while (query.NextChunk(out ChunkView chunk)) {
    Span<LocalTransform> transforms = chunk.Get<LocalTransform>(0);
    ReadOnlySpan<Velocity> velocities = chunk.Get<Velocity>(1);
    for (int i = 0; i < chunk.Count; ++i) { transforms[i].Position += velocities[i].Linear * Time.DeltaTime; }
}
```

`ComponentQuery` and `ChunkView` are `ref struct`s: spans cannot be boxed, captured or stored, so they
cannot outlive the scope (rule enforced by the compiler). `ComponentType<T>.Id` is resolved once per class
load through `component_lookup_type(nameHash, Unsafe.SizeOf<T>(), layoutHash)` where both sides compute
`core::reflect::type_info::layout_hash()` (`10_core.md` 2.11: FNV-1a 64 over `(name, kind, offset, size)` of every
field in declaration order; C# derives names by lower-casing the first letter and kinds from the field type,
`15_bindings.md` B10; fields named `Pad*` are padding and excluded; R61);
`0` means mismatch and `Begin` throws `InvalidOperationException` (caught by D11, logged once). `query_begin`
is rejected (`WRONG_THREAD` / `NOT_IN_PHASE`) unless called on the main thread during a script phase; it
holds an `ecs::iteration_scope` from `world::lock_iteration()` (the ECS refuses structural calls while locked;
while the scope is open every structural request arriving through the bindings is routed into the script
runtime's `command_buffer` and flushed at `query_end`, R5), fills a native-owned `conjure_chunk_view[]` valid
until `query_end`, and conservatively bumps change-detection versions of every requested type. A scope still open
at phase end is force-closed with a WARN naming the class. Debug builds poison the chunk view array on
`query_end`.

Mirrors of native components are `[NativeComponent("conjure.scene.local_transform")]` (the registered component
name, R61) blittable structs
with `StructLayout(LayoutKind.Sequential)`; the layout test in 10 catches drift. User-defined
`[EcsComponent] unmanaged struct` types are registered at assembly load through `component_register_managed`
(name, size, alignment, layout hash, field schema JSON): `native_api_core.cpp` parses the schema into
`ecs::dynamic_field_desc`s and calls `component_registry::register_dynamic` (same name -> same id across
reloads), making them ECS component types usable in queries and `Add/Get/SetComponent<T>` (copy in/out). The
ABI `typeId` is `component_id.value` widened to 32 bits; 0 is never handed out (it is `unresolved_components`)
and means failure (R4). A layout change across reload with live instances runs the ECS sequence
`remove_from_all -> redefine_dynamic -> add_raw(zero bytes)` with a WARN, which is how "reset to zero" is
implemented; without instances `redefine_dynamic` alone suffices.

### 3.10 Building managed projects (`managed_build.h`, `cmake/ConjureManaged.cmake`)

- `conjure_add_managed_project(NAME <n> PROJECT <csproj> OUTPUT <dir> SOURCES <cs files...>)` creates target
  `conjure_managed_<n>` running `dotnet build <csproj> -c $<IF:$<CONFIG:Debug>,Debug,Release> -o <dir> --nologo
  -clp:NoSummary -p:ContinuousIntegrationBuild=true` with a stamp file depending on the listed sources (no
  globbing; csproj uses `EnableDefaultCompileItems=false` with explicit `<Compile>` items, and the CTest check
  `conjure_managed_sources_match` compares both lists). `conjure_scripting` depends on `conjure_managed_sdk`
  when `CONJURE_BUILD_MANAGED`. SDK missing with `CONJURE_WITH_SCRIPTING=ON` is a configure error (already
  the behaviour of `FindNetHost.cmake`); `dotnet` missing with `CONJURE_BUILD_MANAGED=ON` likewise.
- `CONJURE_SCRIPTING_HAS_DOTNET` is defined for test targets when the SDK was built; CLR tests use
  `doctest::skip(!HAS_DOTNET)` and the CTest label `dotnet`.
- Runtime build runner: `managed_build_runner::build(csproj, configuration)` spawns `dotnet build` through
  `core::process` on a job thread, parses MSBuild `path(line,col): error|warning CSxxxx: message` lines into
  `build_diagnostic`, and posts `build_report` to the main thread via `core::jobs::post_to_main_thread`. The editor
  console shows diagnostics; success calls `hot_reload_controller::request()`. Lives in `conjure_scripting`
  so `conjure_editor --headless` can rebuild too.

### 3.11 Export

`project.json` -> `"scripting": { "assembly": "bin/Game.dll", "hot_reload": true, "export_mode":
"framework-dependent" | "self-contained", "debug_symbols": false }` (schema owned by area F, R39). The engine's
`export_pipeline` (`06_engine_loop.md` 3.6, R21) runs `dotnet publish managed/Conjure.Bootstrap
-c Release -r <rid> --self-contained <true|false> -o <export>/managed` and `dotnet build <project>/Scripts -c
Release -o <export>/managed`. Framework-dependent exports show a platform message box with the .NET 8
download URL and exit with code 3 when `get_hostfxr_path` fails. Self-contained exports are validated by the
CI test `self_contained_publish_hosts`; a platform where it fails has `host_capabilities::self_contained ==
false` and the export dialog greys the option out.

---

## 4. Public API (`engine/include/conjure/scripting/`)

| Header | Main types |
|---|---|
| `log_category.h` | `CONJURE_DECLARE_LOG_CATEGORY(scripting)` |
| `native_api.h`, `native_api.inl` | `conjure_native_api`, `conjure_managed_api`, `conjure_frame_block`, `conjure_input_block`, records and enums of 2.1, `CONJURE_NATIVE_API_VERSION`, `CONJURE_NATIVE_API_HASH` |
| `script_error.h` | `enum class script_error : uint8_t`, `core::error make_error(script_error, std::string_view)` |
| `script_host.h` | `script_host` (interface), `script_host_config`, `script_host_status`, `script_backend`, `host_mode`, `host_capabilities`, `create_script_host`, `is_runtime_available()` (static hostfxr probe without starting the CLR; used as a test precondition, R28) |
| `dotnet_script_host.h` | `class dotnet_script_host final : public script_host` |
| `null_script_host.h` | `class null_script_host final : public script_host`, `null_host_recorder` |
| `script_fields.h` | `field_blob_writer`, `field_blob_reader`, `json_to_blob`, `blob_to_json`, `remap_entity_fields_json` |
| `script_class_registry.h` | `script_field_info`, `script_class_info`, `class script_class_registry` |
| `script_component.h` | `script_component`, `script_entry`, `script_set`, `script_entry_id`, `class script_store`, `register_script_component(ecs::component_registry &)` |
| `script_runtime.h` | `class script_runtime`, `script_runtime_config`, `script_mode`, `collision_event` |
| `script_system.h` | `register_script_systems(ecs::scheduler &, script_runtime &)` (the six EXCLUSIVE systems of 3.2) |
| `hot_reload.h` | `class hot_reload_controller`, `enum class reload_state : uint8_t` |
| `managed_build.h` | `class managed_build_runner`, `build_diagnostic`, `build_report` |

Private sources (`engine/src/scripting/`): `hostfxr_api.h`, `hostfxr_runtime.{h,cpp}`,
`dotnet_script_host.cpp`, `null_script_host.cpp`, `script_runtime.cpp`, `script_component.cpp`,
`script_fields.cpp`, `script_class_registry.cpp`, `script_system.cpp`, `hot_reload.cpp`, `managed_build.cpp`,
`native_api_core.cpp` (log_*, script_*, entity_*, component_*, query_* entries owned by this module, R26), `README.md`.

```cpp
namespace conjure::scripting {

    enum class script_error : uint8_t {
        RUNTIME_NOT_FOUND, HOSTFXR_INCOMPATIBLE, RUNTIME_CONFIG_INVALID, MANAGED_SDK_MISSING,
        HOST_ALREADY_RUNNING, ABI_MISMATCH, ASSEMBLY_LOAD_FAILED, ASSEMBLY_LEAKED, CLASS_NOT_FOUND,
        FIELD_NOT_FOUND, FIELD_BLOB_INVALID, INSTANCE_NOT_FOUND, WRONG_THREAD, NOT_IN_PHASE, BUILD_FAILED,
        NOT_INITIALIZED, NOT_SUPPORTED
    };

    enum class script_backend : uint8_t { DOTNET, NULL_HOST };
    enum class script_host_status : uint8_t { UNINITIALIZED, READY, DISABLED };
    enum class script_mode : uint8_t { EDIT, PLAY };

    struct script_host_config {
        script_backend backend = script_backend::DOTNET;
        std::filesystem::path managed_directory = {};       // Conjure.Bootstrap.* and Conjure.Managed.dll
        std::filesystem::path dotnet_root_override = {};    // empty = nethost default lookup
        bool wait_for_debugger = false;
    };

    class script_host {
        public:
            script_host();
            virtual ~script_host();

        public:
            /// <summary>Starts (or attaches to) the runtime and performs the ABI handshake. Main thread only.</summary>
            [[nodiscard]] virtual core::result<void> initialize(
              const script_host_config &config,
              conjure_native_api &nativeApi
            ) = 0;

            /// <summary>Stops managed execution for this host. The CLR stays resident until process exit.</summary>
            virtual void shutdown() = 0;

            [[nodiscard]] virtual const conjure_managed_api &managed_api() const noexcept = 0;
            [[nodiscard]] virtual script_host_status status() const noexcept = 0;
            [[nodiscard]] virtual script_backend backend() const noexcept = 0;
    };

    /// <summary>Creates the host for the backend; NULL_HOST never fails.</summary>
    [[nodiscard]] std::unique_ptr<script_host> create_script_host(script_backend backend);

    struct script_runtime_config {
        script_host_config host = {};
        std::filesystem::path game_assembly_path = {};     // optional at initialise time
        script_mode initial_mode = script_mode::EDIT;
    };

    class script_runtime {
        private:
            std::unique_ptr<script_host> mHost = nullptr;
            conjure_native_api mNativeApi = {};
            alignas(64) conjure_frame_block mFrameBlock = {};
            script_class_registry mClasses = {};
            script_store mStore = {};
            std::vector<pending_create> mPendingCreates = {};
            std::vector<int32_t> mPendingDestroySlots = {};
            std::vector<ecs::entity> mPendingEntityDestroys = {};
            ecs::world *mWorld = nullptr;
            ecs::command_buffer mDeferredCommands = {};   // Structural requests made inside a DOD query scope (R5, R54).
            script_mode mMode = script_mode::EDIT;
            script_phase mCurrentPhase = script_phase::COUNT;

        public:
            explicit script_runtime(std::unique_ptr<script_host> host);
            ~script_runtime();

            script_runtime(const script_runtime &) = delete;
            script_runtime &operator=(const script_runtime &) = delete;

        public:
            /// <summary>
            /// Bindings must have filled native_api() before this call. Failure leaves the runtime DISABLED.
            /// </summary>
            [[nodiscard]] core::result<void> initialize(const script_runtime_config &config);
            void shutdown();
            void attach_world(ecs::world *world);

            [[nodiscard]] core::result<void> load_game_assembly(const std::filesystem::path &assemblyPath);
            [[nodiscard]] core::result<void> reload_game_assembly(const std::filesystem::path &assemblyPath);

            /// <summary>EDIT -> PLAY creates every instance; PLAY -> EDIT destroys them with OnDestroy.</summary>
            void set_mode(script_mode mode);
            void run_phase(script_phase phase, float deltaTime);
            void flush_pending();
            void dispatch_collision_events(std::span<const collision_event> events);
            void destroy_all(bool callOnDestroy);

            [[nodiscard]] script_entry_id add_script(ecs::entity entity, uint64_t classHash, core::json::value fields);
            void remove_script(ecs::entity entity, script_entry_id id);
            [[nodiscard]] core::result<void> set_field(
              script_entry_id id,
              uint64_t fieldHash,
              const core::json::value &value
            );
            [[nodiscard]] core::result<core::json::value> collect_entry_fields(script_entry_id id);
            [[nodiscard]] core::result<core::json::value> serialize_entity_scripts(ecs::entity entity) const;
            [[nodiscard]] core::result<void> deserialize_entity_scripts(
              ecs::entity entity,
              const core::json::value &scripts
            );
            void remap_entity_fields(ecs::entity entity, const std::function<ecs::entity(ecs::entity)> &remap);

            /// <summary>Called by the REMOVED observer on script_component; defers OnDestroy to the next flush.</summary>
            void on_component_removed(ecs::entity entity, const script_component &component);

            [[nodiscard]] conjure_native_api &native_api() noexcept;
            [[nodiscard]] conjure_frame_block &frame_block() noexcept;
            [[nodiscard]] const script_class_registry &classes() const noexcept;
            [[nodiscard]] script_store &store() noexcept;
            [[nodiscard]] script_host_status status() const noexcept;
            [[nodiscard]] script_mode mode() const noexcept;

            /// <summary>Phase being run, or COUNT outside a script phase (bindings report NOT_IN_PHASE then; R54).</summary>
            [[nodiscard]] script_phase current_phase() const noexcept;
            /// <summary>The buffer bindings record structural requests into while world::is_locked(); flushed at query_end and at the phase-end force-close (R5, R54).</summary>
            [[nodiscard]] ecs::command_buffer &deferred_commands() noexcept;

            core::delegate<void()> on_reloaded;
    };

    [[nodiscard]] core::result<std::size_t> json_to_blob(
      const script_class_info &classInfo,
      const core::json::value &fields,
      std::span<uint8_t> output
    );
    [[nodiscard]] core::result<core::json::value> blob_to_json(
      const script_class_info &classInfo,
      std::span<const uint8_t> blob
    );

    class hot_reload_controller {
        private:
            script_runtime *mRuntime = nullptr;
            std::filesystem::path mAssemblyPath = {};
            reload_state mState = reload_state::IDLE;
            double mRequestTime = 0.0;
            double mDebounceSeconds = 0.2;

        public:
            hot_reload_controller(script_runtime &runtime, std::filesystem::path assemblyPath);
            ~hot_reload_controller();

        public:
            void request(bool immediate = false);
            /// <summary>Call once per frame before the first script phase; performs a due reload.</summary>
            void tick(double nowSeconds);
            [[nodiscard]] reload_state state() const noexcept;
    };

    class managed_build_runner {
        public:
            managed_build_runner();
            ~managed_build_runner();

        public:
            /// <summary>Spawns dotnet build on a job; the callback runs on the main thread.</summary>
            void build(
              const std::filesystem::path &projectPath,
              std::string_view configuration,
              std::function<void(const build_report &)> onComplete
            );
            [[nodiscard]] bool is_building() const noexcept;
            [[nodiscard]] static std::vector<build_diagnostic> parse_output(std::string_view msbuildOutput);
    };
}
```

### 4.1 `Conjure.Managed` SDK layout (`managed/`)

```
Conjure.Managed/                      net8.0 class library; AllowUnsafeBlocks, Nullable, TreatWarningsAsErrors,
                                      InvariantGlobalization, EnableDefaultCompileItems=false
  Interop/NativeApi.g.cs              GENERATED struct NativeApi + delegate* unmanaged fields + LayoutHash
  Interop/ManagedApi.cs               struct ManagedApi; [UnmanagedCallersOnly] entry points
  Interop/NativeBridge.cs             Initialize(NativeApi*, ManagedApi*), Api, Frame, MainThreadId, stubs
  Interop/FrameBlock.cs, Records.cs   blittable mirrors of 2.1 (StructLayout.Sequential)
  Runtime/ScriptRuntime.cs            InstanceTable, PhaseLists, Create/Destroy, RunPhase, DispatchCollisions
  Runtime/ScriptClassInfo.cs          discovery (3.7), FieldAccessor
  Runtime/FieldBlob.cs                FieldBlobReader/Writer (ref struct, span based)
  Runtime/GameAssemblyLoadContext.cs  collectible ALC (2.3)
  Runtime/AssemblyLoader.cs           load/unload state machine, WeakReference tracking
  Runtime/Guard.cs                    try/catch wrapper, consecutive fault counters, native logging
  Runtime/NameHash.cs                 FNV-1a 64 identical to core::hash::fnv1a64 (shared test vectors)
  Api/ScriptBehaviour.cs Entity.cs Component.cs Transform.cs Input.cs Key.cs MouseButton.cs GamepadButton.cs
  Api/Time.cs Log.cs Physics.cs Audio.cs Prefab.cs Scene.cs Attributes.cs AssetRef.cs Color.cs
  Api/Components/*.cs                 [NativeComponent] mirrors (LocalTransform, Name, Tag, ...; bindings add more)
  Api/Dod/ComponentQuery.cs ChunkView.cs ComponentType.cs
Conjure.Managed.Tests/                xunit; one CTest entry conjure_managed_tests (dotnet test)
Conjure.Bootstrap/                    OutputType Exe hosting anchor; runtimeconfig.template.json
Conjure.Templates/Game/               Game.csproj (references Conjure.Managed via $(ConjureManagedDir)), Player.cs
```

`ScriptBehaviour` surface: `Entity Entity`, `Transform Transform`, `T GetComponent<T>() where T : unmanaged,
IComponent`, `bool HasComponent<T>()`, `void AddComponent<T>(in T)`, `void SetComponent<T>(in T)`,
`T? GetScript<T>() where T : ScriptBehaviour`, `void Destroy()`, and `protected virtual` `OnCreate, OnStart,
OnUpdate(float), OnFixedUpdate(float), OnLateUpdate(float), OnDestroy, OnCollisionEnter(in Collision),
OnCollisionExit(in Collision), OnTriggerEnter(Entity), OnTriggerExit(Entity)`. `Entity`, `Transform`,
`Rigidbody` (spelt as in `15_bindings.md` 2.5, R61), `AudioSource` etc. are `readonly struct`s holding only the 64-bit id; the SDK allocates nothing
per frame. Vectors and quaternions are `System.Numerics`.

---

## 5. Threading and ownership

- **Threading.** Hosting, handshake, every `managed_api` call, every `native_api` callback, reload and
  `flush_pending` are main-thread only. Native asserts `core::jobs::is_main_thread()` (`CONJURE_ASSERT`)
  and returns `WRONG_THREAD` in Development; managed compares `Environment.CurrentManagedThreadId` with the
  recorded id and throws `InvalidOperationException`. The three script systems and the collision system are
  exclusive scheduler systems. `managed_build_runner` runs `dotnet` on a job thread and marshals results via
  `core::jobs::post_to_main_thread`. GC threads belong to the CLR. Scripts may spawn `Task`s for pure computation
  only; the SDK logs unhandled exceptions on such threads before .NET terminates the process.
- **Ownership.** `engine::application` owns one `script_runtime` (asserted single live instance per process).
  `script_runtime` owns the host, registry, store, frame block and pending queues; `attach_world` is a
  non-owning pointer and the owner must `destroy_all` before destroying the world (the engine's
  `scripting_subsystem` does so from the `scene_manager::on_before_unload` event, so `scene` never calls into
  scripting, R29). The `hostfxr_runtime` singleton owns the `core::dynamic_library` handle and lives until process
  exit. Managed instances are owned by `InstanceTable` and referenced from native only by slot (D9).
  `conjure_chunk_view` memory is owned by the ECS and valid only inside a query scope. No raw owning
  pointers anywhere.
- **Re-entrancy.** Native bindings called during a phase may mutate the world directly (3.2). `flush_pending`
  is not re-entrant: creates/destroys requested from inside `OnCreate`/`OnDestroy` are appended and handled
  by the same flush loop (bounded by `MAX_FLUSH_ITERATIONS = 8`, then WARN).

---

## 6. Failure handling

| Failure | Behaviour |
|---|---|
| .NET runtime not installed / hostfxr missing | `initialize` fails with `RUNTIME_NOT_FOUND`; `script_runtime` stays `DISABLED`; entries keep JSON with slot -1; editor shows a "Scripting unavailable" banner and uses the null host; exported runtime shows message box and exits 3. |
| ABI size/version/hash mismatch (stale `Conjure.Managed.dll`) | `ABI_MISMATCH` logged with both hashes; runtime `DISABLED`. |
| Game assembly missing or fails to load | `ASSEMBLY_LOAD_FAILED`; previous assembly (if any) keeps running; scenes load; inspector shows "class not loaded". |
| Class in scene not in assembly | `ENTRY_CLASS_MISSING`, warning once per class, JSON preserved; restored when the class returns. |
| Field JSON unknown / type mismatch / blob corrupt | Default value, WARN naming entity, class, field; `FIELD_BLOB_INVALID` creates the instance with defaults. |
| Exception in a callback | Logged with stack via `log_write(ERROR)`; third consecutive fault of that callback disables it (`script_mark_faulted` -> `ENTRY_FAULTED`, inspector badge). A success resets the counter. |
| Exception in `OnCreate` | Instance kept (so `OnDestroy` symmetry holds) and all callbacks disabled. |
| ALC unload leaks | `ASSEMBLY_LEAKED` WARN, reload proceeds; zombie count in the editor status bar. |
| Null native entry (module disabled) | C# API throws `NotSupportedException` naming the entry (caught by the guard). |
| Mirror layout mismatch | `component_lookup_type` returns 0; `ComponentQuery.Begin` throws `InvalidOperationException`; logged once. |
| Wrong thread | `WRONG_THREAD` natively, `InvalidOperationException` in C#. |
| Dead entity passed to a binding | Returns 0/false; Debug logs once per call site (rate-limited). |
| `collect_fields` buffer too small | Returns needed size; native retries once with a grown frame buffer. |
| `dotnet build` fails | `build_report.succeeded == false`, diagnostics in the console; no reload requested. |

`script_runtime` and `script_host` APIs return `core::result<T>` built from `script_error`; nothing in the
module throws or catches.

---

## 7. Headless / null behaviour

`null_script_host` implements `conjure_managed_api` in C++: `create_instances` assigns slots from a LIFO free
list and records requests, `destroy_instances` frees them, `run_phase` records `(phase, dt)` and increments
per-slot counters, `apply_fields` stores the blob per slot and `collect_fields` echoes it (or a test-supplied
override), `dispatch_collisions` records, `load_game_assembly` succeeds when the path is empty or exists and
registers classes from `null_script_host::add_fake_class(script_class_info)` and from an optional
`<assembly>.classes.json` fixture through `native.script_register_class/register_field` (so registry, blob
and reload paths run), `unload_game_assembly` returns a configurable 0/1. `null_host_recorder` exposes the
call log for assertions. It is used by every scripting/scene/engine/editor test, by `conjure_editor
--headless` when the runtime is unavailable, and by `conjure_runtime` when `project.json` has no scripting
section. `dotnet_script_host` is itself headless (no window/GPU); its tests run in CI whenever the SDK is
present (`actions/setup-dotnet` 8.0.x on all three runners) and print `SKIPPED (no dotnet)` otherwise.

---

## 8. Integration points

Needs:

- **core**: `result/error`, `slot_map`, `small_vector`, `delegate`, `json`, `uuid`, `hash::fnv1a64` (with
  published test vectors), `core::dynamic_library` (the single dlopen/LoadLibrary RAII wrapper, also used by
  `modules`; R30), `process` (spawn + capture stdout; **new requirement**),
  `jobs::post_to_main_thread`, `memory::frame_allocator`, `jobs::is_main_thread`, `core::reflect` field
  offsets for component layout hashing, log sink capture (tests). Names per R30.
- **ecs** (area A): trivially copyable 64-bit `entity`; O(1) `is_alive`; `world::observe(id, REMOVED)` firing
  before storage removal (R5); exclusive main-thread systems in FRAME_BEGIN / FIXED_UPDATE / UPDATE /
  LATE_UPDATE / FRAME_END that may mutate the world directly; `world::lock_iteration()` + `is_locked()` so the
  bindings can route structural ops into the script command buffer while a query scope is open (R5); chunk
  enumeration (`query::chunk(i)`, `chunk_view::column_data`) exposing entity array + raw column pointers for a
  type-id list; `register_dynamic` / `redefine_dynamic` / `has_instances` for `[EcsComponent]` (R4);
  `component_registry::find(name)` for name -> id lookup; custom `component_ops` and `component_serializer`
  hooks for `script_component` (R29).
- **scene**: `local_transform` layout stable and mirrored; scripts travel inside scene/prefab JSON as the
  `script_component` serializer's own output (R29), so scene makes no scripting call; `entity_remap` reaches the
  serializer's `remapEntities` hook on prefab instantiation and scene load; prefab/scene entry points for the
  `prefab_*` / `scene_*` bindings; `scene_manager::on_before_unload` event so `destroy_all` runs first.
- **platform**: `platform::input::snapshot()` (`input_state`) copyable into `conjure_input_block`.
  **physics**: `physics_world::contact_events()` span (R6); body/raycast functions. **audio**: play/stop/source
  parameters. **assets**: `file_watcher`, `asset_ref` uuid.
- **engine** (wave 4): owns `script_runtime`, writes `frame_block`, registers systems, sequences
  `flush_pending`, `set_mode` on play/stop, loads `project.json`, runs the export steps of 3.11.
- **bindings** (wave 4): fills every non-core `conjure_native_api` entry before `initialize()` by adding
  lines to `native_api.inl` and regenerating; adds `[NativeComponent]` mirrors beyond transform.
- **editor** (area D): inspector from `script_class_registry` + `set_field`; commands `script.compile`,
  `script.reload`, `script.gc`, `script.attach`, `component.remove`, `component.set` on `script.fields.*`
  (names follow the area D catalogue, R15); console shows managed exceptions, fault badges and build
  diagnostics; file watcher wiring to `managed_build_runner`.
- **modules** (area G): plugins may ship C# assemblies; they are loaded into the same game ALC (listed in the
  module manifest; a module reload of scope MANAGED_ONLY is `reload_game_assembly`, R36). The native table
  layout is fixed at engine build time: a plugin's `scripting::native_binding_desc { static constexpr
  core::string_id EXTENSION_POINT{"scripting.binding"}; const char *entryName; void *function; }` extension
  record fills an existing null slot by name; new slots need an `.inl` line in the engine tree (R36).

Offers: `script_runtime`, `register_script_systems`, `script_class_registry` schema, `field_blob` codecs,
`null_script_host` for every other module's tests, `managed_build_runner`, `hot_reload_controller`,
`Conjure.Managed`, the game project template, the generator.

---

## 9. Implementation plan (ordered)

1. `log_category.h`, `script_error.h`, `native_api.h` + `native_api.inl` with the core entries of 2.1 and the
   X-macro struct; `tools/gen_native_api.py` (emits `NativeApi.g.cs`, `CONJURE_NATIVE_API_HASH` into
   `native_api_hash.h`, `--check` mode); CTest `conjure_scripting_api_generated_up_to_date`.
2. `script_fields.h/.cpp`: blob writer/reader, `json_to_blob`, `blob_to_json`, `remap_entity_fields_json`;
   tests `test_script_fields.cpp`.
3. `script_class_registry.h/.cpp` (staging/active swap); `script_component.h/.cpp` (`script_store`, reverse
   map, `register_script_component`); tests.
4. `script_host.h`, `null_script_host.h/.cpp`, `null_host_recorder`; tests.
5. `script_runtime.h/.cpp` against the null host: add/remove, modes, flush, phases, collisions, destroy
   ordering, serialise/deserialise/remap, `on_component_removed`; `script_system.cpp`; tests
   `test_script_runtime.cpp`.
6. `hot_reload.h/.cpp` (debounce, frame boundary, sequence of 3.8 using the null host); tests.
7. `managed_build.h/.cpp` (parser first, then `core::process` spawn); tests.
8. `cmake/ConjureManaged.cmake` (`conjure_add_managed_project`, `conjure_managed_sources_match`), `managed/`
   projects: `Conjure.Managed` (Interop, Runtime, Api), `Conjure.Bootstrap`, `Conjure.Managed.Tests`,
   `Conjure.Templates/Game`, `tests/scripting/managed/Conjure.TestScripts` (v1/v2/v3 variants built from
   one project with `-p:ScriptVariant=`).
9. `hostfxr_api.h`, `hostfxr_runtime.{h,cpp}`, `dotnet_script_host.cpp` (3.1, both modes); tests
   `test_dotnet_script_host.cpp` `[dotnet]`.
10. End-to-end `[dotnet]` tests: lifecycle, fields round trip through reflection, exceptions, reload v1->v2->v3,
    collisions, query scope + layout mismatch, self-contained publish spike.
11. `[EcsComponent]` managed component registration and `Add/Get/SetComponent<T>` (needs ECS dynamic types).
12. Benchmarks `tests/benchmarks/bench_scripting.cpp`; `engine/src/scripting/README.md`; template `Player.cs`
    exercising the whole SDK (consumed by `samples/feature_test`).

---

## 10. Tests

`tests/scripting/` (doctest, `conjure_scripting_tests`; `[dotnet]` cases are skipped without the SDK):

- `test_script_fields.cpp`: `blob_roundtrip_every_kind`, `blob_rejects_truncated_payload`,
  `blob_rejects_bad_magic_and_version`, `json_to_blob_uses_defaults_for_missing_fields`,
  `blob_to_json_preserves_unknown_json_keys`, `list_fields_roundtrip`, `writer_reports_size_without_writing`,
  `empty_blob_is_valid`, `remap_rewrites_entity_and_list_of_entity`.
- `test_script_component.cpp`: `component_is_trivially_copyable_and_4_bytes`, `store_add_and_remove_entry`,
  `eight_scripts_per_entity_max`, `set_handle_invalid_after_free`, `reverse_map_updates_on_remove`.
- `test_script_class_registry.cpp`: `register_and_lookup_by_hash`, `duplicate_class_rejected_in_staging`,
  `swap_staging_replaces_active`, `fields_sorted_by_hash`, `callback_mask_preserved`.
- `test_native_api.cpp`: `struct_is_standard_layout`, `entry_count_matches_inl`, `all_entries_null_before_fill`,
  `hash_constant_matches_generator_output`, `validate_fails_on_missing_core_entries`.
- `test_null_script_host.cpp`: `initialize_fills_managed_api`, `slots_reused_lifo`, `collect_echoes_apply`,
  `run_phase_recorded_in_order`, `load_registers_fake_and_fixture_classes`, `unload_leak_configurable`.
- `test_script_runtime.cpp`: `edit_mode_creates_no_instances`, `enter_play_creates_all`,
  `add_script_creates_after_flush`, `on_create_before_first_phase`, `start_once_before_first_update_or_fixed`,
  `entity_destroy_from_script_runs_on_destroy_while_alive`, `native_remove_defers_on_destroy`,
  `exit_play_calls_on_destroy_once`, `collision_records_resolve_both_sides`,
  `collision_skips_classes_without_callback`, `reload_preserves_fields`, `reload_keeps_started_flag`,
  `reload_failure_keeps_old_assembly`, `reload_missing_class_preserves_json`,
  `disabled_runtime_keeps_entries_with_invalid_slot`, `serialize_deserialize_entity_roundtrip`,
  `frame_block_is_64_byte_aligned`, `flush_handles_creates_requested_during_on_create`.
- `test_hot_reload.cpp`: `debounce_coalesces_events`, `reload_runs_only_in_tick`,
  `state_idle_pending_reloading_idle`, `immediate_request_bypasses_debounce`.
- `test_managed_build.cpp`: `parse_error_and_warning_lines`, `parse_paths_with_spaces_and_columns`,
  `command_line_per_configuration`, `[dotnet] build_test_scripts_succeeds`.
- `test_dotnet_script_host.cpp` `[dotnet]`: `initialize_real_runtime_and_handshake`,
  `second_host_initialize_reuses_runtime`, `abi_mismatch_detected_when_hash_differs`, `wrong_managed_dir_fails`,
  `load_test_scripts_registers_classes_and_fields`, `conjure_managed_is_in_default_alc`,
  `self_contained_publish_hosts`.
- `test_script_lifecycle.cpp` `[dotnet]`: `create_start_update_counters_match`, `fixed_update_per_step`,
  `late_update_after_update`, `destroy_from_script_entity_alive_in_on_destroy`, `prefab_instantiate_scripts_same_frame`,
  `10k_instances_update_under_2ms` (loose), `shutdown_calls_on_destroy`.
- `test_script_hot_reload.cpp` `[dotnet]`: `reload_v1_to_v2_preserves_serialized_resets_private`,
  `reload_reruns_on_create_not_on_start`, `class_removed_in_v2_restored_in_v3`, `leaky_static_timer_warns_but_loads`,
  `reload_twice_alc_collected`.
- `test_script_exceptions.cpp` `[dotnet]`: `throwing_update_logged_and_disabled_after_three_consecutive`,
  `success_resets_fault_counter`, `throwing_on_create_marks_faulted`, `wrong_thread_call_throws`,
  `null_entry_throws_not_supported`.
- `test_script_fields_inspector.cpp` `[dotnet]`: `set_field_live_visible_via_collect`, `unknown_field_error`,
  `type_mismatch_defaults_and_warns`, `enum_names_in_meta`.
- `test_script_collisions.cpp` `[dotnet]`: `enter_exit_trigger_dispatched_to_overriders_only`,
  `one_record_per_instance_per_pair`.
- `test_dod_query.cpp` `[dotnet]`: `span_writes_visible_to_native`, `layout_mismatch_throws_and_logs_once`,
  `forgotten_dispose_force_closed_with_warn`, `structural_op_inside_scope_is_deferred`,
  `ecs_component_registered_and_roundtrips`.
- `tests/benchmarks/bench_scripting.cpp`: `10k_scripts_update_phase` (< 2 ms excluding user work),
  `10k_transform_set_via_facade`, `10k_transform_set_via_span`, `reload_10k_instances` (< 250 ms).
- `tests/integration/test_scripting_scene.cpp`: `scene_json_with_scripts_roundtrips`,
  `prefab_instantiate_remaps_entity_fields`, `scene_unload_calls_on_destroy_alive`.
- `managed/Conjure.Managed.Tests` (xunit): `FieldBlobRoundTrip`, `ClassDiscoveryFieldRules`,
  `OverrideDetectionCallbackMask`, `InstanceTableSlotReuse`, `PhaseListSwapRemove`,
  `NameHashMatchesNativeVectors`, `GameAlcResolvesConjureManagedToSdkAssembly`, `GameAlcUnloadsWhenUnreferenced`,
  `GuardDisablesAfterThreeConsecutive`, `InputBlockBitReads`, `NativeApiLayoutHashMatchesGenerated`.
- CTest checks: `conjure_scripting_api_generated_up_to_date`, `conjure_managed_sources_match`.

---

## 11. Out of scope / future

- Job-parallel managed code (`Parallel.For` over chunks) - would need a non-`ref struct` query handle and
  thread-safe bindings; the single-threaded span path meets the 10k target.
- Native AOT of the SDK or game assembly; ReadyToRun (ignored in collectible contexts anyway).
- Managed debugger attach helpers beyond `wait_for_debugger` (VS/Rider attach to the editor process works
  out of the box because the CLR is in-process).
- Serialising properties, nested user structs, dictionaries or polymorphic references in script fields.
- Script-defined ECS systems (managed systems scheduled by the native scheduler); scripts use
  `ComponentQuery` inside `OnUpdate` instead.
- Multiple simultaneous `script_runtime`s / multiple managed worlds; an editor that needs a separate
  play-world copy swaps `attach_world` and `set_mode` instead.
- Forced unload of leaked ALCs (impossible in .NET); only diagnostics are provided.
- Source-generated bindings for the SDK wrappers (the table is generated; wrappers stay hand-written).
