#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/uuid.h"
#include "conjure/editor/command_registry.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/schema_validator.h"
#include "conjure/editor/ui/editor_workspace.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/undo_stack.h"

#include <imgui.h>

#include <filesystem>
#include <string>
#include <vector>

// editor_workspace.h: the dockspace, the default panels, the global shortcuts, Save Scene As and the layout file.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

namespace {
    void draw_frame(editor_test::headless_editor &editor, editor_workspace &workspace, imgui_frame_scope &scope) {
        workspace.update(editor.context());
        scope.begin_frame();
        workspace.draw(editor.context());
        scope.end_frame();
    }

    /// <summary>
    /// The modifier ImGui matches against ImGuiMod_Ctrl in shortcuts: with macOS behaviours on, AddKeyEvent swaps
    /// Ctrl and Super, so the Cmd key (Super) is what reaches a Ctrl shortcut.
    /// </summary>
    [[nodiscard]] ImGuiKey shortcut_modifier() {
        return ImGui::GetIO().ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl;
    }
}

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

    TEST_CASE("regression - a scene made with New Scene is saved through Save Scene As") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("scene.new", core::json::object({{"name", "Untitled"}}));
        (void) editor.create_entity("Kept");
        CHECK(editor.context().scene_path().empty());
        editor_workspace workspace = {};
        workspace.create_default_panels(editor.context());
        CHECK_FALSE(workspace.save_as_pending());
        // Save Scene (and Ctrl+S) on a scene without a path asks for one instead of failing.
        workspace.request_save_scene(editor.context());
        CHECK(workspace.save_as_pending());
        CHECK(workspace.save_as_path() == "scenes/Untitled");
        imgui_frame_scope scope{};
        for (int frame = 0; frame < 2; ++frame) {
            draw_frame(editor, workspace, scope);
        }
        // The modal stays up across frames until the user saves or cancels.
        CHECK(workspace.save_as_pending());
        scope.begin_frame();
        CHECK(ImGui::IsPopupOpen("Save Scene As"));
        scope.end_frame();
        CHECK_FALSE(workspace.save_scene_as(editor.context(), "   "));
        CHECK_FALSE(workspace.save_as_error().empty());
        CHECK(workspace.save_as_pending());
        REQUIRE(workspace.save_scene_as(editor.context(), "scenes/from_ui"));
        CHECK(workspace.save_as_error().empty());
        CHECK_FALSE(workspace.save_as_pending());
        CHECK(editor.context().scene_path() == "assets://scenes/from_ui.scene.json");
        const std::filesystem::path saved = editor.project_directory() / "assets" / "scenes" / "from_ui.scene.json";
        CHECK(std::filesystem::is_regular_file(saved));
        // The next frame closes the dialog; Save Scene now saves in place.
        draw_frame(editor, workspace, scope);
        scope.begin_frame();
        CHECK_FALSE(ImGui::IsPopupOpen("Save Scene As"));
        scope.end_frame();
        workspace.request_save_scene(editor.context());
        CHECK_FALSE(workspace.save_as_pending());
        CHECK_FALSE(editor.context().scene_dirty());
    }

    TEST_CASE("editor_workspace - Ctrl+Z and F5 work with the toolbar panel closed") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Undone");
        editor_workspace workspace = {};
        workspace.create_default_panels(editor.context());
        REQUIRE(workspace.find_panel("toolbar") != nullptr);
        workspace.find_panel("toolbar")->set_open(false);
        imgui_frame_scope scope{};
        // Shortcut routes are claimed one frame and honoured the next.
        draw_frame(editor, workspace, scope);
        ImGuiIO &io = ImGui::GetIO();
        io.AddKeyEvent(shortcut_modifier(), true);
        io.AddKeyEvent(ImGuiKey_Z, true);
        draw_frame(editor, workspace, scope);
        CHECK(editor.scene().find_guid(*core::uuid::parse(guid)).is_null());
        CHECK(editor.context().undo().can_redo());
        io.AddKeyEvent(ImGuiKey_Z, false);
        io.AddKeyEvent(shortcut_modifier(), false);
        draw_frame(editor, workspace, scope);
        CHECK(editor.context().play().state() == play_state::EDITING);
        io.AddKeyEvent(ImGuiKey_F5, true);
        draw_frame(editor, workspace, scope);
        io.AddKeyEvent(ImGuiKey_F5, false);
        draw_frame(editor, workspace, scope);
        CHECK(editor.context().play().state() == play_state::PLAYING);
        io.AddKeyEvent(ImGuiMod_Shift, true);
        io.AddKeyEvent(ImGuiKey_F5, true);
        draw_frame(editor, workspace, scope);
        io.AddKeyEvent(ImGuiKey_F5, false);
        io.AddKeyEvent(ImGuiMod_Shift, false);
        draw_frame(editor, workspace, scope);
        CHECK(editor.context().play().state() == play_state::EDITING);
    }

    TEST_CASE("editor_workspace - Ctrl+S on a scene without a path opens Save Scene As") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("scene.new", core::json::object({{"name", ""}}));
        editor_workspace workspace = {};
        workspace.create_default_panels(editor.context());
        imgui_frame_scope scope{};
        draw_frame(editor, workspace, scope);
        ImGuiIO &io = ImGui::GetIO();
        io.AddKeyEvent(shortcut_modifier(), true);
        io.AddKeyEvent(ImGuiKey_S, true);
        draw_frame(editor, workspace, scope);
        io.AddKeyEvent(ImGuiKey_S, false);
        io.AddKeyEvent(shortcut_modifier(), false);
        draw_frame(editor, workspace, scope);
        CHECK(workspace.save_as_pending());
        CHECK(workspace.save_as_path() == "scenes/untitled");
    }
}
