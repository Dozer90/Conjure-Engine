#include "conjure/testing/doctest.h"

#include "platform_test_support.h"

#include "conjure/platform/event.h"

#include <array>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

using namespace conjure;
using namespace conjure::platform;
using namespace conjure::platform::test_support;

namespace {
    [[nodiscard]] std::vector<event> every_event(std::span<const std::string> dropPaths) {
        std::vector<event> events = {};
        events.emplace_back(make_key(key::SPACE, button_action::PRESS, key_modifier_flags::SHIFT));
        events.emplace_back(make_text(0x1F600));
        events.emplace_back(make_mouse_move(10.5f, 20.0f));
        events.emplace_back(
          make_mouse_button(mouse_button::RIGHT, button_action::RELEASE, key_modifier_flags::CONTROL)
        );
        events.emplace_back(make_scroll(0.0f, -1.0f));
        cursor_enter_event enter = {};
        enter.entered = true;
        events.emplace_back(enter);
        events.emplace_back(make_gamepad_connection(2, true));
        events.emplace_back(make_gamepad_button(1, gamepad_button::DPAD_LEFT, button_action::PRESS));
        events.emplace_back(make_gamepad_axis(3, gamepad_axis::RIGHT_TRIGGER, 0.75f));
        framebuffer_resized_event resized = {};
        resized.width = 1920;
        resized.height = 1080;
        events.emplace_back(resized);
        content_scale_changed_event scaled = {};
        scaled.scale = math::vec2(1.5f, 1.5f);
        events.emplace_back(scaled);
        events.emplace_back(make_focus(false));
        minimise_event minimised = {};
        minimised.minimised = true;
        events.emplace_back(minimised);
        events.emplace_back(close_event());
        drop_files_event dropped = {};
        dropped.paths = dropPaths;
        events.emplace_back(dropped);
        return events;
    }
}

