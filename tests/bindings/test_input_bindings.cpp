#include "conjure/testing/doctest.h"

#include "bindings_test_support.h"

#include "conjure/bindings/bindings_error.h"
#include "conjure/platform/button_action.h"
#include "conjure/platform/event.h"
#include "conjure/platform/gamepad.h"
#include "conjure/platform/input.h"
#include "conjure/platform/input_state.h"
#include "conjure/platform/key.h"
#include "conjure/platform/mouse_button.h"
#include "conjure/platform/null_backend.h"
#include "conjure/platform/window.h"

#include <array>
#include <string>

// input_* (docs/design/15_bindings.md 10, R56, R57): injection is visible in the next snapshot, cursor mode and
// clipboard round-trip on the null window, gamepad names follow the connection state.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

namespace {

    constexpr uint8_t PRESS = static_cast<uint8_t>(platform::button_action::PRESS);
    constexpr uint8_t RELEASE = static_cast<uint8_t>(platform::button_action::RELEASE);
}

TEST_SUITE("bindings/input_bindings") {

    TEST_CASE("input - an injected key is pressed in the next snapshot and held afterwards") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const uint16_t space = static_cast<uint16_t>(platform::key::SPACE);
        CHECK(api.input_inject_key(space, PRESS) == 0);
        CHECK_FALSE(fixture.input.snapshot().is_key_down(platform::key::SPACE));      // not before end_frame
        fixture.input.end_frame();
        CHECK(fixture.input.snapshot().is_key_pressed(platform::key::SPACE));
        CHECK(fixture.input.snapshot().is_key_down(platform::key::SPACE));
        fixture.input.end_frame();
        CHECK_FALSE(fixture.input.snapshot().is_key_pressed(platform::key::SPACE));
        CHECK(fixture.input.snapshot().is_key_held(platform::key::SPACE));
        CHECK(api.input_inject_key(space, RELEASE) == 0);
        fixture.input.end_frame();
        CHECK(fixture.input.snapshot().is_key_released(platform::key::SPACE));
        CHECK_FALSE(fixture.input.snapshot().is_key_down(platform::key::SPACE));
        CHECK(api.input_inject_key(0, PRESS) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.input_inject_key(space, 7) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.input_inject_key(60000, PRESS) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("input - mouse and gamepad injection") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        CHECK(api.input_inject_mouse_move(10.0f, 20.0f) == 0);
        CHECK(api.input_inject_mouse_move(13.0f, 24.0f) == 0);
        CHECK(api.input_inject_mouse_button(static_cast<uint8_t>(platform::mouse_button::RIGHT), PRESS) == 0);
        CHECK(api.input_inject_gamepad_button(2, static_cast<uint8_t>(platform::gamepad_button::A), PRESS) == 0);
        CHECK(api.input_inject_gamepad_axis(2, static_cast<uint8_t>(platform::gamepad_axis::LEFT_X), 0.75f) == 0);
        fixture.input.end_frame();
        const platform::input_state &state = fixture.input.snapshot();
        CHECK(state.mousePosition == math::vec2(13.0f, 24.0f));
        CHECK(state.mouseDelta == math::vec2(3.0f, 4.0f));                   // the first move sets, the second adds
        CHECK(state.is_mouse_button_pressed(platform::mouse_button::RIGHT));
        CHECK(state.gamepads[2].is_pressed(platform::gamepad_button::A));
        CHECK(state.gamepads[2].axis(platform::gamepad_axis::LEFT_X) == 0.75f);
        CHECK(api.input_inject_mouse_button(9, PRESS) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.input_inject_mouse_button(0, 3) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(
          api.input_inject_mouse_move(std::numeric_limits<float>::infinity(), 0.0f)
          == to_status(bindings_error::INVALID_ARGUMENT)
        );
        CHECK(api.input_inject_gamepad_button(4, 0, PRESS) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.input_inject_gamepad_button(-1, 0, PRESS) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.input_inject_gamepad_button(0, 15, PRESS) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.input_inject_gamepad_axis(0, 6, 0.0f) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(
          api.input_inject_gamepad_axis(0, 0, std::numeric_limits<float>::quiet_NaN())
          == to_status(bindings_error::INVALID_ARGUMENT)
        );
    }

    TEST_CASE("input - cursor mode round-trips on the null window") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        CHECK(api.input_get_cursor_mode() == static_cast<int32_t>(platform::cursor_mode::NORMAL));
        CHECK(api.input_set_cursor_mode(static_cast<uint8_t>(conjure_cursor_mode::DISABLED)) == 0);
        CHECK(api.input_get_cursor_mode() == static_cast<int32_t>(platform::cursor_mode::DISABLED));
        CHECK(fixture.window->current_cursor_mode() == platform::cursor_mode::DISABLED);
        CHECK(api.input_set_cursor_mode(static_cast<uint8_t>(conjure_cursor_mode::CAPTURED)) == 0);
        CHECK(api.input_get_cursor_mode() == static_cast<int32_t>(platform::cursor_mode::CAPTURED));
        CHECK(api.input_set_cursor_mode(static_cast<uint8_t>(conjure_cursor_mode::NORMAL)) == 0);
        // A mode change tells the input to swallow the next mouse delta (the OS warps the cursor).
        CHECK(api.input_inject_mouse_move(100.0f, 100.0f) == 0);
        fixture.input.end_frame();
        CHECK(fixture.input.snapshot().mouseDelta == math::vec2(0.0f));
    }

    TEST_CASE("input - a bad cursor mode is INVALID_ARGUMENT") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        CHECK(api.input_set_cursor_mode(4) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.input_set_cursor_mode(255) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.input_get_cursor_mode() == static_cast<int32_t>(platform::cursor_mode::NORMAL));
    }

    TEST_CASE("input - gamepad_name follows the connection protocol") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        std::array<char, 32> buffer = {};
        CHECK(api.input_gamepad_name(1, buffer.data(), 32) == 0);           // not connected: empty
        platform::null_backend *null = fixture.window->as_null();
        REQUIRE(null != nullptr);
        platform::gamepad_connection_event connected = {};
        connected.gamepadIndex = 1;
        connected.connected = true;
        null->push_event(platform::event(connected));
        static_cast<void>(null->pump_events());
        const std::string name = read_string([&](char *text, int32_t capacity) {
            return api.input_gamepad_name(1, text, capacity);
        });
        CHECK(name == "Null Gamepad 1");
        CHECK(api.input_gamepad_name(1, buffer.data(), 4) == static_cast<int32_t>(name.size()));
        CHECK(std::string_view(buffer.data(), 4) == "Null");
        CHECK(api.input_gamepad_name(0, buffer.data(), 32) == 0);
        CHECK(api.input_gamepad_name(4, buffer.data(), 32) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.input_gamepad_name(-1, buffer.data(), 32) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.input_gamepad_name(1, buffer.data(), -1) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("input - clipboard round-trips on the null window") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        std::array<char, 64> buffer = {};
        // Nothing copied yet.
        CHECK(api.input_clipboard_get(buffer.data(), 64) == to_status(bindings_error::NOT_FOUND));
        const utf8_argument text = utf8("copied text");
        CHECK(api.input_clipboard_set(text.data, text.length) == 0);
        const std::string back = read_string([&](char *out, int32_t capacity) {
            return api.input_clipboard_get(out, capacity);
        });
        CHECK(back == "copied text");
        CHECK(api.input_clipboard_get(nullptr, 0) == 11);
        CHECK(api.input_clipboard_set(nullptr, 0) == 0);                   // empty text is allowed
        CHECK(api.input_clipboard_get(nullptr, 0) == 0);
        CHECK(api.input_clipboard_set(nullptr, 3) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.input_clipboard_get(buffer.data(), -1) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("input - without a window and input the group is stubbed") {
        bare_fixture_options options = {};
        options.window = false;
        bare_fixture fixture(options);
        conjure_native_api &api = fixture.api();
        CHECK(api.input_get_cursor_mode() == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(api.input_inject_key(32, PRESS) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(api.input_clipboard_set("x", 1) == to_status(bindings_error::NOT_SUPPORTED));
    }
}
