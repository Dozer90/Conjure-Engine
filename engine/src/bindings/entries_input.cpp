#include "bindings/entry_support.h"

#include "conjure/platform/button_action.h"
#include "conjure/platform/event.h"
#include "conjure/platform/gamepad.h"
#include "conjure/platform/input.h"
#include "conjure/platform/key.h"
#include "conjure/platform/mouse_button.h"
#include "conjure/platform/window.h"

#include <cmath>
#include <string>

// input_* (docs/design/15_bindings.md 2.3, R56, R57): what the frame block does not carry. Injection goes
// through platform::input::inject (visible in the next frame's snapshot), cursor mode, clipboard and gamepad
// names through the window; the null window answers all of them in memory, so nothing here is a stub.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::bindings::detail;

namespace {

    [[nodiscard]] platform::input *input() noexcept {
        return active_context().input;
    }

    [[nodiscard]] platform::window *window() noexcept {
        return active_context().window;
    }

    [[nodiscard]] bool resolve_action(
      native_function_id id,
      uint8_t action,
      platform::button_action &out,
      int32_t &status
    ) noexcept {
        if (action > static_cast<uint8_t>(platform::button_action::REPEAT)) {
            status = fail_entry(id, bindings_error::INVALID_ARGUMENT, "action");
            return false;
        }
        out = static_cast<platform::button_action>(action);
        return true;
    }

    [[nodiscard]] bool resolve_gamepad(native_function_id id, int32_t index, u8 &out, int32_t &status) noexcept {
        if (index < 0 || static_cast<u32>(index) >= platform::MAX_GAMEPADS) {
            status = fail_entry(id, bindings_error::INVALID_ARGUMENT, "gamepad index");
            return false;
        }
        out = static_cast<u8>(index);
        return true;
    }
}

int32_t conjure::bindings::entries::input_set_cursor_mode(uint8_t mode) {
    constexpr native_function_id ID = native_function_id::input_set_cursor_mode;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (mode > static_cast<uint8_t>(platform::cursor_mode::CAPTURED)) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "mode");
    }
    if (window() == nullptr) {
        return unavailable(ID, "window");
    }
    const core::result<void> changed = window()->set_cursor_mode(static_cast<platform::cursor_mode>(mode));
    if (!changed.has_value()) {
        return fail_entry(ID, changed.get_error());
    }
    if (input() != nullptr) {
        // The OS repositions the cursor on a mode change; the first report must not read as a jump.
        input()->skip_next_mouse_delta();
    }
    return 0;
}

int32_t conjure::bindings::entries::input_get_cursor_mode(void) {
    constexpr native_function_id ID = native_function_id::input_get_cursor_mode;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (window() == nullptr) {
        return unavailable(ID, "window");
    }
    return static_cast<int32_t>(window()->current_cursor_mode());
}

int32_t conjure::bindings::entries::input_gamepad_name(int32_t index, char *utf8, int32_t capacity) {
    constexpr native_function_id ID = native_function_id::input_gamepad_name;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (capacity < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity");
    }
    int32_t status = 0;
    u8 slot = 0;
    if (!resolve_gamepad(ID, index, slot, status)) {
        return status;
    }
    if (window() == nullptr) {
        return unavailable(ID, "window");
    }
    return marshal::write_utf8(window()->gamepad_name(slot), utf8, capacity);
}

int32_t conjure::bindings::entries::input_inject_key(uint16_t key, uint8_t action) {
    constexpr native_function_id ID = native_function_id::input_inject_key;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (!platform::is_valid_key(key)) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "key");
    }
    int32_t status = 0;
    platform::button_action buttonAction = platform::button_action::RELEASE;
    if (!resolve_action(ID, action, buttonAction, status)) {
        return status;
    }
    if (input() == nullptr) {
        return unavailable(ID, "input");
    }
    platform::key_event event = {};
    event.keyCode = static_cast<platform::key>(key);
    event.action = buttonAction;
    input()->inject(event);
    return 0;
}

