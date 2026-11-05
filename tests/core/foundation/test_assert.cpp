#include "conjure_test.h"

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

TEST_SUITE("core.assert") {

    TEST_CASE("passing assertions do nothing") {
        test::assert_capture asserts = {};

        CONJURE_ASSERT(1 + 1 == 2);
        CONJURE_ASSERT(true, "with message {}", 1);
        CONJURE_VERIFY(true);
        CONJURE_FATAL_ASSERT(true);

        CHECK(asserts.count() == 0);
    }

    TEST_CASE("failing assertions reach the handler with the formatted message") {
        test::assert_capture asserts = {};

        const int value = 3;
        CONJURE_ASSERT(value == 4, "value was {}", value);

        CHECK(asserts.count() == 1);
        CHECK(asserts.last_message() == "value was 3");
    }

    TEST_CASE("assertions without a message report the expression") {
        test::assert_capture asserts = {};

        CONJURE_ASSERT(false);

        CHECK(asserts.count() == 1);
        CHECK(asserts.last_message() == "false");
    }

    TEST_CASE("conditions with parenthesised commas are supported") {
        test::assert_capture asserts = {};

        CONJURE_ASSERT(both_positive(1, 2));
        CONJURE_ASSERT((both_positive(1, -2)), "call with comma");

        CHECK(asserts.count() == 1);
        CHECK(asserts.last_message() == "call with comma");
    }

    TEST_CASE("the message is formatted lazily") {
        test::assert_capture asserts = {};
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

    TEST_CASE("the handler receives expression, file, line and function") {
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

    TEST_CASE("CONJURE_VERIFY evaluates its expression exactly once") {
        test::assert_capture asserts = {};

        int evaluations = 0;
        CONJURE_VERIFY(++evaluations == 1);
        CONJURE_VERIFY(++evaluations == 0, "second verify fails");

        CHECK(evaluations == 2);
        CHECK(asserts.count() == 1);
    }

    TEST_CASE("CONJURE_FATAL_ASSERT reaches the handler") {
        test::assert_capture asserts = {};

        CONJURE_FATAL_ASSERT(false, "fatal {}", "check");

        CHECK(asserts.count() == 1);
        CHECK(asserts.last_message() == "fatal check");
    }

    TEST_CASE("set_assert_handler returns the previous handler and nullptr restores the default") {
        const assert_handler original = current_assert_handler();

        const assert_handler previous = set_assert_handler(nullptr);
        CHECK(previous == original);
        CHECK(current_assert_handler() == &default_assert_handler);

        set_assert_handler(original);
        CHECK(current_assert_handler() == original);
    }

    TEST_CASE("the default handler logs a FATAL message to the core category and asks to break") {
        test::log_capture capture = {};

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

    TEST_CASE("report_assert_failure returns true only when the handler asks to break") {
        const assert_handler previous = set_assert_handler(&recording_handler);
        const bool shouldBreak = detail::report_assert_failure("expr", "f", 1, "fn", "msg");
        set_assert_handler(previous);
        CHECK_FALSE(shouldBreak);

        test::log_capture quiet = {};
        set_assert_handler(nullptr);
        const bool defaultBreaks = detail::report_assert_failure("expr", "f", 1, "fn", "msg");
        set_assert_handler(previous);
        CHECK(defaultBreaks);
    }

    TEST_CASE("assert_capture restores the previous handler") {
        const assert_handler before = current_assert_handler();
        {
            test::assert_capture asserts = {};
            CHECK(current_assert_handler() != before);
        }
        CHECK(current_assert_handler() == before);
    }
}
