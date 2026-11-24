#pragma once

// The native -> managed function table (docs/design/03_scripting.md 2.1, 15_bindings.md 2.3).
//
// One CONJURE_NATIVE_FN(group, name, return, (arguments)) per entry; an argument list that would pass
// 120 columns continues on the next lines with a two-space indent (the X-macro and the generator both
// read the whole invocation). The macro expands into the fields of conjure_native_api (native_api.h),
// the descriptor table of native_entry_descriptors() and, through tools/generate_native_api.py, into
// managed/Conjure.Managed/Generated/NativeApi.g.cs and docs/generated/native_api_manifest.json.
// Adding an entry is one invocation here plus a regeneration; entries are appended within a version
// and never reordered (a reorder or signature change bumps CONJURE_NATIVE_API_VERSION).
//
// Status convention of the core entries: an int32_t result >= 0 is a success (0, a count, a bool or a
// needed length) and < 0 is -script_error; uint64_t entries return 0 on failure; uint32_t entries
// return 0 on failure.
//
// Entries before the marker line are implemented by conjure_scripting (native_api_core.cpp, R26/R64);
// everything after `// --- bindings (R26) ---` is filled by conjure_bindings (wave 4) and lives here so
// the struct is complete when conjure_scripting compiles (R54).
//
//                group      name                  return     arguments
CONJURE_NATIVE_FN(log,       write,                void,      (uint8_t level, const char *utf8, int32_t length))
CONJURE_NATIVE_FN(script,    register_class,       void,      (const conjure_script_class_desc *desc))
CONJURE_NATIVE_FN(script, register_field, void,
  (uint64_t classHash, const conjure_script_field_desc *desc))
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
CONJURE_NATIVE_FN(query, begin, int32_t,
  (const uint32_t *ids, int32_t count, conjure_query_scope *out))
CONJURE_NATIVE_FN(query,     end,                  void,      (uint32_t scopeId))
// --- bindings (R26) ---
// transform_*  (scene::local_transform / world_transform; get_world* compose locals up the parent chain)
CONJURE_NATIVE_FN(transform, get_local,            int32_t,   (uint64_t entity, conjure_transform *out))
CONJURE_NATIVE_FN(transform, set_local,            int32_t,   (uint64_t entity, const conjure_transform *value))
CONJURE_NATIVE_FN(transform, get_world_matrix,     int32_t,   (uint64_t entity, conjure_mat4 *out))
CONJURE_NATIVE_FN(transform, set_parent,           int32_t,   (uint64_t entity, uint64_t parent))
CONJURE_NATIVE_FN(transform, get_world,            int32_t,   (uint64_t entity, conjure_transform *out))
CONJURE_NATIVE_FN(transform, set_world,            int32_t,   (uint64_t entity, const conjure_transform *value))
CONJURE_NATIVE_FN(transform, set_parent_ex,        int32_t,
  (uint64_t entity, uint64_t parent, int32_t keepWorld, int32_t index))
CONJURE_NATIVE_FN(transform, get_interpolated,     int32_t,   (uint64_t entity, conjure_transform *out))
// entity_* beyond core (hierarchy, tag, layer, guid, enabled: need scene::scene)
CONJURE_NATIVE_FN(entity,    set_name,             int32_t,   (uint64_t entity, const char *utf8, int32_t length))
CONJURE_NATIVE_FN(entity,    get_parent,           uint64_t,  (uint64_t entity))
CONJURE_NATIVE_FN(entity,    get_root,             uint64_t,  (uint64_t entity))
CONJURE_NATIVE_FN(entity,    get_children,         int32_t,   (uint64_t entity, uint64_t *out, int32_t capacity))
CONJURE_NATIVE_FN(entity,    set_enabled,          int32_t,   (uint64_t entity, int32_t enabled))
CONJURE_NATIVE_FN(entity,    is_enabled,           int32_t,   (uint64_t entity))
CONJURE_NATIVE_FN(entity,    get_tag,              uint64_t,  (uint64_t entity))
CONJURE_NATIVE_FN(entity,    set_tag,              int32_t,   (uint64_t entity, const char *utf8, int32_t length))
CONJURE_NATIVE_FN(entity,    get_layer,            int32_t,   (uint64_t entity))
CONJURE_NATIVE_FN(entity,    set_layer,            int32_t,   (uint64_t entity, int32_t layer))
CONJURE_NATIVE_FN(entity,    get_guid,             int32_t,   (uint64_t entity, conjure_uuid *out))
CONJURE_NATIVE_FN(entity,    find_by_guid,         uint64_t,  (const conjure_uuid *guid))
CONJURE_NATIVE_FN(entity,    find_by_tag,          int32_t,
  (const char *utf8, int32_t length, uint64_t *out, int32_t capacity))
