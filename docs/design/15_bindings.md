# 15 - Bindings module design (`conjure::bindings`, wave 4)

Status: **binding** (owner: lead architect). Expands `ARCHITECTURE.md` 4.6 and resolutions R4-R6, R26-R29,
R36 into an implementable design for module `conjure_bindings`. Headers in `engine/include/conjure/bindings/`,
sources in `engine/src/bindings/`, tests in `tests/bindings/`, managed files under `managed/Conjure.Managed/`
(listed in 4.3). Depends on `scripting`, `scene`, `physics`, `audio`, `assets`, `engine` (and transitively
`ecs`, `platform`, `modules`, `core`); `renderer` only through `engine_context::renderer` for debug draw.

The *mechanism* of the native/managed boundary (tables, handshake, frame block, `[UnmanagedCallersOnly]` entries,
guard, hot reload, DOD scope) is `03_scripting.md` and is not repeated. This document owns the *content*: every
non-core entry of `conjure_native_api`, its C# mirror, the `[NativeComponent]` mirrors beyond `LocalTransform`,
call counters, marshalling rules and the subsystem that installs it all before `script_runtime::initialize`.

---

## 1. Overview and decisions

| # | Decision | Rationale |
|---|---|---|
| B1 | **One `.inl`, one generator, one hash.** Every non-core entry is a line in `conjure/scripting/native_api.inl` under the comment `// --- bindings (R26) ---`, appended in group order `transform, entity, scene, prefab, physics, audio, input, asset, diagnostics, modules`. Bindings PRs edit that section and the record block of `native_api.h`; `tools/generate_native_api.py` regenerates `NativeApi.g.cs`, `native_api_hash.h` and (new) `docs/generated/native_api_manifest.json`. | The struct must be complete when `conjure_scripting` compiles (it is a member of `script_runtime`), so the lines cannot live in a wave-4 header. Splitting the table would need a second hash and a second handshake. |
| B2 | **Filtered X-macro.** `conjure/bindings/native_api_entries.h` re-expands the same `.inl` through `CONJURE_BINDINGS_ENTRY_<group>` macros so bindings code enumerates *only* the groups it owns (declarations, fill loop, descriptors) while `call_counters.h` enumerates *all* entries for `native_function_id`. | Zero duplication of names; a new line in the `.inl` without an implementation is a link error, not a null slot. |
| B3 | **Table is complete after `startup` in every configuration.** A compiled-out or disabled subsystem (`CONJURE_WITH_PHYSICS=OFF`, `rendering.enabled == false`, null audio) gets *stub* entries that return `-NOT_SUPPORTED` and set the last-error message naming the entry. No slot is ever left null for managed to stub (03 2.1 keeps that fallback for safety only). | `test_native_table_complete` holds on every CI job; C# always receives a status it can map, and the counter still increments so `feature_test` exclusions are about reachability, not configuration. |
| B4 | **Status convention for every bindings-owned `int32_t` entry: `>= 0` success (0, a count, a bool, a needed length), `< 0` failure = `-static_cast<int32_t>(bindings_error)`.** `uint64_t`-returning entries return `0` (`NULL_ENTITY`) on failure. Every failure writes the main-thread last-error buffer read by `diagnostics_last_error`. | One return value per call (no out-status parameter), no allocation, and the message is available only when the caller asks. Core-owned entries keep 03's semantics. |
| B5 | **Marshalling is blittable or (pointer, length).** Math types memcpy (core guarantees `math::quat` is stored x,y,z,w: glm 1.0.1 default, no define, `static_assert` on `offsetof`; recorded as R58), entity = `uint64_t`, uuid = `conjure_uuid {high, low}`, strings in = UTF-8 pointer + byte length (never NUL-terminated), strings out = caller buffer + capacity, return needed length. No `BSTR`, no `IntPtr` to managed memory held past the call. | D15 of 03; nothing allocates on the hot path; truncation is impossible to miss because the return value says so. |
| B6 | **Call counters are template thunks, not edits to implementations.** When `!CONJURE_DISTRIBUTION`, `startup` replaces every filled slot (core-owned ones included) with `counting_thunk<Id, Ret(Args...)>::invoke`, a unique forwarding function per entry that increments `sCounters[Id]` and calls the original. In Distribution the raw pointers are installed and `call_counters()` is an empty span. | Scripting's `native_api_core.cpp` needs no bindings dependency (acyclic graph) and no `CONJURE_BINDING_ENTER` macro; the `.inl` already carries `ret` and `args`, which form a function type the template can decompose. |
| B7 | **R5 routing lives in one helper.** `structural_guard` checks `world.is_locked()`; when locked, structural requests (`prefab_*` instantiate, `entity_duplicate/create_many/set_enabled/set_tag/set_layer`, `transform_set_parent*`, component add/remove performed by bindings) are recorded into `script_runtime::deferred_commands()` and flushed at `query_end`; non-structural writes (`transform_set_local`, velocities) go straight to the column. Entries that cannot be expressed as commands return `-STRUCTURE_LOCKED` instead of silently doing nothing. | The ECS refuses structural calls while locked (01 D6); scripts should not need to know which API is structural, except for the few that are impossible to defer. |
| B8 | **Audio playback is data, not a side channel.** `audio_play/stop/pause/resume` write the `playbackRequest` field of `audio::audio_source` (`12_audio.md` 2.6, R50); `engine.audio_sync` (SYNC) consumes the request, talks to the mixer and writes `isPlaying` back. One-shots with no entity go to `audio_engine::play(clip, play_params{ retain = false })` directly. | Bindings cannot reach the engine's private `audio_subsystem` and must not own an entity -> voice map of its own; the component already is that map (R7). Latency: request in UPDATE, mixer command in SYNC of the same frame. |
| B9 | **Asset residency from scripts is owned by the subsystem.** `asset_load` stores the returned `asset_handle` in `script_residency` keyed by uuid; `asset_release` erases it; `scene_manager::on_before_unload` and `script_runtime` PLAY -> EDIT clear it. `asset_ref_get` never loads; `AssetRef<T>.Get`-style payload access does not exist in C# (payloads are native). | Scripts get Unity-like `Load()`/`IsLoaded`, and the engine keeps the invariant "residency is owned by handles" (05 D-section). |
| B10 | **`[NativeComponent]` mirrors are verified by `core::reflect::type_info::layout_hash()`** (`10_core.md` 2.11: FNV-1a 64 over `(name, kind, offset, size)` per field in order). C# computes the same hash with field names derived by lower-casing the first letter of the C# field (`LinearVelocity` -> `linearVelocity`, R31) and kinds derived from the C# field type. The informal "(offset, size) + total size" of 03 3.9 is superseded by this single definition. | One hash definition, owned by core, already implemented there. |
| B11 | **C# exceptions carry the status and the native message.** `NativeCheck.ThrowIfFailed(status)` reads `diagnostics_last_error` and throws `ConjureNativeException` or a subclass (`EntityNotAliveException`, `AssetNotLoadedException`, `NotSupportedException`, `InvalidOperationException` for `WRONG_THREAD`/`NOT_IN_PHASE`/`STRUCTURE_LOCKED`). Predicates (`IsAlive`, `IsPlaying`, `HasComponent`) never throw for dead entities; they return false. | Unity-like ergonomics; the D11 guard turns unhandled exceptions into logged faults, never crashes. |
| B12 | **`Conjure.Modules.Registry` is read-only and index-based.** Four entries (`count`, `find`, `info`, `name`) over `modules::module_registry`; no load/unload from scripts. | Loading modules is an editor command with undo semantics (R14); scripts only need "is feature X present". |
| B13 | **A `modules_*` group exists.** ARCHITECTURE 4.6 listed eight non-core prefixes; the registry needs a ninth, named for what it is rather than hidden under `diagnostics_`. With the hierarchy half of `entity_*` (2.3) bindings owns ten groups (R64 amends R26). | Prefix = C# static class; `Diagnostics.Modules` would mislead. |
| B14 | **Debug drawing is `diagnostics_draw_*`.** Lines/boxes/spheres go to `renderer::debug_draw` through `engine_context::renderer` (no-op success when null); text goes to the engine's `debug_overlay_layer` and is a no-op in Distribution. | The `feature_test` `DebugDrawProbe` needs it; debug draw is a diagnostic facility, not gameplay rendering. |

Rejected: a second generated table for bindings (two hashes, two handshakes); source-generated C# wrappers (03 11);
per-entry `std::function` thunks (allocation and an indirect call per binding); exceptions as the status channel
(engine code never throws); `System.Guid` for uuids (mixed-endian byte layout differs from `core::uuid`).

---

## 2. Data model

### 2.1 Marshalling rules (`marshal.h`)

| Native concept | ABI type | C# type | Rule |
|---|---|---|---|
| `ecs::entity` | `uint64_t` (`entity::to_u64()` = `generation << 32 \| index`) | `readonly struct Entity { ulong Id; }` | `0` is `NULL_ENTITY` and `default(Entity)`. Never a pointer. |
| `core::uuid` / `assets::asset_id` | `conjure_uuid { uint64_t high; uint64_t low; }` (16 bytes, by pointer) | `readonly struct AssetId { ulong High, Low; }` | `AssetId.ToString()` / `Parse` produce the canonical lower-case form of `uuid::to_string`. Not `System.Guid`. |
| `math::vec2/3/4` | `conjure_vec2/3/4` | `Vector2/3/4` | memcpy; `static_assert(sizeof(conjure_vec3) == sizeof(math::vec3))`. |
| `math::quat` | `conjure_quat_xyzw` | `Quaternion` | memcpy; core static_asserts the x,y,z,w storage of glm 1.0.1 (R58, no define) and `tests/core/math/test_math.cpp` plus `test_marshal::quat_storage_is_xyzw` enforce it (a layout hash cannot: a QUAT is 16 bytes either way). |
| `math::mat4` (column-major) | `conjure_mat4 { float m[16]; }` | `Matrix4x4` | memcpy. glm's column-major/column-vector matrix and `System.Numerics`' row-major/row-vector matrix have identical bytes for the same transformation; C# uses `Vector3.Transform(v, m)`. |
| `math::transform` | `conjure_transform { vec3 position; quat_xyzw rotation; vec3 scale; }` (40 bytes) | `struct TransformData` | memcpy; also the `LocalTransform` mirror. |
| string in | `(const char *utf8, int32_t length)` | `ReadOnlySpan<char>` -> `Utf8Marshal.Encode` into `stackalloc byte[256]` or `ArrayPool` | `length` is bytes, not code points; `nullptr` with `length > 0` -> `-INVALID_ARGUMENT`; `length == 0` is the empty string. |
| string out | `(char *utf8, int32_t capacity) -> int32_t needed` | `Utf8Marshal.Read(Func<Span<byte>, int>)` | Writes `min(needed, capacity)` bytes, no terminator, returns `needed`; caller retries once with `needed` bytes. Mirrors `entity_get_name` of 03. |
| `core::string_id` | `uint64_t` hash | `readonly struct StringId { ulong Hash; }` | Tags and audio groups cross as hashes; `NameHash.Fnv1a64` is the managed twin (shared test vectors). |
| colours | `uint32_t rgba` (0xRRGGBBAA) | `Color` (floats) | `Color.ToRgba32()` in C#. |
| bools | `int32_t` (0/1) in and out | `bool` | Never a C `bool` in the ABI. |
| enums | `uint8_t` with the value list fixed in `native_api.h` | `enum : byte` | `conjure_force_mode`, `conjure_cursor_mode`, `conjure_button_action`, `conjure_cast_shape`, `conjure_asset_load_priority`. |
| results | `int32_t` status (B4) + `diagnostics_last_error` | exception via `NativeCheck` | 2.2. |
| arrays out | `(T *out, int32_t capacity) -> int32_t count` | `Span<T>` | Writes `min(count, capacity)` elements, returns the full count; a `capacity == 0` call is a count query. |

