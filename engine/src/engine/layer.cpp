#include "conjure/engine/layer.h"

#include <utility>

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

layer::layer(std::string name) :
    mName(std::move(name)) {}

layer::~layer() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> layer::on_attach(engine_context &context) {
    CONJURE_UNUSED(context);
    return core::result<void>::success();
}

void layer::on_detach(engine_context &context) {
    CONJURE_UNUSED(context);
}

void layer::on_frame_begin(engine_context &context) {
    CONJURE_UNUSED(context);
}

void layer::on_update(engine_context &context) {
    CONJURE_UNUSED(context);
}

void layer::on_imgui(engine_context &context) {
    CONJURE_UNUSED(context);
}

void layer::on_imgui_end(engine_context &context) {
    CONJURE_UNUSED(context);
}

void layer::on_frame_end(engine_context &context) {
    CONJURE_UNUSED(context);
}

bool layer::on_event(engine_context &context, const platform::event &event) {
    CONJURE_UNUSED(context);
    CONJURE_UNUSED(event);
    return false;
}
