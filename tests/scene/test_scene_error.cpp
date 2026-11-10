#include "conjure/testing/doctest.h"

#include "conjure/core/error.h"
#include "conjure/scene/scene_error.h"

#include <string>

using namespace conjure;
using scene::scene_error;

TEST_SUITE("scene/scene_error") {

    TEST_CASE("scene_error - every code has a name") {
        for (u32 code = 0; code < static_cast<u32>(scene_error::COUNT); ++code) {
            CAPTURE(code);
            const std::string_view name = scene::scene_error_name(static_cast<scene_error>(code));
            CHECK_FALSE(name.empty());
            for (const char character : name) {
                CHECK((character == '_' || (character >= 'A' && character <= 'Z')));
            }
        }
        CHECK(scene::scene_error_name(scene_error::COUNT).empty());
        CHECK(scene::scene_error_name(static_cast<scene_error>(900)).empty());
        CHECK(scene::scene_error_name(scene_error::HIERARCHY_CYCLE) == "HIERARCHY_CYCLE");
        CHECK(scene::scene_error_name(scene_error::BUILDER_INVALID) == "BUILDER_INVALID");
    }

    TEST_CASE("scene_error - category name is scene") {
        const core::error failure = core::make_error(scene_error::DUPLICATE_GUID);
        CHECK(failure.category_name() == "scene");
        CHECK(failure.code_name() == "DUPLICATE_GUID");
        CHECK(failure.is(scene_error::DUPLICATE_GUID));
        CHECK_FALSE(failure.is(scene_error::GUID_NOT_FOUND));
        CHECK(failure.is_category<scene_error>());
        CHECK_FALSE(failure.is(core::error_code::NOT_FOUND));
        CHECK(failure.to_string() == "scene.DUPLICATE_GUID");
    }

    TEST_CASE("scene_error - make_error formats detail") {
        const core::error failure = core::make_error(scene_error::HIERARCHY_TOO_DEEP, "entity {} at depth {}", 7, 32);
        CHECK(failure.message() == "entity 7 at depth 32");
        CHECK(failure.to_string() == "scene.HIERARCHY_TOO_DEEP: entity 7 at depth 32");
        const core::error plain = core::make_error(scene_error::SELF_PARENT, "fixed detail");
        CHECK(plain.message() == "fixed detail");
    }
}
