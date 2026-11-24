#include "bindings/entry_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_slot.h"
#include "conjure/ecs/entity_archive.h"
#include "conjure/scene/components.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/scene.h"
#include "conjure/scripting/script_runtime.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

// prefab_* (docs/design/15_bindings.md 2.3, 3.3, B7): instantiation of LOADED conjure.scene.prefab assets
// through scene::prefab while the world is unlocked and through command_buffer::instantiate while a query
// scope holds it (the root id is real at once, components appear at the flush, scene.apply_hierarchy_changes
// links the parent the same frame).

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::bindings::detail;

namespace {

    template <typename T>
    [[nodiscard]] std::span<const std::byte> bytes_of(const T &value) noexcept {
        return std::as_bytes(std::span<const T, 1>(&value, 1));
    }

    struct resolved_prefab {
        const scene::prefab_asset *asset = nullptr;
        const assets::asset_slot *slot = nullptr;
        assets::asset_id id = {};
    };

    /// <summary>
    /// The LOADED prefab behind a uuid; records INVALID_ARGUMENT, NOT_SUPPORTED, ASSET_TYPE_MISMATCH,
    /// ASSET_NOT_LOADED or PREFAB_INVALID.
    /// </summary>
    [[nodiscard]] bool resolve_prefab(
      native_function_id id,
      const conjure_uuid *prefab,
      resolved_prefab &out,
      int32_t &status
    ) noexcept {
        if (prefab == nullptr) {
            status = fail_entry(id, bindings_error::INVALID_ARGUMENT, "prefab");
            return false;
        }
        if (active_context().assets == nullptr || active_scene() == nullptr) {
            status = unavailable(id, "assets");
            return false;
        }
        out.id = marshal::to_uuid(*prefab);
        if (out.id.is_null()) {
            status = fail_entry(id, bindings_error::INVALID_ARGUMENT, "null prefab uuid");
            return false;
        }
        // A read: the slot is looked up, never created, so an unknown or stale uuid costs nothing permanent.
        out.slot = active_context().assets->find_slot(out.id);
        assets::asset_type_id type = assets::asset_type_id::INVALID;
        if (out.slot != nullptr) {
            type = out.slot->type.load(std::memory_order_acquire);
        }
        if (type == assets::asset_type_id::INVALID) {
            type = active_context().assets->type_of(out.id).value_or(assets::asset_type_id::INVALID);
        }
        if (type != assets::asset_type_id::INVALID && type != assets::asset_type_traits<scene::prefab_asset>::id()) {
            status = fail_entry(id, bindings_error::ASSET_TYPE_MISMATCH, "not a conjure.scene.prefab asset");
            return false;
        }
        if (out.slot == nullptr || out.slot->state.load(std::memory_order_acquire) != assets::asset_state::LOADED) {
            status = fail_entry(id, bindings_error::ASSET_NOT_LOADED, "prefab is not resident");
            return false;
        }
        out.asset = static_cast<const scene::prefab_asset *>(out.slot->object.load(std::memory_order_acquire));
        if (out.asset == nullptr) {
            status = fail_entry(id, bindings_error::ASSET_NOT_LOADED, "prefab is not resident");
            return false;
        }
        if (out.asset->archive == nullptr || out.asset->archive->entity_count() == 0) {
            status = fail_entry(id, bindings_error::PREFAB_INVALID, "payload is not an entity archive");
            return false;
        }
        return true;
    }

    [[nodiscard]] bool resolve_parent(
      native_function_id id,
      uint64_t packed,
      ecs::entity &parent,
      int32_t &status
    ) noexcept {
        if (packed == 0) {
            parent = ecs::NULL_ENTITY;
            return true;
        }
        parent = resolve_alive(id, packed, status);
        return !parent.is_null();
    }

    /// <summary>
    /// The locked path of one instance (3.3): instantiate command, root transform and the prefab_instance
    /// marker applied at the flush, the parent link through a scene fixup right after it. prefab_link markers
    /// on the inner entities are the unlocked path's business; a deferred instance carries only the root marker.
    /// </summary>
    [[nodiscard]] int32_t instantiate_deferred(
      native_function_id id,
      marshal::structural_guard &guard,
      const resolved_prefab &prefab,
      ecs::entity parent,
      const conjure_transform *local,
      ecs::entity &outRoot
    ) noexcept {
        scene::scene &active = *active_scene();
        ecs::command_buffer &commands = guard.commands();
        const core::result<ecs::entity> root = commands.instantiate(prefab.asset->archive);
        if (!root.has_value()) {
            return fail_entry(id, root.get_error());
        }
        // The core component_* / script_add / entity_destroy entries record their requests on the root behind the
        // instantiation (17_demo_games.md P2).
        guard.runtime->note_deferred_instance(root.value());
        // The root is adopted (as a child of 'parent' or as a scene root) once the flush unlocked the world;
        // the inner entities carry their archive parents and scene.apply_hierarchy_changes links them.
        scene_fixup fixup = {};
        fixup.operation = scene_fixup::kind::ATTACH;
        fixup.entity = root.value();
        fixup.parent = parent;
        fixup.index = scene::APPEND_CHILD;
        queue_fixup(fixup);
        if (local != nullptr) {
            const scene::local_transform transform =
              scene::local_transform::from_transform(marshal::to_transform(*local));
            commands.set_raw(root.value(), active.ids().localTransform, bytes_of(transform));
        }
        scene::prefab_instance marker = {};
        marker.prefab = assets::asset_ref<scene::prefab_asset>(prefab.id, prefab.slot);
        commands.add_raw(root.value(), active.ids().prefabInstance, bytes_of(marker));
        outRoot = root.value();
        return 0;
    }
}

