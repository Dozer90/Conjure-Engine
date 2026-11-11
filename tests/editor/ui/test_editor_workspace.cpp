#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/editor/command_registry.h"
#include "conjure/editor/schema_validator.h"
#include "conjure/editor/ui/editor_workspace.h"
#include "conjure/editor/ui/imgui_frame_scope.h"

#include <filesystem>
#include <string>
#include <vector>

// editor_workspace.h: the dockspace, the default panels and the layout file.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/editor_workspace") {

    TEST_CASE("editor_workspace - create_default_panels registers every built-in panel and opens the defaults") {
        editor_test::headless_editor editor = {};
        editor_workspace workspace = {};
        CHECK(workspace.open_panel_count() == 0);
        workspace.create_default_panels(editor.context());
        CHECK(workspace.panels().size() == 10);
        CHECK(workspace.open_panel_count() == 6);
        for (const char *name : {
          "toolbar", "viewport", "hierarchy", "inspector", "assets", "console", "history", "profiler", "ai_session",
          "export",
        }) {
            CHECK_MESSAGE(workspace.panels().find(name) != nullptr, name);
        }
        CHECK(workspace.find_panel("hierarchy") != nullptr);
        CHECK(workspace.find_panel("history") == nullptr);
        CHECK(workspace.open_panel("history"));
        CHECK(workspace.find_panel("history") != nullptr);
        CHECK(workspace.open_panel_count() == 7);
        CHECK_FALSE(workspace.open_panel("no_such_panel"));
        CHECK(workspace.close_panel("history"));
        CHECK_FALSE(workspace.close_panel("history"));
        CHECK(workspace.open_panel_count() == 6);
        // Re-opening an instance that was only hidden flips its flag.
        workspace.find_panel("console")->set_open(false);
        CHECK(workspace.open_panel("console"));
        CHECK(workspace.find_panel("console")->is_open());
    }

    TEST_CASE("editor_workspace - draws the dockspace, the menu bar and the default panels for several frames") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Drawn");
        editor_workspace workspace = {};
        workspace.create_default_panels(editor.context());
        imgui_frame_scope scope{};
        for (int frame = 0; frame < 3; ++frame) {
            workspace.update(editor.context());
            scope.begin_frame();
            workspace.draw(editor.context());
            scope.end_frame();
        }
        CHECK(workspace.frames_drawn() == 3);
        CHECK(workspace.open_panel_count() == 6);
        // Drawing outside a frame is a no-op.
        workspace.draw(editor.context());
        CHECK(workspace.frames_drawn() == 3);
    }

    TEST_CASE("editor_workspace - every optional panel draws inside the workspace for several frames") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Drawn");
        for (const char *name : {"history", "profiler", "ai_session", "export"}) {
            SUBCASE(name) {
                editor_workspace workspace = {};
                workspace.create_default_panels(editor.context());
                REQUIRE(workspace.open_panel(name));
                imgui_frame_scope scope{};
                for (int frame = 0; frame < 3; ++frame) {
                    workspace.update(editor.context());
                    scope.begin_frame();
                    workspace.draw(editor.context());
                    scope.end_frame();
                }
                CHECK(workspace.frames_drawn() == 3);
                CHECK(workspace.open_panel_count() == 7);
            }
        }
    }

    TEST_CASE("regression - every menu request names a registered command and validates against its schema") {
        editor_test::headless_editor editor = {};
        const command_registry &registry = editor.context().registry();
        const std::vector<menu_request> requests = editor_workspace::menu_requests();
        REQUIRE(requests.size() == 11);
        bool newScene = false;
        for (const menu_request &request : requests) {
            CAPTURE(request.command);
            const command_descriptor *descriptor = registry.find(request.command);
            REQUIRE(descriptor != nullptr);
            const compiled_schema *schema = registry.args_schema(request.command);
            REQUIRE(schema != nullptr);
            CONJURE_CHECK_OK(schema->validate(request.args.root(), "args"));
            if (request.command == "scene.new") {
                newScene = true;
                CHECK(request.args.root()["name"].as_string() == "Untitled");
                // The menu's request works end to end, not just on paper.
                const core::json::document made = editor.ok(request.command, request.args);
                CHECK(made.root()["name"].as_string() == "Untitled");
            }
        }
        CHECK(newScene);
    }

    TEST_CASE("editor_workspace - module panels follow the consumer and layout files round trip") {
        editor_test::headless_editor editor = {};
        editor_workspace workspace = {};
        workspace.create_default_panels(editor.context());
        workspace.sync_module_panels(editor.context());
        CHECK(workspace.panels().size() == 10);
        const std::filesystem::path layout = editor.project_directory() / ".conjure" / "editor_layout.ini";
        workspace.set_layout_file(layout);
        CHECK(workspace.layout_file() == layout);
        imgui_frame_scope scope{};
        scope.begin_frame();
        workspace.draw(editor.context());
        scope.end_frame();
        workspace.save_layout();
        CHECK(std::filesystem::is_regular_file(layout));
        editor_workspace reloaded = {};
        reloaded.set_layout_file(layout);
        reloaded.create_default_panels(editor.context());
        scope.begin_frame();
        reloaded.draw(editor.context());
        scope.end_frame();
        CHECK(reloaded.frames_drawn() == 1);
        CHECK(workspace.drawers().drawer(core::reflect::field_kind::F32));
    }
}
