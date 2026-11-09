#include "conjure/testing/doctest.h"

#include "platform_test_support.h"

#include "conjure/platform/gamepad.h"

#include <optional>
#include <set>
#include <string_view>

using namespace conjure;
using namespace conjure::platform;

TEST_SUITE("platform/gamepad") {

    TEST_CASE("gamepad - counts and numbering mirror GLFW's gamepad layout") {
        CHECK(MAX_GAMEPADS == 4);
        CHECK(GAMEPAD_BUTTON_COUNT == 15);
        CHECK(GAMEPAD_AXIS_COUNT == 6);
        CHECK(static_cast<u32>(gamepad_button::COUNT) == GAMEPAD_BUTTON_COUNT);
        CHECK(static_cast<u32>(gamepad_axis::COUNT) == GAMEPAD_AXIS_COUNT);

        CHECK(static_cast<u8>(gamepad_button::A) == 0);
        CHECK(static_cast<u8>(gamepad_button::B) == 1);
        CHECK(static_cast<u8>(gamepad_button::X) == 2);
        CHECK(static_cast<u8>(gamepad_button::Y) == 3);
        CHECK(static_cast<u8>(gamepad_button::LEFT_BUMPER) == 4);
        CHECK(static_cast<u8>(gamepad_button::RIGHT_BUMPER) == 5);
        CHECK(static_cast<u8>(gamepad_button::BACK) == 6);
        CHECK(static_cast<u8>(gamepad_button::START) == 7);
        CHECK(static_cast<u8>(gamepad_button::GUIDE) == 8);
        CHECK(static_cast<u8>(gamepad_button::LEFT_THUMB) == 9);
        CHECK(static_cast<u8>(gamepad_button::RIGHT_THUMB) == 10);
        CHECK(static_cast<u8>(gamepad_button::DPAD_UP) == 11);
        CHECK(static_cast<u8>(gamepad_button::DPAD_RIGHT) == 12);
        CHECK(static_cast<u8>(gamepad_button::DPAD_DOWN) == 13);
        CHECK(static_cast<u8>(gamepad_button::DPAD_LEFT) == 14);

        CHECK(static_cast<u8>(gamepad_axis::LEFT_X) == 0);
        CHECK(static_cast<u8>(gamepad_axis::LEFT_Y) == 1);
        CHECK(static_cast<u8>(gamepad_axis::RIGHT_X) == 2);
        CHECK(static_cast<u8>(gamepad_axis::RIGHT_Y) == 3);
        CHECK(static_cast<u8>(gamepad_axis::LEFT_TRIGGER) == 4);
        CHECK(static_cast<u8>(gamepad_axis::RIGHT_TRIGGER) == 5);
    }

    TEST_CASE("gamepad - validity helpers accept exactly the enumerators") {
        for (u8 code = 0; code < GAMEPAD_BUTTON_COUNT; ++code) {
            CAPTURE(code);
            CHECK(is_valid_gamepad_button(static_cast<gamepad_button>(code)));
        }
        CHECK_FALSE(is_valid_gamepad_button(gamepad_button::COUNT));
        CHECK_FALSE(is_valid_gamepad_button(static_cast<gamepad_button>(200)));

        for (u8 code = 0; code < GAMEPAD_AXIS_COUNT; ++code) {
            CAPTURE(code);
            CHECK(is_valid_gamepad_axis(static_cast<gamepad_axis>(code)));
        }
        CHECK_FALSE(is_valid_gamepad_axis(gamepad_axis::COUNT));
        CHECK_FALSE(is_valid_gamepad_axis(static_cast<gamepad_axis>(200)));
    }

    TEST_CASE("gamepad_button - names are unique and parse back in any case") {
        std::set<std::string_view> names = {};
        for (u8 code = 0; code < GAMEPAD_BUTTON_COUNT; ++code) {
            CAPTURE(code);
            const gamepad_button button = static_cast<gamepad_button>(code);
            const std::string_view name = to_string(button);
            CHECK_FALSE(name.empty());
            CHECK(names.insert(name).second);
            const std::optional<gamepad_button> parsed = parse_gamepad_button(name);
            REQUIRE(parsed.has_value());
            CHECK(*parsed == button);
        }
        CHECK(to_string(gamepad_button::LEFT_BUMPER) == "LEFT_BUMPER");
        CHECK(to_string(gamepad_button::DPAD_LEFT) == "DPAD_LEFT");
        CHECK(to_string(gamepad_button::COUNT).empty());
        CHECK(parse_gamepad_button("dpad_up") == gamepad_button::DPAD_UP);
        CHECK(parse_gamepad_button("a") == gamepad_button::A);
        CHECK_FALSE(parse_gamepad_button("COUNT").has_value());
        CHECK_FALSE(parse_gamepad_button("LEFT_X").has_value());
        CHECK_FALSE(parse_gamepad_button("").has_value());
    }

    TEST_CASE("gamepad_axis - names are unique and parse back in any case") {
        std::set<std::string_view> names = {};
        for (u8 code = 0; code < GAMEPAD_AXIS_COUNT; ++code) {
            CAPTURE(code);
            const gamepad_axis axis = static_cast<gamepad_axis>(code);
            const std::string_view name = to_string(axis);
            CHECK_FALSE(name.empty());
            CHECK(names.insert(name).second);
            const std::optional<gamepad_axis> parsed = parse_gamepad_axis(name);
            REQUIRE(parsed.has_value());
            CHECK(*parsed == axis);
        }
        CHECK(to_string(gamepad_axis::RIGHT_TRIGGER) == "RIGHT_TRIGGER");
        CHECK(to_string(gamepad_axis::COUNT).empty());
        CHECK(parse_gamepad_axis("left_y") == gamepad_axis::LEFT_Y);
        CHECK_FALSE(parse_gamepad_axis("A").has_value());
        CHECK_FALSE(parse_gamepad_axis("").has_value());
    }
}
