#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "platform_test_support.h"

#include "conjure/platform/input_state.h"

#include <array>
#include <span>
#include <type_traits>

using namespace conjure;
using namespace conjure::platform;

TEST_SUITE("platform/input_state") {

    TEST_CASE("key_bitset - is 512 bits in eight words and trivially copyable") {
        static_assert(sizeof(key_bitset) == 64);
        static_assert(std::is_trivially_copyable_v<key_bitset>);
        CHECK(key_bitset::WORD_COUNT == 8);
        const key_bitset empty = {};
        CHECK_FALSE(empty.any());
        CHECK(empty.count() == 0);
        for (const u64 word : empty.words) {
            CHECK(word == 0);
        }
    }

    TEST_CASE("key_bitset - set, test, reset and count work across word boundaries") {
        key_bitset bits = {};
        const std::array<key, 4> probes = {static_cast<key>(63), static_cast<key>(64), key::SPACE, key::MENU};

        for (const key code : probes) {
            CAPTURE(static_cast<u32>(code));
            CHECK_FALSE(bits.test(code));
            bits.set(code);
            CHECK(bits.test(code));
        }
        CHECK(bits.count() == 4);
        CHECK(bits.any());
        CHECK(bits.words[0] == ((u64{1} << 63) | (u64{1} << 32)));
        CHECK(bits.words[1] == u64{1});
        CHECK(bits.words[5] == (u64{1} << (348 % 64)));

        bits.reset(static_cast<key>(63));
        CHECK_FALSE(bits.test(static_cast<key>(63)));
        CHECK(bits.test(static_cast<key>(64)));
        CHECK(bits.count() == 3);

        bits.set(key::MENU);    // setting twice is idempotent
        CHECK(bits.count() == 3);
        bits.reset(key::A);     // resetting a clear bit changes nothing
        CHECK(bits.count() == 3);

        bits.clear();
        CHECK_FALSE(bits.any());
        CHECK(bits.count() == 0);
    }

    TEST_CASE("key_bitset - codes at or beyond KEY_COUNT are ignored") {
        key_bitset bits = {};
        bits.set(static_cast<key>(512));
        bits.set(static_cast<key>(0xFFFF));
        CHECK_FALSE(bits.any());
        CHECK_FALSE(bits.test(static_cast<key>(512)));
        CHECK_FALSE(bits.test(static_cast<key>(0xFFFF)));
        bits.set(static_cast<key>(511));
        CHECK(bits.test(static_cast<key>(511)));
        bits.reset(static_cast<key>(0xFFFF));
        CHECK(bits.count() == 1);
    }

    TEST_CASE("key_bitset - the unknown key code 0 is bit 0 and compares by value") {
        key_bitset first = {};
        key_bitset second = {};
        CHECK(first == second);
        first.set(key::UNKNOWN);
        CHECK(first.words[0] == 1);
        CHECK_FALSE(first == second);
        second.set(key::UNKNOWN);
        CHECK(first == second);
    }

    TEST_CASE("gamepad_state - defaults are disconnected and silent") {
        const gamepad_state pad = {};
        CHECK_FALSE(pad.connected);
        CHECK(pad.buttonsDown == 0);
        CHECK(pad.buttonsPressed == 0);
        CHECK(pad.buttonsReleased == 0);
        for (u8 axis = 0; axis < GAMEPAD_AXIS_COUNT; ++axis) {
            CHECK(pad.axis(static_cast<gamepad_axis>(axis)) == 0.0f);
        }
        CHECK_FALSE(pad.is_down(gamepad_button::A));
        CHECK_FALSE(pad.is_pressed(gamepad_button::A));
        CHECK_FALSE(pad.is_released(gamepad_button::A));
    }

    TEST_CASE("gamepad_state - button helpers read the bit of the button") {
        gamepad_state pad = {};
        pad.buttonsDown = u32{1} << static_cast<u32>(gamepad_button::DPAD_LEFT);
        pad.buttonsPressed = u32{1} << static_cast<u32>(gamepad_button::START);
        pad.buttonsReleased = u32{1} << static_cast<u32>(gamepad_button::A);
        CHECK(pad.is_down(gamepad_button::DPAD_LEFT));
        CHECK_FALSE(pad.is_down(gamepad_button::START));
        CHECK(pad.is_pressed(gamepad_button::START));
        CHECK(pad.is_released(gamepad_button::A));
        CHECK_FALSE(pad.is_released(gamepad_button::B));
        CHECK_FALSE(pad.is_down(gamepad_button::COUNT));
        CHECK_FALSE(pad.is_pressed(static_cast<gamepad_button>(31)));
    }

    TEST_CASE("gamepad_state - axis reads the stored value and 0 for an invalid axis") {
        gamepad_state pad = {};
        pad.axes[static_cast<usize>(gamepad_axis::RIGHT_TRIGGER)] = 0.25f;
        pad.axes[static_cast<usize>(gamepad_axis::LEFT_X)] = -1.0f;
        CHECK(pad.axis(gamepad_axis::RIGHT_TRIGGER) == 0.25f);
        CHECK(pad.axis(gamepad_axis::LEFT_X) == -1.0f);
        CHECK(pad.axis(gamepad_axis::LEFT_Y) == 0.0f);
        CHECK(pad.axis(gamepad_axis::COUNT) == 0.0f);
        CHECK(pad.axis(static_cast<gamepad_axis>(200)) == 0.0f);
    }

    TEST_CASE("input_state - the default state is all zero and trivially copyable") {
        static_assert(std::is_trivially_copyable_v<input_state>);
        const input_state state = {};
        CHECK(state.frameIndex == 0);
        CHECK_FALSE(state.keysDown.any());
        CHECK_FALSE(state.keysPressed.any());
        CHECK_FALSE(state.keysReleased.any());
        CHECK(state.modifiers == key_modifier_flags::NONE);
        CHECK(state.mousePosition == math::vec2(0.0f));
        CHECK(state.mouseDelta == math::vec2(0.0f));
        CHECK(state.scrollDelta == math::vec2(0.0f));
        CHECK(state.mouseButtonsDown == 0);
        CHECK(state.mouseButtonsPressed == 0);
        CHECK(state.mouseButtonsReleased == 0);
        CHECK(state.cursorInWindow);
        CHECK(state.textCount == 0);
        CHECK(state.text().empty());
        for (u32 index = 0; index < MAX_GAMEPADS; ++index) {
            CHECK_FALSE(state.gamepad(index).connected);
        }
        CHECK(MAX_TEXT_CODEPOINTS_PER_FRAME == 16);
    }

    TEST_CASE("input_state - key helpers distinguish down, pressed, released and held") {
        input_state state = {};
        state.keysDown.set(key::W);
        state.keysPressed.set(key::W);
        state.keysDown.set(key::A);
        state.keysReleased.set(key::S);

        CHECK(state.is_key_down(key::W));
        CHECK(state.is_key_pressed(key::W));
        CHECK_FALSE(state.is_key_held(key::W));
        CHECK(state.is_key_down(key::A));
        CHECK_FALSE(state.is_key_pressed(key::A));
        CHECK(state.is_key_held(key::A));
        CHECK(state.is_key_released(key::S));
        CHECK_FALSE(state.is_key_down(key::S));
        CHECK_FALSE(state.is_key_held(key::S));
        CHECK_FALSE(state.is_key_down(key::D));
        CHECK_FALSE(state.is_key_down(static_cast<key>(1000)));
    }

    TEST_CASE("input_state - mouse button helpers read the masks and reject invalid buttons") {
        input_state state = {};
        state.mouseButtonsDown = u32{1} << static_cast<u32>(mouse_button::LEFT);
        state.mouseButtonsPressed = state.mouseButtonsDown;
        state.mouseButtonsReleased = u32{1} << static_cast<u32>(mouse_button::BUTTON_8);
        CHECK(state.is_mouse_button_down(mouse_button::LEFT));
        CHECK(state.is_mouse_button_pressed(mouse_button::LEFT));
        CHECK_FALSE(state.is_mouse_button_released(mouse_button::LEFT));
        CHECK(state.is_mouse_button_released(mouse_button::BUTTON_8));
        CHECK_FALSE(state.is_mouse_button_down(mouse_button::RIGHT));
        CHECK_FALSE(state.is_mouse_button_down(mouse_button::COUNT));
        CHECK_FALSE(state.is_mouse_button_pressed(static_cast<mouse_button>(31)));
    }

    TEST_CASE("input_state - gamepad(index) returns the slot and asserts out of range") {
        input_state state = {};
        state.gamepads[3].connected = true;
        CHECK(state.gamepad(3).connected);
        CHECK_FALSE(state.gamepad(0).connected);

        testing::expect_assert guard = {};
        const gamepad_state &clamped = state.gamepad(MAX_GAMEPADS);
        CHECK(guard.count() == 1);
        CHECK(&clamped == &state.gamepads[MAX_GAMEPADS - 1]);
    }

    TEST_CASE("input_state - text() spans exactly textCount code points") {
        input_state state = {};
        state.textCodepoints[0] = 'h';
        state.textCodepoints[1] = 'i';
        state.textCount = 2;
        const std::span<const u32> text = state.text();
        REQUIRE(text.size() == 2);
        CHECK(text[0] == 'h');
        CHECK(text[1] == 'i');

        state.textCount = 99;   // a corrupt count never reads past the array
        CHECK(state.text().size() == MAX_TEXT_CODEPOINTS_PER_FRAME);
    }

    TEST_CASE("input_state - equality compares every field") {
        input_state first = {};
        input_state second = {};
        CHECK(first == second);
        second.frameIndex = 1;
        CHECK_FALSE(first == second);
        second.frameIndex = 0;
        second.gamepads[1].axes[2] = 0.5f;
        CHECK_FALSE(first == second);
        second.gamepads[1].axes[2] = 0.0f;
        second.textCodepoints[15] = 7;
        CHECK_FALSE(first == second);
        second.textCodepoints[15] = 0;
        CHECK(first == second);
    }
}
