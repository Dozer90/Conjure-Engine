#pragma once

#include "conjure/scripting/native_api_hash.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

// The C ABI shared with Conjure.Managed (docs/design/03_scripting.md 2.1, 15_bindings.md 2.4).
//
// Everything in this header is standard-layout and blittable: fixed-width integers, floats, pointers and
// the records below, which C# mirrors field for field with StructLayout.Sequential. The records use
// snake_case fields, the documented C-ABI exception to the camelCase rule (R31). conjure_native_api is
// the X-macro expansion of native_api.inl: native fills it (this module the core entries, bindings the
// rest), managed validates size, version and layout hash before calling anything.

#define CONJURE_NATIVE_API_VERSION 1

// The X-macro table. It is expanded once per translation unit (the struct below); the descriptor table
// of native_entry_descriptors() comes from CONJURE_NATIVE_API_FOR_EACH_ENTRY in the generated
// native_api_hash.h, which the generator derives from the same file.
#define CONJURE_NATIVE_API_TABLE "conjure/scripting/native_api.inl"

/*===========================================================================*/
/*===========================================================================*/
// Math and identity records

struct conjure_vec2 {
    float x;
    float y;
};

struct conjure_vec3 {
    float x;
    float y;
    float z;
};

struct conjure_vec4 {
    float x;
    float y;
    float z;
    float w;
};

/// <summary>
/// A quaternion in explicit x, y, z, w storage order (converted from math::quat by name, D15/R58).
/// </summary>
struct conjure_quat_xyzw {
    float x;
    float y;
    float z;
    float w;
};

/// <summary>
/// Column-major 4x4 matrix.
/// </summary>
struct conjure_mat4 {
    float m[16];
};

struct conjure_transform {
    conjure_vec3 position;
    conjure_quat_xyzw rotation;
    conjure_vec3 scale;
};

struct conjure_uuid {
    uint64_t high;
    uint64_t low;
};

/*===========================================================================*/
/*===========================================================================*/
// Per-frame shared memory (D4)

struct conjure_gamepad_state {
    uint32_t buttons_down;
    uint32_t buttons_pressed;
    uint32_t buttons_released;
    float axes[6];
    uint8_t connected;
};

struct conjure_input_block {
    uint64_t keys_down[8];
    uint64_t keys_pressed[8];
    uint64_t keys_released[8];
    float mouse_x;
    float mouse_y;
    float mouse_dx;
    float mouse_dy;
    float scroll_x;
    float scroll_y;
    uint32_t mouse_buttons_down;
    uint32_t mouse_buttons_pressed;
    uint32_t mouse_buttons_released;
    conjure_gamepad_state gamepads[4];
};

/// <summary>
/// Written once per frame by the engine before FRAME_BEGIN and read directly by Time and Input in C#.
/// </summary>
struct alignas(64) conjure_frame_block {
    double time_seconds;
    double unscaled_time_seconds;
    float delta_time;
    float fixed_delta_time;
    float time_scale;
    uint64_t frame_index;
    uint32_t fixed_step_index;
    uint8_t current_phase;
    uint8_t in_play_mode;
    conjure_input_block input;
    uint8_t padding[8];         // explicit tail padding: 440 content bytes rounded to the 64-byte alignment
};

/*===========================================================================*/
/*===========================================================================*/
// Script records

/// <summary>
/// One instance to create; filled by native, out_slot written by managed.
/// </summary>
struct conjure_create_request {
    uint64_t entity;
    uint64_t class_hash;
    const uint8_t *fields;
    int32_t fields_size;
    uint8_t flags;
    int32_t out_slot;
};

struct conjure_collision_record {
    int32_t slot;
    uint64_t other;
    conjure_vec3 point;
    conjure_vec3 normal;
    float impulse;
    uint8_t kind;
};

struct conjure_script_class_desc {
    uint64_t hash;
    const char *full_name;
    int32_t name_length;
    uint16_t callback_mask;
    uint16_t field_count;
};

/// <summary>
/// One serialised field of a script class. meta_json is {"tooltip","min","max","enumNames","assetType",
/// "hidden"}; default_json is the canonical JSON of the default value.
/// </summary>
struct conjure_script_field_desc {
    uint64_t hash;
    const char *name;
    int32_t name_length;
    uint8_t kind;
    uint8_t element_kind;
    const char *default_json;
    int32_t default_length;
    const char *meta_json;
    int32_t meta_length;
};

