#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/platform/input.h"

#include <string>

// input.* (docs/design/04_editor.md 4.4, 7): synthetic input over the null platform backend.

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/commands_input") {

    TEST_CASE("regression - input.inject text decodes UTF-8 into one text event per code point") {
        editor_test::headless_editor editor = {};
        // "héllo" (5 code points, 6 bytes) and a CJK pair (2 code points, 6 bytes).
        const core::json::document latin = editor.ok("input.inject", core::json::object({{"text", "h\xC3\xA9llo"}}));
        CHECK(latin.root()["injected"].as_i64() == 5);
        const core::json::document cjk = editor.ok(
          "input.inject",
          core::json::object({{"text", "\xE6\x97\xA5\xE6\x9C\xAC"}})
        );
        CHECK(cjk.root()["injected"].as_i64() == 2);
        // ASCII is unchanged, and malformed UTF-8 is refused rather than injected byte by byte.
        CHECK(editor.ok("input.inject", core::json::object({{"text", "abc"}})).root()["injected"].as_i64() == 3);
        CHECK(editor.fail(
          "input.inject",
          core::json::object({{"text", std::string("a\xFF\xFEz")}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "input.inject",
          core::json::object({{"text", std::string("truncated \xE6\x97")}})
        ) == protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("input.inject - a tap presses now and releases after holdFrames through a frame task") {
        editor_test::headless_editor editor = {};
        const core::json::document tapped = editor.ok(
          "input.inject",
          core::json::object({{"key", "Space"}, {"holdFrames", 2}})
        );
        CHECK(tapped.root()["releasePending"].as_bool());
        CHECK(editor.context().frame_task_count() == 1);
        editor.pump(3);
        CHECK(editor.context().frame_task_count() == 0);
        CHECK(editor.fail("input.inject") == protocol_error_code::INVALID_ARGS);
        (void) editor.ok("input.clear");
    }
}
