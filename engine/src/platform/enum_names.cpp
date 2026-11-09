#include "conjure/platform/backend.h"
#include "conjure/platform/button_action.h"
#include "conjure/platform/gamepad.h"
#include "conjure/platform/key.h"
#include "conjure/platform/mouse_button.h"
#include "conjure/platform/platform_error.h"
#include "conjure/platform/window.h"

#include "conjure/core/string_utils.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// Every to_string / parse_* of the platform enumerations. Each table lists the enumerators once;
// to_string indexes it and the parsers scan it case-insensitively (core::str::iequals), so adding an
// enumerator is one table line. The key table is expanded into a 512-entry name array at compile
// time so to_string(key) is an array lookup.

using namespace conjure;
using namespace conjure::platform;

namespace {
    template <typename EnumType>
    struct named_value {
        EnumType value = {};
        std::string_view name = {};
    };

    template <typename EnumType, usize Count>
    [[nodiscard]] constexpr std::string_view name_of(
      const std::array<named_value<EnumType>, Count> &table,
      EnumType value
    ) noexcept {
        for (const named_value<EnumType> &entry : table) {
            if (entry.value == value) {
                return entry.name;
            }
        }
        return "";
    }

    template <typename EnumType, usize Count>
    [[nodiscard]] constexpr std::optional<EnumType> parse_from(
      const std::array<named_value<EnumType>, Count> &table,
      std::string_view name
    ) noexcept {
        for (const named_value<EnumType> &entry : table) {
            if (core::str::iequals(entry.name, name)) {
                return entry.value;
            }
        }
        return std::nullopt;
    }

    /*-----------------------------------------------------------------------*/
    // Tables

    constexpr std::array<named_value<backend_kind>, 2> BACKEND_NAMES = {{
      {backend_kind::NULL_BACKEND, "null"},
      {backend_kind::GLFW, "glfw"},
    }};

    constexpr std::array<named_value<button_action>, 3> BUTTON_ACTION_NAMES = {{
      {button_action::RELEASE, "RELEASE"},
      {button_action::PRESS, "PRESS"},
      {button_action::REPEAT, "REPEAT"},
    }};

    constexpr std::array<named_value<mouse_button>, 8> MOUSE_BUTTON_NAMES = {{
      {mouse_button::LEFT, "LEFT"},
      {mouse_button::RIGHT, "RIGHT"},
      {mouse_button::MIDDLE, "MIDDLE"},
      {mouse_button::BUTTON_4, "BUTTON_4"},
      {mouse_button::BUTTON_5, "BUTTON_5"},
      {mouse_button::BUTTON_6, "BUTTON_6"},
      {mouse_button::BUTTON_7, "BUTTON_7"},
      {mouse_button::BUTTON_8, "BUTTON_8"},
    }};

    constexpr std::array<named_value<gamepad_button>, 15> GAMEPAD_BUTTON_NAMES = {{
      {gamepad_button::A, "A"},
      {gamepad_button::B, "B"},
      {gamepad_button::X, "X"},
      {gamepad_button::Y, "Y"},
      {gamepad_button::LEFT_BUMPER, "LEFT_BUMPER"},
      {gamepad_button::RIGHT_BUMPER, "RIGHT_BUMPER"},
      {gamepad_button::BACK, "BACK"},
      {gamepad_button::START, "START"},
      {gamepad_button::GUIDE, "GUIDE"},
      {gamepad_button::LEFT_THUMB, "LEFT_THUMB"},
      {gamepad_button::RIGHT_THUMB, "RIGHT_THUMB"},
      {gamepad_button::DPAD_UP, "DPAD_UP"},
      {gamepad_button::DPAD_RIGHT, "DPAD_RIGHT"},
      {gamepad_button::DPAD_DOWN, "DPAD_DOWN"},
      {gamepad_button::DPAD_LEFT, "DPAD_LEFT"},
    }};

