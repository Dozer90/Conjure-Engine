#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/log.h"
#include "conjure/editor/console_buffer.h"
#include "conjure/editor/log_category.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/console_panel.h"

#include <string>

// console_panel.h: the console ring with a level filter.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/panels/console_panel") {

    TEST_CASE("console_panel - constructs open at the info level") {
        console_panel panel = {};
        CHECK(panel.title() == "Console");
        CHECK(panel.min_level() == core::log_level::INFO);
        CHECK(panel.lines_drawn() == 0);
        panel.set_min_level(core::log_level::ERR);
        CHECK(panel.min_level() == core::log_level::ERR);
    }

    TEST_CASE("console_panel - draws the records above the minimum level") {
        editor_test::headless_editor editor = {};
        editor.context().console().clear();
        // The headless editor runs at --log-level warn: only warnings and above reach the console ring.
        CONJURE_LOG_WARN(LOG_EDITOR, "console panel first warn line");
        CONJURE_LOG_WARN(LOG_EDITOR, "console panel second warn line");
        console_panel panel = {};
        imgui_frame_scope scope{};
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.lines_drawn() >= 2);
        panel.set_min_level(core::log_level::ERR);
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.lines_drawn() == 0);
    }

    TEST_CASE("regression - F110 the console keeps showing new lines past 5,000 records") {
        editor_test::headless_editor editor = {};
        console_buffer &console = editor.context().console();
        console.clear();
        for (int index = 0; index < 5100; ++index) {
            console.push(core::log_level::WARN, "test", std::to_string(index));
        }
        console_panel panel = {};
        imgui_frame_scope scope{};
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.newest_seq_shown() == console.last_seq());
        console.push(core::log_level::WARN, "test", "one more");
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.newest_seq_shown() == console.last_seq());
    }
}