TEST_SUITE("platform/event") {

    TEST_CASE("event - every alternative and both variants are trivially copyable") {
        static_assert(std::is_trivially_copyable_v<key_event>);
        static_assert(std::is_trivially_copyable_v<text_event>);
        static_assert(std::is_trivially_copyable_v<mouse_move_event>);
        static_assert(std::is_trivially_copyable_v<mouse_button_event>);
        static_assert(std::is_trivially_copyable_v<mouse_scroll_event>);
        static_assert(std::is_trivially_copyable_v<cursor_enter_event>);
        static_assert(std::is_trivially_copyable_v<gamepad_connection_event>);
        static_assert(std::is_trivially_copyable_v<gamepad_button_event>);
        static_assert(std::is_trivially_copyable_v<gamepad_axis_event>);
        static_assert(std::is_trivially_copyable_v<framebuffer_resized_event>);
        static_assert(std::is_trivially_copyable_v<content_scale_changed_event>);
        static_assert(std::is_trivially_copyable_v<focus_event>);
        static_assert(std::is_trivially_copyable_v<minimise_event>);
        static_assert(std::is_trivially_copyable_v<close_event>);
        static_assert(std::is_trivially_copyable_v<drop_files_event>);
        static_assert(std::is_trivially_copyable_v<input_event>);
        static_assert(std::is_trivially_copyable_v<event>);
        CHECK(INPUT_EVENT_COUNT == 9);
        CHECK(EVENT_COUNT == 15);
    }

    TEST_CASE("event - defaults are the documented zero values") {
        const key_event keyValue = {};
        CHECK(keyValue.keyCode == key::UNKNOWN);
        CHECK(keyValue.scancode == 0);
        CHECK(keyValue.action == button_action::RELEASE);
        CHECK(keyValue.modifiers == key_modifier_flags::NONE);
        const mouse_button_event mouseValue = {};
        CHECK(mouseValue.button == mouse_button::LEFT);
        CHECK(mouseValue.action == button_action::RELEASE);
        const gamepad_axis_event axisValue = {};
        CHECK(axisValue.gamepadIndex == 0);
        CHECK(axisValue.axis == gamepad_axis::LEFT_X);
        CHECK(axisValue.value == 0.0f);
        const content_scale_changed_event scaleValue = {};
        CHECK(scaleValue.scale == math::vec2(1.0f));
        const drop_files_event dropValue = {};
        CHECK(dropValue.paths.empty());
        const event defaulted = {};
        CHECK(defaulted.index() == 0);
        CHECK(event_as<key_event>(defaulted) != nullptr);
    }

    TEST_CASE("event - the first INPUT_EVENT_COUNT alternatives are the input events") {
        const std::vector<std::string> paths = {"a.png"};
        const std::vector<event> events = every_event(paths);
        REQUIRE(events.size() == EVENT_COUNT);
        for (usize index = 0; index < events.size(); ++index) {
            CAPTURE(index);
            CHECK(events[index].index() == index);
            CHECK(is_input_event(events[index]) == (index < INPUT_EVENT_COUNT));
        }
    }

    TEST_CASE("event - to_input_event round trips every input alternative and rejects window events") {
        const std::vector<std::string> paths = {"a.png"};
        const std::vector<event> events = every_event(paths);
        for (usize index = 0; index < events.size(); ++index) {
            CAPTURE(index);
            const std::optional<input_event> narrowed = to_input_event(events[index]);
            if (index >= INPUT_EVENT_COUNT) {
                CHECK_FALSE(narrowed.has_value());
                continue;
            }
            REQUIRE(narrowed.has_value());
            CHECK(narrowed->index() == index);
            const event widened = to_event(*narrowed);
            CHECK(widened.index() == index);
            CHECK(to_string(widened) == to_string(events[index]));
        }
    }

    TEST_CASE("event - to_input_event preserves the payload") {
        const event source = make_key(key::F5, button_action::REPEAT, key_modifier_flags::ALT);
        const std::optional<input_event> narrowed = to_input_event(source);
        REQUIRE(narrowed.has_value());
        const key_event *payload = std::get_if<key_event>(&*narrowed);
        REQUIRE(payload != nullptr);
        CHECK(payload->keyCode == key::F5);
        CHECK(payload->action == button_action::REPEAT);
        CHECK(payload->modifiers == key_modifier_flags::ALT);
        CHECK(payload->scancode == static_cast<i32>(key::F5));
    }

    TEST_CASE("event - event_as returns the payload for the held alternative and nullptr otherwise") {
        const event value = make_mouse_move(3.0f, 4.0f);
        const mouse_move_event *move = event_as<mouse_move_event>(value);
        REQUIRE(move != nullptr);
        CHECK(move->position == math::vec2(3.0f, 4.0f));
        CHECK(event_as<key_event>(value) == nullptr);
        CHECK(event_as<close_event>(value) == nullptr);

        const event closing = close_event();
        CHECK(event_as<close_event>(closing) != nullptr);
        CHECK(event_as<mouse_move_event>(closing) == nullptr);
    }

    TEST_CASE("event - event_name and to_string cover every alternative") {
        const std::vector<std::string> paths = {"C:/assets/a.png", "b.gltf"};
        const std::vector<event> events = every_event(paths);
        const std::array<std::string_view, 15> expectedNames = {
          "key_event",
          "text_event",
          "mouse_move_event",
          "mouse_button_event",
          "mouse_scroll_event",
          "cursor_enter_event",
          "gamepad_connection_event",
          "gamepad_button_event",
          "gamepad_axis_event",
          "framebuffer_resized_event",
          "content_scale_changed_event",
          "focus_event",
          "minimise_event",
          "close_event",
          "drop_files_event",
        };
        for (usize index = 0; index < events.size(); ++index) {
            CAPTURE(index);
            CHECK(event_name(events[index]) == expectedNames[index]);
            const std::string text = to_string(events[index]);
            CHECK(text.find(expectedNames[index]) == 0);
            CHECK(text.find('{') != std::string::npos);
            CHECK(text.back() == '}');
        }

        CHECK(to_string(events[0]) == "key_event{key=SPACE, scancode=32, action=PRESS, modifiers=SHIFT}");
        CHECK(to_string(events[1]) == "text_event{codepoint=U+1F600}");
        CHECK(to_string(events[3]) == "mouse_button_event{button=RIGHT, action=RELEASE, modifiers=CONTROL}");
        CHECK(to_string(events[5]) == "cursor_enter_event{entered=true}");
        CHECK(to_string(events[6]) == "gamepad_connection_event{gamepad=2, connected=true}");
        CHECK(to_string(events[7]) == "gamepad_button_event{gamepad=1, button=DPAD_LEFT, action=PRESS}");
        CHECK(to_string(events[8]) == "gamepad_axis_event{gamepad=3, axis=RIGHT_TRIGGER, value=0.75}");
        CHECK(to_string(events[9]) == "framebuffer_resized_event{width=1920, height=1080}");
        CHECK(to_string(events[11]) == "focus_event{focused=false}");
        CHECK(to_string(events[12]) == "minimise_event{minimised=true}");
        CHECK(to_string(events[13]) == "close_event{}");
        CHECK(to_string(events[14]) == "drop_files_event{paths=[\"C:/assets/a.png\", \"b.gltf\"]}");
        CHECK(to_string(events[2]).find("position=(10.5, 20)") != std::string::npos);
        CHECK(to_string(events[4]).find("offset=(0, -1)") != std::string::npos);
        CHECK(to_string(events[10]).find("scale=(1.5, 1.5)") != std::string::npos);
    }

    TEST_CASE("event - to_string prints raw numbers for values outside the enumerations") {
        key_event unknownKey = {};
        unknownKey.keyCode = static_cast<key>(400);
        unknownKey.action = static_cast<button_action>(9);
        CHECK(to_string(event(unknownKey)) == "key_event{key=400, scancode=0, action=9, modifiers=NONE}");

        mouse_button_event unknownButton = {};
        unknownButton.button = static_cast<mouse_button>(12);
        CHECK(to_string(event(unknownButton)) == "mouse_button_event{button=12, action=RELEASE, modifiers=NONE}");

        // Every enumeration goes through the one fallback formatter: a name when known, the number otherwise.
        const gamepad_button_event unknownPadButton =
          make_gamepad_button(0, static_cast<gamepad_button>(99), static_cast<button_action>(7));
        CHECK(to_string(event(unknownPadButton)) == "gamepad_button_event{gamepad=0, button=99, action=7}");

        gamepad_axis_event unknownAxis = {};
        unknownAxis.gamepadIndex = 1;
        unknownAxis.axis = static_cast<gamepad_axis>(42);
        unknownAxis.value = 0.5f;
        CHECK(to_string(event(unknownAxis)) == "gamepad_axis_event{gamepad=1, axis=42, value=0.5}");

        const event emptyDrop = drop_files_event();
        CHECK(to_string(emptyDrop) == "drop_files_event{paths=[]}");
    }
}