/// <summary>
/// An [EcsComponent] struct: schema_json lists {"fields":[{"name","type","offset"}]} with core::reflect
/// type names.
/// </summary>
struct conjure_managed_component_desc {
    uint64_t name_hash;
    const char *full_name;
    int32_t name_length;
    uint32_t size;
    uint32_t alignment;
    uint64_t layout_hash;
    const char *schema_json;
    int32_t schema_length;
};

/// <summary>
/// Columns of one chunk inside a DOD query scope, valid until query_end.
/// </summary>
struct conjure_chunk_view {
    const uint64_t *entities;
    void *columns[8];
    int32_t count;
};

struct conjure_query_scope {
    uint32_t scope_id;
    int32_t chunk_count;
    const conjure_chunk_view *chunks;
};

/*===========================================================================*/
/*===========================================================================*/
// Bindings records (R26; 15_bindings.md 2.4)

struct conjure_ray_query {
    conjure_vec3 origin;
    conjure_vec3 direction;
    float max_distance;
    uint32_t layer_mask;
    uint64_t ignore_entity;
};

struct conjure_shape_query {
    conjure_ray_query ray;
    uint8_t shape;
    uint8_t pad[3];
    conjure_vec3 half_extents;
    float radius;
    conjure_quat_xyzw rotation;
};

struct conjure_raycast_hit {
    uint64_t entity;
    conjure_vec3 point;
    conjure_vec3 normal;
    float distance;
    int32_t layer;
};

struct conjure_asset_info {
    uint8_t state;
    uint8_t pad[3];
    uint32_t version;
    uint64_t type_id;
};

struct conjure_module_record {
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    uint8_t state;
    uint8_t kind;
    uint8_t pad[2];
    uint32_t feature_flags;
};

struct conjure_frame_stats_block {
    float frame_ms;
    float phase_ms[10];
    float submit_ms;
    uint32_t fixed_steps;
    uint32_t jobs_executed;
    uint64_t frame_memory_high_water;
};

enum class conjure_force_mode : uint8_t {
    FORCE,
    IMPULSE,
    VELOCITY_CHANGE,
    ACCELERATION
};

enum class conjure_cast_shape : uint8_t {
    SPHERE,
    BOX,
    CAPSULE
};

enum class conjure_cursor_mode : uint8_t {
    NORMAL,
    HIDDEN,
    DISABLED,
    CAPTURED
};

enum class conjure_button_action : uint8_t {
    RELEASE,
    PRESS,
    REPEAT
};

enum class conjure_asset_load_priority : uint8_t {
    LOW,
    NORMAL,
    HIGH
};

/*===========================================================================*/
/*===========================================================================*/
// The two tables

/// <summary>
/// Functions native provides to managed. Every entry defaults to nullptr; the handshake validates the
/// header, and managed throws NotSupportedException for an entry nobody filled (D11).
/// </summary>
struct conjure_native_api {
    uint32_t size = 0;
    uint32_t version = CONJURE_NATIVE_API_VERSION;
    uint64_t layout_hash = CONJURE_NATIVE_API_HASH;
    const conjure_frame_block *frame = nullptr;

#define CONJURE_NATIVE_FN(group, name, ret, args) ret (*group##_##name) args = nullptr;
#include CONJURE_NATIVE_API_TABLE
#undef CONJURE_NATIVE_FN
};

/// <summary>
/// Functions managed provides to native, filled by Conjure.Interop.NativeBridge.Initialize.
/// </summary>
struct conjure_managed_api {
    uint32_t size = 0;
    uint32_t version = 0;
    uint64_t layout_hash = 0;
    int32_t (*load_game_assembly)(const char *utf8Path, int32_t length) = nullptr;
    int32_t (*unload_game_assembly)(void) = nullptr;
    int32_t (*create_instances)(conjure_create_request *requests, int32_t count) = nullptr;
    int32_t (*destroy_instances)(const int32_t *slots, int32_t count, int32_t callOnDestroy) = nullptr;
    int32_t (*run_phase)(uint8_t phase, float deltaTime) = nullptr;
    int32_t (*dispatch_collisions)(const conjure_collision_record *records, int32_t count) = nullptr;
    int32_t (*apply_fields)(int32_t slot, const uint8_t *blob, int32_t size) = nullptr;
    int32_t (*collect_fields)(int32_t slot, uint8_t *buffer, int32_t capacity) = nullptr;
    int32_t (*collect_garbage)(void) = nullptr;
    int32_t (*shutdown)(void) = nullptr;
};