Pointers inside records are valid for the duration of the call only (03 2.1). Every record is standard-layout,
`static_assert`ed for size, and mirrored in `Interop/BindingRecords.cs` with `StructLayout(LayoutKind.Sequential)`.

### 2.2 Status codes and the last-error buffer (`bindings_error.h`)

```cpp
enum class bindings_error : u16 {
    NONE = 0,
    NOT_SUPPORTED,          // subsystem compiled out, disabled, or null backend cannot answer
    ENTITY_NOT_ALIVE,
    COMPONENT_MISSING,      // entity lacks the component the entry operates on (rigid_body, audio_source, ...)
    INVALID_ARGUMENT,       // null pointer with non-zero length, negative capacity, bad enum value
    NOT_FOUND,              // path, name, layer, module, guid
    BUFFER_TOO_SMALL,       // only from record-array entries that cannot report a count
    WRONG_THREAD,
    NOT_IN_PHASE,           // called outside a script phase (e.g. from a Task)
    STRUCTURE_LOCKED,       // structural request inside a query scope that cannot be deferred (B7)
    ASSET_NOT_LOADED,
    ASSET_TYPE_MISMATCH,
    SCENE_NOT_FOUND,
    PREFAB_INVALID,         // asset loaded but payload is not an entity archive / archive fails validate()
    TABLE_INCOMPLETE,       // startup: an owned slot could not be filled (only reachable in tests)
    INTERNAL                // any other core::error from a called module; message carries error.to_string()
};
[[nodiscard]] std::string_view bindings_error_name(bindings_error code) noexcept;
CONJURE_DECLARE_ERROR_CATEGORY(bindings_error, "bindings", bindings_error_name);

[[nodiscard]] constexpr int32_t to_status(bindings_error code) noexcept;          // -static_cast<int32_t>(code)
[[nodiscard]] int32_t fail(bindings_error code, std::string_view detail = {}) noexcept;  // sets last error, returns status
[[nodiscard]] int32_t fail(const core::error &failure) noexcept;                  // maps ecs/assets/scene errors
[[nodiscard]] bindings_error last_error_code() noexcept;
[[nodiscard]] std::string_view last_error_message() noexcept;
void clear_last_error() noexcept;
```

The last-error state is one `std::array<char, 512>` plus a code, main-thread only (every entry is), written with
`std::format_to_n`, never allocating. `fail(const core::error &)` maps `ecs_error::ENTITY_NOT_ALIVE` ->
`ENTITY_NOT_ALIVE`, `ecs_error::STRUCTURE_LOCKED` -> `STRUCTURE_LOCKED`, `ecs_error::WRONG_THREAD` ->
`WRONG_THREAD`, `ecs_error::COMPONENT_NOT_PRESENT` -> `COMPONENT_MISSING`, `asset_error_code::NOT_FOUND` ->
`NOT_FOUND`, `TYPE_MISMATCH` -> `ASSET_TYPE_MISMATCH`, everything else -> `INTERNAL` with `error.to_string()`.
`bindings_error.inl` lists the enumerators once so the generator can emit `Interop/BindingStatus.g.cs`; the
status list is append-only and is **not** part of the ABI layout hash.

### 2.3 Native API entries owned by this module

Exactly these lines are appended to `native_api.inl` under `// --- bindings (R26) ---`. Return rule: B4.
"struct." = structural request routed by B7 when the world is locked.