CONJURE_NATIVE_FN(entity,    find_by_layer,        int32_t,   (int32_t layer, uint64_t *out, int32_t capacity))
CONJURE_NATIVE_FN(entity,    duplicate,            uint64_t,  (uint64_t entity))
CONJURE_NATIVE_FN(entity,    create_many,          int32_t,   (int32_t count, uint64_t parent, uint64_t *out))
// scene_*  (scene::scene_manager; loads are requests applied at FRAME_BEGIN, never immediate)
CONJURE_NATIVE_FN(scene,     request_load,         int32_t,   (const char *utf8Path, int32_t length, int32_t additive))
CONJURE_NATIVE_FN(scene,     request_load_asset,   int32_t,   (const conjure_uuid *scene, int32_t additive))
CONJURE_NATIVE_FN(scene,     request_unload,       int32_t,   (const char *utf8Path, int32_t length))
CONJURE_NATIVE_FN(scene,     is_loaded,            int32_t,   (const char *utf8Path, int32_t length))
CONJURE_NATIVE_FN(scene,     is_load_pending,      int32_t,   (const char *utf8Path, int32_t length))
CONJURE_NATIVE_FN(scene,     active_path,          int32_t,   (char *utf8, int32_t capacity))
CONJURE_NATIVE_FN(scene,     loaded_count,         int32_t,   (void))
CONJURE_NATIVE_FN(scene,     loaded_path,          int32_t,   (int32_t index, char *utf8, int32_t capacity))
CONJURE_NATIVE_FN(scene,     entity_count,         int32_t,   (void))
CONJURE_NATIVE_FN(scene,     structure_version,    uint64_t,  (void))
CONJURE_NATIVE_FN(scene,     roots,                int32_t,   (uint64_t *out, int32_t capacity))
// prefab_*  (conjure.scene.prefab assets; the asset must be LOADED, else ASSET_NOT_LOADED)
CONJURE_NATIVE_FN(prefab,    instantiate,          int32_t,
  (const conjure_uuid *prefab, uint64_t parent, const conjure_transform *localOrNull, uint64_t *outRoot))
CONJURE_NATIVE_FN(prefab,    instantiate_many,     int32_t,
  (const conjure_uuid *prefab, uint64_t parent, int32_t count, uint64_t *outRoots))
CONJURE_NATIVE_FN(prefab,    source_of,            int32_t,   (uint64_t entity, conjure_uuid *outPrefab))
CONJURE_NATIVE_FN(prefab,    is_instance_root,     int32_t,   (uint64_t entity))
// physics_*  (physics::physics_world; body entries need rigid_body, character entries need character_controller)
CONJURE_NATIVE_FN(physics, raycast, int32_t,
  (const conjure_ray_query *query, conjure_raycast_hit *outHit))
CONJURE_NATIVE_FN(physics,   raycast_all,          int32_t,
  (const conjure_ray_query *query, conjure_raycast_hit *outHits, int32_t capacity))
CONJURE_NATIVE_FN(physics, shape_cast, int32_t,
  (const conjure_shape_query *query, conjure_raycast_hit *outHit))
CONJURE_NATIVE_FN(physics,   overlap_sphere,       int32_t,
  (const conjure_vec3 *center, float radius, uint32_t layerMask, uint64_t *out, int32_t capacity))
CONJURE_NATIVE_FN(physics,   overlap_box,          int32_t,
  (const conjure_vec3 *center, const conjure_vec3 *halfExtents, const conjure_quat_xyzw *rotation,
  uint32_t layerMask, uint64_t *out, int32_t capacity))
