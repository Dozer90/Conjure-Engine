#include "conjure/testing/doctest.h"

#include "conjure/assets/asset_types.h"
#include "conjure/core/hash.h"

#include <string_view>
#include <type_traits>

using namespace conjure;
using namespace conjure::assets;

namespace {
    struct declared_type {
        int value = 0;
    };
}

CONJURE_DECLARE_ASSET_TYPE(declared_type, "conjure.test.declared");

TEST_SUITE("assets/asset_types") {

    TEST_CASE("make_asset_type_id - is constexpr, stable and distinct per name") {
        constexpr asset_type_id TEXTURE = make_asset_type_id("conjure.renderer.texture");
        constexpr asset_type_id MESH = make_asset_type_id("conjure.renderer.mesh");
        static_assert(TEXTURE != asset_type_id::INVALID);
        static_assert(TEXTURE != MESH);

        CHECK(make_asset_type_id("conjure.renderer.texture") == TEXTURE);
        CHECK(make_asset_type_id("") == asset_type_id::INVALID);
        CHECK(static_cast<u64>(TEXTURE) == core::hash::fnv1a64("conjure.renderer.texture"));
        CHECK(static_cast<u64>(make_asset_type_id("a")) == 0xAF63DC4C8601EC8Cull);
    }

    TEST_CASE("asset_type_flags - operators combine and test bits") {
        asset_type_flags flags = asset_type_flags::NONE;
        CHECK_FALSE(has_flag(flags, asset_type_flags::EXCLUDE_FROM_PACK));

        flags |= asset_type_flags::EXCLUDE_FROM_PACK;
        flags |= asset_type_flags::PINNED_FALLBACK;
        CHECK(has_flag(flags, asset_type_flags::EXCLUDE_FROM_PACK));
        CHECK(has_flag(flags, asset_type_flags::PINNED_FALLBACK));
        CHECK_FALSE(has_flag(flags, asset_type_flags::RELOAD_WITH_DEPENDENCIES));

        flags &= ~asset_type_flags::EXCLUDE_FROM_PACK;
        CHECK_FALSE(has_flag(flags, asset_type_flags::EXCLUDE_FROM_PACK));
        CHECK(
          (asset_type_flags::RELOAD_WITH_DEPENDENCIES | asset_type_flags::EXCLUDE_FROM_PACK) != asset_type_flags::NONE
        );
    }

    TEST_CASE("asset_state - terminal states are LOADED and FAILED") {
        CHECK(is_terminal(asset_state::LOADED));
        CHECK(is_terminal(asset_state::FAILED));
        CHECK_FALSE(is_terminal(asset_state::UNLOADED));
        CHECK_FALSE(is_terminal(asset_state::QUEUED));
        CHECK_FALSE(is_terminal(asset_state::LOADING));
        CHECK_FALSE(is_terminal(asset_state::FINALISING));
        CHECK_FALSE(is_terminal(asset_state::RELOADING));
        CHECK(to_string(asset_state::RELOADING) == "RELOADING");
        CHECK(to_string(asset_state::UNLOADED) == "UNLOADED");
    }

    TEST_CASE("asset_load_priority - levels order LOW < NORMAL < HIGH") {
        CHECK(asset_load_priority::LOW < asset_load_priority::NORMAL);
        CHECK(asset_load_priority::NORMAL < asset_load_priority::HIGH);
        CHECK(ASSET_LOAD_PRIORITY_COUNT == 3);
    }

    TEST_CASE("constants - match the design") {
        CHECK(RETIRE_FRAMES == 2);
        CHECK(MAX_ASSET_TYPES == 256);
        CHECK(DEFAULT_MAX_JOBS_IN_FLIGHT == 8);
        CHECK(DEFAULT_WATCH_INTERVAL_SECONDS == 0.5);
        CHECK(COOKED_BLOB_FORMAT_VERSION == 1);
        CHECK(PACK_FORMAT_VERSION == 1);
        CHECK(META_SCHEMA_VERSION == 1);
    }

    TEST_CASE("CONJURE_DECLARE_ASSET_TYPE - traits expose the name and the matching id") {
        static_assert(declared_asset_type<declared_type>);
        static_assert(!declared_asset_type<int>);
        CHECK(asset_type_traits<declared_type>::NAME == "conjure.test.declared");
        CHECK(asset_type_traits<declared_type>::id() == make_asset_type_id("conjure.test.declared"));
    }

    TEST_CASE("asset_id - the null uuid means no asset") {
        const asset_id none = {};
        CHECK(none.is_null());
        CHECK(none == asset_id::null());
        CHECK_FALSE(asset_id::generate().is_null());
    }
}
