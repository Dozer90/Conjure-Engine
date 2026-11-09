#pragma once

#include "conjure/core/api.h"
#include "conjure/core/enum_flags.h"
#include "conjure/core/types.h"

#include <optional>
#include <string>
#include <string_view>

// Keyboard keys and modifier flags (docs/design/11_platform.md 2.1, P5). Key codes use GLFW's
// numbering so the glfw backend maps with a range check, and the null backend, the C# Key enum and the
// editor protocol share one stable numbering. key::UNKNOWN (0) replaces GLFW_KEY_UNKNOWN (-1);
// key::DEL is spelled that way because DELETE is a Windows macro (R56).

namespace conjure::platform {

    inline constexpr u32 KEY_COUNT = 512;

    enum class key : u16 {
        UNKNOWN = 0,
        SPACE = 32,
        APOSTROPHE = 39,
        COMMA = 44,
        MINUS,
        PERIOD,
        SLASH,
        NUM_0 = 48,
        NUM_1,
        NUM_2,
        NUM_3,
        NUM_4,
        NUM_5,
        NUM_6,
        NUM_7,
        NUM_8,
        NUM_9,
        SEMICOLON = 59,
        EQUAL = 61,
        A = 65,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,
        LEFT_BRACKET = 91,
        BACKSLASH,
        RIGHT_BRACKET,
        GRAVE_ACCENT = 96,
        WORLD_1 = 161,
        WORLD_2 = 162,
        ESCAPE = 256,
        ENTER,
        TAB,
        BACKSPACE,
        INSERT,
        DEL,
        RIGHT,
        LEFT,
        DOWN,
        UP,
        PAGE_UP,
        PAGE_DOWN,
        HOME,
        END,
        CAPS_LOCK = 280,
        SCROLL_LOCK,
        NUM_LOCK,
        PRINT_SCREEN,
        PAUSE,
        F1 = 290,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        F13,
        F14,
        F15,
        F16,
        F17,
        F18,
        F19,
        F20,
        F21,
        F22,
        F23,
        F24,
        F25,
        KP_0 = 320,
        KP_1,
        KP_2,
        KP_3,
        KP_4,
        KP_5,
        KP_6,
        KP_7,
        KP_8,
        KP_9,
        KP_DECIMAL,
        KP_DIVIDE,
        KP_MULTIPLY,
        KP_SUBTRACT,
        KP_ADD,
        KP_ENTER,
        KP_EQUAL,
        LEFT_SHIFT = 340,
        LEFT_CONTROL,
        LEFT_ALT,
        LEFT_SUPER,
        RIGHT_SHIFT,
        RIGHT_CONTROL,
        RIGHT_ALT,
        RIGHT_SUPER,
        MENU = 348,
        LAST = MENU
    };

    static_assert(static_cast<u32>(key::LAST) < KEY_COUNT);

    enum class key_modifier_flags : u8 {   // GLFW_MOD_* values
        NONE = 0,
        SHIFT = 1 << 0,
        CONTROL = 1 << 1,
        ALT = 1 << 2,
        SUPER = 1 << 3,
        CAPS_LOCK = 1 << 4,
        NUM_LOCK = 1 << 5
    };
    CONJURE_ENUM_FLAGS(key_modifier_flags);

    /// <summary>
    /// True when code names a real key: 0 (UNKNOWN) is not valid, nor is anything above key::LAST.
    /// </summary>
    [[nodiscard]] constexpr bool is_valid_key(u32 code) noexcept {
        return code > 0 && code <= static_cast<u32>(key::LAST);
    }

    /// <summary>
    /// True when the key value names a real key (is_valid_key on its code).
    /// </summary>
    [[nodiscard]] constexpr bool is_valid_key(key code) noexcept {
        return is_valid_key(static_cast<u32>(code));
    }

    /// <summary>
    /// The enumerator name ("SPACE", "DEL", "KP_ENTER"); "UNKNOWN" for key::UNKNOWN and "" for values
    /// that are not enumerators.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(key code) noexcept;

    /// <summary>
    /// Parses an enumerator name case-insensitively ("space", "F12", "Del"); nullopt for anything else,
    /// including "LAST" (an alias, not a key) and "UNKNOWN".
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<key> parse_key(std::string_view name) noexcept;

    /// <summary>
    /// The set flags joined with '|' ("SHIFT|CONTROL"), "NONE" for an empty set. Bits outside the
    /// known flags are ignored.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string to_string(key_modifier_flags modifiers);

    /// <summary>
    /// Parses a '|' separated list of flag names case-insensitively ("shift|alt", "NONE", ""); nullopt
    /// when any name is unknown.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<key_modifier_flags> parse_key_modifier_flags(
      std::string_view text
    ) noexcept;
}
