#include "conjure/testing/doctest.h"

#include "conjure/engine/engine_context.h"
#include "conjure/engine/layer.h"
#include "conjure/platform/event.h"

#include <string>

// layer.h (docs/design/06_engine_loop.md 2.8): the layer base and its no-op defaults.

using namespace conjure;
using namespace conjure::engine;

namespace {

    class named_layer final : public layer {
        public:
            explicit named_layer(std::string name) :
                layer(std::move(name)) {}
            ~named_layer() override = default;
    };
}

TEST_SUITE("engine/layer") {

    TEST_CASE("layer - keeps its name and every default hook is a harmless no-op") {
        named_layer value("hud");
        engine_context context = {};
        CHECK(value.name() == "hud");
        CONJURE_CHECK_OK(value.on_attach(context));
        value.on_frame_begin(context);
        value.on_update(context);
        value.on_imgui(context);
        value.on_imgui_end(context);
        value.on_frame_end(context);
        const platform::event closing = platform::close_event{};
        CHECK_FALSE(value.on_event(context, closing));
        value.on_detach(context);
    }
}
