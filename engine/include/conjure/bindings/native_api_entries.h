#pragma once

#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/call_counters.h"

#include "conjure/core/api.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/scripting/native_api.h"

#include <cstdint>
#include <string_view>
#include <type_traits>

// The bindings-owned entries of the native table (docs/design/15_bindings.md 2.3, 3.1, B2, B3).
//
// Filtered X-macro (B2): CONJURE_BINDINGS_FOR_EACH_ENTRY(X) expands X(group, name, ret, (args)) for the
// entries of the ten bindings groups (transform, the hierarchy half of entity, scene, prefab, physics,
// audio, input, asset, diagnostics, modules; R26/R64) and nothing for the core entries of scripting. It is
// the generated CONJURE_NATIVE_API_FOR_EACH_BINDINGS_ENTRY of native_api_hash.h, which the generator cuts
// at the `// --- bindings (R26) ---` marker of native_api.inl, so a line added there without an
// implementation here is a link error, never a null slot.
//
// The real implementations live in entries::<group>_<name> (one entries_<group>.cpp each); the stubs that
// answer -NOT_SUPPORTED for a compiled-out or disabled subsystem are stubs::stub_thunk<Id, Fn>::invoke.
// fill_native_table installs one or the other per group, wrap_with_counters replaces every filled slot
// with its counting thunk when !CONJURE_DISTRIBUTION.

/*===========================================================================*/
/*===========================================================================*/
// Group filter

/// <summary>
/// X(group, name, ret, (args)) per bindings-owned entry in table order.
/// </summary>
#define CONJURE_BINDINGS_FOR_EACH_ENTRY(X) CONJURE_NATIVE_API_FOR_EACH_BINDINGS_ENTRY(X)

/// <summary>
/// 1 for the ten bindings groups, 0 for the core groups (the entity group counts as owned: its five core
/// entries sit before the marker and are therefore absent from CONJURE_BINDINGS_FOR_EACH_ENTRY).
/// </summary>
#define CONJURE_BINDINGS_OWNS_GROUP(group) CONJURE_BINDINGS_OWNS_GROUP_##group
#define CONJURE_BINDINGS_OWNS_GROUP_log 0
#define CONJURE_BINDINGS_OWNS_GROUP_script 0
#define CONJURE_BINDINGS_OWNS_GROUP_component 0
#define CONJURE_BINDINGS_OWNS_GROUP_query 0
#define CONJURE_BINDINGS_OWNS_GROUP_transform 1
#define CONJURE_BINDINGS_OWNS_GROUP_entity 1
#define CONJURE_BINDINGS_OWNS_GROUP_scene 1
#define CONJURE_BINDINGS_OWNS_GROUP_prefab 1
#define CONJURE_BINDINGS_OWNS_GROUP_physics 1
#define CONJURE_BINDINGS_OWNS_GROUP_audio 1
#define CONJURE_BINDINGS_OWNS_GROUP_input 1
#define CONJURE_BINDINGS_OWNS_GROUP_asset 1
#define CONJURE_BINDINGS_OWNS_GROUP_diagnostics 1
#define CONJURE_BINDINGS_OWNS_GROUP_modules 1

/// <summary>
/// 1 when the module behind a group is part of this build; the physics and audio groups follow their
/// CONJURE_WITH_* options, every other group is always compiled in.
/// </summary>
#define CONJURE_BINDINGS_GROUP_COMPILED(group) CONJURE_BINDINGS_GROUP_COMPILED_##group
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #define CONJURE_BINDINGS_GROUP_COMPILED_physics 1
#else
    #define CONJURE_BINDINGS_GROUP_COMPILED_physics 0
#endif
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    #define CONJURE_BINDINGS_GROUP_COMPILED_audio 1
#else
    #define CONJURE_BINDINGS_GROUP_COMPILED_audio 0
#endif
#define CONJURE_BINDINGS_GROUP_COMPILED_transform 1
#define CONJURE_BINDINGS_GROUP_COMPILED_entity 1
#define CONJURE_BINDINGS_GROUP_COMPILED_scene 1
#define CONJURE_BINDINGS_GROUP_COMPILED_prefab 1
#define CONJURE_BINDINGS_GROUP_COMPILED_input 1
#define CONJURE_BINDINGS_GROUP_COMPILED_asset 1
#define CONJURE_BINDINGS_GROUP_COMPILED_diagnostics 1
#define CONJURE_BINDINGS_GROUP_COMPILED_modules 1