    constexpr std::array<named_value<gamepad_axis>, 6> GAMEPAD_AXIS_NAMES = {{
      {gamepad_axis::LEFT_X, "LEFT_X"},
      {gamepad_axis::LEFT_Y, "LEFT_Y"},
      {gamepad_axis::RIGHT_X, "RIGHT_X"},
      {gamepad_axis::RIGHT_Y, "RIGHT_Y"},
      {gamepad_axis::LEFT_TRIGGER, "LEFT_TRIGGER"},
      {gamepad_axis::RIGHT_TRIGGER, "RIGHT_TRIGGER"},
    }};

    constexpr std::array<named_value<cursor_mode>, 4> CURSOR_MODE_NAMES = {{
      {cursor_mode::NORMAL, "NORMAL"},
      {cursor_mode::HIDDEN, "HIDDEN"},
      {cursor_mode::DISABLED, "DISABLED"},
      {cursor_mode::CAPTURED, "CAPTURED"},
    }};

    constexpr std::array<named_value<key_modifier_flags>, 6> KEY_MODIFIER_NAMES = {{
      {key_modifier_flags::SHIFT, "SHIFT"},
      {key_modifier_flags::CONTROL, "CONTROL"},
      {key_modifier_flags::ALT, "ALT"},
      {key_modifier_flags::SUPER, "SUPER"},
      {key_modifier_flags::CAPS_LOCK, "CAPS_LOCK"},
      {key_modifier_flags::NUM_LOCK, "NUM_LOCK"},
    }};

