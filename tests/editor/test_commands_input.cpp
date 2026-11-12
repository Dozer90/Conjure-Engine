#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/platform/input.h"
#include "conjure/platform/input_state.h"
#include "conjure/platform/key.h"
#include "conjure/platform/mouse_button.h"

#include <span>
#include <string>
#include <vector>

// input.* (docs/design/04_editor.md 4.4, 7): synthetic input over the null platform backend. The null window
// publishes the input state at the start of every frame, before the editor's frame tasks tick, so a case pumps
// one frame after an inject to read its effect through platform::input::snapshot().

using namespace conjure;
using namespace conjure::editor;

namespace {
    const platform::input_state &published(editor_test::headless_editor &editor) {
        platform::input *input = editor.context().input();
        REQUIRE(input != nullptr);
        return input->snapshot();
    }

    std::vector<u32> published_text(editor_test::headless_editor &editor) {
        const std::span<const u32> text = published(editor).text();
        return std::vector<u32>(text.begin(), text.end());
    }
}

TEST_SUITE("editor/commands_input") {

    TEST_CASE("regression - input.inject text decodes UTF-8 into one text event per code point") {
        editor_test::headless_editor editor = {};
        // "héllo" (5 code points, 6 bytes) and a CJK pair (2 code points, 6 bytes). Each publish clears the
        // pending text, so a frame is pumped after every inject.
        const core::json::document latin = editor.ok("input.inject", core::json::object({{"text", "h\xC3\xA9llo"}}));
        CHECK(latin.root()["injected"].as_i64() == 5);
        editor.pump(1);
        CHECK(published_text(editor) == std::vector<u32>{0x68, 0xE9, 0x6C, 0x6C, 0x6F});
        const core::json::document cjk = editor.ok(
          "input.inject",
          core::json::object({{"text", "\xE6\x97\xA5\xE6\x9C\xAC"}})
        );
        CHECK(cjk.root()["injected"].as_i64() == 2);
        editor.pump(1);
        CHECK(published_text(editor) == std::vector<u32>{0x65E5, 0x672C});
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
        editor.pump(1);
        CHECK(published(editor).is_key_down(platform::key::SPACE));
        editor.pump(3);
        CHECK(editor.context().frame_task_count() == 0);
        // pump(3) ran the release tick and the frame that publishes it.
        CHECK_FALSE(published(editor).is_key_down(platform::key::SPACE));
        CHECK(editor.fail("input.inject") == protocol_error_code::INVALID_ARGS);
        (void) editor.ok("input.clear");
    }

    TEST_CASE("regression - a tap of a key and a mouse button releases both after holdFrames") {
        editor_test::headless_editor editor = {};
        const core::json::document tapped = editor.ok(
          "input.inject",
          core::json::object({{"key", "Space"}, {"button", "Left"}, {"holdFrames", 1}})
        );
        CHECK(tapped.root()["injected"].as_i64() == 2);
        CHECK(tapped.root()["releasePending"].as_bool());
        CHECK(editor.context().frame_task_count() == 1);
        editor.pump(1);
        CHECK(published(editor).is_key_down(platform::key::SPACE));
        CHECK(published(editor).is_mouse_button_down(platform::mouse_button::LEFT));
        editor.pump(2);
        CHECK(editor.context().frame_task_count() == 0);
        CHECK_FALSE(published(editor).is_key_down(platform::key::SPACE));
        CHECK_FALSE(published(editor).is_mouse_button_down(platform::mouse_button::LEFT));
    }

    TEST_CASE("regression - input.inject with an invalid later argument injects nothing") {
        editor_test::headless_editor editor = {};
        CHECK(editor.fail(
          "input.inject",
          core::json::object({{"key", "Space"}, {"button", "NoSuchButton"}, {"action", "press"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "input.inject",
          core::json::object({{"key", "Space"}, {"text", std::string("ok\xFF")}, {"action", "press"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "input.inject",
          core::json::object({{"text", "typed"}, {"key", "Space"}, {"axis", "NoSuchAxis"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.context().frame_task_count() == 0);
        editor.pump(1);
        CHECK_FALSE(published(editor).is_key_down(platform::key::SPACE));
        CHECK(published_text(editor).empty());
    }
}
