#pragma once

#include "conjure/platform/gamepad.h"
#include "conjure/platform/key.h"
#include "conjure/platform/mouse_button.h"

#include "conjure/core/assert.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <algorithm>
#include <array>
#include <bit>
#include <span>
#include <type_traits>

// The immutable per-frame input snapshot (docs/design/11_platform.md 2.3, P4, P6). Edges
// (pressed / released) are set for exactly the frame the transition happened in; levels (down,
// positions, axes) persist. The layout mirrors conjure_input_block of the scripting frame block:
// 512-bit key sets as eight u64 words, u32 button masks, four gamepads with six f32 axes.

namespace conjure::platform {

    /// <summary>
    /// A fixed 512-bit set indexed by key code, laid out as eight u64 words (bit i of word w is key
    /// 64 * w + i). Codes outside [0, KEY_COUNT) are ignored by set/reset and test false.
    /// </summary>
    struct key_bitset {
        static constexpr u32 WORD_COUNT = KEY_COUNT / 64;

        std::array<u64, WORD_COUNT> words = {};

        /// <summary>
        /// True when the key's bit is set.
        /// </summary>
        [[nodiscard]] constexpr bool test(key code) const noexcept {
            const u32 index = static_cast<u32>(code);
            if (index >= KEY_COUNT) {
                return false;
            }
            return (words[index / 64] & (u64{1} << (index % 64))) != 0;
        }

        /// <summary>
        /// Sets the key's bit.
        /// </summary>
        constexpr void set(key code) noexcept {
            const u32 index = static_cast<u32>(code);
            if (index >= KEY_COUNT) {
                return;
            }
            words[index / 64] |= (u64{1} << (index % 64));
        }

        /// <summary>
        /// Clears the key's bit.
        /// </summary>
        constexpr void reset(key code) noexcept {
            const u32 index = static_cast<u32>(code);
            if (index >= KEY_COUNT) {
                return;
            }
            words[index / 64] &= ~(u64{1} << (index % 64));
        }

        /// <summary>
        /// Clears every bit.
        /// </summary>
        constexpr void clear() noexcept {
            words.fill(0);
        }

        /// <summary>
        /// True when at least one bit is set.
        /// </summary>
        [[nodiscard]] constexpr bool any() const noexcept {
            for (const u64 word : words) {
                if (word != 0) {
                    return true;
                }
            }
            return false;
        }

        /// <summary>
        /// Number of set bits.
        /// </summary>
        [[nodiscard]] constexpr u32 count() const noexcept {
            u32 total = 0;
            for (const u64 word : words) {
                total += static_cast<u32>(std::popcount(word));
            }
            return total;
        }

        [[nodiscard]] constexpr bool operator==(const key_bitset &) const noexcept = default;
    };

    static_assert(sizeof(key_bitset) == 64);
    static_assert(std::is_trivially_copyable_v<key_bitset>);

    /// <summary>
    /// One gamepad slot: connection, button masks (bit i = gamepad_button(i)) and axes.
    /// </summary>
    struct gamepad_state {
        bool connected = false;
        u32 buttonsDown = 0;
        u32 buttonsPressed = 0;
        u32 buttonsReleased = 0;
        std::array<f32, GAMEPAD_AXIS_COUNT> axes = {};      // sticks [-1, 1], triggers [0, 1]

        /// <summary>
        /// True while the button is held.
        /// </summary>
        [[nodiscard]] bool is_down(gamepad_button button) const noexcept {
            return is_valid_gamepad_button(button) && (buttonsDown & (u32{1} << static_cast<u32>(button))) != 0;
        }

        /// <summary>
        /// True in the frame the button went down.
        /// </summary>
        [[nodiscard]] bool is_pressed(gamepad_button button) const noexcept {
            return is_valid_gamepad_button(button) && (buttonsPressed & (u32{1} << static_cast<u32>(button))) != 0;
        }

        /// <summary>
        /// True in the frame the button came up.
        /// </summary>
        [[nodiscard]] bool is_released(gamepad_button button) const noexcept {
            return is_valid_gamepad_button(button) && (buttonsReleased & (u32{1} << static_cast<u32>(button))) != 0;
        }

        /// <summary>
        /// The axis value; 0 for an invalid axis.
        /// </summary>
        [[nodiscard]] f32 axis(gamepad_axis axisId) const noexcept {
            if (!is_valid_gamepad_axis(axisId)) {
                return 0.0f;
            }
            return axes[static_cast<usize>(axisId)];
        }

