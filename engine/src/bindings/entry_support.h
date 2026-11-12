#pragma once

#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/call_counters.h"
#include "conjure/bindings/marshal.h"
#include "conjure/bindings/native_api_entries.h"

#include "conjure/core/hash.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"

#include <cstdint>
#include <string_view>
#include <vector>

// Helpers shared by the entries_<group>.cpp files: the active context, the thread guard every entry
// opens with, entity resolution with the ENTITY_NOT_ALIVE / COMPONENT_MISSING distinction of
// 15_bindings.md 3.2 and a main-thread scratch vector for the entries that gather entity lists.

namespace conjure::bindings::detail {

    /// <summary>
    /// False when the caller is not the main thread (asserting in Debug and Development and logging at ERROR);
    /// the guard macros then return the WRONG_THREAD status. The last-error buffer and the per-frame log bits
    /// are not written on that path: only the main thread owns them (15_bindings.md 5).
    /// </summary>
    [[nodiscard]] bool check_main_thread(native_function_id id) noexcept;

    /// <summary>
    /// The scene of the active context, or nullptr when the scene manager is absent.
    /// </summary>
    [[nodiscard]] scene::scene *active_scene() noexcept;

    /// <summary>
    /// The world of the active context, or nullptr.
    /// </summary>
    [[nodiscard]] ecs::world *active_world() noexcept;

    /// <summary>
    /// The packed id as a live entity of the active world; records ENTITY_NOT_ALIVE and yields
    /// NULL_ENTITY otherwise (also when no world is attached, which records NOT_SUPPORTED).
    /// </summary>
    [[nodiscard]] ecs::entity resolve_alive(native_function_id id, uint64_t packed, int32_t &status) noexcept;

    /// <summary>
    /// The component of a live entity for writing (stamps the column); records COMPONENT_MISSING naming
    /// the component, or ENTITY_NOT_ALIVE, and yields nullptr otherwise.
    /// </summary>
    template <typename ComponentType>
    [[nodiscard]] ComponentType *component_for_write(
      native_function_id id,
      uint64_t packed,
      std::string_view componentName,
      int32_t &status
    ) noexcept {
        const ecs::entity entity = resolve_alive(id, packed, status);
        if (entity.is_null()) {
            return nullptr;
        }
        ComponentType *component = active_world()->template get<ComponentType>(entity);
        if (component == nullptr) {
            status = fail_entry(id, bindings_error::COMPONENT_MISSING, componentName);
            return nullptr;
        }
        return component;
    }

    /// <summary>
    /// The component of a live entity for reading (never stamps); errors as component_for_write.
    /// </summary>
    template <typename ComponentType>
    [[nodiscard]] const ComponentType *component_for_read(
      native_function_id id,
      uint64_t packed,
      std::string_view componentName,
      int32_t &status
    ) noexcept {
        const ecs::entity entity = resolve_alive(id, packed, status);
        if (entity.is_null()) {
            return nullptr;
        }
        const ecs::world &world = *active_world();
        const ComponentType *component = world.template get<ComponentType>(entity);
        if (component == nullptr) {
            status = fail_entry(id, bindings_error::COMPONENT_MISSING, componentName);
            return nullptr;
        }
        return component;
    }

    /// <summary>
    /// The id of a component registered under 'registeredName' in the active world's registry (INVALID when
    /// unregistered or without a world). For types another module registers by name only (the engine's
    /// audio components, R7): no component_name specialisation is needed here.
    /// </summary>
    [[nodiscard]] ecs::component_id component_id_of(std::string_view registeredName) noexcept;

    /// <summary>
    /// component_for_write for a component addressed by its registered name.
    /// </summary>
    template <typename ComponentType>
    [[nodiscard]] ComponentType *named_component_for_write(
      native_function_id id,
      uint64_t packed,
      std::string_view registeredName,
      std::string_view componentName,
      int32_t &status
    ) noexcept {
        const ecs::entity entity = resolve_alive(id, packed, status);
        if (entity.is_null()) {
            return nullptr;
        }
        const ecs::component_id componentId = component_id_of(registeredName);
        void *component = componentId.is_valid() ? active_world()->get_raw(entity, componentId, true) : nullptr;
        if (component == nullptr) {
            status = fail_entry(id, bindings_error::COMPONENT_MISSING, componentName);
            return nullptr;
        }
        return static_cast<ComponentType *>(component);
    }

