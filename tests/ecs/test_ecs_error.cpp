#include "conjure/testing/doctest.h"

#include "conjure/core/error.h"
#include "conjure/core/hash.h"
#include "conjure/core/result.h"
#include "conjure/ecs/ecs_error.h"

#include <string_view>

using namespace conjure;
using namespace conjure::ecs;

TEST_SUITE("ecs/ecs_error") {

    TEST_CASE("ecs_error - every code has a name") {
        for (u16 value = 0; value < static_cast<u16>(ecs_error::COUNT); ++value) {
            CAPTURE(value);
            CHECK_FALSE(ecs_error_name(static_cast<ecs_error>(value)).empty());
        }
        CHECK(ecs_error_name(ecs_error::ENTITY_NOT_ALIVE) == "ENTITY_NOT_ALIVE");
        CHECK(ecs_error_name(ecs_error::SYSTEM_NOT_FOUND) == "SYSTEM_NOT_FOUND");
        CHECK(ecs_error_name(ecs_error::COUNT).empty());
        CHECK(ecs_error_name(static_cast<ecs_error>(999)).empty());
    }

    TEST_CASE("ecs_error - make_error binds the ecs category") {
        const core::error failure = make_error(ecs_error::STRUCTURE_LOCKED, "locked");
        CHECK(failure.is(ecs_error::STRUCTURE_LOCKED));
        CHECK_FALSE(failure.is(ecs_error::WRONG_THREAD));
        CHECK(failure.is_category<ecs_error>());
        CHECK(failure.category() == core::hash::fnv1a32("ecs"));
        CHECK(failure.category_name() == "ecs");
        CHECK(failure.code_name() == "STRUCTURE_LOCKED");
        CHECK(failure.message() == "locked");
        CHECK(failure.to_string() == "ecs.STRUCTURE_LOCKED: locked");
        CHECK(failure.generic_code() == core::error_code::NONE);

        const core::error bare = make_error(ecs_error::ARCHIVE_CORRUPT);
        CHECK(bare.message().empty());
        CHECK(bare.to_string() == "ecs.ARCHIVE_CORRUPT");

        const core::error formatted = core::make_error(ecs_error::COMPONENT_LIMIT_REACHED, "{} types", 1024);
        CHECK(formatted.message() == "1024 types");
    }

    TEST_CASE("ecs_error - travels through result") {
        const core::result<int> failed = make_error(ecs_error::ENTITY_NOT_ALIVE, "gone");
        CHECK_FALSE(failed.has_value());
        CONJURE_CHECK_ERROR(failed, ecs_error::ENTITY_NOT_ALIVE);
        CHECK(testing::describe_error(failed) == "ecs.ENTITY_NOT_ALIVE: gone");
    }
}