int32_t conjure::bindings::entries::prefab_instantiate(
  const conjure_uuid *prefab,
  uint64_t parent,
  const conjure_transform *localOrNull,
  uint64_t *outRoot
) {
    constexpr native_function_id ID = native_function_id::prefab_instantiate;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (outRoot == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "outRoot");
    }
    int32_t status = 0;
    resolved_prefab resolved = {};
    if (!resolve_prefab(ID, prefab, resolved, status)) {
        return status;
    }
    ecs::entity parentEntity = ecs::NULL_ENTITY;
    if (!resolve_parent(ID, parent, parentEntity, status)) {
        return status;
    }
    marshal::structural_guard guard = structural();
    if (guard.deferred()) {
        if (!guard.can_defer()) {
            return fail_entry(ID, bindings_error::STRUCTURE_LOCKED, "no deferred command buffer");
        }
        ecs::entity root = ecs::NULL_ENTITY;
        status = instantiate_deferred(ID, guard, resolved, parentEntity, localOrNull, root);
        if (status < 0) {
            return status;
        }
        *outRoot = marshal::from_entity(root);
        return 0;
    }
    scene::instantiate_options options = {};
    options.parent = parentEntity;
    if (localOrNull != nullptr) {
        options.rootTransform = scene::local_transform::from_transform(marshal::to_transform(*localOrNull));
    }
    const core::result<ecs::entity> root = scene::prefab::instantiate(*active_scene(), *resolved.asset, options);
    if (!root.has_value()) {
        return fail_entry(ID, root.get_error());
    }
    *outRoot = marshal::from_entity(root.value());
    return 0;
}

int32_t conjure::bindings::entries::prefab_instantiate_many(
  const conjure_uuid *prefab,
  uint64_t parent,
  int32_t count,
  uint64_t *outRoots
) {
    constexpr native_function_id ID = native_function_id::prefab_instantiate_many;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (count < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "count");
    }
    int32_t status = 0;
    resolved_prefab resolved = {};
    if (!resolve_prefab(ID, prefab, resolved, status)) {
        return status;
    }
    if (count == 0) {
        return 0;
    }
    if (outRoots == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "outRoots");
    }
    ecs::entity parentEntity = ecs::NULL_ENTITY;
    if (!resolve_parent(ID, parent, parentEntity, status)) {
        return status;
    }
    marshal::structural_guard guard = structural();
    if (guard.deferred()) {
        if (!guard.can_defer()) {
            return fail_entry(ID, bindings_error::STRUCTURE_LOCKED, "no deferred command buffer");
        }
        for (int32_t index = 0; index < count; ++index) {
            ecs::entity root = ecs::NULL_ENTITY;
            status = instantiate_deferred(ID, guard, resolved, parentEntity, nullptr, root);
            if (status < 0) {
                return status;
            }
            outRoots[index] = marshal::from_entity(root);
        }
        return count;
    }
    scene::instantiate_options options = {};
    options.parent = parentEntity;
    std::vector<ecs::entity> &roots = scratch_entities();
    roots.resize(static_cast<usize>(count));
    const core::result<void> created =
      scene::prefab::instantiate_many(*active_scene(), *resolved.asset, static_cast<u32>(count), options, roots);
    if (!created.has_value()) {
        return fail_entry(ID, created.get_error());
    }
    for (int32_t index = 0; index < count; ++index) {
        outRoots[index] = marshal::from_entity(roots[static_cast<usize>(index)]);
    }
    return count;
}

int32_t conjure::bindings::entries::prefab_source_of(uint64_t entity, conjure_uuid *outPrefab) {
    constexpr native_function_id ID = native_function_id::prefab_source_of;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (outPrefab == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "outPrefab");
    }
    int32_t status = 0;
    const scene::prefab_instance *instance =
      component_for_read<scene::prefab_instance>(ID, entity, "prefab_instance", status);
    if (instance == nullptr) {
        return status;
    }
    *outPrefab = marshal::from_uuid(instance->prefab.id());
    return 0;
}

int32_t conjure::bindings::entries::prefab_is_instance_root(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::prefab_is_instance_root;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return status;
    }
    return active_world()->has<scene::prefab_instance>(target) ? 1 : 0;
}