    // Every key enumerator except the alias LAST. UNKNOWN is listed so to_string names it; parse_key
    // rejects it explicitly because an event with key::UNKNOWN never changes state.
    constexpr std::array<named_value<key>, 121> KEY_NAMES = {{
      {key::UNKNOWN, "UNKNOWN"},
      {key::SPACE, "SPACE"},
      {key::APOSTROPHE, "APOSTROPHE"},
      {key::COMMA, "COMMA"},
      {key::MINUS, "MINUS"},
      {key::PERIOD, "PERIOD"},
      {key::SLASH, "SLASH"},
      {key::NUM_0, "NUM_0"},
      {key::NUM_1, "NUM_1"},
      {key::NUM_2, "NUM_2"},
      {key::NUM_3, "NUM_3"},
      {key::NUM_4, "NUM_4"},
      {key::NUM_5, "NUM_5"},
      {key::NUM_6, "NUM_6"},
      {key::NUM_7, "NUM_7"},
      {key::NUM_8, "NUM_8"},
      {key::NUM_9, "NUM_9"},
      {key::SEMICOLON, "SEMICOLON"},
      {key::EQUAL, "EQUAL"},
      {key::A, "A"},
      {key::B, "B"},
      {key::C, "C"},
      {key::D, "D"},
      {key::E, "E"},
      {key::F, "F"},
      {key::G, "G"},
      {key::H, "H"},
      {key::I, "I"},
      {key::J, "J"},
      {key::K, "K"},
      {key::L, "L"},
      {key::M, "M"},
      {key::N, "N"},
      {key::O, "O"},
      {key::P, "P"},
      {key::Q, "Q"},
      {key::R, "R"},
      {key::S, "S"},
      {key::T, "T"},
      {key::U, "U"},
      {key::V, "V"},
      {key::W, "W"},
      {key::X, "X"},
      {key::Y, "Y"},
      {key::Z, "Z"},
      {key::LEFT_BRACKET, "LEFT_BRACKET"},
      {key::BACKSLASH, "BACKSLASH"},
      {key::RIGHT_BRACKET, "RIGHT_BRACKET"},
      {key::GRAVE_ACCENT, "GRAVE_ACCENT"},
      {key::WORLD_1, "WORLD_1"},
      {key::WORLD_2, "WORLD_2"},
      {key::ESCAPE, "ESCAPE"},
      {key::ENTER, "ENTER"},
      {key::TAB, "TAB"},
      {key::BACKSPACE, "BACKSPACE"},
      {key::INSERT, "INSERT"},
      {key::DEL, "DEL"},
      {key::RIGHT, "RIGHT"},
      {key::LEFT, "LEFT"},
      {key::DOWN, "DOWN"},
      {key::UP, "UP"},
      {key::PAGE_UP, "PAGE_UP"},
      {key::PAGE_DOWN, "PAGE_DOWN"},
      {key::HOME, "HOME"},
      {key::END, "END"},
      {key::CAPS_LOCK, "CAPS_LOCK"},
      {key::SCROLL_LOCK, "SCROLL_LOCK"},
      {key::NUM_LOCK, "NUM_LOCK"},
      {key::PRINT_SCREEN, "PRINT_SCREEN"},
      {key::PAUSE, "PAUSE"},
      {key::F1, "F1"},
      {key::F2, "F2"},
      {key::F3, "F3"},
      {key::F4, "F4"},
      {key::F5, "F5"},
      {key::F6, "F6"},
      {key::F7, "F7"},
      {key::F8, "F8"},
      {key::F9, "F9"},
      {key::F10, "F10"},
      {key::F11, "F11"},
      {key::F12, "F12"},
      {key::F13, "F13"},
      {key::F14, "F14"},
      {key::F15, "F15"},
      {key::F16, "F16"},
      {key::F17, "F17"},
      {key::F18, "F18"},
      {key::F19, "F19"},
      {key::F20, "F20"},
      {key::F21, "F21"},
      {key::F22, "F22"},
      {key::F23, "F23"},
      {key::F24, "F24"},
      {key::F25, "F25"},
      {key::KP_0, "KP_0"},
      {key::KP_1, "KP_1"},
      {key::KP_2, "KP_2"},
      {key::KP_3, "KP_3"},
      {key::KP_4, "KP_4"},
      {key::KP_5, "KP_5"},
      {key::KP_6, "KP_6"},
      {key::KP_7, "KP_7"},
      {key::KP_8, "KP_8"},
      {key::KP_9, "KP_9"},
      {key::KP_DECIMAL, "KP_DECIMAL"},
      {key::KP_DIVIDE, "KP_DIVIDE"},
      {key::KP_MULTIPLY, "KP_MULTIPLY"},
      {key::KP_SUBTRACT, "KP_SUBTRACT"},
      {key::KP_ADD, "KP_ADD"},
      {key::KP_ENTER, "KP_ENTER"},
      {key::KP_EQUAL, "KP_EQUAL"},
      {key::LEFT_SHIFT, "LEFT_SHIFT"},
      {key::LEFT_CONTROL, "LEFT_CONTROL"},
      {key::LEFT_ALT, "LEFT_ALT"},
      {key::LEFT_SUPER, "LEFT_SUPER"},
      {key::RIGHT_SHIFT, "RIGHT_SHIFT"},
      {key::RIGHT_CONTROL, "RIGHT_CONTROL"},
      {key::RIGHT_ALT, "RIGHT_ALT"},
      {key::RIGHT_SUPER, "RIGHT_SUPER"},
      {key::MENU, "MENU"},
    }};

    [[nodiscard]] constexpr std::array<std::string_view, KEY_COUNT> build_key_name_table() noexcept {
        std::array<std::string_view, KEY_COUNT> names = {};
        for (const named_value<key> &entry : KEY_NAMES) {
            names[static_cast<usize>(entry.value)] = entry.name;
        }
        return names;
    }

    constexpr std::array<std::string_view, KEY_COUNT> KEY_NAME_TABLE = build_key_name_table();

    static_assert(KEY_NAME_TABLE[32] == "SPACE");
    static_assert(KEY_NAME_TABLE[261] == "DEL");
    static_assert(KEY_NAME_TABLE[348] == "MENU");
    static_assert(KEY_NAME_TABLE[349].empty());
}

/*===========================================================================*/
/*===========================================================================*/
// backend_kind

std::string_view conjure::platform::to_string(backend_kind kind) noexcept {
    return name_of(BACKEND_NAMES, kind);
}

