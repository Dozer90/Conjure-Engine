#include "conjure/testing/doctest.h"

#include "conjure/editor/protocol/access_token.h"

#include "conjure/core/types.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

// access_token.h: what a token grants, how one is generated, where it is written, and the comparison and the
// search that do not stop at the first difference.

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/protocol/access_token") {

    TEST_CASE("access_level - NONE is the lowest level and the default of a zeroed value") {
        CHECK(static_cast<u8>(access_level::NONE) == 0);
        CHECK(static_cast<u8>(access_level::FULL_CONTROL) > static_cast<u8>(access_level::NONE));
        const access_level zeroed = {};
        CHECK(zeroed == access_level::NONE);
    }

    TEST_CASE("access_level - INSPECT lies between NONE and FULL_CONTROL, so the levels compare in order") {
        CHECK(access_level::NONE < access_level::INSPECT);
        CHECK(access_level::INSPECT < access_level::FULL_CONTROL);
        CHECK_FALSE(access_level::FULL_CONTROL < access_level::INSPECT);
        CHECK(access_level::INSPECT >= access_level::INSPECT);
    }

    TEST_CASE("access_level - to_string spells each level as session.hello and the audit log write it") {
        CHECK(to_string(access_level::NONE) == "none");
        CHECK(to_string(access_level::INSPECT) == "inspect");
        CHECK(to_string(access_level::FULL_CONTROL) == "full_control");
        // A value that is no enumerator grants nothing.
        CHECK(to_string(static_cast<access_level>(200)) == "none");
    }

    TEST_CASE("generate_access_token - 64 lower-case hexadecimal digits") {
        CHECK(ACCESS_TOKEN_LENGTH == 64);
        const std::string token = generate_access_token();
        CHECK(token.size() == ACCESS_TOKEN_LENGTH);
        const bool allHex = std::ranges::all_of(token, [](char character) {
            return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
        });
        CHECK(allHex);
    }

    TEST_CASE("generate_access_token - no two tokens are alike and none is a run of one digit") {
        std::vector<std::string> tokens = {};
        for (int index = 0; index < 64; ++index) {
            CAPTURE(index);
            std::string token = generate_access_token();
            // A generator that returned a constant would produce exactly this.
            CHECK(token.find_first_not_of(token.front()) != std::string::npos);
            CHECK(std::ranges::find(tokens, token) == tokens.end());
            tokens.push_back(std::move(token));
        }
    }

    TEST_CASE("constant_time_equals - equal texts match and every kind of difference does not") {
        CHECK(constant_time_equals("secret", "secret"));
        CHECK(constant_time_equals("", ""));
        SUBCASE("a difference in the first, a middle and the last character") {
            CHECK_FALSE(constant_time_equals("Xecret", "secret"));
            CHECK_FALSE(constant_time_equals("secXet", "secret"));
            CHECK_FALSE(constant_time_equals("secreX", "secret"));
        }
        SUBCASE("a prefix, an extension and a repetition of the expected text") {
            CHECK_FALSE(constant_time_equals("secre", "secret"));
            CHECK_FALSE(constant_time_equals("secrets", "secret"));
            // The expected text is read cyclically; a guess that repeats it must still fail on its length.
            CHECK_FALSE(constant_time_equals("secretsecret", "secret"));
            CHECK_FALSE(constant_time_equals("aaaa", "a"));
        }
        SUBCASE("an empty text on either side") {
            CHECK_FALSE(constant_time_equals("", "secret"));
            CHECK_FALSE(constant_time_equals("secret", ""));
        }
        SUBCASE("bytes above 127 and embedded zero bytes are compared like any other") {
            const std::string_view high = "\xC3\xA9\xFF";
            CHECK(constant_time_equals(high, high));
            CHECK_FALSE(constant_time_equals("\xC3\xA9\x7F", high));
            const std::string_view withZero("a\0b", 3);
            CHECK(constant_time_equals(withZero, withZero));
            CHECK_FALSE(constant_time_equals(std::string_view("a\0c", 3), withZero));
            CHECK_FALSE(constant_time_equals("a", withZero));
        }
    }

    TEST_CASE("constant_time_equals - a generated token equals itself and no other generated token") {
        const std::string first = generate_access_token();
        const std::string second = generate_access_token();
        CHECK(constant_time_equals(first, first));
        CHECK_FALSE(constant_time_equals(first, second));
        CHECK_FALSE(constant_time_equals(second, first));
    }

    TEST_CASE("constant_time_starts_at - true only where the whole token lies at the offset") {
        const std::string_view text = "say tok-7f3a9c twice: tok-7f3a9c";
        const std::string_view token = "tok-7f3a9c";
        CHECK(constant_time_starts_at(text, 4, token));
        CHECK(constant_time_starts_at(text, text.size() - token.size(), token));
        CHECK(constant_time_starts_at(token, 0, token));
        SUBCASE("one byte to either side of it") {
            CHECK_FALSE(constant_time_starts_at(text, 3, token));
            CHECK_FALSE(constant_time_starts_at(text, 5, token));
        }
        SUBCASE("where the text ends before the token does, at its end and beyond it") {
            CHECK_FALSE(constant_time_starts_at(text, text.size() - token.size() + 1, token));
            CHECK_FALSE(constant_time_starts_at(text, text.size(), token));
            CHECK_FALSE(constant_time_starts_at(text, text.size() + 1, token));
            CHECK_FALSE(constant_time_starts_at("tok-7f3a9", 0, token));
            CHECK_FALSE(constant_time_starts_at("", 0, token));
        }
        SUBCASE("an empty token starts nowhere") {
            CHECK_FALSE(constant_time_starts_at(text, 0, ""));
            CHECK_FALSE(constant_time_starts_at("", 0, ""));
        }
    }

    TEST_CASE("regression - code sweep F740 constant_time_contains finds a token wherever a substring search "
              "would, and nothing else") {
        const std::string_view token = "tok-7f3a9c";
        CHECK(constant_time_contains("tok-7f3a9c", token));
        CHECK(constant_time_contains("tok-7f3a9c and more", token));
        CHECK(constant_time_contains("at the end: tok-7f3a9c", token));
        CHECK(constant_time_contains("tok-7f3a9c in the middle tok-7f3a9c and twice", token));
        SUBCASE("a text that holds all but one byte of the token, in every position") {
            for (usize missing = 0; missing < token.size(); ++missing) {
                CAPTURE(missing);
                std::string almost = std::string(token);
                almost[missing] = '_';
                CHECK_FALSE(constant_time_contains("before " + almost + " after", token));
            }
        }
        SUBCASE("a prefix repeated, the case every early-exit search spends the most time on") {
            std::string repeated = {};
            for (int index = 0; index < 50; ++index) {
                repeated += token.substr(0, token.size() - 1);
            }
            CHECK_FALSE(constant_time_contains(repeated, token));
            CHECK(constant_time_contains(repeated + "c", token));
        }
        SUBCASE("a text shorter than the token, an empty text and an empty token") {
            CHECK_FALSE(constant_time_contains("tok-7f3a9", token));
            CHECK_FALSE(constant_time_contains("", token));
            CHECK_FALSE(constant_time_contains("anything", ""));
            CHECK_FALSE(constant_time_contains("", ""));
        }
        SUBCASE("it agrees with std::string_view::find on every window of a text around a generated token") {
            const std::string generated = generate_access_token();
            const std::string text = "xxx" + generated + "yyy";
            for (usize start = 0; start < 6; ++start) {
                for (usize cut = 0; cut < 6; ++cut) {
                    CAPTURE(start);
                    CAPTURE(cut);
                    const std::string_view window = std::string_view(text).substr(start, text.size() - start - cut);
                    const bool found = window.find(generated) != std::string_view::npos;
                    CHECK(constant_time_contains(window, generated) == found);
                }
            }
        }
        SUBCASE("bytes above 127 and embedded zero bytes") {
            const std::string_view odd("\xFF\0\xC3", 3);
            CHECK(constant_time_contains(std::string("ab") + std::string(odd) + "cd", odd));
            CHECK_FALSE(constant_time_contains(std::string_view("ab\xFF\0\xC2" "cd", 7), odd));
        }
    }

    TEST_CASE("token_file_name - the full-control token is ai_token and NONE has no file") {
        CHECK(token_file_name(access_level::FULL_CONTROL) == "ai_token");
        CHECK(token_file_name(access_level::NONE).empty());
    }

    TEST_CASE("token_file_name - the inspect-only token has a file of its own") {
        CHECK(token_file_name(access_level::INSPECT) == "ai_token_inspect");
        CHECK(token_file_name(access_level::INSPECT) != token_file_name(access_level::FULL_CONTROL));
        CHECK(token_file_name(static_cast<access_level>(200)).empty());
    }
}
