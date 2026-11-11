#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/editor/panel_desc.h"
#include "conjure/editor/ui/editor_panel.h"
#include "conjure/editor/ui/imgui_frame_scope.h"

#include <memory>
#include <string>

// editor_panel.h: the panel base class and the panel registry.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

namespace {
    class counting_panel final : public editor_panel {
        private:
            int *mDraws = nullptr;
            int *mUpdates = nullptr;

        public:
            counting_panel(int *draws, int *updates) :
                editor_panel("Counting"),
                mDraws(draws),
                mUpdates(updates) {}

            ~counting_panel() override = default;

        public:
            void draw_contents(editor_context &) override {
                ++*mDraws;
            }

            void update(editor_context &) override {
                ++*mUpdates;
            }
    };

    void module_draw(void *imguiContext, void *userData) {
        (void) imguiContext;
        ++*static_cast<int *>(userData);
    }
}

TEST_SUITE("editor/ui/editor_panel") {

    TEST_CASE("editor_panel - draws its contents only while open and inside a frame") {
        editor_test::headless_editor editor = {};
        int draws = 0;
        int updates = 0;
        counting_panel panel(&draws, &updates);
        CHECK(panel.title() == "Counting");
        CHECK(panel.is_open());
        panel.draw(editor.context());
        CHECK(draws == 0);
        imgui_frame_scope scope{};
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(draws == 1);
        panel.set_open(false);
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(draws == 1);
        panel.update(editor.context());
        CHECK(updates == 1);
    }

    TEST_CASE("panel_registry - adds, replaces, finds and removes descriptors by owner") {
        panel_registry registry = {};
        panel_descriptor descriptor = {};
        descriptor.name = "a";
        descriptor.title = "A";
        descriptor.create = []() -> std::unique_ptr<editor_panel> {
            return nullptr;
        };
        registry.add(descriptor);
        descriptor.name = "b";
        descriptor.owner = "plugin";
        registry.add(descriptor);
        CHECK(registry.size() == 2);
        REQUIRE(registry.find("a") != nullptr);
        CHECK(registry.find("a")->title == "A");
        descriptor.name = "a";
        descriptor.owner = "editor";
        descriptor.title = "Renamed";
        registry.add(descriptor);
        CHECK(registry.size() == 2);
        CHECK(registry.find("a")->title == "Renamed");
        panel_descriptor invalid = {};
        registry.add(invalid);
        CHECK(registry.size() == 2);
        CHECK(registry.remove_owned_by("plugin") == 1);
        CHECK(registry.find("b") == nullptr);
        CHECK(registry.remove_owned_by("plugin") == 0);
        CHECK(registry.descriptors().size() == 1);
    }

    TEST_CASE("panel_registry - a module panel wraps the C draw callback") {
        editor_test::headless_editor editor = {};
        panel_registry registry = {};
        int draws = 0;
        panel_desc desc = {};
        desc.title = "Tool";
        desc.draw = &module_draw;
        desc.userData = &draws;
        desc.openByDefault = false;
        registry.add_module_panel("plugin", desc);
        panel_desc broken = {};
        registry.add_module_panel("plugin", broken);
        REQUIRE(registry.size() == 1);
        const panel_descriptor *found = registry.find("plugin.Tool");
        REQUIRE(found != nullptr);
        CHECK(found->owner == "plugin");
        CHECK_FALSE(found->openByDefault);
        std::unique_ptr<editor_panel> panel = found->create();
        REQUIRE(panel != nullptr);
        CHECK(panel->title() == "Tool");
        imgui_frame_scope scope{};
        scope.begin_frame();
        panel->set_open(true);
        panel->draw(editor.context());
        scope.end_frame();
        CHECK(draws == 1);
    }
}
