#include "conjure/testing/doctest.h"

#include "conjure/core/error.h"
#include "conjure/physics/physics_error.h"

#include <string>

using namespace conjure;
using physics::physics_error;

TEST_SUITE("physics/physics_error") {

    TEST_CASE("physics_error - every code has an upper-case name") {
        for (u32 code = 0; code < static_cast<u32>(physics_error::COUNT); ++code) {
            CAPTURE(code);
            const std::string_view name = physics::physics_error_name(static_cast<physics_error>(code));
            CHECK_FALSE(name.empty());
            for (const char character : name) {
                CHECK((character == '_' || (character >= 'A' && character <= 'Z')));
            }
        }
        CHECK(physics::physics_error_name(physics_error::COUNT).empty());
        CHECK(physics::physics_error_name(static_cast<physics_error>(4242)).empty());
        CHECK(physics::physics_error_name(physics_error::NO_BODY) == "NO_BODY");
        CHECK(physics::physics_error_name(physics_error::COMMAND_QUEUE_FULL) == "COMMAND_QUEUE_FULL");
        CHECK(physics::physics_error_name(physics_error::WRONG_BODY_KIND) == "WRONG_BODY_KIND");
        // Appended after the last code that existed, so no earlier code changed its value.
        CHECK(
          static_cast<u32>(physics_error::WRONG_BODY_KIND) == static_cast<u32>(physics_error::COMMAND_QUEUE_FULL) + 1
        );
        CHECK(static_cast<u32>(physics_error::COUNT) == 22);
    }

    TEST_CASE("physics_error - category name is physics and is() matches the code") {
        const core::error failure = core::make_error(physics_error::NO_BODY);
        CHECK(failure.category_name() == "physics");
        CHECK(failure.code_name() == "NO_BODY");
        CHECK(failure.is(physics_error::NO_BODY));
        CHECK_FALSE(failure.is(physics_error::NOT_A_CHARACTER));
        CHECK(failure.is_category<physics_error>());
        CHECK_FALSE(failure.is(core::error_code::NOT_FOUND));
        CHECK(failure.to_string() == "physics.NO_BODY");

        // A body of the wrong kind is its own code: the entity has a body, so it is not NO_BODY.
        const core::error wrongKind = core::make_error(physics_error::WRONG_BODY_KIND);
        CHECK(wrongKind.is(physics_error::WRONG_BODY_KIND));
        CHECK_FALSE(wrongKind.is(physics_error::NO_BODY));
        CHECK_FALSE(wrongKind.is(physics_error::NOT_A_CHARACTER));
        CHECK(wrongKind.to_string() == "physics.WRONG_BODY_KIND");
    }

    TEST_CASE("physics_error - make_error formats the detail") {
        const core::error failure = core::make_error(
          physics_error::INVALID_SETTINGS,
          "{}: must be at least {}",
          "maxJobs",
          64
        );
        CHECK(failure.message() == "maxJobs: must be at least 64");
        CHECK(failure.to_string() == "physics.INVALID_SETTINGS: maxJobs: must be at least 64");
    }

    TEST_CASE("physics_error - errors are built with core::make_error, no factory of the module's own") {
        // The error pattern of code sweep F625, which this module already followed: a guard, not a regression.
        CHECK_FALSE(testing::declares_own_make_error<physics_error>);
    }
}