    /// <summary>
    /// component_for_read for a component addressed by its registered name.
    /// </summary>
    template <typename ComponentType>
    [[nodiscard]] const ComponentType *named_component_for_read(
      native_function_id id,
      uint64_t packed,
      std::string_view registeredName,
      std::string_view componentName,
      int32_t &status
    ) noexcept {
        const ecs::entity entity = resolve_alive(id, packed, status);
        if (entity.is_null()) {
            return nullptr;
        }
        const ecs::component_id componentId = component_id_of(registeredName);
        const ecs::world &world = *active_world();
        const void *component = componentId.is_valid() ? world.get_raw(entity, componentId) : nullptr;
        if (component == nullptr) {
            status = fail_entry(id, bindings_error::COMPONENT_MISSING, componentName);
            return nullptr;
        }
        return static_cast<const ComponentType *>(component);
    }

    /// <summary>
    /// A cleared scratch list for entity gathering (main thread only; capacity is kept across calls).
    /// </summary>
    [[nodiscard]] std::vector<ecs::entity> &scratch_entities() noexcept;

    /// <summary>
    /// The structural guard of the active context (world and script runtime).
    /// </summary>
    [[nodiscard]] marshal::structural_guard structural() noexcept;

    /// <summary>
    /// Records NOT_SUPPORTED naming the entry and the missing subsystem.
    /// </summary>
    [[nodiscard]] int32_t unavailable(native_function_id id, std::string_view subsystem) noexcept;

    /// <summary>
    /// A scene-level operation a locked entry could not express as ECS commands (15_bindings.md 3.3): it is
    /// applied through the real scene API as soon as the world is unlocked again (right after the
    /// deferred_commands() flush of query_end, or at the next entry call / begin_frame after a force-close).
    /// ATTACH adopts an entity (a reserved id made alive by the flush, or a reparented one) under 'parent'
    /// (NULL_ENTITY = root) at 'index'; SET_ENABLED toggles the disabled tag and the descendants' inactive tags.
    /// </summary>
    struct scene_fixup {
        enum class kind : u8 {
            ATTACH,
            SET_ENABLED
        };

        kind operation = kind::ATTACH;
        ecs::entity entity = {};
        ecs::entity parent = {};
        u32 index = 0xFFFFFFFFu;
        bool keepWorld = false;
        bool enabled = true;
    };

    /// <summary>
    /// Queues a fixup for the next apply_pending_fixups.
    /// </summary>
    void queue_fixup(const scene_fixup &fixup);

    /// <summary>
    /// Applies every queued fixup in order while the world is unlocked (entities dead by then are skipped with
    /// a DEBUG log, a fixup the scene refuses is logged at WARN); does nothing while it is locked. Returns the
    /// number applied. The queue swaps storage with a scratch queue, so neither loses its capacity.
    /// </summary>
    u32 apply_pending_fixups() noexcept;

    /// <summary>
    /// Fixups still waiting for an unlocked world.
    /// </summary>
    [[nodiscard]] usize pending_fixup_count() noexcept;
}

// Opens an int32_t entry: WRONG_THREAD off the main thread.
#define CONJURE_BINDINGS_GUARD_STATUS(id)                                                                   \
    do {                                                                                                    \
        if (!::conjure::bindings::detail::check_main_thread(id)) {                                          \
            return ::conjure::bindings::to_status(::conjure::bindings::bindings_error::WRONG_THREAD);       \
        }                                                                                                   \
    } while (false)

// Opens a uint64_t entry: 0 (NULL_ENTITY) off the main thread.
#define CONJURE_BINDINGS_GUARD_ENTITY(id)                                                                   \
    do {                                                                                                    \
        if (!::conjure::bindings::detail::check_main_thread(id)) {                                          \
            return 0;                                                                                       \
        }                                                                                                   \
    } while (false)
