#include "engine/subsystems/ecs_subsystem.h"

#include "conjure/engine/log_category.h"

#include "conjure/core/log.h"

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

ecs_subsystem::ecs_subsystem(engine_services &services) :
    mServices(&services) {}

ecs_subsystem::~ecs_subsystem() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::string_view ecs_subsystem::name() const noexcept {
    return "ecs";
}

core::result<void> ecs_subsystem::startup(engine_context &context) {
    mServices->components = std::make_unique<ecs::component_registry>();
    mServices->world = std::make_unique<ecs::world>(*mServices->components);

    ecs::scheduler_desc desc = {};
    const bool parallel = mServices->jobs.worker_count() > 0;
    desc.parallelSystems = parallel;
    desc.parallelChunks = parallel;
    mServices->scheduler = std::make_unique<ecs::scheduler>(*mServices->world, &mServices->jobs, desc);

    context.components = mServices->components.get();
    context.world = mServices->world.get();
    context.scheduler = mServices->scheduler.get();
    CONJURE_LOG_DEBUG(LOG_ENGINE, "ecs: world and scheduler ready ({} worker(s))", mServices->jobs.worker_count());
    return core::result<void>::success();
}

void ecs_subsystem::shutdown(engine_context &context) {
    context.scheduler = nullptr;
    context.world = nullptr;
    context.components = nullptr;
    mServices->scheduler.reset();
    mServices->world.reset();
    mServices->components.reset();
}