        [[nodiscard]] bool operator==(const gamepad_state &) const noexcept = default;
    };

    static_assert(std::is_trivially_copyable_v<gamepad_state>);

    inline constexpr u32 MAX_TEXT_CODEPOINTS_PER_FRAME = 16;

    /// <summary>
    /// Everything a frame may ask about input. Published by input::end_frame(); readers never see a
    /// partially updated snapshot.
    /// </summary>
    struct input_state {
        u64 frameIndex = 0;                             // incremented by every end_frame()
        key_bitset keysDown = {};
        key_bitset keysPressed = {};
        key_bitset keysReleased = {};
        key_modifier_flags modifiers = key_modifier_flags::NONE;    // from the latest key / mouse event
        math::vec2 mousePosition = math::vec2(0.0f);    // logical coordinates
        math::vec2 mouseDelta = math::vec2(0.0f);       // sum of movement this frame (works with cursor_mode::DISABLED)
        math::vec2 scrollDelta = math::vec2(0.0f);      // sum of scroll offsets this frame
        u32 mouseButtonsDown = 0;                       // bit i = mouse_button(i)
        u32 mouseButtonsPressed = 0;
        u32 mouseButtonsReleased = 0;
        bool cursorInWindow = true;
        std::array<gamepad_state, MAX_GAMEPADS> gamepads = {};
        std::array<u32, MAX_TEXT_CODEPOINTS_PER_FRAME> textCodepoints = {};
        u32 textCount = 0;

        /// <summary>
        /// True while the key is held (including the frame it was pressed).
        /// </summary>
        [[nodiscard]] bool is_key_down(key code) const noexcept {
            return keysDown.test(code);
        }

        /// <summary>
        /// True in the frame the key went down.
        /// </summary>
        [[nodiscard]] bool is_key_pressed(key code) const noexcept {
            return keysPressed.test(code);
        }

        /// <summary>
        /// True in the frame the key came up.
        /// </summary>
        [[nodiscard]] bool is_key_released(key code) const noexcept {
            return keysReleased.test(code);
        }

        /// <summary>
        /// True while the key is down and this is not the frame it was pressed in.
        /// </summary>
        [[nodiscard]] bool is_key_held(key code) const noexcept {
            return keysDown.test(code) && !keysPressed.test(code);
        }

        /// <summary>
        /// True while the mouse button is held.
        /// </summary>
        [[nodiscard]] bool is_mouse_button_down(mouse_button button) const noexcept {
            return is_valid_mouse_button(button) && (mouseButtonsDown & (u32{1} << static_cast<u32>(button))) != 0;
        }

        /// <summary>
        /// True in the frame the mouse button went down.
        /// </summary>
        [[nodiscard]] bool is_mouse_button_pressed(mouse_button button) const noexcept {
            return is_valid_mouse_button(button) && (mouseButtonsPressed & (u32{1} << static_cast<u32>(button))) != 0;
        }

        /// <summary>
        /// True in the frame the mouse button came up.
        /// </summary>
        [[nodiscard]] bool is_mouse_button_released(mouse_button button) const noexcept {
            return is_valid_mouse_button(button) && (mouseButtonsReleased & (u32{1} << static_cast<u32>(button))) != 0;
        }

        /// <summary>
        /// The gamepad in a slot. Asserts on index >= MAX_GAMEPADS and then returns the last slot.
        /// </summary>
        [[nodiscard]] const gamepad_state &gamepad(u32 index) const noexcept {
            CONJURE_ASSERT(
              index < MAX_GAMEPADS,
              "gamepad index {} out of range (MAX_GAMEPADS = {})",
              index,
              MAX_GAMEPADS
            );
            return gamepads[std::min(index, MAX_GAMEPADS - 1)];
        }

        /// <summary>
        /// The UTF-32 code points typed this frame, in order (at most MAX_TEXT_CODEPOINTS_PER_FRAME).
        /// </summary>
        [[nodiscard]] std::span<const u32> text() const noexcept {
            return std::span<const u32>(textCodepoints.data(), std::min(textCount, MAX_TEXT_CODEPOINTS_PER_FRAME));
        }

        [[nodiscard]] bool operator==(const input_state &) const noexcept = default;
    };

    static_assert(std::is_trivially_copyable_v<input_state>);
}
