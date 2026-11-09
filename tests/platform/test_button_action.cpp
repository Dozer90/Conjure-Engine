#include "conjure/testing/doctest.h"

#include "platform_test_support.h"

#include "conjure/platform/button_action.h"

#include <optional>

using namespace conjure;
using namespace conjure::platform;

TEST_SUITE("platform/button_action") {

    TEST_CASE("button_action - values mirror GLFW_RELEASE, GLFW_PRESS and GLFW_REPEAT") {
        CHECK(static_cast<u8>(button_action::RELEASE) == 0);
        CHECK(static_cast<u8>(button_action::PRESS) == 1);
        CHECK(static_cast<u8>(button_action::REPEAT) == 2);
    }

    TEST_CASE("button_action - to_string names every value and nothing else") {
        CHECK(to_string(button_action::RELEASE) == "RELEASE");
        CHECK(to_string(button_action::PRESS) == "PRESS");
        CHECK(to_string(button_action::REPEAT) == "REPEAT");
        CHECK(to_string(static_cast<button_action>(3)).empty());
        CHECK(to_string(static_cast<button_action>(255)).empty());
    }

    TEST_CASE("button_action - parse_button_action is case-insensitive and strict") {
        CHECK(parse_button_action("PRESS") == button_action::PRESS);
        CHECK(parse_button_action("press") == button_action::PRESS);
        CHECK(parse_button_action("Release") == button_action::RELEASE);
        CHECK(parse_button_action("repeat") == button_action::REPEAT);
        CHECK_FALSE(parse_button_action("tap").has_value());
        CHECK_FALSE(parse_button_action("").has_value());
        CHECK_FALSE(parse_button_action(" press").has_value());
        CHECK_FALSE(parse_button_action("PRESSED").has_value());
    }

    TEST_CASE("button_action - to_string and parse_button_action round trip") {
        for (u8 code = 0; code <= 2; ++code) {
            CAPTURE(code);
            const button_action action = static_cast<button_action>(code);
            const std::optional<button_action> parsed = parse_button_action(to_string(action));
            REQUIRE(parsed.has_value());
            CHECK(*parsed == action);
        }
    }
}
