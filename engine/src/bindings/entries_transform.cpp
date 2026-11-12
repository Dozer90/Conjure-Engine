#include "bindings/entry_support.h"

#include "conjure/core/math/transform.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"

#include <cstring>
#include <optional>

// transform_* (docs/design/15_bindings.md 2.3, 3.2, 3.3): local and world transforms of scene entities and
// the reparenting entries (structural, routed through deferred_commands() while a query scope is open).

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::bindings::detail;

namespace {

    constexpr std::string_view LOCAL_TRANSFORM = "local_transform";

    /// <summary>
    /// Resolves the parent argument: 0 means "root", anything else must be alive.
    /// </summary>
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

    [[nodiscard]] int32_t write_world(native_function_id id, uint64_t packed, conjure_transform *out) noexcept {
        if (out == nullptr) {
            return fail_entry(id, bindings_error::INVALID_ARGUMENT, "out");
        }
        int32_t status = 0;
        const ecs::entity entity = resolve_alive(id, packed, status);
        if (entity.is_null()) {
            return status;
        }
        const std::optional<math::transform> world = active_scene()->world_transform_of(entity);
        if (!world.has_value()) {
            return fail_entry(id, bindings_error::COMPONENT_MISSING, LOCAL_TRANSFORM);
        }
        *out = marshal::from_transform(*world);
        return 0;
    }

    [[nodiscard]] int32_t reparent(
      native_function_id id,
      uint64_t packed,
      uint64_t parentPacked,
      bool keepWorld,
      int32_t index
    ) noexcept {
        int32_t status = 0;
        const ecs::entity entity = resolve_alive(id, packed, status);
        if (entity.is_null()) {
            return status;
        }
        ecs::entity parent = ecs::NULL_ENTITY;
        if (!resolve_parent(id, parentPacked, parent, status)) {
            return status;
        }
        const u32 position = index < 0 ? scene::APPEND_CHILD : static_cast<u32>(index);
        scene::scene &target = *active_scene();
        marshal::structural_guard guard = structural();
        if (guard.deferred()) {
            if (!guard.can_defer()) {
                return fail_entry(id, bindings_error::STRUCTURE_LOCKED, "no deferred command buffer");
            }
            // Self-parenting is wrong whatever the tree looks like at the flush, so it is refused now. Cycle and
            // depth checks are not: earlier fixups of the same scope may still change the tree they depend on.
            if (parent == entity) {
                return fail_entry(id, bindings_error::INVALID_ARGUMENT, "an entity cannot be its own parent");
            }
            // Reparenting is a scene operation (children cache, depth tags, roots): it runs through the real
            // API once the scope's flush unlocked the world, in call order with the other fixups (3.3).
            scene_fixup fixup = {};
            fixup.operation = scene_fixup::kind::ATTACH;
            fixup.entity = entity;
            fixup.parent = parent;
            fixup.index = position;
            fixup.keepWorld = keepWorld;
            queue_fixup(fixup);
            return 0;
        }
        const core::result<void> moved = target.set_parent(entity, parent, keepWorld, position);
        if (!moved.has_value()) {
            return fail_entry(id, moved.get_error());
        }
        return 0;
    }
}

int32_t conjure::bindings::entries::transform_get_local(uint64_t entity, conjure_transform *out) {
    constexpr native_function_id ID = native_function_id::transform_get_local;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (out == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "out");
    }
    int32_t status = 0;
    const scene::local_transform *local =
      component_for_read<scene::local_transform>(ID, entity, LOCAL_TRANSFORM, status);
    if (local == nullptr) {
        return status;
    }
    std::memcpy(out, local, sizeof(conjure_transform));
    return 0;
}

int32_t conjure::bindings::entries::transform_set_local(uint64_t entity, const conjure_transform *value) {
    constexpr native_function_id ID = native_function_id::transform_set_local;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (value == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "value");
    }
    int32_t status = 0;
    scene::local_transform *local = component_for_write<scene::local_transform>(ID, entity, LOCAL_TRANSFORM, status);
    if (local == nullptr) {
        return status;
    }
    // Non-structural: the column is stamped by the write and the value lands even inside a query scope.
    std::memcpy(local, value, sizeof(conjure_transform));
    return 0;
}

int32_t conjure::bindings::entries::transform_get_world_matrix(uint64_t entity, conjure_mat4 *out) {
    constexpr native_function_id ID = native_function_id::transform_get_world_matrix;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (out == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "out");
    }
    conjure_transform world = {};
    const int32_t status = write_world(ID, entity, &world);
    if (status < 0) {
        return status;
    }
    *out = marshal::from_mat4(marshal::to_transform(world).to_matrix());
    return 0;
}

int32_t conjure::bindings::entries::transform_set_parent(uint64_t entity, uint64_t parent) {
    constexpr native_function_id ID = native_function_id::transform_set_parent;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    return reparent(ID, entity, parent, true, -1);
}

int32_t conjure::bindings::entries::transform_get_world(uint64_t entity, conjure_transform *out) {
    constexpr native_function_id ID = native_function_id::transform_get_world;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    return write_world(ID, entity, out);
}

int32_t conjure::bindings::entries::transform_set_world(uint64_t entity, const conjure_transform *value) {
    constexpr native_function_id ID = native_function_id::transform_set_world;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (value == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "value");
    }
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return status;
    }
    const core::result<void> written = active_scene()->set_world_transform(target, marshal::to_transform(*value));
    if (!written.has_value()) {
        return fail_entry(ID, written.get_error());
    }
    return 0;
}

int32_t conjure::bindings::entries::transform_set_parent_ex(
  uint64_t entity,
  uint64_t parent,
  int32_t keepWorld,
  int32_t index
) {
    constexpr native_function_id ID = native_function_id::transform_set_parent_ex;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    return reparent(ID, entity, parent, keepWorld != 0, index);
}

int32_t conjure::bindings::entries::transform_get_interpolated(uint64_t entity, conjure_transform *out) {
    constexpr native_function_id ID = native_function_id::transform_get_interpolated;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (out == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "out");
    }
    int32_t status = 0;
    const scene::interpolated_transform *interpolated =
      component_for_read<scene::interpolated_transform>(ID, entity, "interpolated_transform", status);
    if (interpolated == nullptr) {
        return status;
    }
    *out = marshal::from_transform(interpolated->current);
    return 0;
}
