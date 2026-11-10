#include "conjure/scripting/script_system.h"

#include "conjure/scripting/hot_reload.h"

#include "conjure/core/string_id.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"

#include <utility>

using namespace conjure;
using namespace conjure::scripting;

namespace {

    [[nodiscard]] ecs::system_desc exclusive_system(std::string_view name, ecs::system_phase phase) {
        ecs::system_desc desc = {};
        desc.name = core::string_id(name);
        desc.phase = phase;
        desc.flags = ecs::system_flags::EXCLUSIVE;
        return desc;
    }
}

core::result<void> conjure::scripting::register_script_systems(
  ecs::scheduler &scheduler,
  script_runtime &runtime,
  hot_reload_controller *reload
) {
    script_runtime *target = &runtime;

    ecs::system_desc frameBegin = exclusive_system(SYSTEM_FRAME_BEGIN, ecs::system_phase::FRAME_BEGIN);
    frameBegin.run = [target, reload](ecs::system_context &context) {
        if (reload != nullptr) {
            reload->tick(context.frame().time);
        }
        target->flush_pending();
    };
    CONJURE_TRY(scheduler.add_system(std::move(frameBegin)));

    ecs::system_desc collisionDispatch = exclusive_system(SYSTEM_COLLISION_DISPATCH, ecs::system_phase::FIXED_UPDATE);
    collisionDispatch.before.push_back(core::string_id(SYSTEM_FIXED_UPDATE));
    collisionDispatch.run = [target](ecs::system_context &) {
        const std::function<std::span<const collision_event>()> &source = target->collision_source();
        if (source) {
            target->dispatch_collision_events(source());
        }
    };
    CONJURE_TRY(scheduler.add_system(std::move(collisionDispatch)));

    ecs::system_desc fixedUpdate = exclusive_system(SYSTEM_FIXED_UPDATE, ecs::system_phase::FIXED_UPDATE);
    fixedUpdate.after.push_back(core::string_id(SYSTEM_COLLISION_DISPATCH));
    fixedUpdate.run = [target](ecs::system_context &context) {
        target->run_phase(script_phase::FIXED_UPDATE, context.frame().fixedDeltaTime);
    };
    CONJURE_TRY(scheduler.add_system(std::move(fixedUpdate)));

    ecs::system_desc update = exclusive_system(SYSTEM_UPDATE, ecs::system_phase::UPDATE);
    update.run = [target](ecs::system_context &context) {
        target->run_phase(script_phase::UPDATE, context.delta_time());
    };
    CONJURE_TRY(scheduler.add_system(std::move(update)));

    ecs::system_desc lateUpdate = exclusive_system(SYSTEM_LATE_UPDATE, ecs::system_phase::LATE_UPDATE);
    lateUpdate.run = [target](ecs::system_context &context) {
        target->run_phase(script_phase::LATE_UPDATE, context.delta_time());
    };
    CONJURE_TRY(scheduler.add_system(std::move(lateUpdate)));

    ecs::system_desc frameEnd = exclusive_system(SYSTEM_FRAME_END, ecs::system_phase::FRAME_END);
    frameEnd.run = [target](ecs::system_context &) {
        target->flush_pending();
    };
    CONJURE_TRY(scheduler.add_system(std::move(frameEnd)));
    return core::result<void>::success();
}
