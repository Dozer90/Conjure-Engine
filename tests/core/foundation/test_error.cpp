#include "conjure/testing/doctest.h"

#include "conjure/core/error.h"
#include "conjure/core/result.h"

#include <format>
#include <utility>

using namespace conjure;
using namespace conjure::core;

namespace {
    // A module-style error enumeration bound to its own category, as every engine module does.
    enum class sample_error : u16 {
        NONE = 0,
        WIDGET_MISSING,
        WIDGET_JAMMED,
        COUNT
    };

    std::string_view sample_error_name(sample_error code) noexcept {
        switch (code) {
            case sample_error::NONE: {
                return "NONE";
            }
            case sample_error::WIDGET_MISSING: {
                return "WIDGET_MISSING";
            }
            case sample_error::WIDGET_JAMMED: {
                return "WIDGET_JAMMED";
            }
            case sample_error::COUNT: {
                return "";
            }
        }
        return "";
    }
}

CONJURE_DECLARE_ERROR_CATEGORY(sample_error, "sample", sample_error_name);

namespace {
    result<int> find_widget(bool present) {
        if (!present) {
            return make_error(sample_error::WIDGET_MISSING, "widget {} not found", 7);
        }
        return 7;
    }
}

TEST_SUITE("core/error") {

    TEST_CASE("error - default error is core NONE with an empty message") {
        const error value = {};

        CHECK(value.category() == error_category_traits<error_code>::ID);
        CHECK(value.category_name() == "core");
        CHECK(value.code() == 0);
        CHECK(value.generic_code() == error_code::NONE);
        CHECK(value.code_name() == "NONE");
        CHECK(value.message().empty());
        CHECK(value.to_string() == "core.NONE");
    }

    TEST_CASE("error - code-only constructor leaves the message empty") {
        const error value(error_code::UNSUPPORTED);

        CHECK(value.generic_code() == error_code::UNSUPPORTED);
        CHECK(value.is(error_code::UNSUPPORTED));
        CHECK_FALSE(value.is(error_code::NOT_FOUND));
        CHECK(value.message().empty());
        CHECK(value.to_string() == "core.UNSUPPORTED");
    }

    TEST_CASE("error - to_string includes the message when present") {
        const error value(error_code::IO_FAILURE, "disk full");

        CHECK(value.to_string() == "core.IO_FAILURE: disk full");
    }

    TEST_CASE("error - every generic error code has a name") {
        for (u16 index = 0; index < static_cast<u16>(error_code::COUNT); ++index) {
            const std::string_view name = to_string(static_cast<error_code>(index));
            CHECK_FALSE(name.empty());
            CHECK(name != "INVALID_ERROR_CODE");
            CHECK(error_category_traits<error_code>::code_name(index) == name);
        }
        CHECK(to_string(error_code::COUNT) == "INVALID_ERROR_CODE");
        CHECK(to_string(static_cast<error_code>(999)) == "INVALID_ERROR_CODE");
        CHECK(error_category_traits<error_code>::code_name(999).empty());
    }

    TEST_CASE("error - module categories are derived from their name") {
        using traits = error_category_traits<sample_error>;

        CHECK(traits::NAME == "sample");
        CHECK(traits::ID == hash::fnv1a32("sample"));
        CHECK(traits::ID != error_category_traits<error_code>::ID);
        CHECK(traits::code_name(static_cast<u32>(sample_error::WIDGET_JAMMED)) == "WIDGET_JAMMED");
        CHECK(traits::code_name(999).empty());
    }

    TEST_CASE("error - make_error builds module errors with and without formatting") {
        const error formatted = make_error(sample_error::WIDGET_MISSING, "widget {} not found", 7);
        CHECK(formatted.is(sample_error::WIDGET_MISSING));
        CHECK(formatted.is_category<sample_error>());
        CHECK_FALSE(formatted.is_category<error_code>());
        CHECK(formatted.generic_code() == error_code::NONE);
        CHECK(formatted.message() == "widget 7 not found");
        CHECK(formatted.to_string() == "sample.WIDGET_MISSING: widget 7 not found");

        const error plain = make_error(sample_error::WIDGET_JAMMED);
        CHECK(plain.message().empty());
        CHECK(plain.to_string() == "sample.WIDGET_JAMMED");

        const error detailed = make_error(sample_error::WIDGET_JAMMED, std::string_view("stuck"));
        CHECK(detailed.message() == "stuck");

        const error generic = make_error(error_code::TIMEOUT, "after {} ms", 250);
        CHECK(generic.is(error_code::TIMEOUT));
        CHECK(generic.to_string() == "core.TIMEOUT: after 250 ms");
    }

    TEST_CASE("error - make_error with one fixed message argument accepts every common string shape") {
        // A lone string argument must pick the std::string_view overload over the std::format_string
        // one (partial ordering: the format overload's trailing pack makes it less specialised), so
        // callers can write a fixed message without going through the error(code, std::string)
        // constructor. Braces in such a message are kept verbatim.
        const std::string owned = "owned {}";
        const char *pointer = "pointer";
        const std::string_view view = "view";

        const error fromLiteral = make_error(error_code::NOT_FOUND, "literal {}");
        const error fromOwned = make_error(sample_error::WIDGET_JAMMED, owned);
        const error fromPointer = make_error(error_code::INVALID_STATE, pointer);
        const error fromView = make_error(sample_error::WIDGET_MISSING, view);
        const error fromTemporary = make_error(error_code::TIMEOUT, std::string("temporary"));

        CHECK(fromLiteral.message() == "literal {}");
        CHECK(fromOwned.message() == "owned {}");
        CHECK(fromPointer.message() == "pointer");
        CHECK(fromView.message() == "view");
        CHECK(fromTemporary.message() == "temporary");
    }

    TEST_CASE("error - unknown codes print numerically") {
        const error value = error::from(static_cast<sample_error>(42), "odd");
        CHECK(value.code() == 42);
        CHECK(value.code_name().empty());
        CHECK(value.to_string() == "sample.42: odd");
    }

    TEST_CASE("error - module errors flow through result") {
        const result<int> found = find_widget(true);
        REQUIRE(found.has_value());
        CHECK(found.value() == 7);

        const result<int> missing = find_widget(false);
        REQUIRE_FALSE(missing.has_value());
        CHECK(missing.get_error().is(sample_error::WIDGET_MISSING));
    }

    TEST_CASE("error - errors compare by category, code and message") {
        const error first(error_code::NOT_FOUND, "a");
        const error second(error_code::NOT_FOUND, "a");
        const error differentMessage(error_code::NOT_FOUND, "b");
        const error differentCode(error_code::TIMEOUT, "a");
        const sample_error sameCode = static_cast<sample_error>(static_cast<u32>(error_code::NOT_FOUND));
        const error differentCategory = error::from(sameCode, "a");

        CHECK(first == second);
        CHECK_FALSE(first == differentMessage);
        CHECK_FALSE(first == differentCode);
        CHECK_FALSE(first == differentCategory);
    }

    TEST_CASE("error - copy and move preserve every field") {
        const error original = make_error(sample_error::WIDGET_JAMMED, "line {}", 3);

        const error copy = original;
        CHECK(copy == original);
        CHECK(copy.category_name() == "sample");
        CHECK(copy.code_name() == "WIDGET_JAMMED");

        error assigned = {};
        assigned = original;
        CHECK(assigned == original);

        error source = make_error(sample_error::WIDGET_JAMMED, "line {}", 3);
        const error moved = std::move(source);
        CHECK(moved == original);
    }

    TEST_CASE("error - errors and codes are formattable, including with format specs") {
        const error value(error_code::TIMEOUT, "slow");

        CHECK(std::format("{}", error_code::TIMEOUT) == "TIMEOUT");
        CHECK(std::format("{}", value) == "core.TIMEOUT: slow");
        CHECK(std::format("{:>12}", error_code::NONE) == "        NONE");
        CHECK(std::format("[{:<20}]", value) == "[core.TIMEOUT: slow  ]");
    }
}
