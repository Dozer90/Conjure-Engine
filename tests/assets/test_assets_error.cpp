#include "conjure/testing/doctest.h"

#include "conjure/assets/assets_error.h"

#include <string_view>

using namespace conjure;
using namespace conjure::assets;

TEST_SUITE("assets/assets_error") {

    TEST_CASE("asset_error_code - every code has a stable upper-case name") {
        CHECK(asset_error_code_name(asset_error_code::NONE) == "NONE");
        CHECK(asset_error_code_name(asset_error_code::NOT_FOUND) == "NOT_FOUND");
        CHECK(asset_error_code_name(asset_error_code::CACHE_MISS) == "CACHE_MISS");
        CHECK(asset_error_code_name(asset_error_code::CYCLIC_DEPENDENCY) == "CYCLIC_DEPENDENCY");
        CHECK(asset_error_code_name(asset_error_code::TIMEOUT) == "TIMEOUT");
        for (u16 code = 0; code < static_cast<u16>(asset_error_code::COUNT); ++code) {
            CAPTURE(code);
            CHECK_FALSE(asset_error_code_name(static_cast<asset_error_code>(code)).empty());
        }
        CHECK(asset_error_code_name(asset_error_code::COUNT).empty());
    }

    TEST_CASE("make_asset_error - carries the assets category, the code and the message") {
        const core::error failure = make_asset_error(asset_error_code::CACHE_CORRUPT, "bad bytes");

        CHECK(failure.category_name() == "assets");
        CHECK(failure.is(asset_error_code::CACHE_CORRUPT));
        CHECK_FALSE(failure.is(asset_error_code::CACHE_MISS));
        CHECK(failure.is_category<asset_error_code>());
        CHECK(failure.code_name() == "CACHE_CORRUPT");
        CHECK(failure.message() == "bad bytes");
        CHECK(failure.to_string() == "assets.CACHE_CORRUPT: bad bytes");
        CHECK(failure.generic_code() == core::error_code::NONE);
    }

    TEST_CASE("make_asset_error - the message is optional") {
        const core::error failure = make_asset_error(asset_error_code::NO_LOADER);
        CHECK(failure.message().empty());
        CHECK(failure.to_string() == "assets.NO_LOADER");
    }
}
