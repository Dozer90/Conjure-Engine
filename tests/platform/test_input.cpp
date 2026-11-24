#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "platform_test_support.h"

#include "conjure/platform/input.h"

#include <vector>

using namespace conjure;
using namespace conjure::platform;
using namespace conjure::platform::test_support;

namespace {
    /// <summary>
    /// A scripted sequence touching every input alternative, used to prove inject == apply.
    /// </summary>
    [[nodiscard]] std::vector<input_event> scripted_sequence() {
        std::vector<input_event> sequence = {};
        sequence.emplace_back(make_key(key::W, button_action::PRESS, key_modifier_flags::SHIFT));
        sequence.emplace_back(make_key(key::A, button_action::PRESS));
        sequence.emplace_back(make_key(key::A, button_action::RELEASE));
        sequence.emplace_back(make_key(key::W, button_action::REPEAT));
        sequence.emplace_back(make_text('x'));
        sequence.emplace_back(make_mouse_move(100.0f, 50.0f));
        sequence.emplace_back(make_mouse_move(110.0f, 45.0f));
        sequence.emplace_back(make_mouse_button(mouse_button::LEFT, button_action::PRESS, key_modifier_flags::CONTROL));
        sequence.emplace_back(make_scroll(0.0f, 2.0f));
        cursor_enter_event leave = {};
        leave.entered = false;
        sequence.emplace_back(leave);
        sequence.emplace_back(make_gamepad_connection(1, true));
        sequence.emplace_back(make_gamepad_button(1, gamepad_button::A, button_action::PRESS));
        sequence.emplace_back(make_gamepad_axis(1, gamepad_axis::LEFT_X, -0.5f));
        return sequence;
    }
}

