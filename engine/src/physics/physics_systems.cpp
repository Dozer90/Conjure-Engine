#include "conjure/physics/register.h"

#include "physics/collision_mesh_importer.h"
#include "physics/collision_mesh_loader.h"

#include "conjure/physics/character_controller.h"
#include "conjure/physics/collision_mesh_asset.h"
#include "conjure/physics/components.h"
#include "conjure/physics/physics_error.h"

#include "conjure/assets/asset_type_registry.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/scene/components.h"

#include <memory>
#include <utility>

namespace conjure::physics {

namespace {

    /// <summary>
    /// Sets the bit of a registered component (unregistered ones are left out).
    /// </summary>
    template <typename ComponentType>
    void declare(const ecs::component_registry &registry, ecs::component_mask &mask) {
        const ecs::component_id id = registry.id_of<ComponentType>();
        if (id.is_valid()) {
            mask.set(id);
        }
    }
}

core::result<void> register_components(ecs::component_registry &registry) {
    CONJURE_TRY(registry.register_component<rigid_body>());
    CONJURE_TRY(registry.register_component<box_collider>());
    CONJURE_TRY(registry.register_component<sphere_collider>());
    CONJURE_TRY(registry.register_component<capsule_collider>());
    CONJURE_TRY(registry.register_component<convex_collider>());
    CONJURE_TRY(registry.register_component<mesh_collider>());
    CONJURE_TRY(registry.register_component<physics_material>());
    CONJURE_TRY(registry.register_component<character_controller>());
    return core::result<void>::success();
}

core::result<void> register_systems(ecs::scheduler &scheduler, physics_world &world) {
    if (!world.is_attached()) {
        return core::make_error(physics_error::NOT_ATTACHED, "attach the physics_world before register_systems");
    }
    physics_world *target = &world;
    // The constants are constant-initialised and therefore never interned; interning once here makes
    // the names readable in scheduler diagnostics.
    CONJURE_UNUSED(core::string_id::intern("physics.push_transforms"));
    CONJURE_UNUSED(core::string_id::intern("physics.step"));
    CONJURE_UNUSED(core::string_id::intern("physics.pull_transforms"));
    CONJURE_UNUSED(core::string_id::intern("physics.contact_events"));

    ecs::system_desc push = {};
    push.name = SYSTEM_PUSH_TRANSFORMS;
    push.phase = ecs::system_phase::PHYSICS_STEP;
    push.flags = ecs::system_flags::MAIN_THREAD;
    push.before.push_back(SYSTEM_STEP);
    push.resolveAccess = [](
      const ecs::component_registry &registry,
      ecs::component_mask &reads,
      ecs::component_mask &writes
    ) {
        declare<rigid_body>(registry, reads);
        declare<box_collider>(registry, reads);
        declare<sphere_collider>(registry, reads);
        declare<capsule_collider>(registry, reads);
        declare<convex_collider>(registry, reads);
        declare<mesh_collider>(registry, reads);
        declare<physics_material>(registry, reads);
        declare<character_controller>(registry, reads);
        declare<scene::local_transform>(registry, reads);
        declare<scene::parent>(registry, reads);
        declare<scene::interpolated_transform>(registry, writes);
    };
    push.run = [target](ecs::system_context &context) {
        target->push_transforms(context.frame().fixedDeltaTime, &context.commands());
    };
    CONJURE_TRY(scheduler.add_system(std::move(push)));

    ecs::system_desc step = {};
    step.name = SYSTEM_STEP;
    step.phase = ecs::system_phase::PHYSICS_STEP;
    step.flags = ecs::system_flags::MAIN_THREAD;
    step.after.push_back(SYSTEM_PUSH_TRANSFORMS);
    step.run = [target](ecs::system_context &context) {
        target->step(context.frame().fixedDeltaTime);
    };
    CONJURE_TRY(scheduler.add_system(std::move(step)));

    ecs::system_desc pull = {};
    pull.name = SYSTEM_PULL_TRANSFORMS;
    pull.phase = ecs::system_phase::FIXED_END;
    pull.flags = ecs::system_flags::NONE;
    pull.before.push_back(SYSTEM_CONTACT_EVENTS);
    pull.resolveAccess = [](
      const ecs::component_registry &registry,
      ecs::component_mask &reads,
      ecs::component_mask &writes
    ) {
        declare<scene::parent>(registry, reads);
        declare<scene::local_transform>(registry, writes);
        declare<scene::interpolated_transform>(registry, writes);
        declare<rigid_body>(registry, writes);
        declare<character_controller>(registry, writes);
    };
    pull.run = [target](ecs::system_context &context) {
        target->pull_transforms(&context);
    };
    CONJURE_TRY(scheduler.add_system(std::move(pull)));

    ecs::system_desc contacts = {};
    contacts.name = SYSTEM_CONTACT_EVENTS;
    contacts.phase = ecs::system_phase::FIXED_END;
    contacts.flags = ecs::system_flags::MAIN_THREAD;
    contacts.after.push_back(SYSTEM_PULL_TRANSFORMS);
    contacts.run = [target](ecs::system_context &) {
        target->collect_contact_events();
    };
    CONJURE_TRY(scheduler.add_system(std::move(contacts)));
    return core::result<void>::success();
}

core::result<void> register_asset_types(assets::asset_type_registry &types, assets::importer_registry &importers) {
    CONJURE_TRY(types.register_type(
      assets::make_asset_type_desc<collision_mesh_asset>(std::make_unique<detail::collision_mesh_loader>(), 1)
    ));
    CONJURE_TRY(importers.register_importer(std::make_unique<detail::collision_mesh_importer>()));
    return core::result<void>::success();
}
}
