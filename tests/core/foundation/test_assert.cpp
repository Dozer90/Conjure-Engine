#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#include <string>

using namespace conjure;
using namespace conjure::core;

namespace {
    assert_info sLastInfo = {};

    assert_action recording_handler(const assert_info &info) {
        sLastInfo = info;
        return assert_action::CONTINUE;
    }

    bool both_positive(int first, int second) {
        return first > 0 && second > 0;
    }
}

TEST_SUITE("core/assert") {

    TEST_CASE("assert - passing assertions do nothing") {
        testing::expect_assert asserts = {};

        CONJURE_ASSERT(1 + 1 == 2);
        CONJURE_ASSERT(true, "with message {}", 1);
        CONJURE_VERIFY(true);
        CONJURE_FATAL_ASSERT(true);

        CHECK(asserts.count() == 0);
    }

    TEST_CASE("assert - failing assertions reach the handler with the formatted message") {
        testing::expect_assert asserts = {};

        const int value = 3;
        CONJURE_ASSERT(value == 4, "value was {}", value);

        CHECK(asserts.count() == 1);
        CHECK(asserts.last_message() == "value was 3");
    }

    TEST_CASE("assert - assertions without a message report the expression") {
        testing::expect_assert asserts = {};

        CONJURE_ASSERT(false);

        CHECK(asserts.count() == 1);
        CHECK(asserts.last_message() == "false");
    }

    TEST_CASE("assert - conditions with parenthesised commas are supported") {
        testing::expect_assert asserts = {};

        CONJURE_ASSERT(both_positive(1, 2));
        CONJURE_ASSERT((both_positive(1, -2)), "call with comma");

        CHECK(asserts.count() == 1);
        CHECK(asserts.last_message() == "call with comma");
    }

    TEST_CASE("assert - the message is formatted lazily") {
        testing::expect_assert asserts = {};
        int evaluations = 0;
        const auto count = [&evaluations] {
            return ++evaluations;
        };

        CONJURE_ASSERT(true, "{}", count());
        CHECK(evaluations == 0);

        CONJURE_ASSERT(false, "{}", count());
        CHECK(evaluations == 1);
        CHECK(asserts.last_message() == "1");
    }

    TEST_CASE("assert - the handler receives expression, file, line and function") {
        const assert_handler previous = set_assert_handler(&recording_handler);
        sLastInfo = {};

        const int expectedLine = __LINE__ + 1;
        CONJURE_ASSERT(1 == 2, "detail {}", 7);

        set_assert_handler(previous);

        CHECK(std::string_view(sLastInfo.expression) == "1 == 2");
        CHECK(std::string_view(sLastInfo.file) == __FILE__);
        CHECK(sLastInfo.line == expectedLine);
        CHECK_FALSE(std::string_view(sLastInfo.function).empty());
        CHECK(sLastInfo.message == "detail 7");
    }

    TEST_CASE("assert - CONJURE_VERIFY evaluates its expression exactly once") {
        testing::expect_assert asserts = {};

        int evaluations = 0;
        CONJURE_VERIFY(++evaluations == 1);
        CONJURE_VERIFY(++evaluations == 0, "second verify fails");

        CHECK(evaluations == 2);
        CHECK(asserts.count() == 1);
    }

    TEST_CASE("assert - CONJURE_FATAL_ASSERT reaches the handler") {
        testing::expect_assert asserts = {};

        CONJURE_FATAL_ASSERT(false, "fatal {}", "check");

        CHECK(asserts.count() == 1);
        CHECK(asserts.last_message() == "fatal check");
    }

    TEST_CASE("assert - set_assert_handler returns the previous handler and nullptr restores the default") {
        const assert_handler original = current_assert_handler();

        const assert_handler previous = set_assert_handler(nullptr);
        CHECK(previous == original);
        CHECK(current_assert_handler() == &default_assert_handler);

        set_assert_handler(original);
        CHECK(current_assert_handler() == original);
    }

    TEST_CASE("assert - the default handler logs a FATAL message to the core category and asks to break") {
        testing::log_capture capture = {};
        capture.allow_errors();

        assert_info info = {};
        info.expression = "x > 0";
        info.file = "file.cpp";
        info.line = 12;
        info.function = "fn";
        info.message = "x was -1";

        CHECK(default_assert_handler(info) == assert_action::BREAK);
        REQUIRE(capture.count(log_level::FATAL) == 1);
        CHECK(capture.contains("Assertion failed: x > 0 - x was -1 (file.cpp:12)"));

        info.message.clear();
        CHECK(default_assert_handler(info) == assert_action::BREAK);
        CHECK(capture.contains("Assertion failed: x > 0 (file.cpp:12)"));
    }

    TEST_CASE("assert - report_assert_failure returns true only when the handler asks to break") {
        const assert_handler previous = set_assert_handler(&recording_handler);
        const bool shouldBreak = detail::report_assert_failure("expr", "f", 1, "fn", "msg");
        set_assert_handler(previous);
        CHECK_FALSE(shouldBreak);

        testing::log_capture quiet = {};
        quiet.allow_errors();
        set_assert_handler(nullptr);
        const bool defaultBreaks = detail::report_assert_failure("expr", "f", 1, "fn", "msg");
        set_assert_handler(previous);
        CHECK(defaultBreaks);
    }

    TEST_CASE("assert - expect_assert restores the previous handler") {
        const assert_handler before = current_assert_handler();
        set_assert_handler(&recording_handler);
        {
            testing::expect_assert asserts = {};
            CHECK(current_assert_handler() != &recording_handler);
        }
        CHECK(current_assert_handler() == &recording_handler);
        set_assert_handler(before);
    }
}
