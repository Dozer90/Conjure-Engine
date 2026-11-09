#pragma once

#include "conjure/testing/doctest.h"

#include "conjure/platform/platform.h"

#include <memory>
#include <string>
#include <utility>

// Helpers shared by the platform tests: a headless window description, event builders and the
// doctest stringification of the platform enumerations.

namespace conjure::platform::test_support {

    /// <summary>
    /// The description headless_defaults() hands the engine (06_engine_loop.md 7, R33): null backend,
    /// 1280 x 720.
    /// </summary>
    [[nodiscard]] inline window_desc null_desc(u32 width = 1280, u32 height = 720) {
        window_desc desc = {};
        desc.backend = backend_kind::NULL_BACKEND;
        desc.title = "Conjure Test";
        desc.width = width;
        desc.height = height;
        return desc;
    }

    /// <summary>
    /// Creates the null window or fails the case; the returned pointer is never null.
    /// </summary>
    [[nodiscard]] inline std::unique_ptr<window> create_null_window(const window_desc &desc = null_desc()) {
        core::result<std::unique_ptr<window>> created = create_window(desc);
        REQUIRE_MESSAGE(created.has_value(), ::conjure::testing::describe_error(created));
        REQUIRE(created.value() != nullptr);
        return std::move(created).value();
    }

    [[nodiscard]] inline key_event make_key(
      key code,
      button_action action,
      key_modifier_flags modifiers = key_modifier_flags::NONE
    ) {
        key_event value = {};
        value.keyCode = code;
        value.scancode = static_cast<i32>(code);
        value.action = action;
        value.modifiers = modifiers;
        return value;
    }

    [[nodiscard]] inline mouse_button_event make_mouse_button(
      mouse_button button,
      button_action action,
      key_modifier_flags modifiers = key_modifier_flags::NONE
    ) {
        mouse_button_event value = {};
        value.button = button;
        value.action = action;
        value.modifiers = modifiers;
        return value;
    }

    [[nodiscard]] inline mouse_move_event make_mouse_move(f32 x, f32 y) {
        mouse_move_event value = {};
        value.position = math::vec2(x, y);
        return value;
    }

    [[nodiscard]] inline mouse_scroll_event make_scroll(f32 x, f32 y) {
        mouse_scroll_event value = {};
        value.offset = math::vec2(x, y);
        return value;
    }

    [[nodiscard]] inline text_event make_text(u32 codepoint) {
        text_event value = {};
        value.codepoint = codepoint;
        return value;
    }

    [[nodiscard]] inline gamepad_connection_event make_gamepad_connection(u8 index, bool connected) {
        gamepad_connection_event value = {};
        value.gamepadIndex = index;
        value.connected = connected;
        return value;
    }

    [[nodiscard]] inline gamepad_button_event make_gamepad_button(
      u8 index,
      gamepad_button button,
      button_action action
    ) {
        gamepad_button_event value = {};
        value.gamepadIndex = index;
        value.button = button;
        value.action = action;
        return value;
    }

    [[nodiscard]] inline gamepad_axis_event make_gamepad_axis(u8 index, gamepad_axis axis, f32 axisValue) {
        gamepad_axis_event value = {};
        value.gamepadIndex = index;
        value.axis = axis;
        value.value = axisValue;
        return value;
    }

    [[nodiscard]] inline focus_event make_focus(bool focused) {
        focus_event value = {};
        value.focused = focused;
        return value;
    }
}

namespace doctest {

    #define CONJURE_PLATFORM_TEST_ENUM_STRING(type)                                                      \
        inline String toString(conjure::platform::type value) {                                        \
            const std::string text = std::string(conjure::platform::to_string(value)) + " (" +           \
              std::to_string(static_cast<unsigned>(value)) + ")";                                        \
            return String(text.c_str());                                                                 \
        }

    CONJURE_PLATFORM_TEST_ENUM_STRING(backend_kind)
    CONJURE_PLATFORM_TEST_ENUM_STRING(button_action)
    CONJURE_PLATFORM_TEST_ENUM_STRING(key)
    CONJURE_PLATFORM_TEST_ENUM_STRING(mouse_button)
    CONJURE_PLATFORM_TEST_ENUM_STRING(gamepad_button)
    CONJURE_PLATFORM_TEST_ENUM_STRING(gamepad_axis)
    CONJURE_PLATFORM_TEST_ENUM_STRING(cursor_mode)

    #undef CONJURE_PLATFORM_TEST_ENUM_STRING

    inline String toString(conjure::platform::platform_error value) {
        const std::string text = std::string(conjure::platform::platform_error_name(value));
        return String(text.c_str());
    }
}