TEST_SUITE("platform/input") {

    TEST_CASE("input - a fresh instance publishes an empty snapshot at frame 0") {
        testing::memory_guard guard = {};
        input accumulator = {};
        CHECK(accumulator.snapshot() == input_state());
        CHECK(accumulator.snapshot().frameIndex == 0);
        CHECK(accumulator.stats().eventsApplied == 0);
        CHECK(accumulator.stats().eventsInjected == 0);
        CHECK(accumulator.stats().ignoredTransitions == 0);
        CHECK(accumulator.stats().textCodepointsDropped == 0);
    }

    TEST_CASE("input - a pressed key is pressed and down for one frame then held") {
        input accumulator = {};
        accumulator.inject(make_key(key::SPACE, button_action::PRESS));
        accumulator.end_frame();

        const input_state &first = accumulator.snapshot();
        CHECK(first.is_key_down(key::SPACE));
        CHECK(first.is_key_pressed(key::SPACE));
        CHECK_FALSE(first.is_key_released(key::SPACE));
        CHECK_FALSE(first.is_key_held(key::SPACE));

        accumulator.end_frame();
        const input_state &second = accumulator.snapshot();
        CHECK(second.is_key_down(key::SPACE));
        CHECK_FALSE(second.is_key_pressed(key::SPACE));
        CHECK(second.is_key_held(key::SPACE));
        CHECK_FALSE(second.is_key_released(key::SPACE));
    }

    TEST_CASE("regression - code sweep F637 mouse and gamepad buttons are pressed for one frame then held") {
        input accumulator = {};
        accumulator.inject(make_gamepad_connection(2, true));

        SUBCASE("mouse button") {
            accumulator.inject(make_mouse_button(mouse_button::RIGHT, button_action::PRESS));
            accumulator.end_frame();
            CHECK(accumulator.snapshot().is_mouse_button_pressed(mouse_button::RIGHT));
            CHECK_FALSE(accumulator.snapshot().is_mouse_button_held(mouse_button::RIGHT));

            accumulator.end_frame();
            CHECK(accumulator.snapshot().is_mouse_button_down(mouse_button::RIGHT));
            CHECK(accumulator.snapshot().is_mouse_button_held(mouse_button::RIGHT));

            accumulator.inject(make_mouse_button(mouse_button::RIGHT, button_action::RELEASE));
            accumulator.end_frame();
            CHECK(accumulator.snapshot().is_mouse_button_released(mouse_button::RIGHT));
            CHECK_FALSE(accumulator.snapshot().is_mouse_button_held(mouse_button::RIGHT));
        }
        SUBCASE("gamepad button") {
            accumulator.inject(make_gamepad_button(2, gamepad_button::Y, button_action::PRESS));
            accumulator.end_frame();
            CHECK(accumulator.snapshot().gamepad(2).is_pressed(gamepad_button::Y));
            CHECK_FALSE(accumulator.snapshot().gamepad(2).is_held(gamepad_button::Y));

            accumulator.end_frame();
            CHECK(accumulator.snapshot().gamepad(2).is_down(gamepad_button::Y));
            CHECK(accumulator.snapshot().gamepad(2).is_held(gamepad_button::Y));

            accumulator.inject(make_gamepad_button(2, gamepad_button::Y, button_action::RELEASE));
            accumulator.end_frame();
            CHECK(accumulator.snapshot().gamepad(2).is_released(gamepad_button::Y));
            CHECK_FALSE(accumulator.snapshot().gamepad(2).is_held(gamepad_button::Y));
        }
        SUBCASE("press and release in one frame are never held") {
            accumulator.inject(make_mouse_button(mouse_button::RIGHT, button_action::PRESS));
            accumulator.inject(make_mouse_button(mouse_button::RIGHT, button_action::RELEASE));
            accumulator.inject(make_gamepad_button(2, gamepad_button::Y, button_action::PRESS));
            accumulator.inject(make_gamepad_button(2, gamepad_button::Y, button_action::RELEASE));
            accumulator.end_frame();
            const input_state &tapped = accumulator.snapshot();
            CHECK(tapped.is_mouse_button_pressed(mouse_button::RIGHT));
            CHECK(tapped.is_mouse_button_released(mouse_button::RIGHT));
            CHECK_FALSE(tapped.is_mouse_button_down(mouse_button::RIGHT));
            CHECK_FALSE(tapped.is_mouse_button_held(mouse_button::RIGHT));
            CHECK(tapped.gamepad(2).is_pressed(gamepad_button::Y));
            CHECK(tapped.gamepad(2).is_released(gamepad_button::Y));
            CHECK_FALSE(tapped.gamepad(2).is_down(gamepad_button::Y));
            CHECK_FALSE(tapped.gamepad(2).is_held(gamepad_button::Y));

            accumulator.end_frame();
            CHECK_FALSE(accumulator.snapshot().is_mouse_button_held(mouse_button::RIGHT));
            CHECK_FALSE(accumulator.snapshot().gamepad(2).is_held(gamepad_button::Y));
        }
        SUBCASE("losing focus ends the hold with a released edge") {
            accumulator.inject(make_mouse_button(mouse_button::RIGHT, button_action::PRESS));
            accumulator.inject(make_gamepad_button(2, gamepad_button::Y, button_action::PRESS));
            accumulator.end_frame();
            accumulator.end_frame();
            REQUIRE(accumulator.snapshot().is_mouse_button_held(mouse_button::RIGHT));
            REQUIRE(accumulator.snapshot().gamepad(2).is_held(gamepad_button::Y));

            CHECK_FALSE(accumulator.apply(event(make_focus(false))));
            accumulator.end_frame();
            const input_state &unfocused = accumulator.snapshot();
            CHECK_FALSE(unfocused.is_mouse_button_held(mouse_button::RIGHT));
            CHECK(unfocused.is_mouse_button_released(mouse_button::RIGHT));
            CHECK_FALSE(unfocused.gamepad(2).is_held(gamepad_button::Y));
            CHECK(unfocused.gamepad(2).is_released(gamepad_button::Y));
        }
        SUBCASE("disconnecting the gamepad ends the hold with a released edge") {
            accumulator.inject(make_gamepad_button(2, gamepad_button::Y, button_action::PRESS));
            accumulator.end_frame();
            accumulator.end_frame();
            REQUIRE(accumulator.snapshot().gamepad(2).is_held(gamepad_button::Y));

            accumulator.inject(make_gamepad_connection(2, false));
            accumulator.end_frame();
            CHECK_FALSE(accumulator.snapshot().gamepad(2).is_held(gamepad_button::Y));
            CHECK(accumulator.snapshot().gamepad(2).is_released(gamepad_button::Y));
        }
    }

    TEST_CASE("input - a released key is released for exactly one frame") {
        input accumulator = {};
        accumulator.inject(make_key(key::E, button_action::PRESS));
        accumulator.end_frame();
        accumulator.inject(make_key(key::E, button_action::RELEASE));
        accumulator.end_frame();

        CHECK_FALSE(accumulator.snapshot().is_key_down(key::E));
        CHECK(accumulator.snapshot().is_key_released(key::E));
        CHECK_FALSE(accumulator.snapshot().is_key_pressed(key::E));

        accumulator.end_frame();
        CHECK_FALSE(accumulator.snapshot().is_key_released(key::E));
        CHECK_FALSE(accumulator.snapshot().is_key_down(key::E));
    }

    TEST_CASE("input - press and release in one frame set both edges and leave the key up") {
        input accumulator = {};
        accumulator.inject(make_key(key::Q, button_action::PRESS));
        accumulator.inject(make_key(key::Q, button_action::RELEASE));
        accumulator.end_frame();

        CHECK(accumulator.snapshot().is_key_pressed(key::Q));
        CHECK(accumulator.snapshot().is_key_released(key::Q));
        CHECK_FALSE(accumulator.snapshot().is_key_down(key::Q));
        CHECK(accumulator.stats().ignoredTransitions == 0);
    }

    TEST_CASE("input - a second press of a down key is ignored and counted") {
        input accumulator = {};
        accumulator.inject(make_key(key::Z, button_action::PRESS));
        accumulator.end_frame();
        accumulator.inject(make_key(key::Z, button_action::PRESS));
        accumulator.end_frame();

        CHECK(accumulator.snapshot().is_key_down(key::Z));
        CHECK_FALSE(accumulator.snapshot().is_key_pressed(key::Z));
        CHECK(accumulator.snapshot().keysDown.count() == 1);
        CHECK(accumulator.stats().ignoredTransitions == 1);
    }

    TEST_CASE("input - a release of an up key is ignored and counted") {
        input accumulator = {};
        accumulator.inject(make_key(key::Z, button_action::RELEASE));
        accumulator.end_frame();

        CHECK_FALSE(accumulator.snapshot().is_key_released(key::Z));
        CHECK_FALSE(accumulator.snapshot().keysReleased.any());
        CHECK(accumulator.stats().ignoredTransitions == 1);
    }

    TEST_CASE("input - REPEAT changes no state but updates the modifiers") {
        input accumulator = {};
        accumulator.inject(make_key(key::R, button_action::REPEAT, key_modifier_flags::ALT));
        accumulator.end_frame();
        CHECK_FALSE(accumulator.snapshot().is_key_down(key::R));
        CHECK_FALSE(accumulator.snapshot().is_key_pressed(key::R));
        CHECK(accumulator.snapshot().modifiers == key_modifier_flags::ALT);
        CHECK(accumulator.stats().ignoredTransitions == 0);

        accumulator.inject(make_key(key::R, button_action::PRESS));
        accumulator.end_frame();
        accumulator.inject(make_key(key::R, button_action::REPEAT));
        accumulator.inject(make_key(key::R, button_action::REPEAT));
        accumulator.end_frame();
        CHECK(accumulator.snapshot().is_key_held(key::R));
        CHECK_FALSE(accumulator.snapshot().is_key_pressed(key::R));
        CHECK(accumulator.stats().ignoredTransitions == 0);
    }

    TEST_CASE("input - the modifiers follow the latest key or mouse event") {
        input accumulator = {};
        accumulator.inject(make_key(key::LEFT_SHIFT, button_action::PRESS, key_modifier_flags::SHIFT));
        accumulator.end_frame();
        CHECK(accumulator.snapshot().modifiers == key_modifier_flags::SHIFT);

        const key_modifier_flags shiftControl = key_modifier_flags::SHIFT | key_modifier_flags::CONTROL;
        accumulator.inject(make_mouse_button(mouse_button::LEFT, button_action::PRESS, shiftControl));
        accumulator.end_frame();
        CHECK(accumulator.snapshot().modifiers == (key_modifier_flags::SHIFT | key_modifier_flags::CONTROL));

        accumulator.end_frame();
        CHECK(accumulator.snapshot().modifiers == (key_modifier_flags::SHIFT | key_modifier_flags::CONTROL));
    }

    TEST_CASE("input - UNKNOWN and out-of-range key codes never touch the bitsets") {
        input accumulator = {};
        accumulator.inject(make_key(key::UNKNOWN, button_action::PRESS, key_modifier_flags::SUPER));
        accumulator.inject(make_key(static_cast<key>(349), button_action::PRESS));
        accumulator.inject(make_key(static_cast<key>(511), button_action::PRESS));
        accumulator.inject(make_key(static_cast<key>(600), button_action::RELEASE));
        accumulator.end_frame();

        CHECK_FALSE(accumulator.snapshot().keysDown.any());
        CHECK_FALSE(accumulator.snapshot().keysPressed.any());
        CHECK_FALSE(accumulator.snapshot().keysReleased.any());
        CHECK(accumulator.snapshot().modifiers == key_modifier_flags::NONE);
        CHECK(accumulator.stats().ignoredTransitions == 4);
        CHECK(accumulator.stats().eventsApplied == 4);
    }

    TEST_CASE("input - mouse buttons follow the same press and release rules") {
        input accumulator = {};
        accumulator.inject(make_mouse_button(mouse_button::RIGHT, button_action::PRESS));
        accumulator.inject(make_mouse_button(mouse_button::RIGHT, button_action::PRESS));
        accumulator.inject(make_mouse_button(mouse_button::MIDDLE, button_action::RELEASE));
        accumulator.inject(make_mouse_button(mouse_button::BUTTON_8, button_action::PRESS));
        accumulator.inject(make_mouse_button(mouse_button::BUTTON_8, button_action::RELEASE));
        accumulator.inject(make_mouse_button(mouse_button::LEFT, button_action::REPEAT));
        accumulator.end_frame();

        const input_state &state = accumulator.snapshot();
        CHECK(state.is_mouse_button_down(mouse_button::RIGHT));
        CHECK(state.is_mouse_button_pressed(mouse_button::RIGHT));
        CHECK_FALSE(state.is_mouse_button_released(mouse_button::MIDDLE));
        CHECK(state.is_mouse_button_pressed(mouse_button::BUTTON_8));
        CHECK(state.is_mouse_button_released(mouse_button::BUTTON_8));
        CHECK_FALSE(state.is_mouse_button_down(mouse_button::BUTTON_8));
        CHECK_FALSE(state.is_mouse_button_down(mouse_button::LEFT));
        CHECK(accumulator.stats().ignoredTransitions == 2);

        accumulator.end_frame();
        CHECK(accumulator.snapshot().is_mouse_button_down(mouse_button::RIGHT));
        CHECK_FALSE(accumulator.snapshot().is_mouse_button_pressed(mouse_button::RIGHT));
        CHECK_FALSE(accumulator.snapshot().is_mouse_button_released(mouse_button::BUTTON_8));
    }

    TEST_CASE("input - an invalid mouse button is ignored and counted") {
        input accumulator = {};
        accumulator.inject(make_mouse_button(mouse_button::COUNT, button_action::PRESS));
        accumulator.inject(make_mouse_button(static_cast<mouse_button>(200), button_action::PRESS));
        accumulator.end_frame();
        CHECK(accumulator.snapshot().mouseButtonsDown == 0);
        CHECK(accumulator.snapshot().mouseButtonsPressed == 0);
        CHECK(accumulator.stats().ignoredTransitions == 2);
    }

    TEST_CASE("input - the first mouse move sets the position without a delta") {
        input accumulator = {};
        accumulator.inject(make_mouse_move(640.0f, 360.0f));
        accumulator.end_frame();
        CHECK(accumulator.snapshot().mousePosition == math::vec2(640.0f, 360.0f));
        CHECK(accumulator.snapshot().mouseDelta == math::vec2(0.0f));
    }

    TEST_CASE("input - mouse deltas accumulate within a frame and reset at end_frame") {
        input accumulator = {};
        accumulator.inject(make_mouse_move(0.0f, 0.0f));
        accumulator.end_frame();

        accumulator.inject(make_mouse_move(10.0f, 5.0f));
        accumulator.inject(make_mouse_move(15.0f, -5.0f));
        accumulator.end_frame();
        CHECK(accumulator.snapshot().mouseDelta == math::vec2(15.0f, -5.0f));
        CHECK(accumulator.snapshot().mousePosition == math::vec2(15.0f, -5.0f));

        accumulator.end_frame();
        CHECK(accumulator.snapshot().mouseDelta == math::vec2(0.0f));
        CHECK(accumulator.snapshot().mousePosition == math::vec2(15.0f, -5.0f));
    }

    TEST_CASE("input - cursor re-entry and skip_next_mouse_delta suppress the next jump") {
        input accumulator = {};
        accumulator.inject(make_mouse_move(0.0f, 0.0f));
        accumulator.end_frame();

        cursor_enter_event leave = {};
        leave.entered = false;
        accumulator.inject(leave);
        cursor_enter_event enter = {};
        enter.entered = true;
        accumulator.inject(enter);
        accumulator.inject(make_mouse_move(500.0f, 500.0f));
        accumulator.inject(make_mouse_move(510.0f, 500.0f));
        accumulator.end_frame();
        CHECK(accumulator.snapshot().cursorInWindow);
        CHECK(accumulator.snapshot().mouseDelta == math::vec2(10.0f, 0.0f));
        CHECK(accumulator.snapshot().mousePosition == math::vec2(510.0f, 500.0f));

        accumulator.skip_next_mouse_delta();
        accumulator.inject(make_mouse_move(0.0f, 0.0f));
        accumulator.inject(make_mouse_move(1.0f, 2.0f));
        accumulator.end_frame();
        CHECK(accumulator.snapshot().mouseDelta == math::vec2(1.0f, 2.0f));

        accumulator.inject(leave);
        accumulator.end_frame();
        CHECK_FALSE(accumulator.snapshot().cursorInWindow);
    }

    TEST_CASE("input - scroll offsets accumulate within a frame and reset at end_frame") {
        input accumulator = {};
        accumulator.inject(make_scroll(0.0f, 1.0f));
        accumulator.inject(make_scroll(1.0f, 1.0f));
        accumulator.inject(make_scroll(0.0f, -0.5f));
        accumulator.end_frame();
        CHECK(accumulator.snapshot().scrollDelta == math::vec2(1.0f, 1.5f));
        accumulator.end_frame();
        CHECK(accumulator.snapshot().scrollDelta == math::vec2(0.0f));
    }

    TEST_CASE("input - text keeps up to 16 code points per frame and counts the dropped ones") {
        input accumulator = {};
        for (u32 index = 0; index < 20; ++index) {
            accumulator.inject(make_text('a' + index));
        }
        accumulator.end_frame();

        const std::span<const u32> text = accumulator.snapshot().text();
        REQUIRE(text.size() == MAX_TEXT_CODEPOINTS_PER_FRAME);
        for (u32 index = 0; index < MAX_TEXT_CODEPOINTS_PER_FRAME; ++index) {
            CAPTURE(index);
            CHECK(text[index] == 'a' + index);
        }
        CHECK(accumulator.stats().textCodepointsDropped == 4);

        accumulator.end_frame();
        CHECK(accumulator.snapshot().textCount == 0);
        CHECK(accumulator.snapshot().text().empty());

        accumulator.inject(make_text(0x1F600));
        accumulator.end_frame();
        REQUIRE(accumulator.snapshot().text().size() == 1);
        CHECK(accumulator.snapshot().text()[0] == 0x1F600);
        CHECK(accumulator.stats().textCodepointsDropped == 4);
    }

    TEST_CASE("input - gamepad connect zeroes the slot and disconnect releases its buttons") {
        input accumulator = {};
        accumulator.inject(make_gamepad_connection(2, true));
        accumulator.inject(make_gamepad_button(2, gamepad_button::X, button_action::PRESS));
        accumulator.inject(make_gamepad_axis(2, gamepad_axis::LEFT_TRIGGER, 1.0f));
        accumulator.inject(make_gamepad_axis(2, gamepad_axis::RIGHT_Y, -0.25f));
        accumulator.end_frame();

        const gamepad_state &pad = accumulator.snapshot().gamepad(2);
        CHECK(pad.connected);
        CHECK(pad.is_down(gamepad_button::X));
        CHECK(pad.is_pressed(gamepad_button::X));
        CHECK(pad.axis(gamepad_axis::LEFT_TRIGGER) == 1.0f);
        CHECK(pad.axis(gamepad_axis::RIGHT_Y) == -0.25f);
        CHECK_FALSE(accumulator.snapshot().gamepad(0).connected);

        accumulator.inject(make_gamepad_connection(2, false));
        accumulator.end_frame();
        const gamepad_state &gone = accumulator.snapshot().gamepad(2);
        CHECK_FALSE(gone.connected);
        CHECK_FALSE(gone.is_down(gamepad_button::X));
        CHECK(gone.is_released(gamepad_button::X));
        CHECK(gone.axis(gamepad_axis::LEFT_TRIGGER) == 0.0f);
        CHECK(gone.axis(gamepad_axis::RIGHT_Y) == 0.0f);

        accumulator.end_frame();
        CHECK_FALSE(accumulator.snapshot().gamepad(2).is_released(gamepad_button::X));
    }

    TEST_CASE("input - reconnecting a gamepad starts from a clean slot") {
        input accumulator = {};
        accumulator.inject(make_gamepad_connection(0, true));
        accumulator.inject(make_gamepad_button(0, gamepad_button::B, button_action::PRESS));
        accumulator.inject(make_gamepad_axis(0, gamepad_axis::RIGHT_X, 0.9f));
        accumulator.inject(make_gamepad_connection(0, true));
        accumulator.end_frame();

        const gamepad_state &pad = accumulator.snapshot().gamepad(0);
        CHECK(pad.connected);
        CHECK(pad.buttonsDown == 0);
        CHECK(pad.buttonsPressed == 0);
        CHECK(pad.axis(gamepad_axis::RIGHT_X) == 0.0f);
    }

    TEST_CASE("input - gamepad buttons follow the press and release rules and axes are levels") {
        input accumulator = {};
        accumulator.inject(make_gamepad_connection(3, true));
        accumulator.inject(make_gamepad_button(3, gamepad_button::START, button_action::PRESS));
        accumulator.inject(make_gamepad_button(3, gamepad_button::START, button_action::PRESS));
        accumulator.inject(make_gamepad_button(3, gamepad_button::BACK, button_action::RELEASE));
        accumulator.inject(make_gamepad_button(3, gamepad_button::GUIDE, button_action::REPEAT));
        accumulator.inject(make_gamepad_button(3, gamepad_button::COUNT, button_action::PRESS));
        accumulator.inject(make_gamepad_axis(3, gamepad_axis::COUNT, 1.0f));
        accumulator.inject(make_gamepad_axis(3, gamepad_axis::LEFT_Y, 0.5f));
        accumulator.inject(make_gamepad_axis(3, gamepad_axis::LEFT_Y, 0.75f));
        accumulator.end_frame();

        const gamepad_state &pad = accumulator.snapshot().gamepad(3);
        CHECK(pad.is_pressed(gamepad_button::START));
        CHECK(pad.is_down(gamepad_button::START));
        CHECK_FALSE(pad.is_released(gamepad_button::BACK));
        CHECK_FALSE(pad.is_down(gamepad_button::GUIDE));
        CHECK(pad.axis(gamepad_axis::LEFT_Y) == 0.75f);
        CHECK(accumulator.stats().ignoredTransitions == 4);

        accumulator.end_frame();
        CHECK(accumulator.snapshot().gamepad(3).is_down(gamepad_button::START));
        CHECK_FALSE(accumulator.snapshot().gamepad(3).is_pressed(gamepad_button::START));
        CHECK(accumulator.snapshot().gamepad(3).axis(gamepad_axis::LEFT_Y) == 0.75f);
    }

    TEST_CASE("input - a gamepad index beyond MAX_GAMEPADS asserts and is dropped") {
        input accumulator = {};
        testing::expect_assert guard = {};
        accumulator.inject(make_gamepad_connection(4, true));
        accumulator.inject(make_gamepad_button(7, gamepad_button::A, button_action::PRESS));
        accumulator.inject(make_gamepad_axis(255, gamepad_axis::LEFT_X, 1.0f));
        CHECK(guard.count() == 3);
        accumulator.end_frame();
        CHECK(accumulator.stats().ignoredTransitions == 3);
        CHECK(accumulator.snapshot() == [] {
            input_state expected = {};
            expected.frameIndex = 1;
            return expected;
        }());
    }

    TEST_CASE("input - losing focus releases every key and button with an edge") {
        input accumulator = {};
        accumulator.inject(make_key(key::W, button_action::PRESS, key_modifier_flags::SHIFT));
        accumulator.inject(make_key(key::LEFT_SHIFT, button_action::PRESS, key_modifier_flags::SHIFT));
        accumulator.inject(make_mouse_button(mouse_button::LEFT, button_action::PRESS));
        accumulator.inject(make_gamepad_connection(1, true));
        accumulator.inject(make_gamepad_button(1, gamepad_button::A, button_action::PRESS));
        accumulator.end_frame();

        CHECK_FALSE(accumulator.apply(event(make_focus(false))));
        // GLFW synthesises its own releases on focus loss; the second release is a no-op (P4).
        accumulator.inject(make_key(key::W, button_action::RELEASE));
        accumulator.end_frame();

        const input_state &state = accumulator.snapshot();
        CHECK_FALSE(state.keysDown.any());
        CHECK(state.is_key_released(key::W));
        CHECK(state.is_key_released(key::LEFT_SHIFT));
        CHECK(state.keysReleased.count() == 2);
        CHECK(state.mouseButtonsDown == 0);
        CHECK(state.is_mouse_button_released(mouse_button::LEFT));
        CHECK(state.gamepad(1).connected);
        CHECK_FALSE(state.gamepad(1).is_down(gamepad_button::A));
        CHECK(state.gamepad(1).is_released(gamepad_button::A));
        CHECK(state.modifiers == key_modifier_flags::NONE);
        CHECK(accumulator.stats().ignoredTransitions == 1);

        accumulator.end_frame();
        CHECK_FALSE(accumulator.snapshot().keysReleased.any());
    }

    TEST_CASE("input - regaining focus changes nothing") {
        input accumulator = {};
        accumulator.inject(make_key(key::W, button_action::PRESS));
        CHECK_FALSE(accumulator.apply(event(make_focus(true))));
        accumulator.end_frame();
        CHECK(accumulator.snapshot().is_key_down(key::W));
        CHECK_FALSE(accumulator.snapshot().keysReleased.any());
    }

    TEST_CASE("input - release_all produces released edges and is idempotent") {
        input accumulator = {};
        accumulator.inject(make_key(key::A, button_action::PRESS));
        accumulator.inject(make_key(key::D, button_action::PRESS));
        accumulator.inject(make_mouse_button(mouse_button::RIGHT, button_action::PRESS));
        accumulator.end_frame();

        accumulator.release_all();
        accumulator.release_all();
        accumulator.end_frame();
        CHECK(accumulator.snapshot().keysReleased.count() == 2);
        CHECK(accumulator.snapshot().is_key_released(key::A));
        CHECK(accumulator.snapshot().is_key_released(key::D));
        CHECK_FALSE(accumulator.snapshot().keysDown.any());
        CHECK(accumulator.snapshot().mouseButtonsReleased == (u32{1} << static_cast<u32>(mouse_button::RIGHT)));
        CHECK(accumulator.snapshot().mouseButtonsDown == 0);

        accumulator.release_all();
        accumulator.end_frame();
        CHECK_FALSE(accumulator.snapshot().keysReleased.any());
        CHECK(accumulator.snapshot().mouseButtonsReleased == 0);
    }

    TEST_CASE("input - window events other than focus loss are ignored by apply") {
        input accumulator = {};
        framebuffer_resized_event resized = {};
        resized.width = 10;
        resized.height = 10;
        CHECK_FALSE(accumulator.apply(event(resized)));
        CHECK_FALSE(accumulator.apply(event(close_event())));
        minimise_event minimised = {};
        minimised.minimised = true;
        CHECK_FALSE(accumulator.apply(event(minimised)));
        content_scale_changed_event scaled = {};
        CHECK_FALSE(accumulator.apply(event(scaled)));
        CHECK_FALSE(accumulator.apply(event(drop_files_event())));
        CHECK(accumulator.apply(event(make_key(key::B, button_action::PRESS))));
        accumulator.end_frame();
        CHECK(accumulator.snapshot().is_key_pressed(key::B));
        CHECK(accumulator.stats().eventsApplied == 1);
        CHECK(accumulator.stats().eventsInjected == 0);
    }

    TEST_CASE("input - inject(event) ignores non-input events and counts the injected ones") {
        input accumulator = {};
        accumulator.inject(event(close_event()));
        accumulator.inject(event(make_focus(false)));
        accumulator.inject(event(make_key(key::C, button_action::PRESS)));
        accumulator.end_frame();
        CHECK(accumulator.snapshot().is_key_pressed(key::C));
        CHECK(accumulator.stats().eventsApplied == 1);
        CHECK(accumulator.stats().eventsInjected == 1);
    }

    TEST_CASE("input - frameIndex increments with every end_frame") {
        input accumulator = {};
        for (u64 frame = 1; frame <= 5; ++frame) {
            accumulator.end_frame();
            CHECK(accumulator.snapshot().frameIndex == frame);
        }
    }

    TEST_CASE("input - the snapshot is unchanged until end_frame") {
        input accumulator = {};
        accumulator.end_frame();
        const input_state before = accumulator.snapshot();

        accumulator.inject(make_key(key::SPACE, button_action::PRESS));
        accumulator.inject(make_mouse_move(5.0f, 5.0f));
        accumulator.inject(make_text('q'));
        CHECK(accumulator.snapshot() == before);
        CHECK_FALSE(accumulator.snapshot().is_key_down(key::SPACE));

        accumulator.end_frame();
        CHECK_FALSE(accumulator.snapshot() == before);
        CHECK(accumulator.snapshot().is_key_down(key::SPACE));
    }

    TEST_CASE("input - levels persist across frames while edges, deltas and text are cleared") {
        input accumulator = {};
        accumulator.inject(make_key(key::W, button_action::PRESS, key_modifier_flags::CONTROL));
        accumulator.inject(make_mouse_move(1.0f, 1.0f));
        accumulator.inject(make_mouse_move(3.0f, 4.0f));
        accumulator.inject(make_mouse_button(mouse_button::LEFT, button_action::PRESS, key_modifier_flags::CONTROL));
        accumulator.inject(make_scroll(0.0f, 1.0f));
        accumulator.inject(make_text('t'));
        accumulator.inject(make_gamepad_connection(0, true));
        accumulator.inject(make_gamepad_axis(0, gamepad_axis::LEFT_X, 0.5f));
        accumulator.inject(make_gamepad_button(0, gamepad_button::A, button_action::PRESS));
        cursor_enter_event leave = {};
        leave.entered = false;
        accumulator.inject(leave);
        accumulator.end_frame();
        accumulator.end_frame();

        const input_state &state = accumulator.snapshot();
        CHECK(state.frameIndex == 2);
        CHECK(state.is_key_down(key::W));
        CHECK_FALSE(state.keysPressed.any());
        CHECK(state.modifiers == key_modifier_flags::CONTROL);
        CHECK(state.mousePosition == math::vec2(3.0f, 4.0f));
        CHECK(state.mouseDelta == math::vec2(0.0f));
        CHECK(state.scrollDelta == math::vec2(0.0f));
        CHECK(state.is_mouse_button_down(mouse_button::LEFT));
        CHECK(state.mouseButtonsPressed == 0);
        CHECK(state.textCount == 0);
        CHECK_FALSE(state.cursorInWindow);
        CHECK(state.gamepad(0).connected);
        CHECK(state.gamepad(0).axis(gamepad_axis::LEFT_X) == 0.5f);
        CHECK(state.gamepad(0).is_down(gamepad_button::A));
        CHECK(state.gamepad(0).buttonsPressed == 0);
    }

    TEST_CASE("input - inject and apply of the same sequence yield identical snapshots") {
        const std::vector<input_event> sequence = scripted_sequence();

        input injected = {};
        for (const input_event &value : sequence) {
            injected.inject(value);
        }
        injected.end_frame();

        input applied = {};
        for (const input_event &value : sequence) {
            CHECK(applied.apply(to_event(value)));
        }
        applied.end_frame();

        CHECK(injected.snapshot() == applied.snapshot());
        CHECK(injected.stats().eventsApplied == applied.stats().eventsApplied);
        CHECK(injected.stats().eventsInjected == sequence.size());
        CHECK(applied.stats().eventsInjected == 0);
        CHECK(injected.snapshot().is_key_held(key::W) == false);
        CHECK(injected.snapshot().is_key_pressed(key::W));
        CHECK(injected.snapshot().is_key_pressed(key::A));
        CHECK(injected.snapshot().is_key_released(key::A));
        CHECK(injected.snapshot().mouseDelta == math::vec2(10.0f, -5.0f));
        CHECK(injected.snapshot().gamepad(1).is_pressed(gamepad_button::A));
    }

    TEST_CASE("input - the same injected sequence is deterministic across instances and runs") {
        const std::vector<input_event> sequence = scripted_sequence();
        input first = {};
        input second = {};
        for (u32 frame = 0; frame < 3; ++frame) {
            for (const input_event &value : sequence) {
                first.inject(value);
                second.inject(value);
            }
            first.end_frame();
            second.end_frame();
            CHECK(first.snapshot() == second.snapshot());
        }
        CHECK(first.snapshot().frameIndex == 3);
        CHECK(first.stats().ignoredTransitions == second.stats().ignoredTransitions);
    }

    TEST_CASE("input - apply and end_frame allocate nothing") {
        testing::memory_guard guard = {};
        input accumulator = {};
        const std::vector<input_event> sequence = scripted_sequence();
        const event focusLoss = make_focus(false);

        CONJURE_CHECK_NO_ALLOCATIONS(for (const input_event &value : sequence) { accumulator.inject(value); });
        CONJURE_CHECK_NO_ALLOCATIONS(accumulator.end_frame());
        CONJURE_CHECK_NO_ALLOCATIONS(accumulator.apply(focusLoss));
        CONJURE_CHECK_NO_ALLOCATIONS(accumulator.release_all());
        CONJURE_CHECK_NO_ALLOCATIONS(static_cast<void>(accumulator.snapshot().is_key_down(key::W)));
    }
}
