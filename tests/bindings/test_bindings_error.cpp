#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"

#include "conjure/assets/assets_error.h"
#include "conjure/bindings/bindings_error.h"
#include "conjure/core/error.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/platform/platform_error.h"
#include "conjure/scene/scene_error.h"

#include <limits>
#include <string>
#include <string_view>

// bindings_error.h (docs/design/15_bindings.md 2.2, B4): the status convention, the fixed last-error
// buffer and the mapping of other modules' errors.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;

TEST_SUITE("bindings/bindings_error") {

    TEST_CASE("bindings_error - status is the negative code and NONE is zero") {
        CHECK(to_status(bindings_error::NONE) == 0);
        CHECK(to_status(bindings_error::NOT_SUPPORTED) == -1);
        CHECK(to_status(bindings_error::ENTITY_NOT_ALIVE) == -2);
        CHECK(to_status(bindings_error::INTERNAL) == -static_cast<int32_t>(bindings_error::INTERNAL));
        CHECK(from_status(0) == bindings_error::NONE);
        CHECK(from_status(42) == bindings_error::NONE);
        CHECK(from_status(-2) == bindings_error::ENTITY_NOT_ALIVE);
        CHECK(from_status(-1000) == bindings_error::INTERNAL);
        CHECK(BINDINGS_ERROR_COUNT == 16);
    }

    TEST_CASE("bindings_error - from_status of INT32_MIN is INTERNAL") {
        // A compile error while the negation overflowed: a constant expression may not contain undefined behaviour.
        static_assert(from_status(std::numeric_limits<int32_t>::min()) == bindings_error::INTERNAL);
        volatile int32_t lowest = std::numeric_limits<int32_t>::min();
        CHECK(from_status(lowest) == bindings_error::INTERNAL);
        CHECK(from_status(std::numeric_limits<int32_t>::min() + 1) == bindings_error::INTERNAL);
        const int32_t lastCode = -static_cast<int32_t>(BINDINGS_ERROR_COUNT - 1);
        CHECK(from_status(lastCode) == static_cast<bindings_error>(BINDINGS_ERROR_COUNT - 1));
        CHECK(from_status(lastCode - 1) == bindings_error::INTERNAL);
    }

    TEST_CASE("bindings_error - fail sets the message without allocating") {
        clear_last_error();
        int32_t status = 0;
        CONJURE_CHECK_NO_ALLOCATIONS(
          status = fail(bindings_error::COMPONENT_MISSING, "transform_set_local", "local_transform")
        );
        CHECK(status == to_status(bindings_error::COMPONENT_MISSING));
        CHECK(last_error_code() == bindings_error::COMPONENT_MISSING);
        CHECK(last_error_message() == "transform_set_local: local_transform");

        CONJURE_CHECK_NO_ALLOCATIONS(status = fail(bindings_error::WRONG_THREAD, "entity_get_parent"));
        CHECK(last_error_message() == "entity_get_parent");
        CHECK(last_error_code() == bindings_error::WRONG_THREAD);
    }

    TEST_CASE("bindings_error - message truncates at 511 bytes") {
        const std::string detail(2000, 'x');
        CHECK(fail(bindings_error::INTERNAL, "entry", detail) == to_status(bindings_error::INTERNAL));
        const std::string_view message = last_error_message();
        CHECK(message.size() == LAST_ERROR_CAPACITY - 1);
        CHECK(message.starts_with("entry: xxx"));
        CHECK(message.back() == 'x');
        clear_last_error();
    }

    TEST_CASE("bindings_error - core error mapping table") {
        CHECK(map_error(core::make_error(ecs::ecs_error::ENTITY_NOT_ALIVE)) == bindings_error::ENTITY_NOT_ALIVE);
        CHECK(map_error(core::make_error(ecs::ecs_error::STRUCTURE_LOCKED)) == bindings_error::STRUCTURE_LOCKED);
        CHECK(map_error(core::make_error(ecs::ecs_error::WRONG_THREAD)) == bindings_error::WRONG_THREAD);
        CHECK(map_error(core::make_error(ecs::ecs_error::COMPONENT_NOT_PRESENT)) == bindings_error::COMPONENT_MISSING);
        CHECK(map_error(core::make_error(assets::asset_error_code::NOT_FOUND)) == bindings_error::NOT_FOUND);
        CHECK(
          map_error(core::make_error(assets::asset_error_code::TYPE_MISMATCH)) == bindings_error::ASSET_TYPE_MISMATCH
        );
        CHECK(map_error(core::make_error(scene::scene_error::INVALID_ENTITY)) == bindings_error::ENTITY_NOT_ALIVE);
        CHECK(map_error(core::make_error(scene::scene_error::SCENE_NOT_LOADED)) == bindings_error::SCENE_NOT_FOUND);
        CHECK(map_error(core::make_error(scene::scene_error::PREFAB_NOT_LOADED)) == bindings_error::ASSET_NOT_LOADED);
        CHECK(map_error(core::make_error(scene::scene_error::NO_TRANSFORM)) == bindings_error::COMPONENT_MISSING);
        CHECK(map_error(core::make_error(scene::scene_error::PREFAB_MULTIPLE_ROOTS)) == bindings_error::PREFAB_INVALID);
        // Hierarchy requests refused for their arguments are the caller's mistake (ArgumentException), 6.
        CHECK(map_error(core::make_error(scene::scene_error::SELF_PARENT)) == bindings_error::INVALID_ARGUMENT);
        CHECK(map_error(core::make_error(scene::scene_error::HIERARCHY_CYCLE)) == bindings_error::INVALID_ARGUMENT);
        CHECK(map_error(core::make_error(scene::scene_error::HIERARCHY_TOO_DEEP)) == bindings_error::INVALID_ARGUMENT);
        CHECK(
          map_error(core::make_error(scene::scene_error::INVALID_SIBLING_INDEX)) == bindings_error::INVALID_ARGUMENT
        );
        CHECK(map_error(core::make_error(scene::scene_error::DUPLICATE_GUID)) == bindings_error::INTERNAL);
        CHECK(
          map_error(core::make_error(platform::platform_error::CLIPBOARD_UNAVAILABLE)) == bindings_error::NOT_FOUND
        );
        CHECK(
          map_error(core::make_error(platform::platform_error::FEATURE_UNAVAILABLE)) == bindings_error::NOT_SUPPORTED
        );
        CHECK(map_error(core::error(core::error_code::INVALID_ARGUMENT)) == bindings_error::INVALID_ARGUMENT);
        CHECK(map_error(core::error(core::error_code::NOT_FOUND)) == bindings_error::NOT_FOUND);
        CHECK(map_error(core::error(core::error_code::UNSUPPORTED)) == bindings_error::NOT_SUPPORTED);
        CHECK(map_error(core::error(core::error_code::TIMEOUT)) == bindings_error::INTERNAL);
        CHECK(map_error(core::make_error(ecs::ecs_error::ARCHIVE_CORRUPT)) == bindings_error::INTERNAL);
        CHECK(map_error(core::make_error(bindings_error::INVALID_ARGUMENT)) == bindings_error::INVALID_ARGUMENT);
        CHECK(map_error(core::make_error(bindings_error::SCENE_NOT_FOUND)) == bindings_error::SCENE_NOT_FOUND);

        // The error form records the module's category, code and message as the detail.
        const int32_t status =
          fail(core::make_error(assets::asset_error_code::TIMEOUT, "waited 30 s"), "asset_load_blocking");
        CHECK(status == to_status(bindings_error::INTERNAL));
        CHECK(last_error_message() == "asset_load_blocking: assets.TIMEOUT: waited 30 s");
        CHECK(
          fail(core::make_error(ecs::ecs_error::ENTITY_NOT_ALIVE), "x") == to_status(bindings_error::ENTITY_NOT_ALIVE)
        );
        CHECK(last_error_message() == "x: ecs.ENTITY_NOT_ALIVE");
        clear_last_error();
    }

    TEST_CASE("bindings_error - clear resets the state") {
        CHECK(fail(bindings_error::NOT_FOUND, "modules_find", "nope") < 0);
        clear_last_error();
        CHECK(last_error_code() == bindings_error::NONE);
        CHECK(last_error_message().empty());
    }

    TEST_CASE("bindings_error - every enumerator has a name and the category is bound") {
        for (u32 index = 0; index < BINDINGS_ERROR_COUNT; ++index) {
            CAPTURE(index);
            CHECK_FALSE(bindings_error_name(static_cast<bindings_error>(index)).empty());
        }
        CHECK(bindings_error_name(bindings_error::NONE) == "NONE");
        CHECK(bindings_error_name(bindings_error::TABLE_INCOMPLETE) == "TABLE_INCOMPLETE");
        CHECK(bindings_error_name(bindings_error::COUNT).empty());
        const core::error failure = core::make_error(bindings_error::STRUCTURE_LOCKED, "inside a scope");
        CHECK(failure.category_name() == "bindings");
        CHECK(failure.code_name() == "STRUCTURE_LOCKED");
        CHECK(failure.is(bindings_error::STRUCTURE_LOCKED));
        CHECK(failure.to_string() == "bindings.STRUCTURE_LOCKED: inside a scope");
    }
}