```cpp
// transform_*  (scene::local_transform / world_transform; get_world* compose locals up the parent chain, O(depth))
CONJURE_NATIVE_FN(transform, get_local,            int32_t,  (uint64_t entity, conjure_transform *out))
CONJURE_NATIVE_FN(transform, set_local,            int32_t,  (uint64_t entity, const conjure_transform *value))
CONJURE_NATIVE_FN(transform, get_world_matrix,     int32_t,  (uint64_t entity, conjure_mat4 *out))
CONJURE_NATIVE_FN(transform, set_parent,           int32_t,  (uint64_t entity, uint64_t parent))                 // keepWorld = 1, struct.
CONJURE_NATIVE_FN(transform, get_world,            int32_t,  (uint64_t entity, conjure_transform *out))
CONJURE_NATIVE_FN(transform, set_world,            int32_t,  (uint64_t entity, const conjure_transform *value))  // local = inverse(parentWorld) * value
CONJURE_NATIVE_FN(transform, set_parent_ex,        int32_t,  (uint64_t entity, uint64_t parent, int32_t keepWorld, int32_t index))  // struct.
CONJURE_NATIVE_FN(transform, get_interpolated,     int32_t,  (uint64_t entity, conjure_transform *out))          // physics-written; COMPONENT_MISSING otherwise

// entity_* beyond core (hierarchy, tag, layer, guid, enabled: need scene::scene)
CONJURE_NATIVE_FN(entity,    set_name,             int32_t,  (uint64_t entity, const char *utf8, int32_t length))
CONJURE_NATIVE_FN(entity,    get_parent,           uint64_t, (uint64_t entity))
CONJURE_NATIVE_FN(entity,    get_root,             uint64_t, (uint64_t entity))
CONJURE_NATIVE_FN(entity,    get_children,         int32_t,  (uint64_t entity, uint64_t *out, int32_t capacity))  // count
CONJURE_NATIVE_FN(entity,    set_enabled,          int32_t,  (uint64_t entity, int32_t enabled))              // struct. (tag add/remove)
CONJURE_NATIVE_FN(entity,    is_enabled,           int32_t,  (uint64_t entity))
CONJURE_NATIVE_FN(entity,    get_tag,              uint64_t, (uint64_t entity))                               // string_id hash, 0 = none
CONJURE_NATIVE_FN(entity,    set_tag,              int32_t,  (uint64_t entity, const char *utf8, int32_t length))  // struct. when tag component absent
CONJURE_NATIVE_FN(entity,    get_layer,            int32_t,  (uint64_t entity))                               // layer index
CONJURE_NATIVE_FN(entity,    set_layer,            int32_t,  (uint64_t entity, int32_t layer))                // struct. when layer component absent
CONJURE_NATIVE_FN(entity,    get_guid,             int32_t,  (uint64_t entity, conjure_uuid *out))
CONJURE_NATIVE_FN(entity,    find_by_guid,         uint64_t, (const conjure_uuid *guid))
CONJURE_NATIVE_FN(entity,    find_by_tag,          int32_t,  (const char *utf8, int32_t length, uint64_t *out, int32_t capacity))
CONJURE_NATIVE_FN(entity,    find_by_layer,        int32_t,  (int32_t layer, uint64_t *out, int32_t capacity))
CONJURE_NATIVE_FN(entity,    duplicate,            uint64_t, (uint64_t entity))                               // subtree, scripts deep-copied (R29); struct.
CONJURE_NATIVE_FN(entity,    create_many,          int32_t,  (int32_t count, uint64_t parent, uint64_t *out)) // scene::create_entities; struct.

// scene_*  (scene::scene_manager; loads are requests applied at FRAME_BEGIN, never immediate)
CONJURE_NATIVE_FN(scene,     request_load,         int32_t,  (const char *utf8Path, int32_t length, int32_t additive))
CONJURE_NATIVE_FN(scene,     request_load_asset,   int32_t,  (const conjure_uuid *scene, int32_t additive))
CONJURE_NATIVE_FN(scene,     request_unload,       int32_t,  (const char *utf8Path, int32_t length))
CONJURE_NATIVE_FN(scene,     is_loaded,            int32_t,  (const char *utf8Path, int32_t length))
CONJURE_NATIVE_FN(scene,     is_load_pending,      int32_t,  (const char *utf8Path, int32_t length))
CONJURE_NATIVE_FN(scene,     active_path,          int32_t,  (char *utf8, int32_t capacity))
CONJURE_NATIVE_FN(scene,     loaded_count,         int32_t,  (void))
CONJURE_NATIVE_FN(scene,     loaded_path,          int32_t,  (int32_t index, char *utf8, int32_t capacity))
CONJURE_NATIVE_FN(scene,     entity_count,         int32_t,  (void))
CONJURE_NATIVE_FN(scene,     structure_version,    uint64_t, (void))
CONJURE_NATIVE_FN(scene,     roots,                int32_t,  (uint64_t *out, int32_t capacity))

// prefab_*  (conjure.scene.prefab assets; the asset must be LOADED, else ASSET_NOT_LOADED)
CONJURE_NATIVE_FN(prefab,    instantiate,          int32_t,  (const conjure_uuid *prefab, uint64_t parent, const conjure_transform *localOrNull, uint64_t *outRoot))  // struct.
CONJURE_NATIVE_FN(prefab,    instantiate_many,     int32_t,  (const conjure_uuid *prefab, uint64_t parent, int32_t count, uint64_t *outRoots))              // struct.
CONJURE_NATIVE_FN(prefab,    source_of,            int32_t,  (uint64_t entity, conjure_uuid *outPrefab))      // prefab_instance component
CONJURE_NATIVE_FN(prefab,    is_instance_root,     int32_t,  (uint64_t entity))

// physics_*  (physics::physics_world; body entries need rigid_body, character entries need character_controller)
CONJURE_NATIVE_FN(physics,   raycast,              int32_t,  (const conjure_ray_query *query, conjure_raycast_hit *outHit))                    // 1 hit / 0 miss
CONJURE_NATIVE_FN(physics,   raycast_all,          int32_t,  (const conjure_ray_query *query, conjure_raycast_hit *outHits, int32_t capacity))  // count, by distance
CONJURE_NATIVE_FN(physics,   shape_cast,           int32_t,  (const conjure_shape_query *query, conjure_raycast_hit *outHit))
CONJURE_NATIVE_FN(physics,   overlap_sphere,       int32_t,  (const conjure_vec3 *center, float radius, uint32_t layerMask, uint64_t *out, int32_t capacity))
CONJURE_NATIVE_FN(physics,   overlap_box,          int32_t,  (const conjure_vec3 *center, const conjure_vec3 *halfExtents, const conjure_quat_xyzw *rotation, uint32_t layerMask, uint64_t *out, int32_t capacity))
CONJURE_NATIVE_FN(physics,   get_linear_velocity,  int32_t,  (uint64_t entity, conjure_vec3 *out))
CONJURE_NATIVE_FN(physics,   set_linear_velocity,  int32_t,  (uint64_t entity, const conjure_vec3 *value))
CONJURE_NATIVE_FN(physics,   get_angular_velocity, int32_t,  (uint64_t entity, conjure_vec3 *out))
CONJURE_NATIVE_FN(physics,   set_angular_velocity, int32_t,  (uint64_t entity, const conjure_vec3 *value))
CONJURE_NATIVE_FN(physics,   add_force,            int32_t,  (uint64_t entity, const conjure_vec3 *force, uint8_t mode))    // conjure_force_mode -> physics_world add_force / add_impulse (14_physics.md 8, R49)
CONJURE_NATIVE_FN(physics,   add_force_at,         int32_t,  (uint64_t entity, const conjure_vec3 *force, const conjure_vec3 *worldPoint, uint8_t mode))
CONJURE_NATIVE_FN(physics,   add_torque,           int32_t,  (uint64_t entity, const conjure_vec3 *torque, uint8_t mode))
CONJURE_NATIVE_FN(physics,   move_kinematic,       int32_t,  (uint64_t entity, const conjure_transform *target))   // applied by physics.push_transforms
CONJURE_NATIVE_FN(physics,   teleport,             int32_t,  (uint64_t entity, const conjure_transform *target))   // resets velocities
CONJURE_NATIVE_FN(physics,   is_sleeping,          int32_t,  (uint64_t entity))
CONJURE_NATIVE_FN(physics,   wake,                 int32_t,  (uint64_t entity))
CONJURE_NATIVE_FN(physics,   get_gravity,          int32_t,  (conjure_vec3 *out))
CONJURE_NATIVE_FN(physics,   set_gravity,          int32_t,  (const conjure_vec3 *value))
CONJURE_NATIVE_FN(physics,   layer_index,          int32_t,  (const char *utf8, int32_t length))             // index or -NOT_FOUND
CONJURE_NATIVE_FN(physics,   layer_name,           int32_t,  (int32_t layer, char *utf8, int32_t capacity))
CONJURE_NATIVE_FN(physics,   layers_collide,       int32_t,  (int32_t layerA, int32_t layerB))
CONJURE_NATIVE_FN(physics,   character_move,       int32_t,  (uint64_t entity, const conjure_vec3 *velocity))   // desired velocity for the next step
CONJURE_NATIVE_FN(physics,   character_is_grounded,int32_t,  (uint64_t entity))
CONJURE_NATIVE_FN(physics,   character_velocity,   int32_t,  (uint64_t entity, conjure_vec3 *out))

// audio_*  (B8: entity entries write audio_source.playbackRequest; the engine's SYNC system applies them)
CONJURE_NATIVE_FN(audio,     play,                 int32_t,  (uint64_t entity))
CONJURE_NATIVE_FN(audio,     stop,                 int32_t,  (uint64_t entity))
CONJURE_NATIVE_FN(audio,     pause,                int32_t,  (uint64_t entity))
CONJURE_NATIVE_FN(audio,     resume,               int32_t,  (uint64_t entity))
CONJURE_NATIVE_FN(audio,     is_playing,           int32_t,  (uint64_t entity))                               // audio_source.isPlaying (one frame old)
CONJURE_NATIVE_FN(audio,     play_one_shot,        int32_t,  (const conjure_uuid *clip, const conjure_vec3 *positionOrNull, float volume, float pitch, uint64_t groupHash))
CONJURE_NATIVE_FN(audio,     set_group_gain,       int32_t,  (uint64_t groupHash, float gain))
CONJURE_NATIVE_FN(audio,     get_group_gain,       int32_t,  (uint64_t groupHash, float *out))
CONJURE_NATIVE_FN(audio,     set_master_gain,      int32_t,  (float gain))
CONJURE_NATIVE_FN(audio,     get_master_gain,      int32_t,  (float *out))
CONJURE_NATIVE_FN(audio,     clip_duration,        int32_t,  (const conjure_uuid *clip, float *outSeconds))   // ASSET_NOT_LOADED until the clip is resident

// input_*  (everything readable per frame is in conjure_input_block, D4; these are the rest)
CONJURE_NATIVE_FN(input,     set_cursor_mode,      int32_t,  (uint8_t mode))                                  // platform::cursor_mode values (R56); works on the null window (R57)
CONJURE_NATIVE_FN(input,     get_cursor_mode,      int32_t,  (void))
CONJURE_NATIVE_FN(input,     gamepad_name,         int32_t,  (int32_t index, char *utf8, int32_t capacity))
CONJURE_NATIVE_FN(input,     inject_key,           int32_t,  (uint16_t key, uint8_t action))                  // platform::input::inject; visible next frame
CONJURE_NATIVE_FN(input,     inject_mouse_button,  int32_t,  (uint8_t button, uint8_t action))
CONJURE_NATIVE_FN(input,     inject_mouse_move,    int32_t,  (float x, float y))
CONJURE_NATIVE_FN(input,     inject_gamepad_button,int32_t,  (int32_t index, uint8_t button, uint8_t action))
CONJURE_NATIVE_FN(input,     inject_gamepad_axis,  int32_t,  (int32_t index, uint8_t axis, float value))
CONJURE_NATIVE_FN(input,     clipboard_get,        int32_t,  (char *utf8, int32_t capacity))
CONJURE_NATIVE_FN(input,     clipboard_set,        int32_t,  (const char *utf8, int32_t length))

// asset_*  (assets::asset_manager; typeId = assets::asset_type_id of the type name)
CONJURE_NATIVE_FN(asset,     ref_get,              int32_t,  (const conjure_uuid *id, uint64_t typeId, conjure_asset_info *out))  // state/version, never loads
CONJURE_NATIVE_FN(asset,     ref_bind,             int32_t,  (const conjure_uuid *id, uint64_t typeId, const void **outSlot))     // asset_manager::bind_slot; for AssetRef fields in mirrors
CONJURE_NATIVE_FN(asset,     find,                 int32_t,  (const char *utf8Path, int32_t length, conjure_uuid *outId))
CONJURE_NATIVE_FN(asset,     path_of,              int32_t,  (const conjure_uuid *id, char *utf8, int32_t capacity))           // NOT_SUPPORTED in cooked builds without a path table
CONJURE_NATIVE_FN(asset,     load,                 int32_t,  (const conjure_uuid *id, uint64_t typeId, uint8_t priority))      // B9 residency
CONJURE_NATIVE_FN(asset,     load_blocking,        int32_t,  (const conjure_uuid *id, uint64_t typeId))                        // R23: WARN once per frame in Development
CONJURE_NATIVE_FN(asset,     release,              int32_t,  (const conjure_uuid *id))
CONJURE_NATIVE_FN(asset,     state_of,             int32_t,  (const conjure_uuid *id))                                         // assets::asset_state
CONJURE_NATIVE_FN(asset,     type_of,              int32_t,  (const conjure_uuid *id, uint64_t *outTypeId))
CONJURE_NATIVE_FN(asset,     type_id,              uint64_t, (const char *utf8TypeName, int32_t length))                       // make_asset_type_id; 0 = unknown

// diagnostics_*
CONJURE_NATIVE_FN(diagnostics, check,              int32_t,  (const char *utf8Name, int32_t nameLength, int32_t passed, const char *utf8Detail, int32_t detailLength))
CONJURE_NATIVE_FN(diagnostics, last_error,         int32_t,  (int32_t *outStatus, char *utf8, int32_t capacity))              // needed length; clears nothing
CONJURE_NATIVE_FN(diagnostics, draw_line,          int32_t,  (const conjure_vec3 *a, const conjure_vec3 *b, uint32_t rgba, float seconds))
CONJURE_NATIVE_FN(diagnostics, draw_box,           int32_t,  (const conjure_vec3 *center, const conjure_vec3 *halfExtents, const conjure_quat_xyzw *rotation, uint32_t rgba, float seconds))
CONJURE_NATIVE_FN(diagnostics, draw_sphere,        int32_t,  (const conjure_vec3 *center, float radius, uint32_t rgba, float seconds))
CONJURE_NATIVE_FN(diagnostics, draw_text,          int32_t,  (const conjure_vec3 *position, const char *utf8, int32_t length, uint32_t rgba, float seconds))
CONJURE_NATIVE_FN(diagnostics, frame_stats,        int32_t,  (conjure_frame_stats_block *out))
CONJURE_NATIVE_FN(diagnostics, call_count,         int32_t,  (int32_t functionIndex))                                         // -NOT_SUPPORTED in Distribution

// modules_*  (read-only view of modules::module_registry, B12)
CONJURE_NATIVE_FN(modules,   count,                int32_t,  (void))
CONJURE_NATIVE_FN(modules,   find,                 int32_t,  (const char *utf8Name, int32_t length))          // index in load order or -NOT_FOUND
CONJURE_NATIVE_FN(modules,   info,                 int32_t,  (int32_t index, conjure_module_record *out))
CONJURE_NATIVE_FN(modules,   name,                 int32_t,  (int32_t index, char *utf8, int32_t capacity))
```

