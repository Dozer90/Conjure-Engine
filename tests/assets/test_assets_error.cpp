#include "conjure/testing/doctest.h"

#include "conjure/assets/assets_error.h"

#include <string_view>

using namespace conjure;
using namespace conjure::assets;

namespace {
    // True again if the factory that code sweep F625 removed comes back: the module builds its errors with
    // core::make_error. The call is dependent, so the name need not exist for the concept to be false.
    template <typename EnumType>
    concept declares_make_asset_error = requires(EnumType code) { make_asset_error(code); };
}

TEST_SUITE("assets/assets_error") {

    TEST_CASE("assets_error - every code has a stable upper-case name") {
        CHECK(assets_error_name(assets_error::NONE) == "NONE");
        CHECK(assets_error_name(assets_error::NOT_FOUND) == "NOT_FOUND");
        CHECK(assets_error_name(assets_error::CACHE_MISS) == "CACHE_MISS");
        CHECK(assets_error_name(assets_error::CYCLIC_DEPENDENCY) == "CYCLIC_DEPENDENCY");
        CHECK(assets_error_name(assets_error::TIMEOUT) == "TIMEOUT");
        for (u16 code = 0; code < static_cast<u16>(assets_error::COUNT); ++code) {
            CAPTURE(code);
            CHECK_FALSE(assets_error_name(static_cast<assets_error>(code)).empty());
        }
        CHECK(assets_error_name(assets_error::COUNT).empty());
    }

    TEST_CASE("assets_error - make_error carries the assets category, the code and the message") {
        const core::error failure = core::make_error(assets_error::CACHE_CORRUPT, "bad bytes");

        CHECK(failure.category_name() == "assets");
        CHECK(failure.is(assets_error::CACHE_CORRUPT));
        CHECK_FALSE(failure.is(assets_error::CACHE_MISS));
        CHECK(failure.is_category<assets_error>());
        CHECK(failure.code_name() == "CACHE_CORRUPT");
        CHECK(failure.message() == "bad bytes");
        CHECK(failure.to_string() == "assets.CACHE_CORRUPT: bad bytes");
        CHECK(failure.generic_code() == core::error_code::NONE);
    }

    TEST_CASE("assets_error - the message of make_error is optional") {
        const core::error failure = core::make_error(assets_error::NO_LOADER);
        CHECK(failure.message().empty());
        CHECK(failure.to_string() == "assets.NO_LOADER");
    }

    TEST_CASE("regression - code sweep F625 assets_error has no factory beside core::make_error") {
        CHECK_FALSE(declares_make_asset_error<assets_error>);
        CHECK_FALSE(testing::declares_own_make_error<assets_error>);
    }
}
