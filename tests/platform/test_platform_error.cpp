#include "conjure/testing/doctest.h"

#include "platform_test_support.h"

#include "conjure/platform/platform_error.h"

#include "conjure/core/error.h"
#include "conjure/core/result.h"

#include <set>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::platform;

TEST_SUITE("platform/platform_error") {

    TEST_CASE("platform_error - every enumerator has a unique upper-case name") {
        std::set<std::string_view> names = {};
        for (u32 code = 1; code < static_cast<u32>(platform_error::COUNT); ++code) {
            CAPTURE(code);
            const std::string_view name = platform_error_name(static_cast<platform_error>(code));
            CHECK_FALSE(name.empty());
            CHECK(names.insert(name).second);
            for (const char character : name) {
                CHECK((character == '_' || (character >= 'A' && character <= 'Z')));
            }
        }
        CHECK(names.size() == static_cast<usize>(platform_error::COUNT) - 1);
    }

    TEST_CASE("platform_error - unknown values have an empty name") {
        CHECK(platform_error_name(platform_error::COUNT).empty());
        CHECK(platform_error_name(static_cast<platform_error>(0)).empty());
        CHECK(platform_error_name(static_cast<platform_error>(0xFFFF)).empty());
    }

    TEST_CASE("platform_error - the enumerator names are the documented ones") {
        CHECK(platform_error_name(platform_error::BACKEND_NOT_COMPILED) == "BACKEND_NOT_COMPILED");
        CHECK(platform_error_name(platform_error::UNKNOWN_BACKEND) == "UNKNOWN_BACKEND");
        CHECK(platform_error_name(platform_error::INIT_FAILED) == "INIT_FAILED");
        CHECK(platform_error_name(platform_error::INVALID_DESC) == "INVALID_DESC");
        CHECK(platform_error_name(platform_error::WINDOW_CREATE_FAILED) == "WINDOW_CREATE_FAILED");
        CHECK(platform_error_name(platform_error::MONITOR_NOT_FOUND) == "MONITOR_NOT_FOUND");
        CHECK(platform_error_name(platform_error::VULKAN_UNSUPPORTED) == "VULKAN_UNSUPPORTED");
        CHECK(platform_error_name(platform_error::SURFACE_CREATE_FAILED) == "SURFACE_CREATE_FAILED");
        CHECK(platform_error_name(platform_error::CLIPBOARD_UNAVAILABLE) == "CLIPBOARD_UNAVAILABLE");
        CHECK(platform_error_name(platform_error::FEATURE_UNAVAILABLE) == "FEATURE_UNAVAILABLE");
        CHECK(static_cast<u32>(platform_error::BACKEND_NOT_COMPILED) == 1);
    }

    TEST_CASE("platform_error - make_error binds the platform category") {
        const error failure = make_error(platform_error::INIT_FAILED, "glfwInit failed");

        CHECK(failure.is(platform_error::INIT_FAILED));
        CHECK_FALSE(failure.is(platform_error::INVALID_DESC));
        CHECK(failure.is_category<platform_error>());
        CHECK_FALSE(failure.is_category<error_code>());
        CHECK(failure.category_name() == "platform");
        CHECK(failure.code() == static_cast<u32>(platform_error::INIT_FAILED));
        CHECK(failure.code_name() == "INIT_FAILED");
        CHECK(failure.generic_code() == error_code::NONE);
        CHECK(failure.message() == "glfwInit failed");
    }

    TEST_CASE("platform_error - to_string formats category, code and message") {
        const error notFound = make_error(platform_error::MONITOR_NOT_FOUND, "index 3");
        CHECK(notFound.to_string() == "platform.MONITOR_NOT_FOUND: index 3");
        CHECK(make_error(platform_error::CLIPBOARD_UNAVAILABLE).to_string() == "platform.CLIPBOARD_UNAVAILABLE");
        CHECK(make_error(platform_error::INVALID_DESC, "{} x {}", 0, 0).message() == "0 x 0");
    }

    TEST_CASE("platform_error - a result carrying a platform error is detected by the test helpers") {
        const result<int> failed = make_error(platform_error::FEATURE_UNAVAILABLE, "no raw motion");
        CHECK(testing::has_error(failed, platform_error::FEATURE_UNAVAILABLE));
        CHECK_FALSE(testing::has_error(failed, platform_error::INIT_FAILED));
        CONJURE_CHECK_ERROR(failed, platform_error::FEATURE_UNAVAILABLE);
    }
}
