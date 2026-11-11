#include "conjure/engine/engine_subsystem.h"

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

engine_subsystem::engine_subsystem() {}

engine_subsystem::~engine_subsystem() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> engine_subsystem::post_startup(engine_context &context) {
    CONJURE_UNUSED(context);
    return core::result<void>::success();
}

void engine_subsystem::register_systems(ecs::scheduler &scheduler, engine_context &context) {
    CONJURE_UNUSED(scheduler);
    CONJURE_UNUSED(context);
}

void engine_subsystem::begin_frame(engine_context &context) {
    CONJURE_UNUSED(context);
}

void engine_subsystem::end_frame(engine_context &context) {
    CONJURE_UNUSED(context);
}
