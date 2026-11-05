#include "conjure_test.h"

#include "conjure/core/result.h"

#include <memory>
#include <string>
#include <utility>

using namespace conjure;
using namespace conjure::core;

namespace {
    result<int> divide(int numerator, int denominator) {
        if (denominator == 0) {
            return make_error(error_code::INVALID_ARGUMENT, "division by zero ({} / {})", numerator, denominator);
        }
        return numerator / denominator;
    }

    result<int> add_quotients(int firstNumerator, int firstDenominator, int secondNumerator, int secondDenominator) {
        CONJURE_TRY_ASSIGN(const int first, divide(firstNumerator, firstDenominator));
        CONJURE_TRY_ASSIGN(const int second, divide(secondNumerator, secondDenominator));
        return first + second;
    }

    status validate_positive(int value) {
        if (value <= 0) {
            return error(error_code::OUT_OF_RANGE, "value must be positive");
        }
        return status::success();
    }

    status validate_all_positive(int first, int second) {
        CONJURE_TRY(validate_positive(first));
        CONJURE_TRY(validate_positive(second));
        return {};
    }

    // CONJURE_TRY must propagate between results of different value types.
    result<std::string> describe_quotient(int numerator, int denominator) {
        CONJURE_TRY_ASSIGN(const int quotient, divide(numerator, denominator));
        return std::to_string(quotient);
    }

    result<std::unique_ptr<int>> make_boxed(int value) {
        if (value < 0) {
            return error(error_code::INVALID_ARGUMENT);
        }
        return std::make_unique<int>(value);
    }

    result<std::unique_ptr<int>> make_boxed_doubled(int value) {
        CONJURE_TRY_ASSIGN(std::unique_ptr<int> boxed, make_boxed(value));
        *boxed *= 2;
        return boxed;
    }

    struct only_explicit {
        int value = 0;

        explicit only_explicit(int initial) :
            value(initial) {}
    };
}

