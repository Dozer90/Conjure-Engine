#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"
#include "conjure/testing/input_script.h"

#include "conjure/platform/event.h"
#include "conjure/platform/input.h"
#include "conjure/platform/input_state.h"

#include <optional>
#include <span>
#include <variant>

using namespace conjure;
using namespace conjure::platform;
using namespace conjure::testing;

namespace {
    [[nodiscard]] const key_event *as_key(const input_event &event) noexcept {
        return std::get_if<key_event>(&event);
    }
}

TEST_SUITE("testing/input_script") {

    TEST_CASE("input_script - events are delivered on their exact frame") {
        input_script script = {};
        script.key(3, key::W, button_action::PRESS);
        script.mouse_move(5, 10.0f, 20.0f);

        CHECK(script.events_for(0).empty());
        CHECK(script.events_for(2).empty());
        CHECK(script.events_for(4).empty());

        const std::span<const input_event> third = script.events_for(3);
        REQUIRE(third.size() == 1);
        const key_event *pressed = as_key(third[0]);
        REQUIRE(pressed != nullptr);
        CHECK(pressed->keyCode == key::W);
        CHECK(pressed->action == button_action::PRESS);

        const std::span<const input_event> fifth = script.events_for(5);
        REQUIRE(fifth.size() == 1);
        const mouse_move_event *moved = std::get_if<mouse_move_event>(&fifth[0]);
        REQUIRE(moved != nullptr);
        CHECK(moved->position.x == doctest::Approx(10.0f));
        CHECK(moved->position.y == doctest::Approx(20.0f));
    }

    TEST_CASE("input_script - out-of-order insertion is sorted by frame and keeps order within a frame") {
        input_script script = {};
        script.key(9, key::A, button_action::RELEASE);
        script.key(2, key::W, button_action::PRESS);
        script.key(2, key::W, button_action::RELEASE);
        script.key(0, key::SPACE, button_action::PRESS);

        const std::span<const u64> frames = script.frames();
        REQUIRE(frames.size() == 4);
        CHECK(frames[0] == 0);
        CHECK(frames[1] == 2);
        CHECK(frames[2] == 2);
        CHECK(frames[3] == 9);

        const std::span<const input_event> second = script.events_for(2);
        REQUIRE(second.size() == 2);
        CHECK(as_key(second[0])->action == button_action::PRESS);
        CHECK(as_key(second[1])->action == button_action::RELEASE);

        // A later insertion in the middle keeps the sorted view consistent.
        script.key(1, key::S, button_action::PRESS);
        CHECK(script.frames()[1] == 1);
        CHECK(script.events_for(1).size() == 1);
        CHECK(script.events().size() == 5);
    }

    TEST_CASE("input_script - key pressed then held through the platform input state machine") {
        input_script script = {};
        script.key(0, key::W, button_action::PRESS);
        script.key(2, key::W, button_action::RELEASE);

        input state = {};
        for (u64 frame = 0; frame < 4; ++frame) {
            CAPTURE(frame);
            for (const input_event &event : script.events_for(frame)) {
                state.inject(event);
            }
            state.end_frame();
            const input_state &snapshot = state.snapshot();
            CHECK(snapshot.is_key_pressed(key::W) == (frame == 0));
            CHECK(snapshot.is_key_held(key::W) == (frame == 1));
            CHECK(snapshot.is_key_down(key::W) == (frame < 2));
            CHECK(snapshot.is_key_released(key::W) == (frame == 2));
        }
    }

    TEST_CASE("input_script - tap_key schedules a press and a release") {
        input_script script = {};
        script.tap_key(4, key::SPACE);
        script.tap_key(10, key::A, 3);
        script.tap_key(20, key::S, 0);   // a zero hold still releases one frame later

        REQUIRE(script.event_count() == 6);
        CHECK(as_key(script.events_for(4)[0])->action == button_action::PRESS);
        CHECK(as_key(script.events_for(5)[0])->action == button_action::RELEASE);
        CHECK(as_key(script.events_for(10)[0])->action == button_action::PRESS);
        CHECK(as_key(script.events_for(13)[0])->action == button_action::RELEASE);
        CHECK(script.events_for(20).size() == 1);
        CHECK(script.events_for(21).size() == 1);
    }

    TEST_CASE("input_script - mouse, scroll, text and gamepad helpers build the right alternatives") {
        input_script script = {};
        script.mouse_button(1, mouse_button::RIGHT, button_action::PRESS);
        script.mouse_scroll(1, 0.0f, -2.0f);
        script.text(1, 0x41);
        script.gamepad_connection(1, 2, true);
        script.gamepad_axis(1, 2, gamepad_axis::LEFT_TRIGGER, 0.75f);
        script.gamepad_button(1, 3, gamepad_button::DPAD_LEFT, button_action::REPEAT);

        const std::span<const input_event> events = script.events_for(1);
        REQUIRE(events.size() == 6);

        const mouse_button_event *button = std::get_if<mouse_button_event>(&events[0]);
        REQUIRE(button != nullptr);
        CHECK(button->button == mouse_button::RIGHT);
        CHECK(button->action == button_action::PRESS);

        const mouse_scroll_event *scroll = std::get_if<mouse_scroll_event>(&events[1]);
        REQUIRE(scroll != nullptr);
        CHECK(scroll->offset.y == doctest::Approx(-2.0f));

        const text_event *text = std::get_if<text_event>(&events[2]);
        REQUIRE(text != nullptr);
        CHECK(text->codepoint == 0x41);

        const gamepad_connection_event *connection = std::get_if<gamepad_connection_event>(&events[3]);
        REQUIRE(connection != nullptr);
        CHECK(connection->gamepadIndex == 2);
        CHECK(connection->connected);

        const gamepad_axis_event *axis = std::get_if<gamepad_axis_event>(&events[4]);
        REQUIRE(axis != nullptr);
        CHECK(axis->gamepadIndex == 2);
        CHECK(axis->axis == gamepad_axis::LEFT_TRIGGER);
        CHECK(axis->value == doctest::Approx(0.75f));

        const gamepad_button_event *pad = std::get_if<gamepad_button_event>(&events[5]);
        REQUIRE(pad != nullptr);
        CHECK(pad->gamepadIndex == 3);
        CHECK(pad->button == gamepad_button::DPAD_LEFT);
        CHECK(pad->action == button_action::REPEAT);
    }

    TEST_CASE("input_script - the gamepad button tap presses on the frame and releases on the next") {
        input_script script = {};
        script.gamepad_button(7, 0, gamepad_button::A);

        REQUIRE(script.event_count() == 2);
        const gamepad_button_event *pressed = std::get_if<gamepad_button_event>(&script.events_for(7)[0]);
        const gamepad_button_event *released = std::get_if<gamepad_button_event>(&script.events_for(8)[0]);
        REQUIRE(pressed != nullptr);
        REQUIRE(released != nullptr);
        CHECK(pressed->action == button_action::PRESS);
        CHECK(released->action == button_action::RELEASE);
        CHECK(pressed->button == gamepad_button::A);
    }

    TEST_CASE("input_script - gamepad helpers reject indices out of range") {
        input_script script = {};

        CONJURE_CHECK_ASSERTS(script.gamepad_connection(0, MAX_GAMEPADS, true));
        CONJURE_CHECK_ASSERTS(script.gamepad_axis(0, MAX_GAMEPADS + 5, gamepad_axis::LEFT_X, 1.0f));
        CONJURE_CHECK_ASSERTS(script.gamepad_button(0, MAX_GAMEPADS, gamepad_button::B, button_action::PRESS));

        CHECK(script.empty());
        CHECK(script.event_count() == 0);
    }

    TEST_CASE("input_script - clear, last_frame and empty") {
        input_script script = {};
        CHECK(script.empty());
        CHECK_FALSE(script.last_frame().has_value());

        script.key(12, key::A, button_action::PRESS);
        script.key(4, key::A, button_action::RELEASE);
        CHECK_FALSE(script.empty());
        CHECK(script.last_frame() == std::optional<u64>(12));

        script.clear();
        CHECK(script.empty());
        CHECK(script.event_count() == 0);
        CHECK_FALSE(script.last_frame().has_value());
        CHECK(script.events_for(12).empty());
        CHECK(script.frames().empty());
    }

    TEST_CASE("input_script - push accepts any input alternative verbatim") {
        input_script script = {};
        cursor_enter_event entered = {};
        entered.entered = true;
        script.push(2, input_event(entered));

        const std::span<const input_event> events = script.events_for(2);
        REQUIRE(events.size() == 1);
        const cursor_enter_event *stored = std::get_if<cursor_enter_event>(&events[0]);
        REQUIRE(stored != nullptr);
        CHECK(stored->entered);
    }

    TEST_CASE("input_script - the readers never allocate, even right after scheduling") {
        input_script script = {};
        for (u64 frame = 0; frame < 64; ++frame) {
            script.key(63 - frame, key::W, button_action::PRESS);
        }
        script.key(7, key::A, button_action::RELEASE);   // the last scheduling call: no query in between

        // Honestly noexcept (08_testing.md 4.5): scheduling keeps the sorted view current, so no reader
        // has a sort or a reserve to run.
        static_assert(noexcept(script.events_for(0)));
        static_assert(noexcept(script.frames()));
        static_assert(noexcept(script.events()));
        static_assert(noexcept(script.last_frame()));
        CONJURE_CHECK_NO_ALLOCATIONS({
            for (u64 frame = 0; frame < 64; ++frame) {
                CONJURE_UNUSED(script.events_for(frame));
            }
            CONJURE_UNUSED(script.frames());
            CONJURE_UNUSED(script.events());
            CONJURE_UNUSED(script.last_frame());
        });
        CHECK(script.events_for(7).size() == 2);
        CHECK(as_key(script.events_for(7)[1])->action == button_action::RELEASE);
        CHECK(script.last_frame() == std::optional<u64>(63));
    }
}