namespace conjure::core::jobs {
    class job_system;
}

namespace conjure::ecs {
    class component_registry;
    class world;
}

namespace conjure::scene {
    class scene_manager;
}

namespace conjure::physics {
    class physics_world;
}

namespace conjure::audio {
    class audio_engine;
}

namespace conjure::assets {
    class asset_manager;
    class asset_database;
}

namespace conjure::platform {
    class input;
    class window;
}

namespace conjure::renderer {
    class renderer;
    class debug_draw;
}

namespace conjure::modules {
    class module_registry;
}

namespace conjure::scripting {
    class script_runtime;
}

namespace conjure::engine {
    struct engine_context;
    struct engine_time;
    class frame_stats;

    namespace diagnostics {
        class check_registry;
    }
}

namespace conjure::bindings {

    /*===========================================================================*/
    /*===========================================================================*/
    // The owned entries

    /// <summary>
    /// Number of bindings-owned entries (everything after the R26 marker of native_api.inl).
    /// </summary>
    inline constexpr u32 BINDINGS_ENTRY_COUNT = CONJURE_NATIVE_API_ENTRY_COUNT - CONJURE_NATIVE_API_CORE_ENTRY_COUNT;

    /// <summary>
    /// Number of groups the bindings own (R64).
    /// </summary>
    inline constexpr u32 BINDINGS_GROUP_COUNT = 10;

    /// <summary>
    /// The real implementations, one per owned entry, with the exact ABI signature of native_api.inl.
    /// They act on the context fill_native_table installed (active_context()) and follow the status
    /// convention of bindings_error.h (B4).
    /// </summary>
    namespace entries {
#define CONJURE_BINDINGS_DECLARE_ENTRY(group, name, ret, args) CONJURE_API ret group##_##name args;
        CONJURE_BINDINGS_FOR_EACH_ENTRY(CONJURE_BINDINGS_DECLARE_ENTRY)
#undef CONJURE_BINDINGS_DECLARE_ENTRY
    }

    /// <summary>
    /// The stand-ins installed for a group whose subsystem is compiled out or disabled (B3): every call
    /// records NOT_SUPPORTED naming the entry and returns the failure value of its return type.
    /// </summary>
    namespace stubs {

        /// <summary>
        /// Records "<entry>: subsystem unavailable" as the last error and returns -NOT_SUPPORTED.
        /// </summary>
        [[nodiscard]] CONJURE_API int32_t report_not_supported(native_function_id id) noexcept;

        template <native_function_id Id, typename Fn>
        struct stub_thunk;

        template <native_function_id Id, typename Ret, typename... Args>
        struct stub_thunk<Id, Ret(Args...)> {
            /// <summary>
            /// The failure value of the entry: -NOT_SUPPORTED for int32_t, 0 (NULL_ENTITY) for uint64_t.
            /// </summary>
            static Ret invoke(Args...) noexcept {
                const int32_t status = report_not_supported(Id);
                if constexpr (std::is_same_v<Ret, int32_t>) {
                    return status;
                } else {
                    return Ret{};
                }
            }
        };
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Context and table filling

    /// <summary>
    /// Non-owning view of the engine services the entries act on (15_bindings.md 4.2). A null pointer
    /// means the subsystem is compiled out or disabled: fill_native_table then installs stubs for the
    /// groups that need it. Every pointer is stable from application::initialise() to shutdown().
    /// </summary>
    struct CONJURE_API binding_context {
        ecs::world *world = nullptr;
        ecs::component_registry *components = nullptr;
        scene::scene_manager *scenes = nullptr;
        physics::physics_world *physics = nullptr;
        audio::audio_engine *audio = nullptr;
        assets::asset_manager *assets = nullptr;
        assets::asset_database *assetDatabase = nullptr;      // asset_path_of; null in cooked projects
        platform::input *input = nullptr;
        platform::window *window = nullptr;                   // cursor mode, clipboard, gamepad names
        renderer::renderer *renderer = nullptr;
        renderer::debug_draw *debugDraw = nullptr;            // diagnostics_draw_*; null = silent success
        engine::diagnostics::check_registry *checks = nullptr;
        const modules::module_registry *modules = nullptr;
        scripting::script_runtime *scripting = nullptr;
        const engine::frame_stats *stats = nullptr;
        const engine::engine_time *time = nullptr;            // frame index of diagnostics_check records