CONJURE_NATIVE_FN(physics,   get_linear_velocity,  int32_t,   (uint64_t entity, conjure_vec3 *out))
CONJURE_NATIVE_FN(physics,   set_linear_velocity,  int32_t,   (uint64_t entity, const conjure_vec3 *value))
CONJURE_NATIVE_FN(physics,   get_angular_velocity, int32_t,   (uint64_t entity, conjure_vec3 *out))
CONJURE_NATIVE_FN(physics,   set_angular_velocity, int32_t,   (uint64_t entity, const conjure_vec3 *value))
CONJURE_NATIVE_FN(physics, add_force, int32_t,
  (uint64_t entity, const conjure_vec3 *force, uint8_t mode))
CONJURE_NATIVE_FN(physics,   add_force_at,         int32_t,
  (uint64_t entity, const conjure_vec3 *force, const conjure_vec3 *worldPoint, uint8_t mode))
CONJURE_NATIVE_FN(physics, add_torque, int32_t,
  (uint64_t entity, const conjure_vec3 *torque, uint8_t mode))
CONJURE_NATIVE_FN(physics,   move_kinematic,       int32_t,   (uint64_t entity, const conjure_transform *target))
CONJURE_NATIVE_FN(physics,   teleport,             int32_t,   (uint64_t entity, const conjure_transform *target))
CONJURE_NATIVE_FN(physics,   is_sleeping,          int32_t,   (uint64_t entity))
CONJURE_NATIVE_FN(physics,   wake,                 int32_t,   (uint64_t entity))
CONJURE_NATIVE_FN(physics,   get_gravity,          int32_t,   (conjure_vec3 *out))
CONJURE_NATIVE_FN(physics,   set_gravity,          int32_t,   (const conjure_vec3 *value))
CONJURE_NATIVE_FN(physics,   layer_index,          int32_t,   (const char *utf8, int32_t length))
CONJURE_NATIVE_FN(physics,   layer_name,           int32_t,   (int32_t layer, char *utf8, int32_t capacity))
CONJURE_NATIVE_FN(physics,   layers_collide,       int32_t,   (int32_t layerA, int32_t layerB))
CONJURE_NATIVE_FN(physics,   character_move,       int32_t,   (uint64_t entity, const conjure_vec3 *velocity))
CONJURE_NATIVE_FN(physics,   character_is_grounded, int32_t,  (uint64_t entity))
CONJURE_NATIVE_FN(physics,   character_velocity,   int32_t,   (uint64_t entity, conjure_vec3 *out))
// audio_*  (B8: entity entries write audio_source.playbackRequest; the engine's SYNC system applies them)
CONJURE_NATIVE_FN(audio,     play,                 int32_t,   (uint64_t entity))
CONJURE_NATIVE_FN(audio,     stop,                 int32_t,   (uint64_t entity))
CONJURE_NATIVE_FN(audio,     pause,                int32_t,   (uint64_t entity))
CONJURE_NATIVE_FN(audio,     resume,               int32_t,   (uint64_t entity))
CONJURE_NATIVE_FN(audio,     is_playing,           int32_t,   (uint64_t entity))
CONJURE_NATIVE_FN(audio,     play_one_shot,        int32_t,
  (const conjure_uuid *clip, const conjure_vec3 *positionOrNull, float volume, float pitch, uint64_t groupHash))
CONJURE_NATIVE_FN(audio,     set_group_gain,       int32_t,   (uint64_t groupHash, float gain))
CONJURE_NATIVE_FN(audio,     get_group_gain,       int32_t,   (uint64_t groupHash, float *out))
CONJURE_NATIVE_FN(audio,     set_master_gain,      int32_t,   (float gain))
CONJURE_NATIVE_FN(audio,     get_master_gain,      int32_t,   (float *out))
CONJURE_NATIVE_FN(audio,     clip_duration,        int32_t,   (const conjure_uuid *clip, float *outSeconds))
// input_*  (everything readable per frame is in conjure_input_block, D4; these are the rest)
CONJURE_NATIVE_FN(input,     set_cursor_mode,      int32_t,   (uint8_t mode))
CONJURE_NATIVE_FN(input,     get_cursor_mode,      int32_t,   (void))
CONJURE_NATIVE_FN(input,     gamepad_name,         int32_t,   (int32_t index, char *utf8, int32_t capacity))
CONJURE_NATIVE_FN(input,     inject_key,           int32_t,   (uint16_t key, uint8_t action))
CONJURE_NATIVE_FN(input,     inject_mouse_button,  int32_t,   (uint8_t button, uint8_t action))
CONJURE_NATIVE_FN(input,     inject_mouse_move,    int32_t,   (float x, float y))
CONJURE_NATIVE_FN(input,     inject_gamepad_button, int32_t,  (int32_t index, uint8_t button, uint8_t action))
CONJURE_NATIVE_FN(input,     inject_gamepad_axis,  int32_t,   (int32_t index, uint8_t axis, float value))
CONJURE_NATIVE_FN(input,     clipboard_get,        int32_t,   (char *utf8, int32_t capacity))
CONJURE_NATIVE_FN(input,     clipboard_set,        int32_t,   (const char *utf8, int32_t length))
// asset_*  (assets::asset_manager; typeId = assets::asset_type_id of the type name)
CONJURE_NATIVE_FN(asset, ref_get, int32_t,
  (const conjure_uuid *id, uint64_t typeId, conjure_asset_info *out))
