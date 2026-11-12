#include "conjure/testing/doctest.h"

#include "engine_test_support.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"
#include "conjure/engine/application.h"
#include "conjure/engine/imgui_layer.h"
#include "conjure/platform/event.h"
#include "conjure/platform/window.h"

#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR
    #include <imgui.h>
#endif

#include <memory>

// imgui_layer.h (docs/design/06_engine_loop.md 2.8, R9): the single ImGui context, opened at frame begin,
// closed at imgui_end, fed from the platform events. Editor builds only.

using namespace conjure;
using namespace conjure::engine;

#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR

TEST_SUITE("engine/imgui_layer") {

    TEST_CASE("imgui_layer - attach creates the context and detach destroys it") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        imgui_layer layer = {};
        CHECK(layer.context() == nullptr);
        CONJURE_REQUIRE_OK(layer.on_attach(app.context()));
        CHECK(layer.context() != nullptr);
        CHECK_FALSE(layer.is_frame_open());
        layer.on_detach(app.context());
        CHECK(layer.context() == nullptr);
        app.shutdown();
    }

    TEST_CASE("imgui_layer - a frame opens at frame begin and closes at imgui_end") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        imgui_layer *layer = static_cast<imgui_layer *>(app.layers().push_overlay(std::make_unique<imgui_layer>()));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(layer->context() != nullptr);
        CONJURE_REQUIRE_OK(app.step_frame());
        CONJURE_REQUIRE_OK(app.step_frame());
        CHECK(layer->stats().framesBegun == 2);
        CHECK(layer->stats().framesRendered == 2);
        CHECK_FALSE(layer->is_frame_open());
        app.shutdown();
        CHECK(layer->context() == nullptr);
    }

    TEST_CASE("imgui_layer - forwards input events and never consumes them without a capturing widget") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        imgui_layer layer = {};
        CONJURE_REQUIRE_OK(layer.on_attach(app.context()));
        layer.on_frame_begin(app.context());

        platform::key_event key = {};
        key.keyCode = platform::key::A;
        key.action = platform::button_action::PRESS;
        CHECK_FALSE(layer.on_event(app.context(), platform::event(key)));
        platform::mouse_move_event move = {};
        move.position = math::vec2(10.0f, 20.0f);
        CHECK_FALSE(layer.on_event(app.context(), platform::event(move)));
        platform::mouse_button_event button = {};
        button.button = platform::mouse_button::LEFT;
        button.action = platform::button_action::PRESS;
        CHECK_FALSE(layer.on_event(app.context(), platform::event(button)));
        platform::text_event text = {};
        text.codepoint = 'a';
        CHECK_FALSE(layer.on_event(app.context(), platform::event(text)));
        CHECK_FALSE(layer.on_event(app.context(), platform::event(platform::close_event{})));
        CHECK(layer.stats().eventsForwarded == 4);
        CHECK(layer.stats().eventsConsumed == 0);

        layer.on_imgui_end(app.context());
        CHECK(layer.stats().framesRendered == 1);
        layer.on_detach(app.context());
        app.shutdown();
    }

    TEST_CASE("imgui_layer - releases reach the engine while a widget captures the keyboard and the mouse") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        imgui_layer layer = {};
        CONJURE_REQUIRE_OK(layer.on_attach(app.context()));
        layer.on_frame_begin(app.context());

        // What NewFrame sets while a text field or a window under the cursor holds capture.
        ImGui::SetCurrentContext(layer.context());
        ImGui::GetIO().WantCaptureKeyboard = true;
        ImGui::GetIO().WantCaptureMouse = true;

        platform::key_event key = {};
        key.keyCode = platform::key::A;
        key.action = platform::button_action::PRESS;
        CHECK(layer.on_event(app.context(), platform::event(key)));
        key.action = platform::button_action::REPEAT;
        CHECK(layer.on_event(app.context(), platform::event(key)));
        key.action = platform::button_action::RELEASE;
        CHECK_FALSE(layer.on_event(app.context(), platform::event(key)));

        platform::mouse_button_event button = {};
        button.button = platform::mouse_button::LEFT;
        button.action = platform::button_action::PRESS;
        CHECK(layer.on_event(app.context(), platform::event(button)));
        button.action = platform::button_action::RELEASE;
        CHECK_FALSE(layer.on_event(app.context(), platform::event(button)));
        CHECK(layer.stats().eventsConsumed == 3);

        layer.on_imgui_end(app.context());
        layer.on_detach(app.context());
        app.shutdown();
    }

    TEST_CASE("imgui_layer - the clipboard round-trips through the null window") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        imgui_layer layer = {};
        CONJURE_REQUIRE_OK(layer.on_attach(app.context()));
        layer.set_clipboard_text("copied");
        CHECK(std::string_view(layer.clipboard_text()) == "copied");
        const core::result<std::string> windowText = app.context().window->clipboard_text();
        CONJURE_REQUIRE_OK(windowText);
        CHECK(windowText.value() == "copied");
        layer.on_detach(app.context());
        app.shutdown();
    }
}

#endif