core::result<backend_kind> conjure::platform::parse_backend_kind(std::string_view name) noexcept {
    const std::optional<backend_kind> parsed = parse_from(BACKEND_NAMES, name);
    if (!parsed.has_value()) {
        return core::make_error(platform_error::UNKNOWN_BACKEND, "unknown platform backend '{}'", name);
    }
    return *parsed;
}

/*===========================================================================*/
/*===========================================================================*/
// button_action

std::string_view conjure::platform::to_string(button_action action) noexcept {
    return name_of(BUTTON_ACTION_NAMES, action);
}

std::optional<button_action> conjure::platform::parse_button_action(std::string_view name) noexcept {
    return parse_from(BUTTON_ACTION_NAMES, name);
}

/*===========================================================================*/
/*===========================================================================*/
// key and modifiers

std::string_view conjure::platform::to_string(key code) noexcept {
    const usize index = static_cast<usize>(code);
    if (index >= KEY_COUNT) {
        return "";
    }
    return KEY_NAME_TABLE[index];
}

std::optional<key> conjure::platform::parse_key(std::string_view name) noexcept {
    const std::optional<key> parsed = parse_from(KEY_NAMES, name);
    if (parsed.has_value() && *parsed == key::UNKNOWN) {
        return std::nullopt;
    }
    return parsed;
}

std::string conjure::platform::to_string(key_modifier_flags modifiers) {
    std::string text = {};
    for (const named_value<key_modifier_flags> &entry : KEY_MODIFIER_NAMES) {
        if (!has_flag(modifiers, entry.value)) {
            continue;
        }
        if (!text.empty()) {
            text.push_back('|');
        }
        text.append(entry.name);
    }
    if (text.empty()) {
        text = "NONE";
    }
    return text;
}

std::optional<key_modifier_flags> conjure::platform::parse_key_modifier_flags(std::string_view text) noexcept {
    key_modifier_flags result = key_modifier_flags::NONE;
    std::string_view remaining = core::str::trim(text);
    if (remaining.empty() || core::str::iequals(remaining, "NONE")) {
        return result;
    }

    while (!remaining.empty()) {
        const usize separator = remaining.find('|');
        const std::string_view piece = core::str::trim(remaining.substr(0, separator));
        remaining = separator == std::string_view::npos ? std::string_view() : remaining.substr(separator + 1);

        if (core::str::iequals(piece, "NONE")) {
            continue;
        }
        const std::optional<key_modifier_flags> flag = parse_from(KEY_MODIFIER_NAMES, piece);
        if (!flag.has_value()) {
            return std::nullopt;
        }
        result |= *flag;
    }
    return result;
}

/*===========================================================================*/
/*===========================================================================*/
// mouse_button

std::string_view conjure::platform::to_string(mouse_button button) noexcept {
    return name_of(MOUSE_BUTTON_NAMES, button);
}

std::optional<mouse_button> conjure::platform::parse_mouse_button(std::string_view name) noexcept {
    return parse_from(MOUSE_BUTTON_NAMES, name);
}

/*===========================================================================*/
/*===========================================================================*/
// gamepad

std::string_view conjure::platform::to_string(gamepad_button button) noexcept {
    return name_of(GAMEPAD_BUTTON_NAMES, button);
}

std::string_view conjure::platform::to_string(gamepad_axis axis) noexcept {
    return name_of(GAMEPAD_AXIS_NAMES, axis);
}

std::optional<gamepad_button> conjure::platform::parse_gamepad_button(std::string_view name) noexcept {
    return parse_from(GAMEPAD_BUTTON_NAMES, name);
}

std::optional<gamepad_axis> conjure::platform::parse_gamepad_axis(std::string_view name) noexcept {
    return parse_from(GAMEPAD_AXIS_NAMES, name);
}

/*===========================================================================*/
/*===========================================================================*/
// cursor_mode

std::string_view conjure::platform::to_string(cursor_mode mode) noexcept {
    return name_of(CURSOR_MODE_NAMES, mode);
}

std::optional<cursor_mode> conjure::platform::parse_cursor_mode(std::string_view name) noexcept {
    return parse_from(CURSOR_MODE_NAMES, name);
}
