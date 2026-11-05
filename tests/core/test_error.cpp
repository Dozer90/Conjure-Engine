#include "conjure_test.h"

#include "conjure/core/error.h"

#include <format>
#include <utility>

using namespace conjure;
using namespace conjure::core;

TEST_SUITE("core.error") {

    TEST_CASE("default error is NONE with an empty message") {
        const error value = {};

        CHECK(value.code() == error_code::NONE);
        CHECK(value.message().empty());
        CHECK(value.to_string() == "NONE");
    }

    TEST_CASE("code-only constructor leaves the message empty") {
        const error value(error_code::UNSUPPORTED);

        CHECK(value.code() == error_code::UNSUPPORTED);
        CHECK(value.message().empty());
        CHECK(value.to_string() == "UNSUPPORTED");
    }

    TEST_CASE("to_string includes the message when present") {
        const error value(error_code::IO_FAILURE, "disk full");

        CHECK(value.to_string() == "IO_FAILURE: disk full");
    }

    TEST_CASE("every error code has a name") {
        for (u16 index = 0; index < static_cast<u16>(error_code::COUNT); ++index) {
            const std::string_view name = to_string(static_cast<error_code>(index));
            CHECK_FALSE(name.empty());
            CHECK(name != "INVALID_ERROR_CODE");
        }
        CHECK(to_string(error_code::COUNT) == "INVALID_ERROR_CODE");
        CHECK(to_string(static_cast<error_code>(999)) == "INVALID_ERROR_CODE");
    }

    TEST_CASE("errors compare by code and message") {
        const error first(error_code::NOT_FOUND, "a");
        const error second(error_code::NOT_FOUND, "a");
        const error differentMessage(error_code::NOT_FOUND, "b");
        const error differentCode(error_code::TIMEOUT, "a");

        CHECK(first == second);
        CHECK_FALSE(first == differentMessage);
        CHECK_FALSE(first == differentCode);
    }

    TEST_CASE("copy and move preserve code and message") {
        const error original(error_code::PARSE_FAILURE, "line 3");

        const error copy = original;
        CHECK(copy == original);

        error assigned = {};
        assigned = original;
        CHECK(assigned == original);

        error source(error_code::PARSE_FAILURE, "line 3");
        const error moved = std::move(source);
        CHECK(moved == original);
    }

    TEST_CASE("errors and codes are formattable, including with format specs") {
        const error value(error_code::TIMEOUT, "slow");

        CHECK(std::format("{}", error_code::TIMEOUT) == "TIMEOUT");
        CHECK(std::format("{}", value) == "TIMEOUT: slow");
        CHECK(std::format("{:>12}", error_code::NONE) == "        NONE");
        CHECK(std::format("[{:<14}]", value) == "[TIMEOUT: slow ]");
    }
}
