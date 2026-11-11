#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/core/log.h"
#include "conjure/editor/log_category.h"

#include <string_view>
#include <vector>

// log_category.h: the editor's log category.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::testing;

TEST_SUITE("editor/log_category") {

    TEST_CASE("log_category - LOG_EDITOR is named 'editor' and routes messages") {
        CHECK(std::string_view(LOG_EDITOR.name()) == "editor");
        log_capture logs = {};
        CONJURE_LOG_INFO(LOG_EDITOR, "editor category probe {}", 42);
        CHECK(logs.contains(core::log_level::INFO, "editor category probe 42"));
        const std::vector<captured_log_entry> entries = logs.entries();
        REQUIRE_FALSE(entries.empty());
        CHECK(entries.back().category == "editor");
    }
}