CONJURE_NATIVE_FN(asset, ref_bind, int32_t,
  (const conjure_uuid *id, uint64_t typeId, const void **outSlot))
CONJURE_NATIVE_FN(asset, find, int32_t,
  (const char *utf8Path, int32_t length, conjure_uuid *outId))
CONJURE_NATIVE_FN(asset,     path_of,              int32_t,   (const conjure_uuid *id, char *utf8, int32_t capacity))
CONJURE_NATIVE_FN(asset, load, int32_t,
  (const conjure_uuid *id, uint64_t typeId, uint8_t priority))
CONJURE_NATIVE_FN(asset,     load_blocking,        int32_t,   (const conjure_uuid *id, uint64_t typeId))
CONJURE_NATIVE_FN(asset,     release,              int32_t,   (const conjure_uuid *id))
CONJURE_NATIVE_FN(asset,     state_of,             int32_t,   (const conjure_uuid *id))
CONJURE_NATIVE_FN(asset,     type_of,              int32_t,   (const conjure_uuid *id, uint64_t *outTypeId))
CONJURE_NATIVE_FN(asset,     type_id,              uint64_t,  (const char *utf8TypeName, int32_t length))
// diagnostics_*
CONJURE_NATIVE_FN(diagnostics, check,              int32_t,
  (const char *utf8Name, int32_t nameLength, int32_t passed, const char *utf8Detail, int32_t detailLength))
CONJURE_NATIVE_FN(diagnostics, last_error,         int32_t,   (int32_t *outStatus, char *utf8, int32_t capacity))
CONJURE_NATIVE_FN(diagnostics, draw_line,          int32_t,
  (const conjure_vec3 *a, const conjure_vec3 *b, uint32_t rgba, float seconds))
CONJURE_NATIVE_FN(diagnostics, draw_box,           int32_t,
  (const conjure_vec3 *center, const conjure_vec3 *halfExtents, const conjure_quat_xyzw *rotation, uint32_t rgba,
  float seconds))
CONJURE_NATIVE_FN(diagnostics, draw_sphere,        int32_t,
  (const conjure_vec3 *center, float radius, uint32_t rgba, float seconds))
CONJURE_NATIVE_FN(diagnostics, draw_text,          int32_t,
  (const conjure_vec3 *position, const char *utf8, int32_t length, uint32_t rgba, float seconds))
CONJURE_NATIVE_FN(diagnostics, frame_stats,        int32_t,   (conjure_frame_stats_block *out))
CONJURE_NATIVE_FN(diagnostics, call_count,         int32_t,   (int32_t functionIndex))
// modules_*  (read-only view of modules::module_registry, B12)
CONJURE_NATIVE_FN(modules,   count,                int32_t,   (void))
CONJURE_NATIVE_FN(modules,   find,                 int32_t,   (const char *utf8Name, int32_t length))
CONJURE_NATIVE_FN(modules,   info,                 int32_t,   (int32_t index, conjure_module_record *out))
CONJURE_NATIVE_FN(modules,   name,                 int32_t,   (int32_t index, char *utf8, int32_t capacity))
// Appended at the end, not beside entity_get_tag: entries are never reordered within CONJURE_NATIVE_API_VERSION
// (only the layout hash changes). entity_get_tag_text, code sweep OWN-TAG.
CONJURE_NATIVE_FN(entity,    get_tag_text,         int32_t,   (uint64_t entity, char *utf8, int32_t capacity))
