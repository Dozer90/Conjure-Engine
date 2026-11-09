#include "conjure/testing/doctest.h"

#include "conjure/modules/module_version.h"

#include "conjure/core/build_info.h"
#include "conjure/core/error.h"

#include <array>
#include <format>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {
    semantic_version version_of(std::string_view text) {
        result<semantic_version> parsed = parse_version(text);
        REQUIRE(parsed.has_value());
        return parsed.value();
    }

    version_range range_of(std::string_view text) {
        result<version_range> parsed = parse_version_range(text);
        INFO("range text: ", text);
        REQUIRE(parsed.has_value());
        return parsed.value();
    }
}

TEST_SUITE("modules/module_version") {

    TEST_CASE("semantic_version - parse accepts major.minor.patch and rejects anything else") {
        CHECK(version_of("1.2.3") == semantic_version{1, 2, 3});
        CHECK(version_of("0.0.0") == semantic_version{0, 0, 0});
        CHECK(version_of("65535.65535.65535") == MAX_SEMANTIC_VERSION);
        CHECK(version_of("  7.8.9 ") == semantic_version{7, 8, 9});

        const std::array<std::string_view, 10> rejected = {
            "", "1", "1.2", "1.2.3.4", "1.2.x", "-1.2.3", "1.2.3-beta", "65536.0.0", "a.b.c", "1..2",
        };
        for (const std::string_view text : rejected) {
            CAPTURE(text);
            CONJURE_CHECK_ERROR(parse_version(text), error_code::PARSE_FAILURE);
        }
    }

    TEST_CASE("semantic_version - ordering compares major, then minor, then patch") {
        CHECK(semantic_version{1, 0, 0} < semantic_version{2, 0, 0});
        CHECK(semantic_version{1, 9, 9} < semantic_version{2, 0, 0});
        CHECK(semantic_version{1, 2, 3} < semantic_version{1, 3, 0});
        CHECK(semantic_version{1, 2, 3} < semantic_version{1, 2, 4});
        CHECK(semantic_version{1, 2, 3} == semantic_version{1, 2, 3});
        CHECK(semantic_version{1, 2, 3} >= semantic_version{1, 2, 3});
    }

    TEST_CASE("semantic_version - to_string and the formatter print dotted digits") {
        CHECK(to_string(semantic_version{1, 2, 3}) == "1.2.3");
        CHECK(to_string(semantic_version{}) == "0.0.0");
        CHECK(std::format("{}", semantic_version{10, 0, 7}) == "10.0.7");
        CHECK(version_of(to_string(semantic_version{300, 2, 1})) == semantic_version{300, 2, 1});
    }

    TEST_CASE("semantic_version - host_engine_version mirrors build_info") {
        const semantic_version host = host_engine_version();
        CHECK(host.majorVersion == CONJURE_VERSION_MAJOR);
        CHECK(host.minorVersion == CONJURE_VERSION_MINOR);
        CHECK(host.patchVersion == CONJURE_VERSION_PATCH);
        CHECK(to_string(host) == CONJURE_VERSION_STRING);
    }

    TEST_CASE("version_range - exact, caret, tilde, explicit, open and star forms") {
        CHECK(range_of("1.2.3") == version_range{{1, 2, 3}, {1, 2, 4}});
        CHECK(range_of("^1.2.3") == version_range{{1, 2, 3}, {2, 0, 0}});
        CHECK(range_of("~1.2.3") == version_range{{1, 2, 3}, {1, 3, 0}});
        CHECK(range_of(">=1.0.0 <2.0.0") == version_range{{1, 0, 0}, {2, 0, 0}});
        CHECK(range_of("<2.0.0 >=1.0.0") == version_range{{1, 0, 0}, {2, 0, 0}});
        CHECK(range_of(">=1.0.0") == version_range{{1, 0, 0}, MAX_SEMANTIC_VERSION});
        CHECK(range_of("<2.0.0") == version_range{{0, 0, 0}, {2, 0, 0}});
        CHECK(range_of("*") == version_range{});
        CHECK(range_of("  ^ 1.2.3 ") == version_range{{1, 2, 3}, {2, 0, 0}});
    }

    TEST_CASE("version_range - caret on 0.x bumps the minor and on 0.0.x the patch") {
        CHECK(range_of("^0.2.3") == version_range{{0, 2, 3}, {0, 3, 0}});
        CHECK(range_of("^0.0.3") == version_range{{0, 0, 3}, {0, 0, 4}});
        CHECK(range_of("^0.1.0") == version_range{{0, 1, 0}, {0, 2, 0}});
    }

    TEST_CASE("version_range - invalid and reversed ranges are PARSE_FAILURE") {
        const std::array<std::string_view, 12> rejected = {
            "", "   ", ">=2.0.0 <1.0.0", ">=1.0.0 <1.0.0", "<=1.0.0", ">1.0.0", "1.2", "^", "~x.y.z",
            ">=1.0.0 >=2.0.0", "1.0.0 2.0.0", "^65535.0.0",
        };
        for (const std::string_view text : rejected) {
            CAPTURE(text);
            CONJURE_CHECK_ERROR(parse_version_range(text), error_code::PARSE_FAILURE);
        }
        const result<version_range> reversed = parse_version_range(">=2.0.0 <1.0.0");
        REQUIRE_FALSE(reversed.has_value());
        CHECK(reversed.get_error().message().find("empty") != std::string::npos);
    }

    TEST_CASE("version_range - contains is inclusive below and exclusive above") {
        const version_range range = range_of(">=1.0.0 <2.0.0");
        CHECK(range.contains({1, 0, 0}));
        CHECK(range.contains({1, 9, 65535}));
        CHECK_FALSE(range.contains({2, 0, 0}));
        CHECK_FALSE(range.contains({0, 65535, 65535}));

        const version_range exact = range_of("1.2.3");
        CHECK(exact.contains({1, 2, 3}));
        CHECK_FALSE(exact.contains({1, 2, 4}));
        CHECK_FALSE(exact.contains({1, 2, 2}));

        CHECK(version_range{}.contains({0, 0, 0}));
        CHECK(version_range{}.contains({65535, 65535, 65534}));
        CHECK_FALSE(version_range{}.contains(MAX_SEMANTIC_VERSION));
        CHECK(version_range{}.is_unbounded());
        CHECK_FALSE(exact.is_unbounded());
    }

    TEST_CASE("version_range - to_string round trips through parse_version_range") {
        const std::array<std::string_view, 7> texts = {
            "*", "1.2.3", "^1.2.3", "~1.2.3", ">=1.0.0 <2.0.0", ">=1.0.0", "<2.0.0",
        };
        for (const std::string_view text : texts) {
            CAPTURE(text);
            const version_range range = range_of(text);
            const std::string printed = to_string(range);
            CHECK(range_of(printed) == range);
        }
        CHECK(to_string(range_of("*")) == "*");
        CHECK(to_string(range_of("1.2.3")) == "1.2.3");
        CHECK(to_string(range_of("^1.2.3")) == ">=1.2.3 <2.0.0");
        CHECK(to_string(range_of(">=1.0.0")) == ">=1.0.0");
        CHECK(std::format("{}", range_of("~1.2.3")) == ">=1.2.3 <1.3.0");
    }
}
