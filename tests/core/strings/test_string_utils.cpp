#include "conjure/testing/doctest.h"

#include "conjure/core/string_utils.h"

#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core;

namespace {
    using pieces = std::vector<std::string_view>;

    // UTF-8 / UTF-16 samples spelled as escapes so the test does not depend on the source encoding.
    // e-acute, euro sign and grinning face, as UTF-8 and UTF-16.
    constexpr std::string_view UTF8_MIXED = "h\xC3\xA9llo \xE2\x82\xAC \xF0\x9F\x98\x80";
    constexpr std::u16string_view UTF16_MIXED = u"h\u00E9llo \u20AC \xD83D\xDE00";
    constexpr std::u32string_view UTF32_MIXED = U"h\u00E9llo \u20AC \U0001F600";
}

TEST_SUITE("core/string_utils") {

    TEST_CASE("string_utils - character classification is ASCII only") {
        static_assert(str::is_space(' ') && str::is_space('\t') && str::is_space('\n') && str::is_space('\r'));
        static_assert(str::is_space('\f') && str::is_space('\v'));
        static_assert(!str::is_space('a') && !str::is_space('\0'));
        static_assert(str::is_digit('0') && str::is_digit('9') && !str::is_digit('a'));
        static_assert(str::is_alpha('a') && str::is_alpha('Z') && !str::is_alpha('1') && !str::is_alpha('_'));
        static_assert(str::is_alnum('x') && str::is_alnum('5') && !str::is_alnum('-'));
        CHECK_FALSE(str::is_alpha('\xC3'));
        CHECK_FALSE(str::is_space('\xA0'));
    }

    TEST_CASE("string_utils - to_lower and to_upper convert ASCII letters only") {
        static_assert(str::to_lower('A') == 'a');
        static_assert(str::to_upper('z') == 'Z');
        static_assert(str::to_lower('1') == '1');
        CHECK(str::to_lower("Hello World 123 \xC3\x89") == "hello world 123 \xC3\x89");
        CHECK(str::to_upper("Hello World 123 \xC3\xA9") == "HELLO WORLD 123 \xC3\xA9");
        CHECK(str::to_lower("") == "");
        CHECK(str::to_upper("") == "");

        std::string inPlace = "MiXeD";
        str::to_lower_in_place(inPlace);
        CHECK(inPlace == "mixed");
        str::to_upper_in_place(inPlace);
        CHECK(inPlace == "MIXED");
    }

    TEST_CASE("string_utils - trim removes surrounding whitespace and returns a view into the input") {
        const std::string text = "  \t hello world \r\n";
        const std::string_view trimmed = str::trim(text);
        CHECK(trimmed == "hello world");
        CHECK(trimmed.data() == text.data() + 4);

        CHECK(str::trim_left(text) == "hello world \r\n");
        CHECK(str::trim_right(text) == "  \t hello world");
        CHECK(str::trim("") == "");
        CHECK(str::trim("   ") == "");
        CHECK(str::trim_left("   ") == "");
        CHECK(str::trim_right("   ") == "");
        CHECK(str::trim("x") == "x");
        CHECK(str::trim("no-trim") == "no-trim");
        static_assert(str::trim("  a  ") == "a");
    }

    TEST_CASE("regression - sweep-2026-11-12 F216: trimming everything leaves an empty view into the input") {
        const std::string blank = "  \t ";

        const std::string_view trimmed = str::trim(blank);
        CHECK(trimmed.empty());
        CHECK(trimmed.data() == blank.data() + blank.size());

        const std::string_view left = str::trim_left(blank);
        CHECK(left.empty());
        CHECK(left.data() == blank.data() + blank.size());

        const std::string_view right = str::trim_right(blank);
        CHECK(right.empty());
        CHECK(right.data() == blank.data());

        const std::string dashes = "---";
        CHECK(str::trim(dashes, "-").data() == dashes.data() + dashes.size());

        static_assert(str::trim("   ").empty());
        static_assert(str::trim_left("   ").empty());
        static_assert(str::trim_right("   ").empty());
    }

    TEST_CASE("string_utils - trim accepts a custom character set") {
        CHECK(str::trim("--x--", "-") == "x");
        CHECK(str::trim_left("//path/", "/") == "path/");
        CHECK(str::trim_right("//path/", "/") == "//path");
        CHECK(str::trim("abc", "") == "abc");
        CHECK(str::trim("\"quoted\"", "\"") == "quoted");
    }

    TEST_CASE("string_utils - starts_with and ends_with are byte-wise") {
        static_assert(str::starts_with("conjure.core", "conjure"));
        static_assert(!str::starts_with("conjure", "conjure.core"));
        static_assert(str::starts_with("anything", ""));
        static_assert(str::starts_with("", ""));
        static_assert(!str::starts_with("", "a"));
        static_assert(str::ends_with("scene.json", ".json"));
        static_assert(!str::ends_with("scene.json", ".JSON"));
        static_assert(str::ends_with("x", ""));
        static_assert(!str::ends_with("", "x"));
        CHECK(str::starts_with("abc", "abc"));
        CHECK(str::ends_with("abc", "abc"));
    }

    TEST_CASE("string_utils - case-insensitive comparisons ignore ASCII case only") {
        static_assert(str::iequals("Hello", "hELLO"));
        static_assert(!str::iequals("Hello", "Hello!"));
        static_assert(str::iequals("", ""));
        static_assert(!str::iequals("a", ""));
        CHECK(str::iequals("\xC3\xA9", "\xC3\xA9"));
        CHECK_FALSE(str::iequals("\xC3\xA9", "\xC3\x89"));

        CHECK(str::istarts_with("CONJURE.core", "conjure"));
        CHECK_FALSE(str::istarts_with("con", "conjure"));
        CHECK(str::istarts_with("x", ""));
        CHECK(str::iends_with("scene.JSON", ".json"));
        CHECK_FALSE(str::iends_with("json", ".json"));
        CHECK(str::iends_with("x", ""));
    }

    TEST_CASE("string_utils - contains finds substrings and characters") {
        static_assert(str::contains("hello world", "o w"));
        static_assert(!str::contains("hello", "xyz"));
        static_assert(str::contains("hello", ""));
        static_assert(str::contains("", ""));
        static_assert(str::contains("hello", 'e'));
        static_assert(!str::contains("hello", 'z'));
        static_assert(!str::contains("", 'a'));
        CHECK(str::contains("a.b", '.'));
    }

    TEST_CASE("string_utils - split by character keeps or drops empty pieces") {
        const std::string text = "a,b,,c,";
        const pieces kept = str::split(text, ',');
        CHECK(kept == pieces{"a", "b", "", "c", ""});
        CHECK(kept[0].data() == text.data());

        CHECK(str::split(text, ',', false) == pieces{"a", "b", "c"});
        CHECK(str::split("", ',') == pieces{""});
        CHECK(str::split("", ',', false) == pieces{});
        CHECK(str::split(",", ',') == pieces{"", ""});
        CHECK(str::split(",", ',', false) == pieces{});
        CHECK(str::split("single", ',') == pieces{"single"});
        CHECK(str::split("a/b/c", '/') == pieces{"a", "b", "c"});
    }

    TEST_CASE("string_utils - split by string handles multi-character and empty delimiters") {
        CHECK(str::split("a::b::c", "::") == pieces{"a", "b", "c"});
        CHECK(str::split("a::b::::c", "::") == pieces{"a", "b", "", "c"});
        CHECK(str::split("a::b::::c", "::", false) == pieces{"a", "b", "c"});
        CHECK(str::split("::", "::") == pieces{"", ""});
        CHECK(str::split("abc", "") == pieces{"abc"});
        CHECK(str::split("", "") == pieces{""});
        CHECK(str::split("abc", "abcd") == pieces{"abc"});
        CHECK(str::split("a-->b", "-->") == pieces{"a", "b"});
    }

    TEST_CASE("string_utils - split_lines handles LF, CRLF, lone CR and trailing terminators") {
        CHECK(str::split_lines("a\nb\nc") == pieces{"a", "b", "c"});
        CHECK(str::split_lines("a\r\nb\r\nc\r\n") == pieces{"a", "b", "c"});
        CHECK(str::split_lines("a\rb") == pieces{"a", "b"});
        CHECK(str::split_lines("a\n\nb") == pieces{"a", "", "b"});
        CHECK(str::split_lines("a\n") == pieces{"a"});
        CHECK(str::split_lines("\n") == pieces{""});
        CHECK(str::split_lines("") == pieces{});
        CHECK(str::split_lines("no newline") == pieces{"no newline"});
        CHECK(str::split_lines("\r\n\r\n") == pieces{"", ""});
    }

    TEST_CASE("string_utils - join concatenates any range of string-like pieces") {
        const std::vector<std::string> strings = {"a", "b", "c"};
        CHECK(str::join(strings, ", ") == "a, b, c");

        const pieces views = {"x", "", "z"};
        CHECK(str::join(views, "/") == "x//z");
        CHECK(str::join(std::span<const std::string_view>(views), "") == "xz");

        const std::array<const char *, 2> literals = {"one", "two"};
        CHECK(str::join(literals, " and ") == "one and two");

        CHECK(str::join(pieces{}, ", ") == "");
        CHECK(str::join(pieces{"only"}, ", ") == "only");
        CHECK(str::join(str::split("a,b,c", ','), ";") == "a;b;c");
    }

    TEST_CASE("string_utils - replace_all replaces every non-overlapping occurrence") {
        CHECK(str::replace_all("a.b.c", ".", "::") == "a::b::c");
        CHECK(str::replace_all("aaaa", "aa", "b") == "bb");
        CHECK(str::replace_all("aaa", "aa", "b") == "ba");
        CHECK(str::replace_all("hello", "l", "") == "heo");
        CHECK(str::replace_all("hello", "xyz", "!") == "hello");
        CHECK(str::replace_all("", "a", "b") == "");
        CHECK(str::replace_all("abc", "", "-") == "abc");
        CHECK(str::replace_all("abc", "abc", "abcabc") == "abcabc");
        CHECK(str::replace_all("x", "x", "x") == "x");
    }

    TEST_CASE("string_utils - parse_int accepts whole decimal integers with an optional sign") {
        CHECK(str::parse_int("0") == 0);
        CHECK(str::parse_int("42") == 42);
        CHECK(str::parse_int("-42") == -42);
        CHECK(str::parse_int("+42") == 42);
        CHECK(str::parse_int("007") == 7);
        CHECK(str::parse_int("9223372036854775807") == std::numeric_limits<i64>::max());
        CHECK(str::parse_int("-9223372036854775808") == std::numeric_limits<i64>::min());
    }

    TEST_CASE("string_utils - parse_int rejects malformed, partial and out-of-range input") {
        CHECK_FALSE(str::parse_int("").has_value());
        CHECK_FALSE(str::parse_int("-").has_value());
        CHECK_FALSE(str::parse_int("+").has_value());
        CHECK_FALSE(str::parse_int("+-1").has_value());
        CHECK_FALSE(str::parse_int("--1").has_value());
        CHECK_FALSE(str::parse_int(" 42").has_value());
        CHECK_FALSE(str::parse_int("42 ").has_value());
        CHECK_FALSE(str::parse_int("42abc").has_value());
        CHECK_FALSE(str::parse_int("4.2").has_value());
        CHECK_FALSE(str::parse_int("1e3").has_value());
        CHECK_FALSE(str::parse_int("9223372036854775808").has_value());
        CHECK_FALSE(str::parse_int("-9223372036854775809").has_value());
        CHECK_FALSE(str::parse_int("99999999999999999999").has_value());
        CHECK_FALSE(str::parse_int("12", 1).has_value());
        CHECK_FALSE(str::parse_int("12", 37).has_value());
    }

    TEST_CASE("string_utils - parse_int supports other bases including a 0x prefix for hexadecimal") {
        CHECK(str::parse_int("ff", 16) == 255);
        CHECK(str::parse_int("FF", 16) == 255);
        CHECK(str::parse_int("0xff", 16) == 255);
        CHECK(str::parse_int("0XFF", 16) == 255);
        CHECK(str::parse_int("-0x10", 16) == -16);
        CHECK(str::parse_int("1010", 2) == 10);
        CHECK(str::parse_int("777", 8) == 511);
        CHECK(str::parse_int("zz", 36) == 35 * 36 + 35);
        CHECK_FALSE(str::parse_int("0x", 16).has_value());
        CHECK_FALSE(str::parse_int("0xff", 10).has_value());
        CHECK_FALSE(str::parse_int("8", 8).has_value());
        CHECK_FALSE(str::parse_int("2", 2).has_value());
        CHECK_FALSE(str::parse_int("g", 16).has_value());
    }

    TEST_CASE("string_utils - parse_uint rejects negative numbers and covers the full u64 range") {
        CHECK(str::parse_uint("0") == 0u);
        CHECK(str::parse_uint("18446744073709551615") == std::numeric_limits<u64>::max());
        CHECK(str::parse_uint("+7") == 7u);
        CHECK(str::parse_uint("0xFFFFFFFFFFFFFFFF", 16) == std::numeric_limits<u64>::max());
        CHECK_FALSE(str::parse_uint("18446744073709551616").has_value());
        CHECK_FALSE(str::parse_uint("-1").has_value());
        CHECK_FALSE(str::parse_uint("-0").has_value());
        CHECK_FALSE(str::parse_uint("").has_value());
    }

    TEST_CASE("string_utils - parse_integer range-checks narrower types") {
        CHECK(str::parse_integer<i8>("127") == i8{127});
        CHECK(str::parse_integer<i8>("-128") == i8{-128});
        CHECK_FALSE(str::parse_integer<i8>("128").has_value());
        CHECK_FALSE(str::parse_integer<i8>("-129").has_value());
        CHECK(str::parse_integer<u8>("255") == u8{255});
        CHECK_FALSE(str::parse_integer<u8>("256").has_value());
        CHECK_FALSE(str::parse_integer<u8>("-1").has_value());
        CHECK(str::parse_integer<u16>("65535") == u16{65535});
        CHECK_FALSE(str::parse_integer<u16>("65536").has_value());
        CHECK(str::parse_integer<i32>("-2147483648") == std::numeric_limits<i32>::min());
        CHECK_FALSE(str::parse_integer<i32>("2147483648").has_value());
        CHECK(str::parse_integer<u32>("4294967295") == std::numeric_limits<u32>::max());
        CHECK_FALSE(str::parse_integer<u32>("4294967296").has_value());
        CHECK(str::parse_integer<int>("-5") == -5);
    }

    TEST_CASE("string_utils - parse_double accepts decimal floating-point forms") {
        CHECK(str::parse_double("0") == 0.0);
        CHECK(str::parse_double("1.5") == 1.5);
        CHECK(str::parse_double("-2.25") == -2.25);
        CHECK(str::parse_double("+3.0") == 3.0);
        CHECK(str::parse_double("1e3") == 1000.0);
        CHECK(str::parse_double("1E-3") == 0.001);
        CHECK(str::parse_double("2.5e+2") == 250.0);
        CHECK(str::parse_double(".5") == 0.5);
        CHECK(str::parse_double("7.") == 7.0);
        CHECK(str::parse_double("-0") == 0.0);
        CHECK(std::signbit(*str::parse_double("-0.0")));
        CHECK(str::parse_double("0.1") == 0.1);
        CHECK(str::parse_double("123456789.125") == 123456789.125);

        const std::optional<f64> infinity = str::parse_double("inf");
        REQUIRE(infinity.has_value());
        CHECK(std::isinf(*infinity));
        const std::optional<f64> negativeInfinity = str::parse_double("-INF");
        REQUIRE(negativeInfinity.has_value());
        CHECK(std::isinf(*negativeInfinity));
        CHECK(*negativeInfinity < 0.0);
        const std::optional<f64> notANumber = str::parse_double("nan");
        REQUIRE(notANumber.has_value());
        CHECK(std::isnan(*notANumber));
    }

    TEST_CASE("string_utils - parse_double rejects malformed, partial, hexadecimal and overflowing input") {
        CHECK_FALSE(str::parse_double("").has_value());
        CHECK_FALSE(str::parse_double(" 1").has_value());
        CHECK_FALSE(str::parse_double("1 ").has_value());
        CHECK_FALSE(str::parse_double("1.5x").has_value());
        CHECK_FALSE(str::parse_double("abc").has_value());
        CHECK_FALSE(str::parse_double("-").has_value());
        CHECK_FALSE(str::parse_double("+").has_value());
        CHECK_FALSE(str::parse_double(".").has_value());
        CHECK_FALSE(str::parse_double("+-1").has_value());
        CHECK_FALSE(str::parse_double("1e").has_value());
        CHECK_FALSE(str::parse_double("1,5").has_value());
        CHECK_FALSE(str::parse_double("0x1p3").has_value());
        CHECK_FALSE(str::parse_double("0X10").has_value());
        CHECK_FALSE(str::parse_double("1e400").has_value());
        CHECK_FALSE(str::parse_double("-1e400").has_value());
    }

    TEST_CASE("regression - sweep-2026-11-12 F217: parse_double rejects the 'infinity' and 'nan(...)' spellings") {
        CHECK_FALSE(str::parse_double("infinity").has_value());
        CHECK_FALSE(str::parse_double("-Infinity").has_value());
        CHECK_FALSE(str::parse_double("+INFINITY").has_value());
        CHECK_FALSE(str::parse_double("nan(abc)").has_value());
        CHECK_FALSE(str::parse_double("nan()").has_value());
        CHECK_FALSE(str::parse_double("NaN(1)").has_value());
        CHECK_FALSE(str::parse_double("-nan(ind)").has_value());
        CHECK_FALSE(str::parse_double("in").has_value());
        CHECK_FALSE(str::parse_double("n").has_value());
        CHECK_FALSE(str::parse_float("infinity").has_value());

        const std::optional<f64> upper = str::parse_double("INF");
        REQUIRE(upper.has_value());
        CHECK(std::isinf(*upper));
        const std::optional<f64> negative = str::parse_double("-inf");
        REQUIRE(negative.has_value());
        CHECK(*negative < 0.0);
        CHECK(std::isinf(*negative));
        const std::optional<f64> signedNan = str::parse_double("+NaN");
        REQUIRE(signedNan.has_value());
        CHECK(std::isnan(*signedNan));
        const std::optional<f64> plainNan = str::parse_double("nan");
        REQUIRE(plainNan.has_value());
        CHECK(std::isnan(*plainNan));
        CHECK(str::parse_double("-NAN").has_value());
    }

    TEST_CASE("string_utils - parse_double rejects underflow to zero but accepts denormals on every implementation") {
        CHECK_FALSE(str::parse_double("1e-400").has_value());
        CHECK_FALSE(str::parse_double("-1e-400").has_value());

        const std::optional<f64> denormal = str::parse_double("1e-320");
        REQUIRE(denormal.has_value());
        CHECK(*denormal > 0.0);
        CHECK(*denormal < std::numeric_limits<f64>::min());
        CHECK(str::parse_double("0e-400") == 0.0);
    }

    TEST_CASE("string_utils - parse_float narrows and rejects values that overflow f32") {
        CHECK(str::parse_float("1.5") == 1.5f);
        CHECK(str::parse_float("-0.25") == -0.25f);
        CHECK(str::parse_float("3.4028234e38").has_value());
        CHECK(str::parse_float("-3.4028234e38").has_value());
        // The shortest round-trip spelling of FLT_MAX is slightly above it and must still parse.
        CHECK(str::parse_float("3.4028235e38") == std::numeric_limits<f32>::max());
        CHECK(str::parse_float("-3.4028235e38") == -std::numeric_limits<f32>::max());
        CHECK(str::parse_float(std::format("{}", std::numeric_limits<f32>::max())) == std::numeric_limits<f32>::max());
        CHECK_FALSE(str::parse_float("3.4028236e38").has_value());
        CHECK_FALSE(str::parse_float("3.5e38").has_value());
        CHECK_FALSE(str::parse_float("-3.5e38").has_value());
        CHECK_FALSE(str::parse_float("-1e39").has_value());
        CHECK_FALSE(str::parse_float("1e300").has_value());
        CHECK(str::parse_float("1e-50") == 0.0f);
        CHECK_FALSE(str::parse_float("nope").has_value());
        CHECK_FALSE(str::parse_float("").has_value());

        const std::optional<f32> infinity = str::parse_float("inf");
        REQUIRE(infinity.has_value());
        CHECK(std::isinf(*infinity));
    }

    TEST_CASE("string_utils - parse_bool understands the common spellings case-insensitively") {
        CHECK(str::parse_bool("true") == true);
        CHECK(str::parse_bool("TRUE") == true);
        CHECK(str::parse_bool("Yes") == true);
        CHECK(str::parse_bool("on") == true);
        CHECK(str::parse_bool("1") == true);
        CHECK(str::parse_bool("false") == false);
        CHECK(str::parse_bool("No") == false);
        CHECK(str::parse_bool("OFF") == false);
        CHECK(str::parse_bool("0") == false);
        CHECK_FALSE(str::parse_bool("").has_value());
        CHECK_FALSE(str::parse_bool("2").has_value());
        CHECK_FALSE(str::parse_bool("maybe").has_value());
        CHECK_FALSE(str::parse_bool(" true").has_value());
        CHECK_FALSE(str::parse_bool("truee").has_value());
    }

    TEST_CASE("string_utils - format_bytes picks binary units with one decimal above 1 KiB") {
        CHECK(str::format_bytes(0) == "0 B");
        CHECK(str::format_bytes(1) == "1 B");
        CHECK(str::format_bytes(1023) == "1023 B");
        CHECK(str::format_bytes(1024) == "1.0 KiB");
        CHECK(str::format_bytes(1536) == "1.5 KiB");
        CHECK(str::format_bytes(1024 * 1024) == "1.0 MiB");
        CHECK(str::format_bytes(3 * MIB / 2) == "1.5 MiB");
        CHECK(str::format_bytes(2 * GIB) == "2.0 GiB");
        CHECK(str::format_bytes(1024ull * GIB) == "1.0 TiB");
        CHECK(str::format_bytes(1024ull * 1024ull * GIB) == "1.0 PiB");
        CHECK(str::format_bytes(1024ull * 1024ull * 1024ull * GIB) == "1.0 EiB");
        CHECK(str::format_bytes(std::numeric_limits<u64>::max()) == "16.0 EiB");
    }

    TEST_CASE("string_utils - format_bytes rounds up into the next unit instead of printing 1024.0") {
        // 1023.95 KiB is the first value that rounds to "1024.0 KiB"; that is 1 MiB - 51.2 bytes.
        CHECK(str::format_bytes(1024 * 1024 - 1) == "1.0 MiB");
        CHECK(str::format_bytes(1024 * 1024 - 51) == "1.0 MiB");
        CHECK(str::format_bytes(1024 * 1024 - 52) == "1023.9 KiB");
        CHECK(str::format_bytes(1023 * 1024) == "1023.0 KiB");
        CHECK(str::format_bytes(1024ull * GIB - 1) == "1.0 TiB");
    }

    TEST_CASE("string_utils - UTF-8 to UTF-16 and back round trips well-formed text") {
        const std::u16string utf16 = str::to_utf16(UTF8_MIXED);
        CHECK(utf16 == UTF16_MIXED);
        CHECK(str::from_utf16(utf16) == UTF8_MIXED);

        CHECK(str::to_utf16("") == u"");
        CHECK(str::from_utf16(u"") == "");
        CHECK(str::to_utf16("ascii only") == u"ascii only");
        CHECK(str::from_utf16(u"ascii only") == "ascii only");

        // Boundaries of each encoding length.
        CHECK(str::to_utf16("\x7F") == u"\u007F");
        CHECK(str::to_utf16("\xC2\x80") == u"\u0080");
        CHECK(str::to_utf16("\xDF\xBF") == u"\u07FF");
        CHECK(str::to_utf16("\xE0\xA0\x80") == u"\u0800");
        CHECK(str::to_utf16("\xEF\xBF\xBF") == u"\uFFFF");
        CHECK(str::to_utf16("\xF0\x90\x80\x80") == u"\xD800\xDC00");
        CHECK(str::to_utf16("\xF4\x8F\xBF\xBF") == u"\xDBFF\xDFFF");
        CHECK(str::from_utf16(u"\xDBFF\xDFFF") == "\xF4\x8F\xBF\xBF");
    }

    TEST_CASE("string_utils - UTF-8 with embedded NUL converts as data") {
        const std::string_view withNul("a\0b", 3);
        const std::u16string utf16 = str::to_utf16(withNul);
        REQUIRE(utf16.size() == 3);
        CHECK(utf16[1] == u'\0');
        CHECK(str::from_utf16(utf16) == withNul);
    }

    TEST_CASE("string_utils - malformed UTF-8 becomes one U+FFFD per error and never fails") {
        const std::u16string replacement = u"\uFFFD";

        CHECK(str::to_utf16("\x80") == replacement);                       // stray continuation
        CHECK(str::to_utf16("\xC0\x80") == replacement + replacement);     // overlong NUL
        CHECK(str::to_utf16("\xC1\xBF") == replacement + replacement);     // overlong 2-byte
        CHECK(str::to_utf16("\xE0\x80\x80") == replacement + replacement + replacement);   // overlong 3-byte
        CHECK(str::to_utf16("\xED\xA0\x80") == replacement + replacement + replacement);   // encoded surrogate
        CHECK(str::to_utf16("\xF4\x90\x80\x80").size() == 4);              // beyond U+10FFFF
        CHECK(str::to_utf16("\xF5\x80") == replacement + replacement);     // invalid lead byte
        CHECK(str::to_utf16("\xFF") == replacement);
        CHECK(str::to_utf16("\xC3") == replacement);                       // truncated at end
        CHECK(str::to_utf16("\xE2\x82") == replacement);                   // truncated 3-byte
        CHECK(str::to_utf16("\xF0\x9F\x98") == replacement);               // truncated 4-byte
        CHECK(str::to_utf16("a\xC3(b") == u"a\uFFFD(b");                   // bad continuation resumes at '('
        CHECK(str::to_utf16("\xE2\x82" "x") == u"\uFFFDx");                // maximal subpart then 'x'
    }

    TEST_CASE("string_utils - unpaired UTF-16 surrogates become U+FFFD") {
        const std::string replacement = "\xEF\xBF\xBD";

        CHECK(str::from_utf16(u"\xD83D") == replacement);                  // lone high at end
        CHECK(str::from_utf16(u"\xD83D" u"a") == replacement + "a");        // high followed by non-low
        CHECK(str::from_utf16(u"\xDE00") == replacement);                  // lone low
        CHECK(str::from_utf16(u"\xDE00\xD83D") == replacement + replacement);
        CHECK(str::from_utf16(u"\xD83D\xD83D\xDE00") == replacement + "\xF0\x9F\x98\x80");
    }

    TEST_CASE("string_utils - UTF-8 to UTF-32 and back round trips well-formed text") {
        const std::u32string utf32 = str::to_utf32(UTF8_MIXED);
        CHECK(utf32 == UTF32_MIXED);
        CHECK(utf32.size() == 9);
        CHECK(str::from_utf32(utf32) == UTF8_MIXED);

        CHECK(str::to_utf32("") == U"");
        CHECK(str::from_utf32(U"") == "");
        CHECK(str::to_utf32("ascii only") == U"ascii only");
        CHECK(str::from_utf32(U"ascii only") == "ascii only");

        CHECK(str::to_utf32("\x7F") == U"\u007F");
        CHECK(str::to_utf32("\xC2\x80") == U"\u0080");
        CHECK(str::to_utf32("\xDF\xBF") == U"\u07FF");
        CHECK(str::to_utf32("\xE0\xA0\x80") == U"\u0800");
        CHECK(str::to_utf32("\xEF\xBF\xBF") == U"\uFFFF");
        CHECK(str::to_utf32("\xF0\x90\x80\x80") == U"\U00010000");
        CHECK(str::to_utf32("\xF4\x8F\xBF\xBF") == U"\U0010FFFF");
        CHECK(str::from_utf32(U"\U0010FFFF") == "\xF4\x8F\xBF\xBF");

        const std::u32string_view withNul(U"a\0b", 3);
        CHECK(str::from_utf32(withNul) == std::string_view("a\0b", 3));
        CHECK(str::to_utf32(std::string_view("a\0b", 3)) == withNul);
    }

    TEST_CASE("string_utils - malformed UTF-8 becomes one U+FFFD per error in UTF-32 output") {
        const std::u32string replacement = U"\uFFFD";

        CHECK(str::to_utf32("\x80") == replacement);
        CHECK(str::to_utf32("\xC0\x80") == replacement + replacement);
        CHECK(str::to_utf32("\xED\xA0\x80") == replacement + replacement + replacement);
        CHECK(str::to_utf32("\xF4\x90\x80\x80").size() == 4);
        CHECK(str::to_utf32("\xC3") == replacement);
        CHECK(str::to_utf32("\xE2\x82" "x") == U"\uFFFDx");
        CHECK(str::to_utf32("a\xC3(b") == U"a\uFFFD(b");
    }

    TEST_CASE("string_utils - invalid UTF-32 code points become U+FFFD") {
        const std::string replacement = "\xEF\xBF\xBD";

        CHECK(str::from_utf32(std::u32string(1, char32_t{0xD800})) == replacement);       // high surrogate
        CHECK(str::from_utf32(std::u32string(1, char32_t{0xDFFF})) == replacement);       // low surrogate
        CHECK(str::from_utf32(std::u32string(1, char32_t{0x110000})) == replacement);     // beyond U+10FFFF
        CHECK(str::from_utf32(std::u32string(1, char32_t{0xFFFFFFFF})) == replacement);   // wrapped negative
        CHECK(str::from_utf32(U"\uFFFD") == replacement);                                 // encoded U+FFFD is valid
        CHECK(str::from_utf32(std::u32string(1, char32_t{0xD7FF})) == "\xED\x9F\xBF");   // just below surrogates
        CHECK(str::from_utf32(std::u32string(1, char32_t{0xE000})) == "\xEE\x80\x80");   // just above surrogates
        CHECK(str::from_utf32(U"a" + std::u32string(1, char32_t{0xD800}) + U"b") == "a" + replacement + "b");
    }

    TEST_CASE("string_utils - to_wide and from_wide round trip through the platform wide string") {
        const std::wstring wide = str::to_wide(UTF8_MIXED);
        CHECK(str::from_wide(wide) == UTF8_MIXED);
        CHECK(str::to_wide("") == L"");
        CHECK(str::from_wide(L"") == "");
        CHECK(str::to_wide("C:/Projects/Game") == L"C:/Projects/Game");
        CHECK(str::from_wide(L"C:/Projects/Game") == "C:/Projects/Game");
        CHECK(str::to_wide("\xC3\xA9") == L"\u00E9");
        CHECK(str::from_wide(L"\u20AC") == "\xE2\x82\xAC");

        if constexpr (sizeof(wchar_t) == 2) {
            CHECK(wide.size() == UTF16_MIXED.size());
            CHECK(str::from_wide(L"\xD83D") == "\xEF\xBF\xBD");
        } else {
            CHECK(wide.size() == UTF32_MIXED.size());
            CHECK(str::from_wide(std::wstring(1, static_cast<wchar_t>(0x110000))) == "\xEF\xBF\xBD");
            CHECK(str::from_wide(std::wstring(1, static_cast<wchar_t>(0xD800))) == "\xEF\xBF\xBD");
            CHECK(str::from_wide(std::wstring(1, static_cast<wchar_t>(-1))) == "\xEF\xBF\xBD");
        }
        CHECK(str::from_wide(str::to_wide("\xFF")) == "\xEF\xBF\xBD");
    }

    TEST_CASE("string_utils - is_valid_utf8 accepts well-formed text and rejects every malformed class") {
        CHECK(str::is_valid_utf8(""));
        CHECK(str::is_valid_utf8("plain ascii"));
        CHECK(str::is_valid_utf8(UTF8_MIXED));
        CHECK(str::is_valid_utf8("\xEF\xBF\xBD"));           // an encoded U+FFFD is valid
        CHECK(str::is_valid_utf8(std::string_view("\0", 1)));

        CHECK_FALSE(str::is_valid_utf8("\x80"));
        CHECK_FALSE(str::is_valid_utf8("\xC0\x80"));
        CHECK_FALSE(str::is_valid_utf8("\xE0\x80\x80"));
        CHECK_FALSE(str::is_valid_utf8("\xED\xA0\x80"));
        CHECK_FALSE(str::is_valid_utf8("\xF4\x90\x80\x80"));
        CHECK_FALSE(str::is_valid_utf8("\xF5\x80\x80\x80"));
        CHECK_FALSE(str::is_valid_utf8("\xC3"));
        CHECK_FALSE(str::is_valid_utf8("ok\xE2\x82"));
        CHECK_FALSE(str::is_valid_utf8("\xC3\x28"));
    }

    TEST_CASE("string_utils - is_valid_identifier follows C identifier rules") {
        static_assert(str::is_valid_identifier("name"));
        static_assert(str::is_valid_identifier("_private"));
        static_assert(str::is_valid_identifier("Mixed_Case_123"));
        static_assert(str::is_valid_identifier("x"));
        static_assert(str::is_valid_identifier("_"));
        static_assert(!str::is_valid_identifier(""));
        static_assert(!str::is_valid_identifier("1abc"));
        static_assert(!str::is_valid_identifier("has space"));
        static_assert(!str::is_valid_identifier("dotted.name"));
        static_assert(!str::is_valid_identifier("dash-name"));
        CHECK_FALSE(str::is_valid_identifier("caf\xC3\xA9"));
        CHECK_FALSE(str::is_valid_identifier(std::string_view("a\0b", 3)));
    }
}
