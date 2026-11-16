#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/asset_browser_panel.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

// asset_browser_panel.h: the asset database as a table, and the confirmation before the open scene is deleted.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;
using editor_test::TEST_SCENE;
using editor_test::test_scene_file;

namespace {
    std::string uuid_of(editor_test::headless_editor &editor, std::string_view asset) {
        return std::string(editor.ok("asset.info", core::json::object({{"asset", asset}})).root()["uuid"].as_string());
    }

    /// <summary>
    /// One frame on a scope the case keeps: an open popup lives in the ImGui context, which a scope of its own
    /// per frame would throw away.
    /// </summary>
    void draw_frame(imgui_frame_scope &scope, asset_browser_panel &panel, editor_test::headless_editor &editor) {
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
    }
}

TEST_SUITE("editor/ui/panels/asset_browser_panel") {

    TEST_CASE("asset_browser_panel - constructs open and empty") {
        asset_browser_panel panel = {};
        CHECK(panel.title() == "Assets");
        CHECK(panel.is_open());
        CHECK(panel.rows_drawn() == 0);
        CHECK(panel.selected_path().empty());
        CHECK_FALSE(panel.has_pending_delete());
        CHECK(panel.pending_delete_path().empty());
    }

    TEST_CASE("asset_browser_panel - draws one row per primary asset") {
        editor_test::headless_editor editor = {};
        (void) editor_test::write_tiny_png(editor.project_directory() / "assets" / "one.png");
        (void) editor.ok("asset.import", core::json::object({{"source", "one.png"}}));
        (void) editor.ok("scene.save");
        asset_browser_panel panel = {};
        imgui_frame_scope scope{};
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.rows_drawn() == 2);
    }

    TEST_CASE("asset_browser_panel - without a project it draws a hint") {
        editor_test::headless_editor_options options = {};
        options.withProject = false;
        options.withScene = false;
        editor_test::headless_editor editor(options);
        asset_browser_panel panel = {};
        imgui_frame_scope scope{};
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.rows_drawn() == 0);
    }

    TEST_CASE("asset_browser_panel - request_delete deletes an ordinary asset at once and undo restores it") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("scene.save");
        const std::filesystem::path file = editor_test::write_tiny_png(
          editor.project_directory() / "assets" / "one.png"
        );
        (void) editor.ok("asset.import", core::json::object({{"source", "one.png"}}));
        asset_browser_panel panel = {};
        panel.request_delete(editor.context(), uuid_of(editor, "one.png"), "assets://one.png");
        CHECK_FALSE(panel.has_pending_delete());
        CHECK_FALSE(std::filesystem::exists(file));
        CHECK(editor.context().has_scene());
        (void) editor.ok("history.undo");
        CHECK(std::filesystem::is_regular_file(file));
        // A scene file that is not the open scene is an ordinary asset too.
        (void) editor.ok("scene.save_as", core::json::object({{"path", "scenes/other"}}));
        panel.request_delete(editor.context(), uuid_of(editor, TEST_SCENE), TEST_SCENE);
        CHECK_FALSE(panel.has_pending_delete());
        CHECK_FALSE(std::filesystem::exists(test_scene_file(editor)));
        CHECK(editor.context().scene_path() == "assets://scenes/other.scene.json");
    }

    TEST_CASE("regression - code sweep F656 asset_browser_panel asks for the open scene under another name of its "
      "file") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        (void) editor.ok("scene.save");
        const std::string uuid = uuid_of(editor, TEST_SCENE);
        // The scene is open under a name the database does not register for it (a hard link here; on a volume
        // that ignores case, another spelling does the same).
        const std::filesystem::path alias = editor.project_directory() / "assets" / "scenes" / "alias.scene.json";
        std::error_code linkError = {};
        std::filesystem::create_hard_link(test_scene_file(editor), alias, linkError);
        if (linkError) {
            MESSAGE("the volume refused a hard link (" << linkError.message() << "): the alias half is skipped");
            return;
        }
        (void) editor.ok("scene.open", core::json::object({{"path", "scenes/alias"}}));
        REQUIRE(editor.context().scene_path() == "assets://scenes/alias.scene.json");

        asset_browser_panel panel = {};
        panel.request_delete(editor.context(), uuid, TEST_SCENE);
        // The question is raised exactly where the command would refuse: nothing was deleted behind the dialog.
        CHECK(panel.has_pending_delete());
        CHECK(std::filesystem::is_regular_file(test_scene_file(editor)));
        CHECK(editor.context().has_scene());
        CHECK(editor.fail("asset.delete", core::json::object({{"asset", uuid}})) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("regression - code sweep F656 asset_browser_panel asks before deleting the open scene") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        (void) editor.ok("scene.save");
        const std::string uuid = uuid_of(editor, TEST_SCENE);
        asset_browser_panel panel = {};
        imgui_frame_scope scope{};

        // The Delete action only raises the question: nothing is closed and nothing is deleted.
        panel.request_delete(editor.context(), uuid, TEST_SCENE);
        REQUIRE(panel.has_pending_delete());
        CHECK(panel.pending_delete_path() == TEST_SCENE);
        CHECK(std::filesystem::is_regular_file(test_scene_file(editor)));
        CHECK(editor.context().has_scene());
        // Drawing shows the dialog and waits; it answers nothing on its own.
        draw_frame(scope, panel, editor);
        draw_frame(scope, panel, editor);
        CHECK(panel.has_pending_delete());
        CHECK(std::filesystem::is_regular_file(test_scene_file(editor)));

        // Cancel keeps the scene and its file.
        panel.cancel_pending_delete();
        CHECK_FALSE(panel.has_pending_delete());
        CHECK(panel.pending_delete_path().empty());
        draw_frame(scope, panel, editor);
        CHECK(std::filesystem::is_regular_file(test_scene_file(editor)));
        CHECK(editor.context().scene_path() == TEST_SCENE);
        (void) editor.ok("entity.get", core::json::object({{"entity", "Kept"}}));

        // "Close and Delete" deletes the file and closes the scene; undo brings the file back.
        (void) editor.create_entity("Unsaved");
        panel.request_delete(editor.context(), uuid, TEST_SCENE);
        REQUIRE(panel.has_pending_delete());
        draw_frame(scope, panel, editor);
        panel.confirm_pending_delete(editor.context());
        CHECK_FALSE(panel.has_pending_delete());
        CHECK_FALSE(std::filesystem::exists(test_scene_file(editor)));
        CHECK_FALSE(editor.context().has_scene());
        draw_frame(scope, panel, editor);
        (void) editor.ok("history.undo");
        CHECK(std::filesystem::is_regular_file(test_scene_file(editor)));
        CHECK(uuid_of(editor, TEST_SCENE) == uuid);
    }

    TEST_CASE("regression - code sweep F656 asset_browser_panel keeps the question up while playing instead of "
      "sending a delete the command refuses") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Kept");
        (void) editor.ok("scene.save");
        const std::string uuid = uuid_of(editor, TEST_SCENE);
        asset_browser_panel panel = {};
        imgui_frame_scope scope{};
        (void) editor.ok("play.start");
        panel.request_delete(editor.context(), uuid, TEST_SCENE);
        REQUIRE(panel.has_pending_delete());
        draw_frame(scope, panel, editor);
        // asset.delete {closeOpenScene} answers WRONG_PLAY_STATE while playing: the confirmation sends nothing,
        // and the question is still there once play has stopped.
        panel.confirm_pending_delete(editor.context());
        draw_frame(scope, panel, editor);
        CHECK(panel.has_pending_delete());
        CHECK(std::filesystem::is_regular_file(test_scene_file(editor)));
        CHECK(editor.context().has_scene());
        CHECK_FALSE(editor.log().contains("asset.delete failed"));
        (void) editor.ok("play.pause");
        panel.confirm_pending_delete(editor.context());
        CHECK(panel.has_pending_delete());
        CHECK_FALSE(editor.log().contains("asset.delete failed"));

        (void) editor.ok("play.stop");
        draw_frame(scope, panel, editor);
        REQUIRE(panel.has_pending_delete());
        panel.confirm_pending_delete(editor.context());
        CHECK_FALSE(panel.has_pending_delete());
        CHECK_FALSE(std::filesystem::exists(test_scene_file(editor)));
        CHECK_FALSE(editor.context().has_scene());
    }

    TEST_CASE("asset_browser_panel - a pending delete is dropped when its scene is no longer the open one") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("scene.save");
        const std::string uuid = uuid_of(editor, TEST_SCENE);
        asset_browser_panel panel = {};
        imgui_frame_scope scope{};
        panel.request_delete(editor.context(), uuid, TEST_SCENE);
        REQUIRE(panel.has_pending_delete());
        draw_frame(scope, panel, editor);
        // The scene is closed from elsewhere while the dialog is up: the question no longer applies, and the
        // old confirmation must not turn into a delete of a file nobody was asked about.
        (void) editor.ok("scene.close");
        draw_frame(scope, panel, editor);
        CHECK_FALSE(panel.has_pending_delete());
        CHECK(std::filesystem::is_regular_file(test_scene_file(editor)));
        // Confirming with nothing pending does nothing.
        panel.confirm_pending_delete(editor.context());
        CHECK(std::filesystem::is_regular_file(test_scene_file(editor)));
        CHECK(editor.context().undo().size() == 0);
    }
}