106 entries. The four `transform_*` lines already present in 03 2.1 are the first four above (same signatures;
`transform_set_parent` keeps world position, Unity's default). Appending only; a signature change bumps
`CONJURE_NATIVE_API_VERSION` (03 D3).

### 2.4 ABI records added to `native_api.h` (section `// bindings records (R26)`)

```cpp
struct conjure_uuid              { uint64_t high; uint64_t low; };                                  // 16
struct conjure_ray_query         { conjure_vec3 origin; conjure_vec3 direction; float max_distance; uint32_t layer_mask; uint64_t ignore_entity; };  // 40
struct conjure_shape_query       { conjure_ray_query ray; uint8_t shape; uint8_t pad[3]; conjure_vec3 half_extents; float radius; conjure_quat_xyzw rotation; };  // 80
struct conjure_raycast_hit       { uint64_t entity; conjure_vec3 point; conjure_vec3 normal; float distance; int32_t layer; };   // 40
struct conjure_asset_info        { uint8_t state; uint8_t pad[3]; uint32_t version; uint64_t type_id; };                        // 16
struct conjure_module_record     { uint32_t version_major; uint32_t version_minor; uint32_t version_patch; uint8_t state; uint8_t kind; uint8_t pad[2]; uint32_t feature_flags; };  // 20
struct conjure_frame_stats_block { float frame_ms; float phase_ms[10]; float submit_ms; uint32_t fixed_steps; uint32_t jobs_executed; uint64_t frame_memory_high_water; };          // 64

enum class conjure_force_mode : uint8_t   { FORCE, IMPULSE, VELOCITY_CHANGE, ACCELERATION };
enum class conjure_cast_shape : uint8_t   { SPHERE, BOX, CAPSULE };
enum class conjure_cursor_mode : uint8_t  { NORMAL, HIDDEN, DISABLED, CAPTURED };                      // platform::cursor_mode values (11_platform.md 2.4, R56)
enum class conjure_button_action : uint8_t { RELEASE, PRESS, REPEAT };                                // platform::button_action values (R56); "tap" is C# sugar = PRESS now, RELEASE queued for the next frame by Input.Inject
enum class conjure_asset_load_priority : uint8_t { LOW, NORMAL, HIGH };
```

Snake_case fields are the documented C-ABI exception (R31). Sizes are `static_assert`ed natively and checked in
`Conjure.Managed.Tests` (`BindingRecordSizesMatchDocumented`); the generator does not parse records (03 4.1), so this table is the contract.

### 2.5 C# facade types (`Conjure.Managed/Api`)

Facades are `readonly struct`s holding one `Entity` (03 4.1) or `static` classes; only `Modules.Registry` and
`AssetRef<T>.Path` allocate. Every call is `NativeBridge.Api->group_name(...)` then `NativeCheck.ThrowIfFailed(status)`.

| Type | Surface (per-call native entry) |
|---|---|
| `Transform` | `Local {get;set;}` (`transform_get_local`/`set_local`), `Position`/`Rotation`/`Scale` (get + set = two calls, documented), `World {get;set;}`, `WorldMatrix`, `Forward/Right/Up` (computed from `Rotation`), `Parent` (`entity_get_parent`), `SetParent(Entity, bool keepWorld = true, int index = -1)`, `ChildCount`, `GetChildren(Span<Entity>)`, `Translate`, `Rotate`, `LookAt`, `Interpolated` |
| `Rigidbody` (03's `RigidBody`, renamed here) | `LinearVelocity`, `AngularVelocity`, `AddForce(Vector3, ForceMode = Force)`, `AddForceAtPosition`, `AddTorque`, `MoveKinematic(TransformData)`, `Teleport(TransformData)`, `IsSleeping`, `WakeUp()`, `Data` (`RigidBodyData` mirror via `component_get/set`) |
| `CharacterController` | `Move(Vector3 velocity)`, `IsGrounded`, `Velocity`, `Data` |
| `Physics` (static) | `Raycast(Vector3 origin, Vector3 dir, float maxDistance, out RaycastHit, LayerMask mask = All, Entity ignore = default)`, `RaycastAll(..., Span<RaycastHit>)`, `SphereCast/BoxCast/CapsuleCast`, `OverlapSphere(center, radius, Span<Entity>, mask)`, `OverlapBox`, `Gravity {get;set;}`, `LayerMask Layer(string)`, `LayerName(int)`, `LayersCollide(int, int)`; `readonly struct LayerMask { uint Bits; }`, `readonly struct RaycastHit` = `conjure_raycast_hit` |
| `AudioSource` | `Play()`, `Stop()`, `Pause()`, `Resume()`, `IsPlaying`, `Volume/Pitch/Loop {get;set;}` (component field set on the `AudioSourceData` mirror), `Data` |
| `Audio` (static) | `PlayOneShot(AssetRef<AudioClipAsset>, Vector3? position = null, float volume = 1, float pitch = 1, string group = null)`, `MasterGain`, `GetGroupGain(string)`, `SetGroupGain(string, float)`, `ClipDuration(AssetRef<AudioClipAsset>)` |
| `Prefab` | `AssetRef<PrefabAsset> Asset`, `IsLoaded`, `Load()`, `Entity Instantiate(Entity parent = default, TransformData? local = null)`, `int InstantiateMany(int count, Span<Entity> roots, Entity parent = default)`, `static Prefab Find(string path)`, `static AssetRef<PrefabAsset> SourceOf(Entity)`, `static bool IsInstanceRoot(Entity)` |
| `Scene` (static) | `Load(string path, bool additive = false)` (request), `Load(AssetRef<SceneAsset>, bool additive)`, `Unload(string)`, `IsLoaded`, `IsLoadPending`, `ActivePath`, `LoadedCount`, `LoadedPath(int)`, `EntityCount`, `StructureVersion`, `GetRoots(Span<Entity>)` |
| `Input` (static partial; the frame-block half is scripting's) | `CursorMode {get;set;}`, `GamepadName(int)`, `Clipboard {get;set;}`, nested `Input.Inject`: `Key(Key, ButtonAction)`, `MouseButton`, `MouseMove`, `GamepadButton`, `GamepadAxis` |
| `AssetRef<T> where T : struct, IAssetType` | `AssetId Id`, `IsValid`, `State` (`AssetState`), `IsLoaded`, `Version`, `Load(AssetLoadPriority = Normal)`, `LoadBlocking()`, `Release()`, `Path`, `static AssetRef<T> Find(string path)`; `IAssetType { static abstract string TypeName; }` with tags `TextureAsset`, `MeshAsset`, `MaterialAsset`, `ModelAsset`, `EnvironmentAsset`, `AudioClipAsset`, `PrefabAsset`, `SceneAsset`, `CollisionMeshAsset` (`conjure.physics.collision_mesh`, R44); `AssetTypeId<T>.Value` cached once per class load through `asset_type_id` |
| `Diagnostics` (static) | `Check(string name, bool passed, string detail = "")`, `Diagnostics.Draw.Line/Box/Sphere/Text(..., Color, float seconds = 0)`, `FrameStats Stats`, `int CallCount(NativeFunction)` (`NativeFunction` is the generated enum of `.inl` indices) |
| `Modules.Registry` (static) | `Count`, `ModuleInfo Get(int)`, `ModuleInfo? Find(string)`, `bool IsLoaded(string)`; `readonly struct ModuleInfo { string Name; Version Version; ModuleState State; ModuleKind Kind; uint FeatureFlags; }` |
| `Entity` (partial, hierarchy half) | `Name {set;}`, `Parent`, `Root`, `GetChildren`, `Enabled {get;set;}`, `Tag {get;set;}` (`StringId` / string), `Layer {get;set;}`, `Guid`, `Duplicate()`, `static Entity FindByGuid`, `static int FindByTag(string, Span<Entity>)`, `static int FindByLayer`, `static int CreateMany(int, Span<Entity>, Entity parent = default)` |

### 2.6 `[NativeComponent]` mirrors beyond `LocalTransform` (`Api/Components/*.cs`)

`[NativeComponent("<registered name>")] [StructLayout(LayoutKind.Sequential)] public struct X : IComponent`.
`ComponentType<T>.Id` resolves once per class load through `component_lookup_type(nameHash, size, layoutHash)`
(03 3.9); a mismatch yields id 0 and `InvalidOperationException` on first use. Layout hash per B10. Field name
mapping: C# `PascalCase` -> native `camelCase`; kinds: `float` F32, `int` I32, `uint` U32, `byte` U8, `ulong` U64,
`NativeBool` (1 byte) BOOL, `Vector2/3/4`, `Quaternion` QUAT, `Matrix4x4` MAT4, `TransformData` TRANSFORM,
`Entity` ENTITY, `StringId` STRING_ID, `AssetId` UUID, `AssetRef<T>` ASSET_REF (24 bytes: `AssetId Id; nint Slot`),
`enum : byte/int` ENUM. Tag components are empty structs used only with `Has/Add/RemoveComponent`.

| Mirror | Native type (registering module) | Fields (declaration order) | Size |
|---|---|---|---|
| `LocalTransform` | `scene.local_transform` (scene; already in 03) | `Vector3 Position; Quaternion Rotation; Vector3 Scale` | 40 |
| `WorldTransform` | `scene.world_transform` (read-only by convention) | `Matrix4x4 Matrix` | 64 |
| `InterpolatedTransform` | `scene.interpolated_transform` | `TransformData Previous; TransformData Current` | 80 |
| `Parent` | `scene.parent` | `Entity Value; uint SiblingIndex; uint Pad0` (`13_scene.md` 2.1, R55) | 16 |
| `Name` | `scene.name` | `StringId Value` | 8 |
| `Tag` | `scene.tag` | `StringId Value` | 8 |
| `Layer` | `scene.layer` | `byte Index` (`u8` natively, R55) | 1 |
| `EntityGuid` | `scene.guid` | `AssetId Value` (two `ulong`) | 16 |
| `Disabled`, `TransformDirty` | `scene.disabled`, `scene.transform_dirty` | tags | 0 |
| `PrefabInstance` | `scene.prefab_instance` | `AssetRef<PrefabAsset> Prefab` (the instance root is the carrying entity; R55) | 24 |
| `RigidBodyData` | `physics.rigid_body` (`14_physics.md` 2.3, R48) | `MotionType MotionType (byte); byte Layer; RigidBodyFlags Flags (ushort); float Mass; float LinearDamping; float AngularDamping; float GravityFactor; float MaxLinearVelocity; Vector3 LinearVelocity; Vector3 AngularVelocity; NativeBool IsSleeping; byte Pad0..2` | 52 |
| `BoxColliderData` | `physics.box_collider` | `Vector3 HalfExtents; Vector3 Offset; Quaternion Rotation; NativeBool IsTrigger; byte Pad0..2` | 44 |
| `SphereColliderData` | `physics.sphere_collider` | `float Radius; Vector3 Offset; NativeBool IsTrigger; byte Pad0..2` | 20 |
| `CapsuleColliderData` | `physics.capsule_collider` | `float Radius; float HalfHeight; Vector3 Offset; Quaternion Rotation; NativeBool IsTrigger; byte Pad0..2` | 40 |
| `ConvexColliderData` | `physics.convex_collider` | `AssetRef<CollisionMeshAsset> Mesh; Vector3 Offset; Quaternion Rotation; float ConvexRadius; NativeBool IsTrigger; byte Pad0..2; uint Pad1` | 64 |
| `MeshColliderData` | `physics.mesh_collider` | `AssetRef<CollisionMeshAsset> Mesh; NativeBool IsTrigger; byte Pad0..6` | 32 |
| `PhysicsMaterialData` | `physics.physics_material` | `float Friction; float Restitution` | 8 |
| `CharacterControllerData` | `physics.character_controller` | `float Radius; float Height; float MaxSlopeAngleDegrees; float StepHeight; float Mass; float MaxStrength; float Padding; byte Layer; CharacterFlags Flags (byte); byte Pad0..1; Vector3 Velocity; GroundState Ground (byte); byte Pad2..4; Vector3 GroundNormal; uint Pad5; Entity GroundEntity` | 72 |
| `AudioSourceData` | `audio.audio_source` (`12_audio.md` 2.6; registered by the engine's `audio_subsystem`, R7, R50) | `AssetRef<AudioClipAsset> Clip; float Volume; float Pitch; NativeBool Loop; NativeBool Spatial; NativeBool PlayOnStart; NativeBool OneShot; StringId Group; AttenuationModel Attenuation (byte); byte Pad0..2; float MinDistance; float MaxDistance; float Rolloff; AudioPlaybackRequest PlaybackRequest (byte); NativeBool IsPlaying; byte Pad3..8; SourceHandle Handle` (opaque 16 bytes: `ulong Voice; uint ClipVersion; byte Flags; byte Pad0..2`) | 88 |
| `AudioListenerData` | `audio.audio_listener` | `float Gain; NativeBool Active; byte Pad0..2` | 8 |
| `ScriptComponentHandle` | `scripting.script_component` (hidden; `EDITOR_HIDDEN`) | `uint SetHandle` | 4 |

Native type names are abbreviated (`scene.parent` = registered name `conjure.scene.parent`). The owning design
notes (`13_scene.md` 2.1, `14_physics.md` 2.3, `12_audio.md` 2.6) define the fields; the rows above restate them in
order and must change in the same PR as the native struct (the hash test fails otherwise; R48, R50, R55). Fields
named `Pad*` exist only to make `Marshal.OffsetOf` agree with the C++ layout and are excluded from the layout hash on
both sides (`type_info` has no padding fields; R61). `children` (a `small_vector`) is never mirrored, use
`entity_get_children`. `SetComponent<T>` on a mirror with `AssetRef<T>` fields calls `asset_ref_bind` per field
(offsets cached at class load) so the slot pointer is valid when the bytes land (the R25 bind pass for scripts).

### 2.7 Call counters (`call_counters.h`)

```cpp
enum class native_function_id : u16 {
#define CONJURE_NATIVE_FN(group, name, ret, args) group##_##name,
#include "conjure/scripting/native_api.inl"
#undef CONJURE_NATIVE_FN
    COUNT
};
inline constexpr u32 NATIVE_FUNCTION_COUNT = static_cast<u32>(native_function_id::COUNT);

struct native_entry_descriptor { native_function_id id; std::string_view group; std::string_view name; std::string_view signature; bool ownedByBindings; };
[[nodiscard]] std::span<const native_entry_descriptor> native_entry_descriptors() noexcept;   // .inl order
[[nodiscard]] std::span<const u32> call_counters() noexcept;      // NATIVE_FUNCTION_COUNT entries; empty in Distribution
[[nodiscard]] u32 call_count(native_function_id id) noexcept;     // 0 in Distribution
void reset_call_counters() noexcept;
[[nodiscard]] std::string_view function_name(native_function_id id) noexcept;   // "transform_get_local"
```

Storage: `static std::array<u32, NATIVE_FUNCTION_COUNT> sCounters` (plain `u32`: every call is main-thread). A
function's index is its `.inl` line order, identical to the C# `NativeFunction` enum and the manifest's `index`.

---

## 3. Algorithms and flow

### 3.1 Startup: filling the table (step 16, before `script_runtime::initialize` at step 17; R27)

```
application::initialise
  14  scripting_subsystem::startup      -> script_runtime constructed; native_api() has only core entries filled
  15  modules_subsystem::startup        -> plugins may fill still-null slots by name (native_binding_desc, R36)
  16  scripting_bindings_subsystem::startup(context)
        1. context.scripting == nullptr  -> INFO "scripting disabled", return ok (nothing to fill)
        2. build binding_context from engine_context (world, scenes, physics, audio, assets, input, renderer,
           checks, modules, scripting) - all non-owning, nulls allowed
        3. fill_native_table(api, bindingContext):
             for each bindings-owned entry (CONJURE_BINDINGS_FOR_EACH_ENTRY):
               implementation = subsystem compiled in and context pointer non-null ? real function : stub_<entry>
               if (slot != nullptr && group is compiled in)  WARN "slot already filled, overriding"  (B3)
               if (slot != nullptr && group is compiled out)  keep (plugin-provided), continue
               slot = implementation
        4. #if !CONJURE_DISTRIBUTION: wrap_with_counters(api): for every entry with a non-null slot
             counting_thunk<Id, Fn>::sTarget = slot; slot = &counting_thunk<Id, Fn>::invoke
        5. validate: every slot non-null, else fail(TABLE_INCOMPLETE, names) -> startup error (unwinds 06 3.4)
        6. subscribe scene_manager::on_before_unload -> script_residency.clear(); store subscription
  17  scripting_subsystem::post_startup -> script_runtime::initialize (handshake: size, version, hash) -> load assembly
```

`fill_native_table` is a free function so a test can fill a stack `conjure_native_api` from a hand-built
`binding_context` with no `application`. `wrap_with_counters` is idempotent (a slot already pointing at a thunk is
skipped) so a second `startup` in tests does not double-count.

### 3.2 One call end to end

```
C#  transform.Position = p
    -> TransformData local = Local;                       // Api->transform_get_local(entity.Id, &local)
    -> local.Position = p; Local = local;                 // Api->transform_set_local(entity.Id, &local)
    -> NativeCheck.ThrowIfFailed(status)                  // status >= 0: return; < 0: read last error, throw

native  counting_thunk<transform_set_local, int32_t(uint64_t, const conjure_transform *)>::invoke
    -> ++sCounters[id]; return sTarget(entity, value)
    -> transform_set_local(entity, value):
         if (!is_main_thread()) return fail(WRONG_THREAD)            // CONJURE_ASSERT in Debug too
         if (value == nullptr) return fail(INVALID_ARGUMENT, "value")
         auto *local = world.get<scene::local_transform>(marshal::to_entity(entity))   // stamps the column
         if (local == nullptr) return is_alive ? fail(COMPONENT_MISSING, "local_transform") : fail(ENTITY_NOT_ALIVE)
         *local = marshal::to_transform(*value); return 0             // memcpy after the xyzw static_assert
```

Non-structural writes go straight to the column even inside a query scope (chunks do not move while locked; this
is what `Span<T>` does). Phase check: `script_runtime::current_phase() != COUNT`, else `NOT_IN_PHASE` (a `Task`
calling a facade is the case that hits it).

### 3.3 Structural routing while a query scope is open (R5)

```cpp
struct structural_guard {               // marshal.h; constructed per structural entry
    ecs::world &world; scripting::script_runtime &runtime;
    [[nodiscard]] bool deferred() const noexcept { return world.is_locked(); }
    [[nodiscard]] ecs::command_buffer &commands() noexcept { return runtime.deferred_commands(); }
};
```

| Entry | Unlocked | Locked (world.is_locked()) |
|---|---|---|
| `prefab_instantiate` | `scene::prefab::instantiate(scene, asset, options{ parent, rootTransform })` -> root (`13_scene.md` 3.12) | `commands().instantiate(archive)` -> reserved real id (01 D6); then `commands().add_raw<scene::parent>(root, {parent, APPEND_CHILD})`, `set<scene::local_transform>` when given; scripts inside the archive are deep-copied by `script_component`'s `ops.copy` at flush (R29); returned id is alive immediately, components appear at `query_end`; `scene.apply_hierarchy_changes` links it into the tree the same frame (13 3.7) |
| `prefab_instantiate_many` | `scene::prefab::instantiate_many` | one `instantiate` command per root (same buffer, same flush) |
| `entity_duplicate` | `scene::duplicate_subtree(entity)` (R55) | `commands().instantiate(scene_serializer::capture_subtree(scene, entity))` + parent command |
| `entity_create_many` | `scene::create_entities(count, NULL_ENTITY, parent, out)` (R55) | `count` x `commands().create_entity()` + `add<scene::local_transform>` + parent |
| `transform_set_parent*` | `scene::set_parent(child, parent, keepWorld, index)` | `scene::deferred_set_parent(commands(), child, parent, keepWorld, index)` (`13_scene.md` 3.7/4.3, R55): records `set_raw/add_raw/remove_raw<parent>`, the `keepWorld` local transform and `transform_dirty`; `scene.apply_hierarchy_changes` (TRANSFORM, same frame) rebuilds `children` and depth tags without a WARN |
| `entity_set_enabled`, `set_tag`/`set_layer` without the component | `add`/`remove` | `commands().add_raw/remove_raw` |
| `entity_destroy` (core-owned) | per 03 3.4 | scripting's own entry defers already |
| `scene_request_*` | request applied at FRAME_BEGIN (never structural now) | identical |

The buffer is `script_runtime::deferred_commands()` (one `ecs::command_buffer` owned by the runtime, required of
03 by this document, see 8); `query_end` flushes it, and 03's "scope still open at phase end is force-closed"
flushes it too. Commands on an entity dead at flush are skipped and counted (01 6). A locked request whose
result cannot be expressed as commands (none in the table today) returns `-STRUCTURE_LOCKED`.

### 3.4 Error propagation to managed exceptions

1. Native entry fails: `fail(code, detail)` formats `"<entry>: <detail>"` into the 512-byte buffer, stores the
   code, returns `-code`; in Debug one `CONJURE_LOG_DEBUG(LOG_BINDINGS, ...)` per entry per frame (bit in
   `std::bitset<NATIVE_FUNCTION_COUNT>`, cleared in `begin_frame`) so a dead-entity loop cannot flood the console.
2. `NativeCheck.ThrowIfFailed(int status)`: `status >= 0` returns; otherwise `diagnostics_last_error` into a
   `stackalloc byte[512]`, then `throw BindingStatusMap.Create((BindingStatus)(-status), message)`: `ENTITY_NOT_ALIVE ->
   EntityNotAliveException`, `ASSET_NOT_LOADED -> AssetNotLoadedException`, `NOT_SUPPORTED -> NotSupportedException`,
   `WRONG_THREAD/NOT_IN_PHASE/STRUCTURE_LOCKED -> InvalidOperationException`, `INVALID_ARGUMENT -> ArgumentException`,
   `NOT_FOUND/SCENE_NOT_FOUND -> KeyNotFoundException`, rest -> `ConjureNativeException { BindingStatus Status; }`.
3. The exception unwinds to the `[UnmanagedCallersOnly]` guard of the phase (03 D11): logged with class and
   callback, consecutive-fault counter, disabled after three. Nothing crosses the boundary.
4. Predicates never throw on dead entities (B11): `IsAlive`, `HasComponent`, `IsPlaying`, `IsSleeping` return
   `false` on `-ENTITY_NOT_ALIVE`; `GetChildren` and `FindBy*` return 0.

### 3.5 Incrementing and reading a counter

Increment: `counting_thunk<Id, Ret(Args...)>::invoke(Args... args) { ++sCounters[Id]; return sTarget(args...); }`,
one relaxed `u32` increment, blittable args forwarded by value, inlined target call through a static pointer.
Read: C++ `bindings::call_count(native_function_id::transform_set_local)` or the span; C#
`Diagnostics.CallCount(NativeFunction.TransformSetLocal)`; the `feature_test` `FeatureChecklist` script reports one
`Conjure.Diagnostics.Check("binding.<name>.called", count > 0)` per non-excluded entry on frame 600, and
`tests/integration/test_feature_test_scene.cpp` asserts the same from the native span. `reset_call_counters()`
is called by `headless_engine` fixtures between cases. Overflow at 2^32 is irrelevant (10k calls/frame = 119 h).

### 3.6 Regenerating `NativeApi.g.cs` and checking it

`tools/generate_native_api.py` (defaults: `--inl engine/include/conjure/scripting/native_api.inl --out-cs
managed/Conjure.Managed/Generated/NativeApi.g.cs --out-hash engine/include/conjure/scripting/native_api_hash.h
--out-manifest docs/generated/native_api_manifest.json`; `--check` diffs the committed outputs instead of writing
them; the `--status engine/include/conjure/bindings/bindings_error.inl` option that emits `BindingStatus.g.cs` is
the wave-4 extension of step 2 in section 9) (no Python at build time; outputs are committed). It parses the `CONJURE_NATIVE_FN` lines, maps C types to C#
(`uint64_t -> ulong`, `const char * -> byte*`, `conjure_transform * -> TransformData*`, ...), emits the
`delegate* unmanaged<...>` fields in order, the `NativeFunction` enum, `BindingStatus.g.cs`, and the manifest
`{ "hash", "version", "entries": [{ "index", "group", "name", "returns", "args": [{"type","name"}], "signature" }] }`
where `signature` is the normalised `"<ret> <args>"` text (single spaces, `*` hugging the name), the same
normalisation `native_entry_descriptors()` applies to the stringified macro arguments. Checks: CTest
`conjure_scripting_api_generated_up_to_date` (03) regenerates to a temp dir and diffs all four outputs; doctest
`test_generated_manifest_matches` compares the committed manifest (via `core::json`) with `native_entry_descriptors()`
and `CONJURE_NATIVE_API_HASH` on count, order, names, groups, signatures and hash.

### 3.7 Asset residency from scripts (B9)

`asset_load(id, typeId, priority)`: `type_of(id)` must equal `typeId` (else `ASSET_TYPE_MISMATCH`); the handle from
`assets.load(id, priority)` goes into `script_residency` (repeat calls are no-ops); `asset_release` erases it (slots
are never freed, 05). `asset_ref_get` never loads. `Prefab.Instantiate` on an unloaded prefab -> `ASSET_NOT_LOADED`;
the recommended pattern is a serialised `AssetRef<PrefabAsset>` field (resident because scene load gates on
`asset_load_batch::is_complete()`) or `Load()` in `OnCreate`. `script_residency` is cleared on `on_before_unload` and
when the runtime leaves PLAY; `scene_manager`'s `asset_manager::collect()` then frees what nothing else holds.

### 3.8 Audio commands through component fields (B8)

`audio_play(entity)`: `world.get<audio::audio_source>(target)` (missing -> `COMPONENT_MISSING`), then
`source->playbackRequest = audio_playback_request::PLAY` (stamps the column). `engine.audio_sync` (SYNC) walks
`changed<audio_source>` chunks, issues the mixer command for each non-NONE request, resets it to NONE and writes
`isPlaying`; `engine.audio_finish_one_shots` (FRAME_END) writes `isPlaying = false` when a voice ends (`12_audio.md`
8, R50). PLAY on a playing source restarts it (`seek(0)`). `audio_play_one_shot` requires a LOADED clip and calls
`audio_engine::play(clipRef, play_params{ retain = false, spatial = position != nullptr, volume, pitch, group =
find_group(groupHash) })` followed by `set_position` when a position is given; the audio engine owns that voice and
releases it in `end_frame()`.

---

## 4. Public API (`engine/include/conjure/bindings/`)

### 4.1 Headers

| Header | Main types |
|---|---|
| `log_category.h` | `CONJURE_DECLARE_LOG_CATEGORY(LOG_BINDINGS)` (defined in `log_category.cpp` as `"bindings"`) |
| `bindings_error.h` (+ `bindings_error.inl`) | `bindings_error`, `bindings_error_name`, `CONJURE_DECLARE_ERROR_CATEGORY`, `to_status`, `fail`, `last_error_code`, `last_error_message`, `clear_last_error` |
| `native_api_entries.h` | `CONJURE_BINDINGS_FOR_EACH_ENTRY(X)` filtered X-macro, `CONJURE_BINDINGS_OWNS_GROUP(group)`, declarations of every owned entry (`int32_t transform_get_local(uint64_t, conjure_transform *)` ... in `namespace conjure::bindings::entries`) and their stubs (`namespace stubs`), `binding_context`, `fill_native_table`, `wrap_with_counters` |
| `call_counters.h` | `native_function_id`, `NATIVE_FUNCTION_COUNT`, `native_entry_descriptor`, `native_entry_descriptors`, `call_counters`, `call_count`, `reset_call_counters`, `function_name`, `counting_thunk` |
| `marshal.h` | `namespace marshal`: `to_entity/from_entity`, `to_uuid/from_uuid`, `to_vec2/3/4`, `to_quat/from_quat`, `to_mat4`, `to_transform/from_transform`, `read_utf8(const char *, int32_t) -> result<std::string_view>`, `write_utf8(std::string_view, char *, int32_t) -> int32_t`, `write_entities(std::span<const ecs::entity>, uint64_t *, int32_t) -> int32_t`, `rgba_to_color`, `structural_guard`, `layout static_asserts` |
| `bindings_subsystem.h` | `class scripting_bindings_subsystem final : public engine::engine_subsystem`, `script_residency` |
| `bindings.h` | umbrella |

Private (`engine/src/bindings/`): `bindings_subsystem.cpp`, `call_counters.cpp`, `bindings_error.cpp`, `marshal.cpp`,
`native_api_fill.cpp` (fill, stubs, counter wrap), one `entries_<group>.cpp` per group of 2.3, `log_category.cpp`,
`README.md`. An `entries_*.cpp` whose module is compiled out (`#if !CONJURE_WITH_PHYSICS`) compiles only its stubs.

### 4.2 Key signatures

```cpp
namespace conjure::bindings {

    struct binding_context {                          // non-owning view of engine_context; nulls mean "stub"
        ecs::world *world = nullptr;
        ecs::component_registry *components = nullptr;
        scene::scene_manager *scenes = nullptr;
        physics::physics_world *physics = nullptr;
        audio::audio_engine *audio = nullptr;
        assets::asset_manager *assets = nullptr;
        platform::input *input = nullptr;
        platform::window *window = nullptr;           // cursor mode, clipboard
        renderer::renderer *renderer = nullptr;       // debug draw; null = silent success
        engine::diagnostics::check_registry *checks = nullptr;
        const modules::module_registry *modules = nullptr;
        scripting::script_runtime *scripting = nullptr;
        engine::frame_stats *stats = nullptr;
        [[nodiscard]] static binding_context from(const engine::engine_context &context) noexcept;
    };

    /// <summary>Fills every bindings-owned slot (real or stub). Idempotent; never touches core-owned slots.</summary>
    [[nodiscard]] core::result<void> fill_native_table(conjure_native_api &api, const binding_context &context);

    /// <summary>Wraps every non-null slot with a counting thunk (no-op in Distribution). Idempotent.</summary>
    void wrap_with_counters(conjure_native_api &api) noexcept;

    class script_residency { /* flat_hash_map<uuid, asset_handle>; hold(id, handle), release(id), clear(), size() (B9) */ };

    class scripting_bindings_subsystem final : public engine::engine_subsystem {
        private:
            binding_context mContext = {};
            script_residency mResidency = {};
            core::event_subscription mUnloadSubscription = {};
            bool mFilled = false;

        public:
            scripting_bindings_subsystem();
            ~scripting_bindings_subsystem() override;

        public:
            [[nodiscard]] std::string_view name() const noexcept override;           // "bindings"
            [[nodiscard]] core::result<void> startup(engine::engine_context &context) override;   // 3.1
            void begin_frame(engine::engine_context &context) override;              // clears per-frame log bits, last error
            void shutdown(engine::engine_context &context) override;                 // residency.clear(); safe after failed startup
            [[nodiscard]] const binding_context &context() const noexcept;
            [[nodiscard]] script_residency &residency() noexcept;
    };

    template <native_function_id Id, typename Fn> struct counting_thunk;                 // specialised on Ret(Args...), 3.5
}
```

`register_systems` is not overridden: bindings add no scheduler systems (audio requests ride the engine's SYNC system, B8).

### 4.3 C# files owned by this module (`managed/Conjure.Managed/`)

```
Interop/BindingRecords.cs        mirrors of 2.4 (ConjureUuid, RayQuery, ShapeQuery, RaycastHit, AssetInfo, ModuleRecord, FrameStatsBlock, enums)
Interop/BindingStatus.g.cs       GENERATED from bindings_error.inl
Interop/NativeCheck.cs           ThrowIfFailed, ReadLastError, BindingStatusMap
Interop/ConjureNativeException.cs  + EntityNotAliveException, AssetNotLoadedException
Interop/Utf8Marshal.cs           Encode(ReadOnlySpan<char>, Span<byte>) with pooled fallback, Read(needed-length protocol)
Api/Transform.cs Rigidbody.cs CharacterController.cs Physics.cs LayerMask.cs RaycastHit.cs
Api/AudioSource.cs Audio.cs Prefab.cs Scene.cs Input.Bindings.cs (partial) Entity.Hierarchy.cs (partial)
Api/AssetRef.cs AssetId.cs AssetTypes.cs (IAssetType + tags) AssetState.cs
Api/Diagnostics.cs (Check, Draw, Stats, CallCount) Modules.cs (Registry, ModuleInfo)
Api/Components/Scene/*.cs Physics/*.cs Audio/*.cs   the mirrors of 2.6 (LocalTransform stays scripting's)
Api/Components/NativeBool.cs StringId.cs
```

Shared, scripting-owned and consumed here: `Interop/NativeApi.g.cs` (whole table), `NativeBridge`, `Records.cs`,
`FrameBlock.cs`, `Attributes.cs` (`[NativeComponent]`), `Dod/ComponentType.cs` (layout hash), `Runtime/NameHash.cs`.
`Conjure.Managed.Tests` gains `Bindings/*Tests.cs` (10). The csproj lists every file explicitly (03 3.10).

---

## 5. Threading and ownership

- **Threading.** Every entry runs on the main thread inside one of the six `EXCLUSIVE` scripting systems (R27,
  03 D10); `WRONG_THREAD` / `NOT_IN_PHASE` otherwise. The world is unlocked there (01 4.7) except inside a DOD
  scope (B7). Counters, the last-error buffer and the per-frame log bits are plain statics because nothing else
  touches them; `module_registry` reads happen at a sync point (07 4.6); debug draw appends are mutex-guarded by
  the renderer. No bindings code runs on a job thread; no threads are created.
- **Ownership.** `engine::application` owns the subsystem (step 16); the subsystem owns `script_residency` and one
  event subscription. `binding_context` pointers are non-owning and stable from `initialise()` to `shutdown()`
  (06 2.5). Thunk targets point into `conjure_scripting`/`conjure_bindings` code, never into managed memory (03 D9).
  `conjure_native_api` is owned by `script_runtime`; bindings only writes slots. Nothing native is referenced from
  C# after a call returns except the opaque, never-freed `asset_slot *` in `AssetRef<T>.Slot` (05).
- **Re-entrancy.** Entries never call back into managed code; scripts created by `prefab_instantiate` are
  instantiated by the runtime's `flush_pending` of the current phase (03 3.3), not inside the binding call.

---

## 6. Failure handling

| Situation | Behaviour |
|---|---|
| Dead or null entity to any entity-taking entry | `-ENTITY_NOT_ALIVE`; predicates return 0; Debug log once per entry per frame |
| Entity lacks the component (`rigid_body`, `audio_source`, `character_controller`, `local_transform`) | `-COMPONENT_MISSING` naming the component |
| Subsystem compiled out / disabled / backend cannot answer (e.g. `physics_*` with `CONJURE_WITH_PHYSICS=OFF`, `asset_path_of` on a cooked pack; the `null` window answers clipboard and cursor calls, R57) | stub returns `-NOT_SUPPORTED` naming the entry; C# `NotSupportedException`; counter still increments |
| Null pointer with non-zero length, negative capacity, enum out of range, `count < 0` | `-INVALID_ARGUMENT` |
| Output buffer too small (strings, arrays) | writes what fits, returns the full needed length/count; never an error |
| Structural request inside a query scope | deferred to `deferred_commands()` (B7); `-STRUCTURE_LOCKED` only for a request with no command form |
| Request on an entity dead at flush time | skipped and counted in `flush_report::skipped` (01), DEBUG log |
| Prefab asset not `LOADED` / wrong type / payload not an archive | `-ASSET_NOT_LOADED` / `-ASSET_TYPE_MISMATCH` / `-PREFAB_INVALID` |
| `asset_load_blocking` inside a frame | permitted on main (R23), `WARN` once per frame in Development, `TIMEOUT` -> `-INTERNAL` with the assets message |
| Scene path unknown | `-SCENE_NOT_FOUND`; `scene_request_unload` of a scene not loaded is a no-op returning 0 |
| Layer name unknown / module name unknown / asset path unknown | `-NOT_FOUND` |
| Wrong thread / outside a script phase | `-WRONG_THREAD` / `-NOT_IN_PHASE` (+ `CONJURE_ASSERT` in Debug) |
| `fill_native_table` leaves an owned slot null (only possible with a broken build of `entries_*.cpp`) | `TABLE_INCOMPLETE` error from `startup`; application startup unwinds (06 3.4) |
| Plugin `native_binding_desc` names a bindings-owned slot | bindings overrides it at step 16 with a `WARN` naming the module (B3) |
| Mirror layout drift | `component_lookup_type` returns 0 (03), `InvalidOperationException` on first use, logged once; `test_layout_hashes` fails in CI first |
| Generated files stale | `conjure_scripting_api_generated_up_to_date` and `test_generated_manifest_matches` fail; the handshake would report `ABI_MISMATCH` at runtime |

All asserts compile out in Distribution; every asserted path also returns a status. The module never throws or
catches; `core::result` is used only by `startup`/`fill_native_table`.

---

## 7. Headless and null behaviour

Bindings have no backend of their own. With `null` platform/rhi/audio backends and `null_script_host` (03 7) every
group has real implementations (Jolt runs headless; `null_backend_stats` records play requests after
`engine.audio_sync`; assets use `memory_file_system`; the `null` window keeps clipboard text, cursor mode and gamepad
names in memory, `11_platform.md` 3.5, so `input_clipboard_*`, `input_set/get_cursor_mode` and `input_gamepad_name`
are real implementations there too, R57); only `diagnostics_draw_*` are silent successes when `renderer == nullptr`
and `asset_path_of` is `NOT_SUPPORTED` on a cooked pack without a path table. Because the
null host never calls back, a C++ test exercises an entry by calling through the table it just filled:

```cpp
conjure::testing::headless_engine engine(headless_engine_desc{}.with_subsystem<scripting_bindings_subsystem>());
conjure_native_api &api = engine.context().scripting->native_api();
const uint64_t player = api.entity_create(u8"Player", 6);
conjure_transform local = {};
CHECK(api.transform_get_local(player, &local) == 0);
local.position.y = 3.0f;
CHECK(api.transform_set_local(player, &local) == 0);
CHECK(bindings::call_count(native_function_id::transform_set_local) == 1);
```

Without an application, `fill_native_table(api, context)` over a `binding_context` built from a bare `ecs::world`
+ `scene::scene_manager` + `asset_manager` exercises the same code with every other pointer null (stubs). The
`[dotnet]` integration tests (`test_bindings_managed.cpp`) drive the C# facades through `Conjure.TestScripts`
and are skipped without the SDK, which CI treats as a failure where the SDK is installed (08 3.2).

---

## 8. Integration points

**Needs from `scripting` (03):** the `.inl` section and record block reserved for bindings (03 2.1, R54);
`script_runtime::deferred_commands() -> ecs::command_buffer &` (the buffer `query_end` flushes, R5) and
`script_runtime::current_phase()` (both in 03 4 now, R54); `generate_native_api.py` extended with `--status`
(`--out-manifest` exists since wave 3); `[NativeComponent]` and `ComponentType<T>` hashing per B10 (03 3.9, R61); `NameHash`; `null_script_host`.

**Needs from `core` (10):** the x,y,z,w quaternion storage guarantee in `core/math` (B5, R58; otherwise mirrors need an explicit
`NativeQuaternion` and transforms stop being memcpy-able) with a `static_assert`/`offsetof` check in `tests/core/test_math.cpp`;
the byte-level `type_info::layout_hash()` definition (`10_core.md` 2.11) with test vectors in
`tests/bindings/data/layout_hash_vectors.json` (shared with `Conjure.Managed.Tests`); `flat_hash_map<uuid, ...>`,
`event_subscription`, the error-category macro.

**Needs from `ecs` (01):** `world::get<T>` stamping, `*_raw`, `is_locked`, `command_buffer::instantiate/create_entity/
add_raw/set_raw/remove_raw`, `entity_archive::capture`, `component_registry::find`.

**Needs from `scene` (`13_scene.md`, R55):** hierarchy API (`set_parent`, `tree().parent_of/children_of/root_of`,
`name_of/set_name`, `guid_of/find_guid`, `find_by_tag/find_by_layer`, `create_entities(count, NULL_ENTITY, parent, out)`,
`structure_version`); `deferred_set_parent(command_buffer &, child, parent, keepWorld, index)` and
`duplicate_subtree(entity)` (13 4.3); `prefab::instantiate(scene, asset, instantiate_options{ parent, rootTransform })`
returning the root and `prefab::instantiate_many`; the `prefab_instance { asset_ref<prefab_asset> prefab }` layout
(24 bytes; the carrying entity is the instance root); `scene_manager::request_load(asset_id, mode)` /
`request_unload(scene_id)` / `is_loaded` / `is_load_pending(asset_id)` / `loaded_scenes()` (`loaded_scene_info::path`
is the string the `scene_*` entries return; path arguments resolve through `asset_manager::find(path)`),
`on_before_unload`.

**Needs from `physics` (`14_physics.md` 4.2, R49):** entity-keyed `physics_world` API: `set_linear/angular_velocity`
(reads come from the `rigid_body` mirror), `add_force(_at)`, `add_impulse(_at)`, `add_torque`, `add_angular_impulse`
(the `conjure_force_mode` mapping is 14 8), `move_kinematic`, `teleport`, `is_sleeping`, `wake`, `gravity()/set_gravity`,
`raycast`, `raycast_all(span<hit>)`, `shape_cast`, `overlap_sphere/box(span<entity>)`, `layers().find/name/collides`
(from `project.json` `extensions.physics_layers`), `character_move`, `character_ground`; component field lists are
14 2.3 and the mirrors of 2.6 follow them (R48).

**Needs from `audio` (`12_audio.md`) and the engine's `audio_subsystem` (R7, R50):** the `audio_source` fields
`playbackRequest : audio_playback_request` and `isPlaying : bool` (12 2.6; hidden, never serialised); `engine.audio_sync`
consumes requests and writes `isPlaying`; `audio_engine::play(clipRef, play_params)`, `find_group(string_id)`,
`set_group_gain(group_id, f32)` / `group(group_id)->gain`, `set_master_gain` / `group(MASTER_GROUP)->gain`,
`audio_clip::duration_seconds()` from the payload.

**Needs from `assets` (05 8):** `asset_manager::load(id, priority) -> asset_handle`, `load_blocking`, `bind_slot(id)`,
`state_of`, `type_of`, `version_of`, `find(path)`, `path_of(id)` (dev; cooked packs without a path table ->
`NOT_SUPPORTED`), `make_asset_type_id`.

**Needs from `platform` (`11_platform.md`, R56, R57):** `input::inject(event)` on both backends,
`window::set_cursor_mode/current_cursor_mode`, `window::clipboard_text/set_clipboard_text` and `window::gamepad_name(index)`
(all implemented in memory by `null_backend`; `CLIPBOARD_UNAVAILABLE` from glfw maps to `-NOT_FOUND` with the
message); `button_action` and `cursor_mode` values are passed through unchanged. **From `modules` (07):** the
`module_registry` getters of 4.6; the scripting consumer applies `native_binding_desc` at step 15 (B3 decides precedence at 16).

**Needs from `engine` (06):** `engine_subsystem`, `engine_context` including `const frame_stats *stats` (06 2.5, R51),
`check_registry::report`, `debug_overlay_layer::draw_text` (non-Distribution); `editor/main.cpp` and `runtime/main.cpp`
call `application.add_subsystem(std::make_unique<scripting_bindings_subsystem>())` before `initialise()` (06 3.4 step 16).

**Offers:** a complete `conjure_native_api` to `scripting` before `initialize`; `call_counters()` to `08_testing` 7.2
and `samples/feature_test`; `Conjure.Diagnostics.Check` to every sample script; the facades of 2.5 to game code and
`Conjure.Templates/Game/Player.cs`; `fill_native_table` + `binding_context` to other modules' tests;
`docs/generated/native_api_manifest.json` to the `editor-ai-control` skill generator (agents can list the scripting API).

CMake: `conjure_add_module(bindings SOURCES ... PUBLIC_DEPS conjure_scripting conjure_scene conjure_assets
conjure_engine PRIVATE_DEPS conjure_modules $<$<BOOL:${CONJURE_WITH_PHYSICS}>:conjure_physics>
$<$<BOOL:${CONJURE_WITH_AUDIO}>:conjure_audio> $<$<BOOL:${CONJURE_WITH_RENDERER}>:conjure_renderer>)`; the module is
excluded when `CONJURE_WITH_SCRIPTING=OFF`. `conjure_add_test_module(bindings ENGINE SOURCES ...)`.

---

## 9. Implementation plan (ordered; each step lands with its tests green)

1. `log_category.h`, `bindings_error.h/.inl/.cpp`, `marshal.h/.cpp` (layout `static_assert`s, xyzw check);
   `test_bindings_error.cpp`, `test_marshal.cpp`.
2. Append the 106 lines and the records of 2.3/2.4 to `native_api.inl`/`native_api.h` (scripting PR); extend
   `generate_native_api.py` (`NativeFunction`, `BindingStatus.g.cs`, manifest); regenerate and commit.
3. `call_counters.h/.cpp` (ids, descriptors with normalised signatures, counters, `counting_thunk`);
   `test_call_counters.cpp`; `test_generated_manifest_matches`.
4. `native_api_entries.h` + `native_api_fill.cpp` with **stubs for every owned entry**, `fill_native_table`,
   `wrap_with_counters`; `test_native_table_complete`.
5. `entries_transform.cpp`, `entries_entity.cpp`; their tests against a bare `scene::scene`.
6. `structural_guard` and the locked paths of 3.3; `test_span_scope_routing.cpp` (null host + `deferred_commands()`).
7. `entries_scene.cpp`, `entries_prefab.cpp`, `entries_asset.cpp` (residency); `test_prefab_scene_bindings.cpp`,
   `test_asset_bindings.cpp`.
8. `entries_physics.cpp`, `entries_audio.cpp`, `entries_input.cpp`; their tests on null backends (audio asserted
   through `null_backend_stats` after one SYNC step).
9. `entries_diagnostics.cpp`, `entries_modules.cpp`; their tests.
10. `bindings_subsystem.h/.cpp`; `test_bindings_subsystem.cpp`; wire `add_subsystem` into both executables.
11. Managed side (4.3) and `Conjure.Managed.Tests/Bindings`; `[dotnet]` `test_bindings_managed.cpp` over `Conjure.TestScripts`.
12. `samples/feature_test` scripts covering every entry (10), `tests/binding_exclusions.json`, `test_feature_test_scene`
    counter assertion, `engine/src/bindings/README.md`, `bench_bindings.cpp`.

---

## 10. Tests (`tests/bindings/`, target `conjure_bindings_tests`, `ENGINE`, all null backends)

One file per public header plus the topic files of `08_testing.md` 5.2 (`test_binding_tables` is realised as
`test_native_api_entries`; `test_logging_bindings` is covered by scripting's `log_*` tests; `test_dod_span_bindings` is
`test_span_scope_routing`). Case names are the minimum.

| File | Cases |
|---|---|
| `test_bindings_error.cpp` | `status_is_negative_code`, `fail_sets_message_without_allocating`, `message_truncates_at_511_bytes`, `core_error_mapping_table` (every row of 2.2), `clear_resets_state`, `every_enumerator_has_a_name` |
| `test_marshal.cpp` | `entity_roundtrip_u64`, `null_entity_is_zero`, `uuid_high_low_roundtrip`, `quat_storage_is_xyzw`, `transform_memcpy_equals_by_name_conversion`, `mat4_column_major_bytes`, `read_utf8_rejects_null_with_length`, `write_utf8_returns_needed_and_truncates`, `write_entities_count_protocol` |
| `test_call_counters.cpp` | `count_matches_inl_line_count`, `descriptor_order_is_inl_order`, `thunk_increments_and_forwards_return`, `thunk_forwards_every_argument_kind`, `wrap_is_idempotent`, `reset_zeroes_all`, `function_name_matches_descriptor`, `distribution_span_is_empty` |
| `test_native_api_entries.cpp` | `test_native_table_complete` (null context: every slot non-null, every stub returns `-NOT_SUPPORTED` naming the entry; full headless context: no stub for a compiled-in group), `owned_groups_are_exactly_the_documented_ten`, `core_slots_untouched_by_fill`, `test_generated_manifest_matches` (count, order, names, groups, signatures, hash against `docs/generated/native_api_manifest.json`), `override_of_filled_owned_slot_warns`, `plugin_fill_kept_for_compiled_out_group` |
| `test_bindings_subsystem.cpp` | `startup_before_scripting_post_startup`, `startup_without_scripting_is_noop`, `table_incomplete_fails_startup_and_unwinds`, `unload_event_clears_residency`, `shutdown_safe_after_failed_startup`, `begin_frame_clears_rate_limit_bits` |
| `test_transform_bindings.cpp` | `get_set_local_roundtrip`, `set_local_stamps_change_detection`, `world_matrix_composes_parent_chain_depth_8`, `set_world_computes_local_against_parent`, `set_parent_keeps_world_by_default`, `set_parent_ex_index_and_no_keep`, `interpolated_missing_is_component_missing`, `dead_entity_status_and_single_debug_log`, `10k_set_local_allocation_free` |
| `test_entity_bindings.cpp` | `set_name_find_roundtrip`, `parent_root_children_protocol_with_small_buffer`, `enabled_toggles_disabled_tag`, `tag_hash_equals_fnv1a64`, `layer_set_get_find`, `guid_get_find`, `find_by_tag_counts_without_buffer`, `duplicate_copies_subtree_and_scripts_deep`, `create_many_parents_all` |
| `test_prefab_scene_bindings.cpp` | `request_load_applies_at_frame_begin_not_immediately`, `additive_keeps_existing_entities`, `is_load_pending_until_batch_complete`, `unknown_path_scene_not_found`, `loaded_paths_and_roots_protocol`, `prefab_instantiate_requires_loaded`, `instantiate_with_local_and_parent`, `instantiate_many_roots_distinct`, `source_of_and_is_instance_root`, `payload_not_archive_is_prefab_invalid` |
| `test_span_scope_routing.cpp` | `structural_requests_inside_scope_land_in_deferred_commands` (prefab, duplicate, create_many, set_parent, set_enabled: `command_count` grows, world unchanged), `span_over_1000_transforms_stays_valid_until_query_end` (column pointer identity before/after the requests, 08 5.2), `query_end_flushes_in_request_order`, `non_structural_writes_apply_immediately_inside_scope`, `instantiate_inside_scope_returns_alive_id`, `dead_at_flush_is_skipped`, `phase_end_force_close_flushes` |
| `test_physics_bindings.cpp` | `raycast_hits_ground_within_1mm`, `raycast_all_sorted_by_distance`, `ignore_entity_and_layer_mask`, `shape_cast_sphere_box_capsule`, `overlap_counts`, `velocity_set_get_after_step`, `force_modes_change_velocity_as_expected`, `move_kinematic_applied_in_push_transforms`, `teleport_resets_velocity`, `sleep_wake`, `gravity_roundtrip`, `layer_index_name_collide_from_project_json`, `character_move_and_grounded`, `no_body_is_component_missing`, `compiled_out_physics_all_not_supported` |
| `test_audio_bindings.cpp` | `play_sets_request_and_sync_starts_voice` (`null_backend_stats`), `stop_pause_resume_states`, `is_playing_reflects_previous_sync`, `play_on_playing_restarts`, `one_shot_requires_loaded_clip`, `group_and_master_gain_roundtrip`, `clip_duration_from_payload`, `no_source_is_component_missing` |
| `test_input_bindings.cpp` | `inject_key_visible_next_frame_block` (pressed one frame, then held), `inject_mouse_and_gamepad`, `cursor_mode_roundtrip_on_null_window`, `cursor_mode_bad_value_is_invalid_argument`, `gamepad_name_protocol` (connected slot named, disconnected slot empty), `clipboard_roundtrip_on_null_window` (R57) |
| `test_asset_bindings.cpp` | `ref_get_state_version_without_loading`, `load_holds_handle_release_drops_it`, `load_twice_is_noop`, `type_mismatch_rejected`, `find_and_path_of_roundtrip`, `load_blocking_warns_once_per_frame`, `ref_bind_returns_stable_slot`, `type_id_matches_make_asset_type_id`, `unload_event_releases_everything` |
| `test_diagnostics_bindings.cpp` | `check_reports_into_registry_with_frame`, `last_error_code_and_message_protocol`, `draw_without_renderer_is_silent_success`, `draw_with_null_device_appends_expected_vertex_counts`, `frame_stats_block_matches_latest_sample`, `call_count_entry_matches_span` |
| `test_modules_bindings.cpp` | `count_and_load_order`, `find_known_unknown`, `info_record_fields`, `name_protocol`, `manifest_only_static_modules_listed` (R18) |
| `test_bindings_managed.cpp` `[dotnet]` | `facade_calls_increment_counters`, `exception_type_per_status`, `dead_entity_predicates_false_setters_throw`, `assetref_find_load_isloaded`, `mirror_hashes_match_native` (every row of 2.6 resolves to a non-zero id), `setcomponent_with_assetref_binds_slot`, `modules_registry_lists_sample_plugin`, `input_inject_seen_next_frame` |
| `tests/integration/test_feature_test_scene.cpp` (08) | after 600 headless frames every `native_function_id` not listed in `tests/binding_exclusions.json` has `call_count > 0`; every exclusion names an existing entry; `checks().all_passed()` |
| `Conjure.Managed.Tests/Bindings/` | `BindingRecordSizesMatchDocumented`, `Utf8MarshalRoundTripAndRetry`, `NativeCheckMapsEveryStatus`, `LayoutHashMatchesNativeVectors`, `FieldNameMappingPascalToCamel`, `SpanCannotEscapeQueryScope` (CS8352 compile fixture), `AssetIdCanonicalString`, `NativeFunctionEnumMatchesManifest` |
| `tests/benchmarks/bench_bindings.cpp` | `10k_transform_set_via_facade` (< 1.5 ms, two calls each), `10k_transform_set_via_span` (< 0.1 ms), `1k_raycasts` (< 1 ms), `counter_thunk_overhead` (< 2 ns/call against the unwrapped pointer) |

`feature_test` scripts that must exercise every binding (08 7.2 table; API areas in brackets): `CameraOrbit`
(Transform get/set, World, cursor mode), `ChainWiggle` (Parent/Children/SetParent/Root, `LocalTransform` and
`WorldTransform` mirrors), `ForceApplier` (velocities, forces, torque, sleep/wake, `RigidBodyData`), `PlatformMover`
(MoveKinematic, Teleport), `PlayerController` (CharacterController, raycast, `AudioSource.Play` on collision),
`RaycastProbe` (Raycast/RaycastAll/casts/overlaps/layers), `LayerAsserter` (layer index/name/collide), `AudioParams`
+ `OneShotTrigger` (every `audio_*`), `SpriteGridSpawner` + `CoinSpawner` + `ParticleSpawner` (Prefab
find/load/instantiate/many/SourceOf, Destroy, Duplicate, CreateMany), `DodSpanMover` (`ComponentQuery` with
`Prefab.Instantiate` inside the scope: the routing path), `SceneLoader` (every `scene_*`), `FieldZoo` (`AssetRef<T>`
load/state/release/path/find), `ComponentApi` (every mirror of 2.6 through Has/Get/Add/Set/Remove, `asset_ref_bind`
via `AudioSourceData.Clip`), `PluginProbe` (`Modules.Registry`), `DebugDrawProbe` + `TimeProbe` (`diagnostics_draw_*`,
`frame_stats`), `InputProbe` (every `input_inject_*`, gamepad name, cursor mode and clipboard round trips - all real on `null`, R57),
`FeatureChecklist` (`Diagnostics.Check`, `CallCount`, `last_error` by provoking one `EntityNotAliveException`).

`tests/binding_exclusions.json` (authored, not downloaded; `08_testing.md` 7.2 agrees, R62):

```json
{
  "schema": 1,
  "exclusions": [
    { "function": "asset_path_of",       "reason": "cooked feature_test export has no path table; dev run covers it" },
    { "function": "diagnostics_call_count", "reason": "Distribution stub only; counted in Development runs" }
  ]
}
```

Rules: `function` must name an entry of the manifest (stale entries fail the test); `reason` is mandatory and one
line; the list is reviewed in the PR that adds an entry; an exclusion for an entry that *is* reachable in
`feature_test` is a review rejection. Only the C++ integration test applies the exclusions (parsed with `core::json`);
the C# `FeatureChecklist` reports every count unconditionally and never reads the file.

---

## 11. Out of scope / future

- Source-generated C# facades from the manifest (wrappers stay hand-written so semantics such as "two calls per
  `Position` set" are explicit); job-parallel managed code or bindings callable off the main thread (03 11).
- Scripts loading/unloading modules, applying/unpacking prefabs, saving scenes or importing assets: editor
  commands with undo (04 4.4), not runtime bindings.
- Audio automation (fades, curves) above component fields; debug text in Distribution; a 3D text renderer.
- Script writes to `interpolated_transform` (read-only mirror; physics owns it, R47).
- Per-call-site rate limiting of dead-entity logs (native sees entries, not managed call sites).
- A stable ABI across engine versions: `CONJURE_NATIVE_API_VERSION` bumps on any signature change and the editor
  rebuilds the game assembly; no shim layer.
