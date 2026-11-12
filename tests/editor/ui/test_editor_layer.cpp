#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/editor/ui/editor_layer.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/layer.h"
#include "conjure/platform/event.h"
#include "conjure/platform/null_backend.h"
#include "conjure/platform/window.h"

#include <memory>

// editor_layer.h: the engine layer that owns the workspace.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/editor_layer") {

    TEST_CASE("editor_layer - attaches a workspace with the default panels and detaches it") {
        editor_test::headless_editor editor = {};
        editor_layer layer(editor.context());
        CHECK(layer.name() == "editor-ui");
        CHECK(layer.workspace() == nullptr);
        CONJURE_REQUIRE_OK(layer.on_attach(editor.context().engine_context()));
        REQUIRE(layer.workspace() != nullptr);
        CHECK(layer.workspace()->open_panel_count() == 6);
        CHECK(layer.workspace()->layout_file() == editor.project_directory() / ".conjure" / "editor_layout.ini");
        layer.on_detach(editor.context().engine_context());
        CHECK(layer.workspace() == nullptr);
    }

    TEST_CASE("editor_layer - attaching gives the window the editor icon") {
        editor_test::headless_editor editor = {};
        platform::window *window = editor.context().engine_context().window;
        REQUIRE(window != nullptr);
        REQUIRE(window->as_null() != nullptr);
        CONJURE_REQUIRE_OK(window->set_icon({}));
        editor_layer layer(editor.context());
        CONJURE_REQUIRE_OK(layer.on_attach(editor.context().engine_context()));
        REQUIRE_FALSE(window->as_null()->icons().empty());
        CHECK(window->as_null()->icons().front().width() == 16);
        CHECK(window->as_null()->icons().back().width() == 256);
        layer.on_detach(editor.context().engine_context());
    }

    TEST_CASE("editor_layer - on_update refreshes and on_imgui draws only inside an ImGui frame") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Visible");
        editor_layer layer(editor.context());
        CONJURE_REQUIRE_OK(layer.on_attach(editor.context().engine_context()));
        layer.on_update(editor.context().engine_context());
        layer.on_imgui(editor.context().engine_context());
        CHECK(layer.frames_drawn() == 0);
        imgui_frame_scope scope{};
        scope.begin_frame();
        layer.on_update(editor.context().engine_context());
        layer.on_imgui(editor.context().engine_context());
        scope.end_frame();
        CHECK(layer.frames_drawn() == 1);
        platform::key_event key = {};
        key.keyCode = platform::key::A;
        key.action = platform::button_action::PRESS;
        CHECK_FALSE(layer.on_event(editor.context().engine_context(), platform::event(key)));
        layer.on_detach(editor.context().engine_context());
    }

    TEST_CASE("create_editor_layer - returns an editor_layer") {
        editor_test::headless_editor editor = {};
        std::unique_ptr<engine::layer> layer = create_editor_layer(editor.context());
        REQUIRE(layer != nullptr);
        CHECK(layer->name() == "editor-ui");
        CHECK(dynamic_cast<editor_layer *>(layer.get()) != nullptr);
    }
}
