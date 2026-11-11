#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/profiler_panel.h"

// profiler_panel.h: frame timings from engine::frame_stats.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/panels/profiler_panel") {

    TEST_CASE("profiler_panel - constructs closed by default") {
        profiler_panel panel = {};
        CHECK(panel.title() == "Profiler");
        CHECK_FALSE(panel.is_open());
        CHECK(panel.samples_plotted() == 0);
    }

    TEST_CASE("profiler_panel - plots the frames the engine ran") {
        editor_test::headless_editor editor = {};
        editor.pump(5);
        profiler_panel panel = {};
        panel.set_open(true);
        imgui_frame_scope scope{};
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.samples_plotted() >= 5);
        CHECK(panel.samples_plotted() == editor.context().engine().stats().sample_count());
    }
}