/*===========================================================================*/
/*===========================================================================*/
// Layout pins shared with the C# mirrors

static_assert(std::is_standard_layout_v<conjure_native_api>, "conjure_native_api must be standard layout");
static_assert(std::is_standard_layout_v<conjure_managed_api>, "conjure_managed_api must be standard layout");
static_assert(std::is_standard_layout_v<conjure_frame_block>, "conjure_frame_block must be standard layout");
static_assert(sizeof(conjure_vec2) == 8);
static_assert(sizeof(conjure_vec3) == 12);
static_assert(sizeof(conjure_vec4) == 16);
static_assert(sizeof(conjure_quat_xyzw) == 16);
static_assert(sizeof(conjure_mat4) == 64);
static_assert(sizeof(conjure_transform) == 40);
static_assert(sizeof(conjure_uuid) == 16);
static_assert(sizeof(conjure_gamepad_state) == 40);
static_assert(sizeof(conjure_input_block) == 392);
static_assert(sizeof(conjure_frame_block) == 448);
static_assert(alignof(conjure_frame_block) == 64);
static_assert(offsetof(conjure_frame_block, frame_index) == 32);
static_assert(offsetof(conjure_frame_block, input) == 48);
static_assert(sizeof(conjure_create_request) == 40);
static_assert(sizeof(conjure_collision_record) == 48);
static_assert(sizeof(conjure_script_class_desc) == 24);
static_assert(sizeof(conjure_script_field_desc) == 56);
static_assert(sizeof(conjure_managed_component_desc) == 56);
static_assert(sizeof(conjure_chunk_view) == 80);
static_assert(sizeof(conjure_query_scope) == 16);
static_assert(sizeof(conjure_ray_query) == 40);
static_assert(sizeof(conjure_shape_query) == 80);
static_assert(sizeof(conjure_raycast_hit) == 40);
static_assert(sizeof(conjure_asset_info) == 16);
static_assert(sizeof(conjure_module_record) == 20);
static_assert(sizeof(conjure_frame_stats_block) == 64);
static_assert(offsetof(conjure_native_api, frame) == 16);
static_assert(offsetof(conjure_native_api, log_write) == 24, "the function table starts right after the header");
static_assert(
  sizeof(conjure_native_api) == 24 + CONJURE_NATIVE_API_ENTRY_COUNT * sizeof(void *),
  "native_api_hash.h is stale: run tools/generate_native_api.py"
);

namespace conjure::scripting {

    /// <summary>
    /// Kind of a serialised script field (field_blob payload and inspector schema).
    /// </summary>
    enum class script_field_kind : uint8_t {
        BOOL,
        INT32,
        INT64,
        FLOAT,
        DOUBLE,
        STRING,
        VEC2,
        VEC3,
        VEC4,
        QUAT,
        COLOR,
        ENTITY,
        ASSET,
        ENUM,
        LIST,
        COUNT
    };

    /// <summary>
    /// The three callback phases run_phase takes.
    /// </summary>
    enum class script_phase : uint8_t {
        FIXED_UPDATE,
        UPDATE,
        LATE_UPDATE,
        COUNT
    };

    /// <summary>
    /// Bit index of conjure_script_class_desc::callback_mask and index of the fault counters.
    /// </summary>
    enum class script_callback : uint8_t {
        CREATE,
        START,
        FIXED_UPDATE,
        UPDATE,
        LATE_UPDATE,
        DESTROY,
        COLLISION_ENTER,
        COLLISION_EXIT,
        TRIGGER_ENTER,
        TRIGGER_EXIT,
        COUNT
    };

    /// <summary>
    /// conjure_collision_record::kind.
    /// </summary>
    enum class collision_kind : uint8_t {
        COLLISION_ENTER,
        COLLISION_EXIT,
        TRIGGER_ENTER,
        TRIGGER_EXIT,
        COUNT
    };

