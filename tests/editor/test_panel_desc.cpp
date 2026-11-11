#include "conjure/testing/doctest.h"

#include "conjure/editor/panel_desc.h"

#include <type_traits>

// panel_desc.h: the ABI-stable panel descriptor modules register.

using namespace conjure;
using namespace conjure::editor;

namespace {
    void count_draw(void *imguiContext, void *userData) {
        (void) imguiContext;
        ++*static_cast<int *>(userData);
    }
}

TEST_SUITE("editor/panel_desc") {

    TEST_CASE("panel_desc - is trivially copyable with sane defaults") {
        static_assert(std::is_trivially_copyable_v<panel_desc>);
        CHECK(panel_desc::EXTENSION_POINT == core::string_id("editor.panel"));
        const panel_desc desc = {};
        CHECK(desc.title == nullptr);
        CHECK(desc.draw == nullptr);
        CHECK(desc.userData == nullptr);
        CHECK(desc.openByDefault);
    }

    TEST_CASE("panel_desc - the draw callback receives the user data") {
        int draws = 0;
        panel_desc desc = {};
        desc.title = "Tool";
        desc.draw = &count_draw;
        desc.userData = &draws;
        desc.draw(nullptr, desc.userData);
        desc.draw(nullptr, desc.userData);
        CHECK(draws == 2);
    }
}
