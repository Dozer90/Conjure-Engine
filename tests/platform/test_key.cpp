#include "conjure/testing/doctest.h"

#include "platform_test_support.h"

#include "conjure/platform/key.h"

#include <optional>
#include <set>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::platform;

TEST_SUITE("platform/key") {

    TEST_CASE("key - codes follow GLFW's numbering") {
        CHECK(static_cast<u16>(key::UNKNOWN) == 0);
        CHECK(static_cast<u16>(key::SPACE) == 32);
        CHECK(static_cast<u16>(key::APOSTROPHE) == 39);
        CHECK(static_cast<u16>(key::COMMA) == 44);
        CHECK(static_cast<u16>(key::SLASH) == 47);
        CHECK(static_cast<u16>(key::NUM_0) == 48);
        CHECK(static_cast<u16>(key::NUM_9) == 57);
        CHECK(static_cast<u16>(key::SEMICOLON) == 59);
        CHECK(static_cast<u16>(key::EQUAL) == 61);
        CHECK(static_cast<u16>(key::A) == 65);
        CHECK(static_cast<u16>(key::Z) == 90);
        CHECK(static_cast<u16>(key::LEFT_BRACKET) == 91);
        CHECK(static_cast<u16>(key::RIGHT_BRACKET) == 93);
        CHECK(static_cast<u16>(key::GRAVE_ACCENT) == 96);
        CHECK(static_cast<u16>(key::WORLD_1) == 161);
        CHECK(static_cast<u16>(key::WORLD_2) == 162);
        CHECK(static_cast<u16>(key::ESCAPE) == 256);
        CHECK(static_cast<u16>(key::ENTER) == 257);
        CHECK(static_cast<u16>(key::DEL) == 261);
        CHECK(static_cast<u16>(key::END) == 269);
        CHECK(static_cast<u16>(key::CAPS_LOCK) == 280);
        CHECK(static_cast<u16>(key::PAUSE) == 284);
        CHECK(static_cast<u16>(key::F1) == 290);
        CHECK(static_cast<u16>(key::F25) == 314);
        CHECK(static_cast<u16>(key::KP_0) == 320);
        CHECK(static_cast<u16>(key::KP_9) == 329);
        CHECK(static_cast<u16>(key::KP_EQUAL) == 336);
        CHECK(static_cast<u16>(key::LEFT_SHIFT) == 340);
        CHECK(static_cast<u16>(key::RIGHT_SUPER) == 347);
        CHECK(static_cast<u16>(key::MENU) == 348);
        CHECK(key::LAST == key::MENU);
        CHECK(KEY_COUNT == 512);
    }

    TEST_CASE("key - is_valid_key accepts [1, 348] only") {
        CHECK_FALSE(is_valid_key(0u));
        CHECK(is_valid_key(1u));
        CHECK(is_valid_key(32u));
        CHECK(is_valid_key(348u));
        CHECK_FALSE(is_valid_key(349u));
        CHECK_FALSE(is_valid_key(511u));
        CHECK_FALSE(is_valid_key(512u));
        CHECK_FALSE(is_valid_key(0xFFFFFFFFu));
        CHECK_FALSE(is_valid_key(key::UNKNOWN));
        CHECK(is_valid_key(key::SPACE));
        CHECK(is_valid_key(key::LAST));
        CHECK_FALSE(is_valid_key(static_cast<key>(349)));
    }

    TEST_CASE("key - to_string and parse_key round trip for every key") {
        std::set<std::string_view> names = {};
        u32 named = 0;
        for (u32 code = 1; code <= static_cast<u32>(key::LAST); ++code) {
            const key value = static_cast<key>(code);
            const std::string_view name = to_string(value);
            if (name.empty()) {
                CHECK_FALSE(parse_key(std::to_string(code)).has_value());
                continue;
            }
            CAPTURE(code);
            CAPTURE(name);
            ++named;
            CHECK(names.insert(name).second);
            const std::optional<key> parsed = parse_key(name);
            REQUIRE(parsed.has_value());
            CHECK(*parsed == value);
        }
        CHECK(named == 120);
    }

    TEST_CASE("key - to_string names UNKNOWN and nothing outside the enumerators") {
        CHECK(to_string(key::UNKNOWN) == "UNKNOWN");
        CHECK(to_string(key::SPACE) == "SPACE");
        CHECK(to_string(key::NUM_0) == "NUM_0");
        CHECK(to_string(key::KP_ENTER) == "KP_ENTER");
        CHECK(to_string(key::LAST) == "MENU");
        CHECK(to_string(static_cast<key>(1)).empty());
        CHECK(to_string(static_cast<key>(349)).empty());
        CHECK(to_string(static_cast<key>(511)).empty());
        CHECK(to_string(static_cast<key>(512)).empty());
        CHECK(to_string(static_cast<key>(0xFFFF)).empty());
    }

    TEST_CASE("key - DEL is the spelling of the delete key") {
        CHECK(to_string(key::DEL) == "DEL");
        CHECK(parse_key("DEL") == key::DEL);
        CHECK(parse_key("del") == key::DEL);
        CHECK_FALSE(parse_key("DELETE").has_value());
    }

    TEST_CASE("key - parse_key is case-insensitive and rejects aliases and junk") {
        CHECK(parse_key("space") == key::SPACE);
        CHECK(parse_key("Space") == key::SPACE);
        CHECK(parse_key("f12") == key::F12);
        CHECK(parse_key("kp_0") == key::KP_0);
        CHECK(parse_key("left_shift") == key::LEFT_SHIFT);
        CHECK(parse_key("a") == key::A);
        CHECK_FALSE(parse_key("LAST").has_value());
        CHECK_FALSE(parse_key("UNKNOWN").has_value());
        CHECK_FALSE(parse_key("").has_value());
        CHECK_FALSE(parse_key(" SPACE").has_value());
        CHECK_FALSE(parse_key("SPACE ").has_value());
        CHECK_FALSE(parse_key("F26").has_value());
        CHECK_FALSE(parse_key("32").has_value());
    }

    TEST_CASE("key_modifier_flags - values mirror GLFW_MOD_* and the flag operators work") {
        CHECK(static_cast<u8>(key_modifier_flags::NONE) == 0);
        CHECK(static_cast<u8>(key_modifier_flags::SHIFT) == 0x01);
        CHECK(static_cast<u8>(key_modifier_flags::CONTROL) == 0x02);
        CHECK(static_cast<u8>(key_modifier_flags::ALT) == 0x04);
        CHECK(static_cast<u8>(key_modifier_flags::SUPER) == 0x08);
        CHECK(static_cast<u8>(key_modifier_flags::CAPS_LOCK) == 0x10);
        CHECK(static_cast<u8>(key_modifier_flags::NUM_LOCK) == 0x20);

        key_modifier_flags flags = key_modifier_flags::SHIFT | key_modifier_flags::ALT;
        CHECK(has_flag(flags, key_modifier_flags::SHIFT));
        CHECK(has_flag(flags, key_modifier_flags::ALT));
        CHECK_FALSE(has_flag(flags, key_modifier_flags::CONTROL));
        CHECK((flags & key_modifier_flags::SHIFT) == key_modifier_flags::SHIFT);
        flags |= key_modifier_flags::CONTROL;
        CHECK(has_flag(flags, key_modifier_flags::CONTROL));
        flags &= ~key_modifier_flags::SHIFT;
        CHECK_FALSE(has_flag(flags, key_modifier_flags::SHIFT));
        flags ^= key_modifier_flags::ALT;
        CHECK(flags == key_modifier_flags::CONTROL);
        CHECK(has_any_flag(flags, key_modifier_flags::CONTROL | key_modifier_flags::SUPER));
        CHECK_FALSE(has_any_flag(key_modifier_flags::NONE, key_modifier_flags::SHIFT));
    }

    TEST_CASE("key_modifier_flags - to_string joins the set flags and names the empty set NONE") {
        CHECK(to_string(key_modifier_flags::NONE) == "NONE");
        CHECK(to_string(key_modifier_flags::SHIFT) == "SHIFT");
        CHECK(to_string(key_modifier_flags::SHIFT | key_modifier_flags::CONTROL) == "SHIFT|CONTROL");
        CHECK(to_string(key_modifier_flags::NUM_LOCK | key_modifier_flags::ALT) == "ALT|NUM_LOCK");
        CHECK(to_string(static_cast<key_modifier_flags>(0xC0)) == "NONE");
    }

    TEST_CASE("key_modifier_flags - parse_key_modifier_flags accepts pipe-separated names") {
        CHECK(parse_key_modifier_flags("SHIFT|CONTROL") == (key_modifier_flags::SHIFT | key_modifier_flags::CONTROL));
        CHECK(parse_key_modifier_flags("shift | alt") == (key_modifier_flags::SHIFT | key_modifier_flags::ALT));
        CHECK(parse_key_modifier_flags("NONE") == key_modifier_flags::NONE);
        CHECK(parse_key_modifier_flags("") == key_modifier_flags::NONE);
        CHECK(parse_key_modifier_flags("none|super") == key_modifier_flags::SUPER);
        CHECK(parse_key_modifier_flags("Caps_Lock") == key_modifier_flags::CAPS_LOCK);
        CHECK_FALSE(parse_key_modifier_flags("shift|bogus").has_value());
        CHECK_FALSE(parse_key_modifier_flags("ctrl").has_value());
        CHECK_FALSE(parse_key_modifier_flags("shift,alt").has_value());

        const std::optional<key_modifier_flags> all =
          parse_key_modifier_flags("SHIFT|CONTROL|ALT|SUPER|CAPS_LOCK|NUM_LOCK");
        REQUIRE(all.has_value());
        CHECK(static_cast<u8>(*all) == 0x3F);
        CHECK(parse_key_modifier_flags(to_string(*all)) == *all);
    }
}
