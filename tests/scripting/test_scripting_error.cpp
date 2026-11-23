#include "conjure/testing/doctest.h"

#include "conjure/core/error.h"
#include "conjure/core/hash.h"
#include "conjure/scripting/scripting_error.h"

#include <string>

using namespace conjure;
using namespace conjure::scripting;

TEST_SUITE("scripting/scripting_error") {

    TEST_CASE("script_error - every code has a name and COUNT has none") {
        for (u16 code = 1; code < static_cast<u16>(script_error::COUNT); ++code) {
            CAPTURE(code);
            CHECK_FALSE(script_error_name(static_cast<script_error>(code)).empty());
        }
        CHECK(script_error_name(script_error::COUNT).empty());
        CHECK(script_error_name(static_cast<script_error>(0)).empty());
        CHECK(script_error_name(script_error::ABI_MISMATCH) == "ABI_MISMATCH");
        CHECK(script_error_name(script_error::NOT_SUPPORTED) == "NOT_SUPPORTED");
    }

    TEST_CASE("script_error - NOT_INITIALISED keeps the code 16 that managed code maps by number") {
        // The rename to the British spelling changed the name only: Conjure.Managed (ComponentQuery.BeginFailure)
        // reads the negated number, so the value must not move.
        CHECK(static_cast<u16>(script_error::NOT_INITIALISED) == 16);
        CHECK(status_of(script_error::NOT_INITIALISED) == -16);
        CHECK(script_error_name(script_error::NOT_INITIALISED) == "NOT_INITIALISED");
        const core::error failure = core::make_error(script_error::NOT_INITIALISED, "no runtime");
        CHECK(failure.to_string() == "scripting.NOT_INITIALISED: no runtime");
    }

    TEST_CASE("script_error - binds to the scripting category") {
        const core::error failure = core::make_error(script_error::CLASS_NOT_FOUND, "Game.Missing");
        CHECK(failure.category_name() == "scripting");
        CHECK(failure.category() == core::hash::fnv1a32("scripting"));
        CHECK(failure.code_name() == "CLASS_NOT_FOUND");
        CHECK(failure.is(script_error::CLASS_NOT_FOUND));
        CHECK_FALSE(failure.is(script_error::FIELD_NOT_FOUND));
        CHECK(failure.is_category<script_error>());
        CHECK(failure.to_string() == "scripting.CLASS_NOT_FOUND: Game.Missing");
    }

    TEST_CASE("script_error - formatted messages carry their arguments") {
        const core::error failure = core::make_error(
          script_error::FIELD_NOT_FOUND,
          "field {} of {}",
          "speed",
          "Player"
        );
        CHECK(failure.message() == "field speed of Player");
    }

    TEST_CASE("status_of - is the negated code and never zero") {
        CHECK(status_of(script_error::RUNTIME_NOT_FOUND) == -1);
        CHECK(status_of(script_error::NOT_SUPPORTED) == -static_cast<i32>(script_error::NOT_SUPPORTED));
        for (u16 code = 1; code < static_cast<u16>(script_error::COUNT); ++code) {
            CAPTURE(code);
            CHECK(status_of(static_cast<script_error>(code)) < 0);
        }
    }
}
