#include "conjure/scene/register.h"

#include "scene/asset_ref_raw.h"
#include "scene/prefab_importer.h"
#include "scene/scene_loader.h"

#include "conjure/scene/components.h"
#include "conjure/scene/log_category.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/scene_serializer.h"

#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/binary_asset.h"
#include "conjure/core/assert.h"
#include "conjure/core/log.h"
#include "conjure/core/string_id.h"

#include <cstring>
#include <memory>
#include <utility>

namespace conjure::scene {

namespace {

    template <u8... Depths>
    core::result<void> register_depth_tags(ecs::component_registry &registry, std::integer_sequence<u8, Depths...>) {
        constexpr ecs::component_flags HIDDEN = ecs::component_flags::TRANSIENT | ecs::component_flags::EDITOR_HIDDEN;
        core::result<void> outcome = core::result<void>::success();
        const auto registerOne = [&](auto tagType) -> bool {
            using tag_type = decltype(tagType);
            if (!outcome.has_value()) {
                return false;
            }
            const core::result<ecs::component_id> registered = registry.register_component<tag_type>(HIDDEN);
            if (!registered.has_value()) {
                outcome = registered.get_error();
                return false;
            }
            return true;
        };
        const bool allRegistered = (registerOne(depth_tag<static_cast<u8>(Depths + 1)>{}) && ...);
        CONJURE_UNUSED(allRegistered);
        return outcome;
    }

    /// <summary>
    /// The layout contract of the bind pass (R25): a constructed asset_ref must read back through the
    /// raw view with the same uuid and slot pointer.
    /// </summary>
    [[nodiscard]] bool verify_asset_ref_layout() noexcept {
        const assets::asset_id id(0x0123456789ABCDEFull, 0xFEDCBA9876543210ull);
        assets::asset_slot slot = {};
        const assets::asset_ref<assets::binary_asset> reference(id, &slot);
        detail::asset_ref_raw raw = {};
        std::memcpy(&raw, &reference, sizeof(raw));
        return raw.id == id && raw.slot == &slot;
    }
}

core::result<void> register_components(ecs::component_registry &registry) {
    using ecs::component_flags;
    constexpr component_flags TRANSIENT = component_flags::TRANSIENT;
    constexpr component_flags HIDDEN = component_flags::TRANSIENT | component_flags::EDITOR_HIDDEN;

    CONJURE_TRY(registry.register_component<parent>());
    CONJURE_TRY(registry.register_component<children>(TRANSIENT));
    CONJURE_TRY(registry.register_component<local_transform>());
    CONJURE_TRY(registry.register_component<world_transform>(TRANSIENT));
    CONJURE_TRY(registry.register_component<render_world_transform>(TRANSIENT));
    CONJURE_TRY(registry.register_component<interpolated_transform>(TRANSIENT));
    CONJURE_TRY(registry.register_component<transform_state>(HIDDEN));
    CONJURE_TRY(registry.register_component<transform_dirty>(HIDDEN));
    CONJURE_TRY(register_depth_tags(registry, std::make_integer_sequence<u8, MAX_HIERARCHY_DEPTH - 1>{}));
    CONJURE_TRY(registry.register_component<name>());
    CONJURE_TRY(registry.register_component<tag>());
    CONJURE_TRY(registry.register_component<layer>());
    CONJURE_TRY(registry.register_component<guid>());
    CONJURE_TRY(registry.register_component<disabled>());
    CONJURE_TRY(registry.register_component<inactive>(HIDDEN));
    CONJURE_TRY(registry.register_component<local_bounds>(TRANSIENT));
    CONJURE_TRY(registry.register_component<prefab_instance>());
    CONJURE_TRY(registry.register_component<prefab_link>());

    const bool layoutOk = verify_asset_ref_layout();
    CONJURE_VERIFY(layoutOk, "asset_ref<T> does not match the raw view the scene bind pass writes through");
    if (!layoutOk) {
        return core::error(core::error_code::INVALID_STATE, "asset_ref layout contract violated");
    }
    return core::result<void>::success();
}

core::result<void> register_asset_types(
  assets::asset_type_registry &types,
  assets::importer_registry &importers,
  const ecs::component_registry &registry
) {
    CONJURE_TRY(types.register_type(
      assets::make_asset_type_desc<scene_asset>(std::make_unique<detail::scene_asset_loader>(registry), 1)
    ));
    CONJURE_TRY(types.register_type(
      assets::make_asset_type_desc<prefab_asset>(std::make_unique<detail::prefab_asset_loader>(registry), 1)
    ));
    CONJURE_TRY(importers.register_importer(std::make_unique<detail::scene_importer>(registry)));
    CONJURE_TRY(importers.register_importer(std::make_unique<detail::prefab_importer>(registry)));
    return core::result<void>::success();
}

core::result<void> register_systems(ecs::scheduler &scheduler, scene_manager &scenes) {
    constexpr ecs::system_flags EXCLUSIVE_PAUSED = ecs::system_flags::EXCLUSIVE | ecs::system_flags::RUNS_WHILE_PAUSED;
    scene_manager *manager = &scenes;

    ecs::system_desc pendingLoads = {};
    pendingLoads.name = core::string_id(SYSTEM_APPLY_PENDING_LOADS);
    pendingLoads.phase = ecs::system_phase::FRAME_BEGIN;
    pendingLoads.flags = EXCLUSIVE_PAUSED;
    pendingLoads.run = [manager](ecs::system_context &) {
        manager->apply_pending_loads();
    };
    CONJURE_TRY(scheduler.add_system(std::move(pendingLoads)));

    ecs::system_desc hierarchy = {};
    hierarchy.name = core::string_id(SYSTEM_APPLY_HIERARCHY_CHANGES);
    hierarchy.phase = ecs::system_phase::TRANSFORM;
    hierarchy.flags = EXCLUSIVE_PAUSED;
    hierarchy.before.push_back(core::string_id(SYSTEM_PROPAGATE_TRANSFORMS));
    hierarchy.run = [manager](ecs::system_context &) {
        manager->active().apply_hierarchy_changes();
    };
    CONJURE_TRY(scheduler.add_system(std::move(hierarchy)));

    CONJURE_TRY(scheduler.add_system(scenes.transforms().describe()));

    ecs::system_desc destroyPending = {};
    destroyPending.name = core::string_id(SYSTEM_DESTROY_PENDING);
    destroyPending.phase = ecs::system_phase::FRAME_END;
    destroyPending.flags = EXCLUSIVE_PAUSED;
    destroyPending.after.push_back(core::string_id("scripting.frame_end"));
    destroyPending.run = [manager](ecs::system_context &) {
        manager->active().destroy_pending();
    };
    CONJURE_TRY(scheduler.add_system(std::move(destroyPending)));
    return core::result<void>::success();
}
}