int32_t conjure::bindings::entries::input_inject_mouse_button(uint8_t button, uint8_t action) {
    constexpr native_function_id ID = native_function_id::input_inject_mouse_button;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (!platform::is_valid_mouse_button(static_cast<platform::mouse_button>(button))) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "button");
    }
    int32_t status = 0;
    platform::button_action buttonAction = platform::button_action::RELEASE;
    if (!resolve_action(ID, action, buttonAction, status)) {
        return status;
    }
    if (input() == nullptr) {
        return unavailable(ID, "input");
    }
    platform::mouse_button_event event = {};
    event.button = static_cast<platform::mouse_button>(button);
    event.action = buttonAction;
    input()->inject(event);
    return 0;
}

int32_t conjure::bindings::entries::input_inject_mouse_move(float x, float y) {
    constexpr native_function_id ID = native_function_id::input_inject_mouse_move;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "position must be finite");
    }
    if (input() == nullptr) {
        return unavailable(ID, "input");
    }
    platform::mouse_move_event event = {};
    event.position = math::vec2(x, y);
    input()->inject(event);
    return 0;
}

int32_t conjure::bindings::entries::input_inject_gamepad_button(int32_t index, uint8_t button, uint8_t action) {
    constexpr native_function_id ID = native_function_id::input_inject_gamepad_button;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    int32_t status = 0;
    u8 slot = 0;
    if (!resolve_gamepad(ID, index, slot, status)) {
        return status;
    }
    if (!platform::is_valid_gamepad_button(static_cast<platform::gamepad_button>(button))) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "button");
    }
    platform::button_action buttonAction = platform::button_action::RELEASE;
    if (!resolve_action(ID, action, buttonAction, status)) {
        return status;
    }
    if (input() == nullptr) {
        return unavailable(ID, "input");
    }
    platform::gamepad_button_event event = {};
    event.gamepadIndex = slot;
    event.button = static_cast<platform::gamepad_button>(button);
    event.action = buttonAction;
    input()->inject(event);
    return 0;
}

int32_t conjure::bindings::entries::input_inject_gamepad_axis(int32_t index, uint8_t axis, float value) {
    constexpr native_function_id ID = native_function_id::input_inject_gamepad_axis;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    int32_t status = 0;
    u8 slot = 0;
    if (!resolve_gamepad(ID, index, slot, status)) {
        return status;
    }
    if (!platform::is_valid_gamepad_axis(static_cast<platform::gamepad_axis>(axis))) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "axis");
    }
    if (!std::isfinite(value)) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "value must be finite");
    }
    if (input() == nullptr) {
        return unavailable(ID, "input");
    }
    platform::gamepad_axis_event event = {};
    event.gamepadIndex = slot;
    event.axis = static_cast<platform::gamepad_axis>(axis);
    event.value = value;
    input()->inject(event);
    return 0;
}

int32_t conjure::bindings::entries::input_clipboard_get(char *utf8, int32_t capacity) {
    constexpr native_function_id ID = native_function_id::input_clipboard_get;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (capacity < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity");
    }
    if (window() == nullptr) {
        return unavailable(ID, "window");
    }
    const core::result<std::string> text = window()->clipboard_text();
    if (!text.has_value()) {
        return fail_entry(ID, text.get_error());
    }
    return marshal::write_utf8(text.value(), utf8, capacity);
}

int32_t conjure::bindings::entries::input_clipboard_set(const char *utf8, int32_t length) {
    constexpr native_function_id ID = native_function_id::input_clipboard_set;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    const core::result<std::string_view> text = marshal::read_utf8(utf8, length);
    if (!text.has_value()) {
        return fail_entry(ID, text.get_error());
    }
    if (window() == nullptr) {
        return unavailable(ID, "window");
    }
    const core::result<void> written = window()->set_clipboard_text(*text);
    if (!written.has_value()) {
        return fail_entry(ID, written.get_error());
    }
    return 0;
}