TEST_SUITE("core.result") {

    TEST_CASE("value result reports has_value and exposes the value") {
        const result<int> value = 42;

        REQUIRE(value.has_value());
        REQUIRE(static_cast<bool>(value));
        CHECK(value.value() == 42);
        CHECK(*value == 42);
    }

    TEST_CASE("error result exposes the error") {
        const result<int> failure = error(error_code::NOT_FOUND, "missing");

        REQUIRE_FALSE(failure.has_value());
        REQUIRE_FALSE(static_cast<bool>(failure));
        CHECK(failure.get_error().code() == error_code::NOT_FOUND);
        CHECK(failure.get_error().message() == "missing");
    }

    TEST_CASE("value_or returns the value or the fallback") {
        const result<int> value = 7;
        const result<int> failure = error(error_code::UNKNOWN);

        CHECK(value.value_or(-1) == 7);
        CHECK(failure.value_or(-1) == -1);
        CHECK(result<int>(9).value_or(-1) == 9);
        CHECK(result<int>(error(error_code::UNKNOWN)).value_or(-1) == -1);
    }

    TEST_CASE("move-only values are supported") {
        result<std::unique_ptr<int>> value = std::make_unique<int>(5);

        REQUIRE(value.has_value());
        std::unique_ptr<int> taken = std::move(value).value();
        REQUIRE(taken != nullptr);
        CHECK(*taken == 5);
    }

    TEST_CASE("rvalue accessors move out of the result") {
        std::unique_ptr<int> viaStar = *result<std::unique_ptr<int>>(std::make_unique<int>(3));
        REQUIRE(viaStar != nullptr);
        CHECK(*viaStar == 3);

        result<int> failure = error(error_code::TIMEOUT, "moved");
        const error taken = std::move(failure).get_error();
        CHECK(taken.code() == error_code::TIMEOUT);
        CHECK(taken.message() == "moved");
    }

    TEST_CASE("non default constructible value types are supported") {
        const result<only_explicit> value = only_explicit(11);

        REQUIRE(value.has_value());
        CHECK(value->value == 11);
    }

    TEST_CASE("operator-> forwards to the value") {
        const result<std::string> text = std::string("hello");

        REQUIRE(text.has_value());
        CHECK(text->size() == 5);
    }

    TEST_CASE("make_error formats the message") {
        const result<int> failure = divide(1, 0);

        REQUIRE_FALSE(failure.has_value());
        CHECK(failure.get_error().code() == error_code::INVALID_ARGUMENT);
        CHECK(failure.get_error().message() == "division by zero (1 / 0)");

        const error plain = make_error(error_code::IO_FAILURE, "no arguments");
        CHECK(plain.message() == "no arguments");
    }

    TEST_CASE("CONJURE_TRY_ASSIGN propagates errors and assigns values") {
        const result<int> ok = add_quotients(10, 2, 9, 3);
        REQUIRE(ok.has_value());
        CHECK(ok.value() == 8);

        const result<int> firstFails = add_quotients(10, 0, 9, 3);
        REQUIRE_FALSE(firstFails.has_value());
        CHECK(firstFails.get_error().code() == error_code::INVALID_ARGUMENT);

        const result<int> secondFails = add_quotients(10, 2, 9, 0);
        REQUIRE_FALSE(secondFails.has_value());
        CHECK(secondFails.get_error().message() == "division by zero (9 / 0)");
    }

    TEST_CASE("CONJURE_TRY_ASSIGN works across value types and with move-only values") {
        const result<std::string> described = describe_quotient(9, 3);
        REQUIRE(described.has_value());
        CHECK(described.value() == "3");

        const result<std::string> failed = describe_quotient(9, 0);
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.get_error().code() == error_code::INVALID_ARGUMENT);

        const result<std::unique_ptr<int>> doubled = make_boxed_doubled(21);
        REQUIRE(doubled.has_value());
        CHECK(*doubled.value() == 42);

        const result<std::unique_ptr<int>> rejected = make_boxed_doubled(-1);
        CHECK_FALSE(rejected.has_value());
    }

    TEST_CASE("status reports success and failure") {
        const status ok = validate_all_positive(1, 2);
        CHECK(ok.has_value());
        CHECK(static_cast<bool>(ok));

        const status failure = validate_all_positive(1, -2);
        REQUIRE_FALSE(failure.has_value());
        CHECK(failure.get_error().code() == error_code::OUT_OF_RANGE);

        const status defaulted = {};
        CHECK(defaulted.has_value());
        CHECK(status::success().has_value());
    }

    TEST_CASE("status is copyable and movable") {
        const status failure = error(error_code::CANCELLED, "stop");

        status copy = failure;
        REQUIRE_FALSE(copy.has_value());
        CHECK(copy.get_error().message() == "stop");

        status assigned = {};
        assigned = copy;
        REQUIRE_FALSE(assigned.has_value());

        const status moved = std::move(copy);
        REQUIRE_FALSE(moved.has_value());
        CHECK(moved.get_error().code() == error_code::CANCELLED);

        const error taken = std::move(assigned).get_error();
        CHECK(taken.code() == error_code::CANCELLED);
    }

    TEST_CASE("result is copyable and assignable") {
        const result<std::string> original = std::string("copy me");
        const result<std::string> copy = original;

        REQUIRE(copy.has_value());
        CHECK(copy.value() == "copy me");
        CHECK(original.value() == "copy me");

        result<std::string> assigned = error(error_code::UNKNOWN);
        assigned = original;
        REQUIRE(assigned.has_value());
        CHECK(assigned.value() == "copy me");

        result<std::string> movedInto = std::string("other");
        movedInto = result<std::string>(error(error_code::NOT_FOUND, "gone"));
        REQUIRE_FALSE(movedInto.has_value());
        CHECK(movedInto.get_error().message() == "gone");
    }

    TEST_CASE("mutating through value and operator* changes the held value") {
        result<int> value = 1;

        value.value() = 2;
        CHECK(*value == 2);

        *value = 3;
        CHECK(value.value() == 3);
    }
}