        /// <summary>
        /// The view of an engine_context (every pointer copied; debugDraw stays null because the
        /// engine does not publish it, so debug drawing is a silent success there).
        /// </summary>
        [[nodiscard]] static binding_context from(const engine::engine_context &context) noexcept;
    };

    /// <summary>
    /// True when the group's real implementations have what they need in 'context' (the world and
    /// the scene manager for transform, entity, scene and prefab; the physics world; the audio engine
    /// and the world; the input and the window; the asset manager; the module registry); the
    /// diagnostics group is always available. Unknown groups are never available.
    /// </summary>
    [[nodiscard]] CONJURE_API bool group_available(std::string_view group, const binding_context &context) noexcept;

    /// <summary>
    /// True when the module behind the group is compiled into this build (CONJURE_BINDINGS_GROUP_COMPILED).
    /// </summary>
    [[nodiscard]] CONJURE_API bool group_compiled(std::string_view group) noexcept;

    /// <summary>
    /// Fills every bindings-owned slot of 'api' (3.1 step 3): the real implementation when the group is
    /// compiled in and available, the stub otherwise. A slot already holding a foreign pointer (a plugin's
    /// native_binding_desc) is overridden with a WARN when the group is compiled in and kept when it is
    /// compiled out (B3). Core-owned slots are never touched. Installs 'context' as the active context
    /// (one table per process). Idempotent. Fails with TABLE_INCOMPLETE when an owned slot is still null
    /// afterwards (only reachable with a broken build).
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> fill_native_table(
      conjure_native_api &api, const binding_context &context
    );

    /// <summary>
    /// Replaces every non-null slot (core-owned ones included) with its counting_thunk and remembers the
    /// original as the thunk's target (B6), then installs the scope-end hook over query_end that applies the
    /// scene fixups of the locked entries (3.3). A slot already pointing at its thunk is left alone and the
    /// hook is re-installed over whatever it already covered, so a second call never double-counts and
    /// never chains the hook behind itself. In Distribution only the hook is installed.
    /// </summary>
    CONJURE_API void wrap_with_counters(conjure_native_api &api) noexcept;

    /// <summary>
    /// True while the scope-end hook of wrap_with_counters sits over query_end. False again after
    /// script_runtime::initialize refilled the core slots (a runtime re-initialised after shutdown()), which
    /// is the signal to call wrap_with_counters once more.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_scope_hook_installed(const conjure_native_api &api) noexcept;

    /// <summary>
    /// The context the last fill_native_table installed (every pointer null before the first fill).
    /// </summary>
    [[nodiscard]] CONJURE_API const binding_context &active_context() noexcept;

    /// <summary>
    /// Forgets the active context (shutdown; the entries then answer NOT_SUPPORTED).
    /// </summary>
    CONJURE_API void clear_active_context() noexcept;

    /// <summary>
    /// True when the slot of 'id' holds the entry's stub (directly or behind its counting thunk).
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_stub_installed(const conjure_native_api &api, native_function_id id) noexcept;

    /// <summary>
    /// True when the slot of 'id' holds the entry's real implementation (directly or behind its thunk).
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_implementation_installed(
      const conjure_native_api &api,
      native_function_id id
    ) noexcept;

    /// <summary>
    /// True when the slot of 'id' holds the entry's counting thunk.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_counting_installed(const conjure_native_api &api, native_function_id id) noexcept;

    namespace detail {
        /// <summary>
        /// fail() for an entry by id: records the failure and, outside Distribution, logs it at DEBUG once per
        /// entry per frame (a dead-entity loop cannot flood the console, 15_bindings.md 3.4).
        /// </summary>
        [[nodiscard]] CONJURE_API int32_t fail_entry(
          native_function_id id,
          bindings_error code,
          std::string_view detail = {}
        ) noexcept;

        /// <summary>
        /// fail_entry with the code and message of a core::error from a called module.
        /// </summary>
        [[nodiscard]] CONJURE_API int32_t fail_entry(native_function_id id, const core::error &failure) noexcept;

        /// <summary>
        /// Forgets which entries logged a failure this frame (begin_frame).
        /// </summary>
        CONJURE_API void clear_frame_failure_log() noexcept;

        /// <summary>
        /// True when the entry already logged a failure since the last clear_frame_failure_log.
        /// </summary>
        [[nodiscard]] CONJURE_API bool failure_logged_this_frame(native_function_id id) noexcept;
    }
}
