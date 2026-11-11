#include "conjure/testing/doctest.h"

#include "engine_test_support.h"

#include "conjure/engine/application.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/exit_code.h"
#include "conjure/engine/game_layer.h"
#include "conjure/platform/event.h"

// game_layer.h (docs/design/06_engine_loop.md 2.8): the runtime's bottom layer turns a window close into
// request_quit(OK) and leaves every other event alone.

using namespace conjure;
using namespace conjure::engine;

TEST_SUITE("engine/game_layer") {

    TEST_CASE("game_layer - attaches without a running application") {
        game_layer layer = {};
        engine_context context = {};
        CHECK(layer.name() == "game");
        CONJURE_CHECK_OK(layer.on_attach(context));
        CHECK(layer.close_requests() == 0);
        layer.on_detach(context);
    }

    TEST_CASE("game_layer - a close event requests OK and is not consumed") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        game_layer layer = {};
        const platform::event closing = platform::close_event{};
        CHECK_FALSE(layer.on_event(app.context(), closing));
        CHECK(layer.close_requests() == 1);
        CHECK(app.is_quit_requested());
        CHECK(app.current_exit_code() == exit_code::OK);
    }

    TEST_CASE("game_layer - other events are ignored") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        game_layer layer = {};
        platform::key_event key = {};
        key.keyCode = platform::key::SPACE;
        key.action = platform::button_action::PRESS;
        CHECK_FALSE(layer.on_event(app.context(), platform::event(key)));
        CHECK(layer.close_requests() == 0);
        CHECK_FALSE(app.is_quit_requested());
    }
}