    /// <summary>
    /// conjure_create_request::flags bit: the entry already ran OnStart (reload, snapshot restore).
    /// </summary>
    inline constexpr uint8_t CREATE_FLAG_ALREADY_STARTED = 1;

    /// <summary>
    /// Column slots of a conjure_chunk_view.
    /// </summary>
    inline constexpr int32_t QUERY_MAX_COMPONENTS = 8;

    /// <summary>
    /// Entries in the table, core and bindings together.
    /// </summary>
    inline constexpr u32 NATIVE_API_ENTRY_COUNT = CONJURE_NATIVE_API_ENTRY_COUNT;

    /// <summary>
    /// Entries this module implements (everything before the bindings marker in native_api.inl).
    /// </summary>
    inline constexpr u32 NATIVE_API_CORE_ENTRY_COUNT = CONJURE_NATIVE_API_CORE_ENTRY_COUNT;

    /// <summary>
    /// The callback_mask bit of a callback.
    /// </summary>
    [[nodiscard]] constexpr uint16_t callback_bit(script_callback callback) noexcept {
        return static_cast<uint16_t>(1u << static_cast<unsigned>(callback));
    }

    /// <summary>
    /// The stable upper-case name of a field kind ("VEC3"); "COUNT" for out-of-range values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(script_field_kind kind) noexcept;

    /// <summary>
    /// Parses a name produced by to_string(script_field_kind). COUNT when unknown.
    /// </summary>
    [[nodiscard]] CONJURE_API script_field_kind parse_script_field_kind(std::string_view name) noexcept;

    /// <summary>
    /// The upper-case name of a callback ("UPDATE"); "COUNT" for out-of-range values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(script_callback callback) noexcept;

    /// <summary>
    /// Parses a name produced by to_string(script_callback). COUNT when unknown.
    /// </summary>
    [[nodiscard]] CONJURE_API script_callback parse_script_callback(std::string_view name) noexcept;

    /// <summary>
    /// The upper-case name of a phase ("FIXED_UPDATE"); "COUNT" for out-of-range values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(script_phase phase) noexcept;

    /// <summary>
    /// One row of the table as text, as the generator normalised it: signature is "<return> <arguments>"
    /// with single spaces and '*' hugging the parameter name.
    /// </summary>
    struct native_entry_descriptor {
        std::string_view group = {};
        std::string_view name = {};
        std::string_view signature = {};
        u32 index = 0;
    };

    /// <summary>
    /// Every entry of native_api.inl in table order; the bindings entries start at
    /// NATIVE_API_CORE_ENTRY_COUNT.
    /// </summary>
    [[nodiscard]] CONJURE_API std::span<const native_entry_descriptor> native_entry_descriptors() noexcept;

    /// <summary>
    /// FNV-1a 64 over "<group>_<name> <signature>\n" per descriptor, the generator's algorithm run at
    /// runtime; equals CONJURE_NATIVE_API_HASH, which proves both implementations agree.
    /// </summary>
    [[nodiscard]] CONJURE_API u64 compute_native_api_hash() noexcept;

    /// <summary>
    /// A fresh table: header filled, every entry nullptr, frame pointing at 'frame' (may be null).
    /// </summary>
    [[nodiscard]] CONJURE_API conjure_native_api make_native_api(const conjure_frame_block *frame) noexcept;

    /// <summary>
    /// Checks the header (size, version, hash) and that every core entry is filled. ABI_MISMATCH or
    /// NOT_INITIALIZED naming the first missing entry.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> validate_native_api(const conjure_native_api &api);

    /// <summary>
    /// Checks a managed table a host received: size, version, hash and that every function is set.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> validate_managed_api(const conjure_managed_api &api);

    /// <summary>
    /// The function pointer slots of a table as an array of void pointers (index = descriptor index).
    /// </summary>
    [[nodiscard]] CONJURE_API std::span<void *const> native_api_slots(const conjure_native_api &api) noexcept;

    /// <summary>
    /// Mutable form of native_api_slots for fillers that address entries by index.
    /// </summary>
    [[nodiscard]] CONJURE_API std::span<void *> native_api_slots(conjure_native_api &api) noexcept;

    /// <summary>
    /// Index of the entry named "<group>_<name>", or -1.
    /// </summary>
    [[nodiscard]] CONJURE_API i32 find_native_entry(std::string_view fullName) noexcept;
}
