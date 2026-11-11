#include "conjure/engine/game_layer.h"

#include "conjure/engine/application.h"

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

game_layer::game_layer() :
    layer("game") {}

game_layer::~game_layer() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> game_layer::on_attach(engine_context &context) {
    // Scene loading is startup step 20 (06_engine_loop.md 3.4); nothing to do here.
    CONJURE_UNUSED(context);
    return core::result<void>::success();
}

bool game_layer::on_event(engine_context &context, const platform::event &event) {
    if (platform::event_as<platform::close_event>(event) == nullptr) {
        return false;
    }
    ++mCloseRequests;
    if (context.app != nullptr) {
        context.app->request_quit(exit_code::OK);
    }
    return false;
}
