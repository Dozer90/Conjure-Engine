#include "conjure/testing/doctest.h"

#include "conjure/core/error.h"
#include "conjure/core/result.h"

#include <string>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::testing;

namespace {
    result<int> parse_positive(int value, int &evaluations) {
        ++evaluations;
        if (value <= 0) {
            return make_error(error_code::INVALID_ARGUMENT, "{} is not positive", value);
        }
        return value;
    }

    status succeed() {
        return status::success();
    }

    [[deprecated("pinned by the deprecated-call marker case")]] [[nodiscard]] int former_answer() noexcept {
        return 42;
    }
}

TEST_SUITE("testing/doctest") {

    TEST_CASE("describe_error - ok for values and the error text otherwise") {
        int evaluations = 0;

        CHECK(describe_error(parse_positive(3, evaluations)) == "ok");
        CHECK(describe_error(parse_positive(-1, evaluations)) == "core.INVALID_ARGUMENT: -1 is not positive");
        CHECK(describe_error(succeed()) == "ok");
    }

    TEST_CASE("has_error - matches one specific code") {
        int evaluations = 0;
        const result<int> failed = parse_positive(0, evaluations);

        CHECK(has_error(failed, error_code::INVALID_ARGUMENT));
        CHECK_FALSE(has_error(failed, error_code::NOT_FOUND));
        CHECK_FALSE(has_error(parse_positive(1, evaluations), error_code::INVALID_ARGUMENT));
    }

    TEST_CASE("CONJURE_REQUIRE_OK and CONJURE_CHECK_OK - evaluate the expression once") {
        int evaluations = 0;

        CONJURE_REQUIRE_OK(parse_positive(5, evaluations));
        CHECK(evaluations == 1);

        CONJURE_CHECK_OK(parse_positive(6, evaluations));
        CHECK(evaluations == 2);

        CONJURE_CHECK_OK(succeed());
    }

    TEST_CASE("CONJURE_CHECK_ERROR - accepts the expected code and evaluates once") {
        int evaluations = 0;

        CONJURE_CHECK_ERROR(parse_positive(-4, evaluations), error_code::INVALID_ARGUMENT);

        CHECK(evaluations == 1);
    }

    TEST_CASE("CONJURE_TEST_DEPRECATED_BEGIN - a deprecated call between the markers builds as a warning-free call") {
        CONJURE_TEST_DEPRECATED_BEGIN
        const int answer = former_answer();
        CONJURE_TEST_DEPRECATED_END

        CHECK(answer == 42);
    }

    TEST_CASE("StringMaker - prints errors and codes") {
        const error failure(error_code::NOT_FOUND, "missing.txt");

        CHECK(std::string(doctest::toString(failure).c_str()) == "core.NOT_FOUND: missing.txt");
        CHECK(std::string(doctest::toString(error_code::TIMEOUT).c_str()) == "TIMEOUT");
    }

    TEST_CASE("environment - is reachable from every test") {
        CHECK_FALSE(environment().executableName.empty());
    }
}
