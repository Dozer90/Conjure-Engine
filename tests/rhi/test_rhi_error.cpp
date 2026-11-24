#include "conjure/testing/doctest.h"

#include "conjure/rhi/rhi_error.h"

#include "conjure/core/error.h"
#include "conjure/core/result.h"

#include <string_view>

using namespace conjure;
using namespace conjure::rhi;

TEST_SUITE("rhi/rhi_error") {

    TEST_CASE("rhi_error - rhi_error_name names every code and nothing else") {
        CHECK(rhi_error_name(rhi_error::DEVICE_CREATION_FAILED) == "DEVICE_CREATION_FAILED");
        CHECK(rhi_error_name(rhi_error::DEVICE_LOST) == "DEVICE_LOST");
        CHECK(rhi_error_name(rhi_error::OUT_OF_DATE) == "OUT_OF_DATE");
        CHECK(rhi_error_name(rhi_error::SWAPCHAIN_CREATION_FAILED) == "SWAPCHAIN_CREATION_FAILED");
        CHECK(rhi_error_name(rhi_error::INVALID_DESC) == "INVALID_DESC");
        CHECK(rhi_error_name(rhi_error::PRESENT_FAILED) == "PRESENT_FAILED");
        CHECK(rhi_error_name(rhi_error::COUNT) == "");
        CHECK(rhi_error_name(static_cast<rhi_error>(0)) == "");
    }

    TEST_CASE("rhi_error - make_error binds the codes to the rhi category") {
        const core::error failure = core::make_error(rhi_error::DEVICE_LOST, "adapter {} removed", 1);
        CHECK(failure.category_name() == "rhi");
        CHECK(failure.code_name() == "DEVICE_LOST");
        CHECK(failure.message() == "adapter 1 removed");
        CHECK(failure.is(rhi_error::DEVICE_LOST));
        CHECK_FALSE(failure.is(rhi_error::OUT_OF_DATE));
        CHECK_FALSE(failure.is(rhi_error::PRESENT_FAILED));
        CHECK_FALSE(failure.is(core::error_code::UNSUPPORTED));
        CHECK(failure.is_category<rhi_error>());
        CHECK(failure.generic_code() == core::error_code::NONE);
        CHECK(failure.to_string() == "rhi.DEVICE_LOST: adapter 1 removed");
    }

    TEST_CASE("rhi_error - a result carrying an rhi error works with the test macros") {
        const core::result<int> failed = core::make_error(rhi_error::INVALID_DESC, "width is zero");
        CONJURE_CHECK_ERROR(failed, rhi_error::INVALID_DESC);
        CHECK_FALSE(failed.has_value());
    }

    TEST_CASE("rhi_error - errors are built with core::make_error, no factory of the module's own") {
        // The error pattern of code sweep F625, which this module already followed: a guard, not a regression.
        CHECK_FALSE(testing::declares_own_make_error<rhi_error>);
    }
}
