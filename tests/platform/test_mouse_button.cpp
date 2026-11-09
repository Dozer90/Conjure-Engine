#include "conjure/testing/doctest.h"

#include "platform_test_support.h"

#include "conjure/platform/mouse_button.h"

#include <optional>
#include <set>
#include <string_view>

using namespace conjure;
using namespace conjure::platform;

TEST_SUITE("platform/mouse_button") {

    TEST_CASE("mouse_button - numbering mirrors GLFW_MOUSE_BUTTON_*") {
        CHECK(static_cast<u8>(mouse_button::LEFT) == 0);
        CHECK(static_cast<u8>(mouse_button::RIGHT) == 1);
        CHECK(static_cast<u8>(mouse_button::MIDDLE) == 2);
        CHECK(static_cast<u8>(mouse_button::BUTTON_4) == 3);
        CHECK(static_cast<u8>(mouse_button::BUTTON_8) == 7);
        CHECK(static_cast<u32>(mouse_button::COUNT) == MOUSE_BUTTON_COUNT);
        CHECK(MOUSE_BUTTON_COUNT == 8);
    }

    TEST_CASE("mouse_button - is_valid_mouse_button accepts exactly the eight buttons") {
        for (u8 code = 0; code < 8; ++code) {
            CAPTURE(code);
            CHECK(is_valid_mouse_button(static_cast<mouse_button>(code)));
        }
        CHECK_FALSE(is_valid_mouse_button(mouse_button::COUNT));
        CHECK_FALSE(is_valid_mouse_button(static_cast<mouse_button>(255)));
    }

    TEST_CASE("mouse_button - every button has a unique name and unknown values have none") {
        std::set<std::string_view> names = {};
        for (u8 code = 0; code < 8; ++code) {
            CAPTURE(code);
            const std::string_view name = to_string(static_cast<mouse_button>(code));
            CHECK_FALSE(name.empty());
            CHECK(names.insert(name).second);
        }
        CHECK(to_string(mouse_button::LEFT) == "LEFT");
        CHECK(to_string(mouse_button::MIDDLE) == "MIDDLE");
        CHECK(to_string(mouse_button::BUTTON_5) == "BUTTON_5");
        CHECK(to_string(mouse_button::COUNT).empty());
    }

    TEST_CASE("mouse_button - parse_mouse_button round trips and is case-insensitive") {
        for (u8 code = 0; code < 8; ++code) {
            CAPTURE(code);
            const mouse_button button = static_cast<mouse_button>(code);
            const std::optional<mouse_button> parsed = parse_mouse_button(to_string(button));
            REQUIRE(parsed.has_value());
            CHECK(*parsed == button);
        }
        CHECK(parse_mouse_button("left") == mouse_button::LEFT);
        CHECK(parse_mouse_button("Button_4") == mouse_button::BUTTON_4);
        CHECK_FALSE(parse_mouse_button("BUTTON_9").has_value());
        CHECK_FALSE(parse_mouse_button("COUNT").has_value());
        CHECK_FALSE(parse_mouse_button("").has_value());
    }
}
